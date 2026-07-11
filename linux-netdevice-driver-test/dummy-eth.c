// dummy-eth.c
/*
 * A minimal virtual ethernet netdevice ("deth0").
 *
 * Frames transmitted on the device are pushed to the simulated HW layer
 * (dummy-hw.c), which loops them back into dummy_eth_rx().  That callback acts
 * as a reflector:
 *
 *   - ARP requests are answered with a synthetic MAC for the requested IP.
 *   - ICMP echo requests are turned into echo replies.
 *
 * As a result, once an address is configured on the interface you can ping any
 * peer in its subnet and it will answer.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/u64_stats_sync.h>
#include <linux/rtnetlink.h>
#include <net/rtnetlink.h>
#include <net/checksum.h>

#include "dummy.h"

#define DRV_NAME    "deth"
#define DRV_VERSION "1.0"
#define DRV_AUTHOR  "linux-driver-test"

/* Per-CPU statistics kept in the private area. */
//struct pcpu_dstats {
//    u64 rx_packets;
//    u64 rx_bytes;
//    u64 tx_packets;
//    u64 tx_bytes;
//    struct u64_stats_sync syncp;
//};

struct dummy_priv {
    struct pcpu_dstats __percpu *dstats;
};

/* ------------------------------------------------------------------ */
/* RX reflector                                                        */
/* ------------------------------------------------------------------ */

/*
 * Build a stable, locally-administered synthetic MAC for a given IPv4 address
 * so that the kernel can resolve ARP for our "peers".
 */
static void dummy_fake_mac(const unsigned char *ip4, unsigned char *mac)
{
    mac[0] = 0x02; /* locally administered, unicast */
    mac[1] = 0xde;
    mac[2] = ip4[0];
    mac[3] = ip4[1];
    mac[4] = ip4[2];
    mac[5] = ip4[3];
}

static int dummy_arp_reply(struct sk_buff *skb)
{
    struct ethhdr *eth = (struct ethhdr *)skb->data;
    struct arphdr *arp = (struct arphdr *)(skb->data + ETH_HLEN);
    unsigned char *p;
    unsigned char sha[ETH_ALEN], sip[4], tip[4];
    unsigned char fake[ETH_ALEN];

    if (!pskb_may_pull(skb, ETH_HLEN + sizeof(*arp) + 2 * (ETH_ALEN + 4)))
        return -EINVAL;

    /* Only respond to "who has <tip>" requests for ethernet/IPv4. */
    if (arp->ar_op != htons(ARPOP_REQUEST) ||
        arp->ar_pro != htons(ETH_P_IP) ||
        arp->ar_hln != ETH_ALEN || arp->ar_pln != 4)
        return -EINVAL;

    p = (unsigned char *)(arp + 1);
    memcpy(sha, p, ETH_ALEN);            /* requester HW  */
    memcpy(sip, p + ETH_ALEN, 4);        /* requester IP  */
    memcpy(tip, p + ETH_ALEN + 4 + ETH_ALEN, 4); /* wanted IP */

    dummy_fake_mac(tip, fake);

    /* Turn the request into a reply, sender = the requested IP/fake MAC. */
    arp->ar_op = htons(ARPOP_REPLY);
    memcpy(p, fake, ETH_ALEN);                     /* sender HW  */
    memcpy(p + ETH_ALEN, tip, 4);                  /* sender IP  */
    memcpy(p + ETH_ALEN + 4, sha, ETH_ALEN);       /* target HW  */
    memcpy(p + ETH_ALEN + 4 + ETH_ALEN, sip, 4);   /* target IP  */

    /* Deliver back to us: dst = original requester, src = fake. */
    memcpy(eth->h_dest, sha, ETH_ALEN);
    memcpy(eth->h_source, fake, ETH_ALEN);

    return 0;
}

