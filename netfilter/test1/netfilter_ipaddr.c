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

/* Netfilter hook function */
static unsigned int nf_ipaddr_show(void *priv, 
                                   struct sk_buff *skb, 
                                   const struct nf_hook_state *state)
{
    u32 sip, dip;
    struct iphdr *iph;

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

    printk(KERN_INFO "Source IP: %u.%u.%u.%u -> Destination IP: %u.%u.%u.%u\n",
           NIPQUAD(sip), NIPQUAD(dip));

    return NF_ACCEPT;
}

/* Netfilter hook registration structure */
static struct nf_hook_ops nf_ipaddr_show_ops __read_mostly = {
    .hook       = nf_ipaddr_show,
    .hooknum    = NF_INET_PRE_ROUTING,
    .pf         = NFPROTO_IPV4,
    .priority   = NF_IP_PRI_FIRST,
};

/* Module initialization function */
static int __init nf_ipaddr_show_init(void)
{
    int ret;

    /* For kernel 4.13+ use nf_register_net_hook */
    ret = nf_register_net_hook(&init_net, &nf_ipaddr_show_ops);
    if (ret) {
        printk(KERN_ERR "nf_register_net_hook() failed: %d\n", ret);
        return ret;
    }

    printk(KERN_INFO "nf_ipaddr_show: Netfilter hook registered successfully\n");
    return 0;
}

/* Module cleanup function */
static void __exit nf_ipaddr_show_exit(void)
{
    nf_unregister_net_hook(&init_net, &nf_ipaddr_show_ops);
    printk(KERN_INFO "nf_ipaddr_show: Netfilter hook unregistered\n");
}

module_init(nf_ipaddr_show_init);
module_exit(nf_ipaddr_show_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Netfilter module to show source and destination IP addresses");
MODULE_VERSION("1.0");
