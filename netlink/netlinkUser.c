#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <errno.h>

#define NETLINK_USER 31
#define MAX_PAYLOAD 1024

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;

int main(int argc, char *argv[])
{
    int sock_fd;
    int ret;

    // 创建 Netlink socket
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0) {
        perror("socket creation failed");
        return -1;
    }

    // 绑定源地址（用户进程自身）
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();  // 使用进程 PID 作为端口号
    src_addr.nl_groups = 0;      // 不加入多播组

    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind failed");
        close(sock_fd);
        return -1;
    }

    // 设置目标地址（内核）
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;        // 内核的 PID 为 0
    dest_addr.nl_groups = 0;     // 单播

    // 分配 Netlink 消息缓冲区
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (!nlh) {
        perror("malloc failed");
        close(sock_fd);
        return -1;
    }

    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

    // 填充 Netlink 消息头
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();   // 发送方 PID
    nlh->nlmsg_flags = 0;        // 无特殊标志

    // 拷贝用户数据到消息负载
    char *data = NLMSG_DATA(nlh);
    strcpy(data, "Hello from user space!");

    // 设置 I/O 向量
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    // 设置消息头
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // 发送消息到内核
    printf("Sending message to kernel: %s\n", (char *)NLMSG_DATA(nlh));
    ret = sendmsg(sock_fd, &msg, 0);
    if (ret < 0) {
        perror("sendmsg failed");
        free(nlh);
        close(sock_fd);
        return -1;
    }

    // 等待并接收内核回复
    printf("Waiting for message from kernel...\n");
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    ret = recvmsg(sock_fd, &msg, 0);
    if (ret < 0) {
        perror("recvmsg failed");
        free(nlh);
        close(sock_fd);
        return -1;
    }

    // 打印内核回复的内容
    printf("Received message from kernel: %s\n", (char *)NLMSG_DATA(nlh));

    // 清理资源
    free(nlh);
    close(sock_fd);

    return 0;
}