static int dummy_icmp_reply(struct sk_buff *skb)
{
    struct ethhdr *eth = (struct ethhdr *)skb->data;
    struct iphdr *iph = (struct iphdr *)(skb->data + ETH_HLEN);
    struct icmphdr *icmph;
    unsigned char mac[ETH_ALEN];
    __be32 addr;
    unsigned int ihl, icmp_len;

    if (!pskb_may_pull(skb, ETH_HLEN + sizeof(*iph)))
        return -EINVAL;

    if (iph->protocol != IPPROTO_ICMP)
        return -EINVAL;

    ihl = iph->ihl * 4;
    icmph = (struct icmphdr *)((unsigned char *)iph + ihl);

    if (!pskb_may_pull(skb, ETH_HLEN + ihl + sizeof(*icmph)))
        return -EINVAL;

    if (icmph->type != ICMP_ECHO)
        return -EINVAL;

    print_hex_dump(KERN_DEBUG, "DETH B: ", DUMP_PREFIX_OFFSET, 16, 1,
                   skb->data, skb->len, 0);

    /* Swap MAC addresses so the frame comes back to us. */
    memcpy(mac, eth->h_dest, ETH_ALEN);
    memcpy(eth->h_dest, eth->h_source, ETH_ALEN);
    memcpy(eth->h_source, mac, ETH_ALEN);

    /* Swap IP addresses. */
    addr = iph->daddr;
    iph->daddr = iph->saddr;
    iph->saddr = addr;

    /* ICMP echo -> echo reply. */
    icmph->type = ICMP_ECHOREPLY;

    /* Recompute the IP header checksum. */
    iph->check = 0;
    iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

    /* Recompute the ICMP checksum (type changed). */
    icmp_len = ntohs(iph->tot_len) - ihl;
    icmph->checksum = 0;
    icmph->checksum = csum_fold(csum_partial(icmph, icmp_len, 0));

    print_hex_dump(KERN_DEBUG, "DETH A: ", DUMP_PREFIX_OFFSET, 16, 1,
                   skb->data, skb->len, 0);

    return 0;
}

int32_t dummy_eth_rx(struct sk_buff *skb)
{
    struct ethhdr *eth;
    struct net_device *dev;
    struct dummy_priv *priv;
    struct pcpu_dstats *dstats;
    unsigned int len;
    int ret = -EINVAL;

    if (skb == NULL) {
        printk(KERN_ERR "DETH: skb is null\n");
        return -EINVAL;
    }

    dev = skb->dev;
    if (!pskb_may_pull(skb, ETH_HLEN))
        goto free;

    eth = (struct ethhdr *)skb->data;
    switch (ntohs(eth->h_proto)) {
    case ETH_P_ARP:
        ret = dummy_arp_reply(skb);
        break;
    case ETH_P_IP:
        ret = dummy_icmp_reply(skb);
        break;
    default:
        break;
    }

    if (ret)
        goto free;

    len = skb->len;

    /* Reinject into the stack as a received frame. */
    skb->pkt_type = PACKET_HOST;
    skb->protocol = eth_type_trans(skb, dev);

    if (dev) {
        priv = netdev_priv(dev);
        dstats = this_cpu_ptr(priv->dstats);
        u64_stats_update_begin(&dstats->syncp);
        dstats->rx_packets++;
        dstats->rx_bytes += len;
        u64_stats_update_end(&dstats->syncp);
    }

    netif_rx(skb);
    return 0;

free:
    dev_kfree_skb(skb);
    return ret;
}

/* ------------------------------------------------------------------ */
/* netdev ops                                                          */
/* ------------------------------------------------------------------ */

static int dummy_dev_init(struct net_device *dev)
{
    struct dummy_priv *priv = netdev_priv(dev);

    priv->dstats = netdev_alloc_pcpu_stats(struct pcpu_dstats);
    if (!priv->dstats)
        return -ENOMEM;

    return 0;
}

static void dummy_dev_uninit(struct net_device *dev)
{
    struct dummy_priv *priv = netdev_priv(dev);

    free_percpu(priv->dstats);
}

static int dummy_open(struct net_device *dev)
{
    printk(KERN_INFO "DETH: %s() - called\n", __func__);
    netif_carrier_on(dev);
    netif_start_queue(dev);

    return 0;
}

static int dummy_close(struct net_device *dev)
{
    printk(KERN_INFO "DETH: %s() - called\n", __func__);
    netif_stop_queue(dev);
    netif_carrier_off(dev);

    return 0;
}

