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
#include <linux/icmp.h>
#include <net/net_namespace.h>
#include <linux/slab.h>

/* Netfilter hook operations structure */
static struct nf_hook_ops *nf_blockpkt_ops = NULL;

/* Netfilter hook handler function */
static unsigned int nf_blockpkt_handler(void *priv, 
                                        struct sk_buff *skb, 
                                        const struct nf_hook_state *state)
{
    struct iphdr *iph;
    struct icmphdr *icmph;
    
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
    
    /* Check protocol type */
    switch (iph->protocol) {
        case IPPROTO_ICMP:
            icmph = icmp_hdr(skb);
            if (icmph) {
                printk(KERN_INFO "Blocking ICMP packet - Type: %d, Code: %d\n", 
                       icmph->type, icmph->code);
            }
            return NF_DROP;
            
        case IPPROTO_UDP:
            /* Allow UDP packets */
            return NF_ACCEPT;
            
        case IPPROTO_TCP:
            /* Allow TCP packets */
            return NF_ACCEPT;
            
        default:
            /* Allow other protocols */
            return NF_ACCEPT;
    }
}

/* Module initialization function */
static int __init nf_blockpkt_init(void)
{
    int ret = -ENOMEM;
    
    /* Allocate memory for nf_hook_ops structure */
    nf_blockpkt_ops = (struct nf_hook_ops *)kmalloc(sizeof(struct nf_hook_ops), GFP_KERNEL);
    if (!nf_blockpkt_ops) {
        printk(KERN_ERR "nf_blockpkt: Failed to allocate memory for nf_hook_ops\n");
        return -ENOMEM;
    }
    
    /* Initialize netfilter hook operations */
    nf_blockpkt_ops->hook = nf_blockpkt_handler;
    nf_blockpkt_ops->hooknum = NF_INET_PRE_ROUTING;
    nf_blockpkt_ops->pf = NFPROTO_IPV4;
    nf_blockpkt_ops->priority = NF_IP_PRI_FIRST;
    
    /* Register the hook */
    ret = nf_register_net_hook(&init_net, nf_blockpkt_ops);
    if (ret) {
        printk(KERN_ERR "nf_blockpkt: Failed to register netfilter hook: %d\n", ret);
        kfree(nf_blockpkt_ops);
        nf_blockpkt_ops = NULL;
        return ret;
    }
    
    printk(KERN_INFO "nf_blockpkt: Module loaded successfully - All ICMP packets will be dropped\n");
    return 0;
}

/* Module cleanup function */
static void __exit nf_blockpkt_exit(void)
{
    if (nf_blockpkt_ops) {
        nf_unregister_net_hook(&init_net, nf_blockpkt_ops);
        kfree(nf_blockpkt_ops);
        nf_blockpkt_ops = NULL;
        printk(KERN_INFO "nf_blockpkt: Module unloaded successfully\n");
    }
}

module_init(nf_blockpkt_init);
module_exit(nf_blockpkt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Netfilter module to block all ICMP packets");
MODULE_VERSION("1.0");
