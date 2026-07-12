#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <net/net_namespace.h>

#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[3], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[0]

/* Netfilter hook function for LOCAL_OUT */
static unsigned int nf_local_out_show(void *priv, 
                                      struct sk_buff *skb, 
                                      const struct nf_hook_state *state)
{
    u32 sip, dip;
    struct iphdr *iph;
    struct tcphdr *tcp;
    struct udphdr *udp;
    u16 sport = 0, dport = 0;
    u8 protocol = 0;

    if (!skb) {
        return NF_ACCEPT;
    }

    iph = ip_hdr(skb);
    if (!iph) {
        return NF_ACCEPT;
    }

    /* Only handle IPv4 packets */
    if (iph->version != 4) {
        return NF_ACCEPT;
    }

    sip = ntohl(iph->saddr);
    dip = ntohl(iph->daddr);
    protocol = iph->protocol;

    /* Get transport layer ports if TCP or UDP */
    if (protocol == IPPROTO_TCP) {
        tcp = tcp_hdr(skb);
        if (tcp) {
            sport = ntohs(tcp->source);
            dport = ntohs(tcp->dest);
        }
    } else if (protocol == IPPROTO_UDP) {
        udp = udp_hdr(skb);
        if (udp) {
            sport = ntohs(udp->source);
            dport = ntohs(udp->dest);
        }
    }

    /* Print packet information */
    printk(KERN_INFO "[LOCAL_OUT] SRC: %u.%u.%u.%u:%u -> DST: %u.%u.%u.%u:%u Proto: %u\n",
           NIPQUAD(sip), sport, NIPQUAD(dip), dport, protocol);

    return NF_ACCEPT;
}

/* Netfilter hook registration structure for LOCAL_OUT */
static struct nf_hook_ops nf_local_out_ops __read_mostly = {
    .hook       = nf_local_out_show,
    .hooknum    = NF_INET_LOCAL_OUT,
    .pf         = NFPROTO_IPV4,
    .priority   = NF_IP_PRI_FILTER + 2,
};

/* Module initialization function */
static int __init nf_local_out_init(void)
{
    int ret;

    ret = nf_register_net_hook(&init_net, &nf_local_out_ops);
    if (ret) {
        printk(KERN_ERR "nf_register_net_hook(LOCAL_OUT) failed: %d\n", ret);
        return ret;
    }

    printk(KERN_INFO "nf_local_out: Netfilter LOCAL_OUT hook registered successfully\n");
    return 0;
}

/* Module cleanup function */
static void __exit nf_local_out_exit(void)
{
    nf_unregister_net_hook(&init_net, &nf_local_out_ops);
    printk(KERN_INFO "nf_local_out: Netfilter LOCAL_OUT hook unregistered\n");
}

module_init(nf_local_out_init);
module_exit(nf_local_out_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Netfilter module to show local outgoing IP packets");
MODULE_VERSION("1.0");