static netdev_tx_t dummy_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct dummy_priv *priv = netdev_priv(dev);
    struct pcpu_dstats *dstats = this_cpu_ptr(priv->dstats);

    u64_stats_update_begin(&dstats->syncp);
    dstats->tx_packets++;
    dstats->tx_bytes += skb->len;
    u64_stats_update_end(&dstats->syncp);

    skb_tx_timestamp(skb);

    /* Call HW xmit function */
    if (lt_hw_tx(skb))
        dev_kfree_skb(skb);

    return NETDEV_TX_OK;
}

static void dummy_get_stats64(struct net_device *dev,
                              struct rtnl_link_stats64 *stats)
{
    struct dummy_priv *priv = netdev_priv(dev);
    int i;

    for_each_possible_cpu(i) {
        struct pcpu_dstats *dstats = per_cpu_ptr(priv->dstats, i);
        u64 rx_packets, rx_bytes, tx_packets, tx_bytes;
        unsigned int start;

        do {
            start = u64_stats_fetch_begin(&dstats->syncp);
            rx_packets = dstats->rx_packets;
            rx_bytes   = dstats->rx_bytes;
            tx_packets = dstats->tx_packets;
            tx_bytes   = dstats->tx_bytes;
        } while (u64_stats_fetch_retry(&dstats->syncp, start));

        stats->rx_packets += rx_packets;
        stats->rx_bytes   += rx_bytes;
        stats->tx_packets += tx_packets;
        stats->tx_bytes   += tx_bytes;
    }
}

static int dummy_change_carrier(struct net_device *dev, bool new_carrier)
{
    if (new_carrier)
        netif_carrier_on(dev);
    else
        netif_carrier_off(dev);

    return 0;
}

static const struct net_device_ops dummy_netdev_ops = {
    .ndo_init            = dummy_dev_init,
    .ndo_uninit          = dummy_dev_uninit,
    .ndo_open            = dummy_open,
    .ndo_stop            = dummy_close,
    .ndo_start_xmit      = dummy_xmit,
    .ndo_get_stats64     = dummy_get_stats64,
    .ndo_validate_addr   = eth_validate_addr,
    .ndo_set_mac_address = eth_mac_addr,
    .ndo_change_carrier  = dummy_change_carrier,
};

static void dummy_setup(struct net_device *dev)
{
    ether_setup(dev);

    dev->netdev_ops = &dummy_netdev_ops;
    dev->needs_free_netdev = true;

    /* Leave ARP enabled: we answer ARP ourselves in the RX reflector. */
    dev->features |= NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA;

    eth_hw_addr_random(dev);
}

static struct rtnl_link_ops dummy_link_ops __read_mostly = {
    .kind      = DRV_NAME,
    .priv_size = sizeof(struct dummy_priv),
    .setup     = dummy_setup,
};

/* ------------------------------------------------------------------ */
/* module init / exit                                                  */
/* ------------------------------------------------------------------ */

static int __init dummy_init_one(void)
{
    struct net_device *dev_dummy;
    int err;

    dev_dummy = alloc_netdev(sizeof(struct dummy_priv),
                             "deth%d", NET_NAME_ENUM, dummy_setup);
    if (!dev_dummy)
        return -ENOMEM;

    dev_dummy->rtnl_link_ops = &dummy_link_ops;
    err = register_netdevice(dev_dummy);
    if (err < 0)
        goto err;

    /* True : Register, False : Deregister */
    err = lt_request_irq(true, &dummy_eth_rx);
    if (err)
        goto err;

    return 0;

err:
    free_netdev(dev_dummy);
    return err;
}

static int __init dummy_init_module(void)
{
    int err = 0;

    printk(KERN_INFO "Dummy eth module init\n");

    rtnl_lock();
    err = __rtnl_link_register(&dummy_link_ops);
    if (err < 0)
        goto out;

    err = dummy_init_one();
    if (err < 0)
        __rtnl_link_unregister(&dummy_link_ops);

out:
    rtnl_unlock();

    return err;
}

static void __exit dummy_cleanup_module(void)
{
    printk(KERN_INFO "Dummy eth module exit\n");

    /* Stop receiving loopback frames before the device goes away. */
    lt_request_irq(false, NULL);

    /* Removes the link kind and all devices of that kind (takes rtnl). */
    rtnl_link_unregister(&dummy_link_ops);
}

module_init(dummy_init_module);
module_exit(dummy_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK(DRV_NAME);
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION("Dummy ethernet netdevice that reflects ARP/ICMP");
