#include <linux/module.h>
#include <linux/kernel.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/init.h>

#define NETLINK_USER 31

struct sock *nl_sk = NULL;

// 接收来自用户空间消息的回调函数
static void hello_nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    int msg_size;
    char *msg = "Hello from kernel";
    int res;

    printk(KERN_INFO "Entering: %s\n", __func__);

    // 获取 Netlink 消息头
    nlh = (struct nlmsghdr *)skb->data;
    printk(KERN_INFO "Netlink received msg payload: %s\n", (char *)nlmsg_data(nlh));

    // 获取发送进程的 PID
    pid = nlh->nlmsg_pid;

    // 准备回复消息
    msg_size = strlen(msg) + 1;  // +1 for null terminator

    // 分配新的 skb 用于回复
    skb_out = nlmsg_new(msg_size, GFP_KERNEL);
    if (!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }

    // 构造 Netlink 消息
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    if (!nlh) {
        printk(KERN_ERR "nlmsg_put failed\n");
        nlmsg_free(skb_out);
        return;
    }

    // 设置目标组（不加入多播组）
    NETLINK_CB(skb_out).dst_group = 0;

    // 拷贝消息数据
    strncpy(nlmsg_data(nlh), msg, msg_size);

    // 单播发送回复给用户进程
    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0) {
        printk(KERN_INFO "Error while sending back to user: %d\n", res);
    } else {
        printk(KERN_INFO "Reply sent successfully to PID: %d\n", pid);
    }
}

// 模块初始化函数
static int __init hello_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = hello_nl_recv_msg,
    };

    printk(KERN_INFO "Entering: %s\n", __func__);

    // 创建 Netlink socket
    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ALERT "Error creating Netlink socket.\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "Netlink module initialized successfully!\n");
    return 0;
}

// 模块退出函数
static void __exit hello_exit(void)
{
    printk(KERN_INFO "Exiting hello module\n");

    // 释放 Netlink socket
    if (nl_sk) {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
    }
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Netlink Kernel Module Example");
MODULE_VERSION("1.0");