#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/icmp.h>
#include <net/net_namespace.h>

/* Module parameter: queue number */
static int queue_num = 0;
module_param(queue_num, int, 0644);
MODULE_PARM_DESC(queue_num, "Netfilter queue number (default: 0)");

/* Netfilter hook handler function */
static unsigned int nf_queue_pkt(void *priv, 
                                 struct sk_buff *skb, 
                                 const struct nf_hook_state *state)
{
    struct iphdr *iph;
    
    if (!skb) {
        return NF_ACCEPT;
    }
    
    iph = ip_hdr(skb);
    if (!iph) {
        return NF_ACCEPT;
    }
    
    if (iph->version != 4) {
        return NF_ACCEPT;
    }
    
    /* Queue ICMP packets to specified queue */
    if (iph->protocol == IPPROTO_ICMP) {
        /* NF_QUEUE with specific queue number and NF_VERDICT_FLAG_QUEUE_BYPASS */
        return NF_QUEUE_NR(queue_num);
    }
    
    return NF_ACCEPT;
}

/* Netfilter hook registration structure */
static struct nf_hook_ops nf_queue_pkt_ops __read_mostly = {
    .hook       = nf_queue_pkt,
    .hooknum    = NF_INET_LOCAL_IN,
    .pf         = NFPROTO_IPV4,
    .priority   = NF_IP_PRI_FIRST,
};

/* Module initialization function */
static int __init my_nfqueue_init(void)
{
    int ret;
    
    ret = nf_register_net_hook(&init_net, &nf_queue_pkt_ops);
    if (ret) {
        printk(KERN_ERR "nf_register_net_hook() failed: %d\n", ret);
        return ret;
    }
    
    printk(KERN_INFO "nf_queue_pkt: Registered with queue number %d\n", queue_num);
    return 0;
}

/* Module cleanup function */
static void __exit my_nfqueue_exit(void)
{
    nf_unregister_net_hook(&init_net, &nf_queue_pkt_ops);
    printk(KERN_INFO "nf_queue_pkt: Unregistered\n");
}

module_init(my_nfqueue_init);
module_exit(my_nfqueue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Netfilter module to queue ICMP packets to userspace");
MODULE_VERSION("1.0");
