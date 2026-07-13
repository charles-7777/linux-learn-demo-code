#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/select.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <time.h>
#include <signal.h>
#include <net/if.h>
#include <linux/if.h>
#include <linux/if_arp.h>

#define BUFFER_SIZE 4096
#define TIMER_INTERVAL_SEC 5  // 定时器间隔5秒

// 全局变量 - 文件描述符
int g_netlink_fd = -1;    // netlink套接字
int g_timer_fd = -1;      // timerfd
int g_signal_fd = -1;     // signalfd

// 全局变量 - 状态控制
bool g_running = true;

// 全局变量 - 统计信息
typedef struct {
    unsigned long link_events;      // 链路事件计数
    unsigned long timer_ticks;      // 定时器触发次数
    unsigned long signal_count;     // 信号接收次数
    time_t start_time;              // 启动时间
} stats_t;

stats_t g_stats = {
    .link_events = 0,
    .timer_ticks = 0,
    .signal_count = 0,
    .start_time = 0
};

// 创建netlink套接字
int create_netlink_socket(int protocol, uint32_t groups) {
    int sockfd;
    struct sockaddr_nl src_addr;
    int bufsize = BUFFER_SIZE;

    sockfd = socket(AF_NETLINK, SOCK_RAW, protocol);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
        perror("setsockopt SO_RCVBUF failed");
        close(sockfd);
        return -1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    src_addr.nl_groups = groups;

    if (bind(sockfd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// 初始化netlink套接字
bool init_netlink_socket() {
    g_netlink_fd = create_netlink_socket(NETLINK_ROUTE, RTMGRP_LINK);
    if (g_netlink_fd < 0) {
        return false;
    }

    printf("Netlink socket initialized: fd=%d (监听网络链路事件)\n", g_netlink_fd);
    return true;
}

// 初始化timerfd (5秒间隔)
bool init_timerfd() {
    g_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (g_timer_fd < 0) {
        perror("timerfd_create failed");
        return false;
    }

    struct itimerspec timer_spec = {
        .it_interval = {
            .tv_sec = TIMER_INTERVAL_SEC,
            .tv_nsec = 0
        },
        .it_value = {
            .tv_sec = TIMER_INTERVAL_SEC,
            .tv_nsec = 0
        }
    };

    if (timerfd_settime(g_timer_fd, 0, &timer_spec, NULL) < 0) {
        perror("timerfd_settime failed");
        close(g_timer_fd);
        return false;
    }

    printf("Timerfd initialized: fd=%d (每%d秒触发一次)\n", 
           g_timer_fd, TIMER_INTERVAL_SEC);
    return true;
}

// 初始化signalfd
bool init_signalfd() {
    sigset_t mask;
    
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        perror("sigprocmask failed");
        return false;
    }

    g_signal_fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (g_signal_fd < 0) {
        perror("signalfd failed");
        return false;
    }

    printf("Signalfd initialized: fd=%d (信号: INT, TERM, USR1, USR2)\n", g_signal_fd);
    return true;
}

// 解析链路状态
const char* get_link_state(unsigned int flags) {
    if (flags & IFF_RUNNING) {
        return "UP";
    } else if (flags & IFF_UP) {
        return "UP (但未运行)";
    } else {
        return "DOWN";
    }
}

// 获取接口名称
void get_interface_name(int ifindex, char *name, size_t size) {
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        snprintf(name, size, "unknown");
        return;
    }
    
    ifr.ifr_ifindex = ifindex;
    if (ioctl(sock, SIOCGIFNAME, &ifr) == 0) {
        strncpy(name, ifr.ifr_name, size - 1);
        name[size - 1] = '\0';
    } else {
        snprintf(name, size, "ifindex-%d", ifindex);
    }
    close(sock);
}

// 处理netlink LINK消息
void handle_netlink_link_message() {
    char buffer[BUFFER_SIZE];
    struct nlmsghdr *nlh;
    struct sockaddr_nl src_addr;
    socklen_t addrlen = sizeof(src_addr);
    ssize_t recv_len;

    recv_len = recvfrom(g_netlink_fd, buffer, sizeof(buffer), 0,
                       (struct sockaddr *)&src_addr, &addrlen);
    if (recv_len < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom failed");
        }
        return;
    }

    g_stats.link_events++;  // 统计链路事件

    nlh = (struct nlmsghdr *)buffer;
    while (NLMSG_OK(nlh, recv_len)) {
        if (nlh->nlmsg_type == RTM_NEWLINK || nlh->nlmsg_type == RTM_DELLINK) {
            struct ifinfomsg *ifinfo = (struct ifinfomsg *)NLMSG_DATA(nlh);
            char ifname[IFNAMSIZ];
            
            get_interface_name(ifinfo->ifi_index, ifname, sizeof(ifname));
            
            printf("\n[LINK EVENT #%lu] ", g_stats.link_events);
            if (nlh->nlmsg_type == RTM_NEWLINK) {
                printf("网络接口状态变化: ");
            } else {
                printf("网络接口删除: ");
            }
            
            printf("接口: %s (index=%d), ", ifname, ifinfo->ifi_index);
            printf("状态: %s, ", get_link_state(ifinfo->ifi_flags));
            printf("类型: %d\n", ifinfo->ifi_type);
            
            // 解析属性
            struct rtattr *rta;
            int rta_len = NLMSG_PAYLOAD(nlh, sizeof(struct ifinfomsg));
            
            printf("  详细信息:\n");
            for (rta = IFLA_RTA(ifinfo); RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
                switch (rta->rta_type) {
                    case IFLA_ADDRESS:
                        printf("    MAC地址: ");
                        for (unsigned int i = 0; i < RTA_PAYLOAD(rta) && i < 6; i++) {
                            printf("%02x%s", ((unsigned char *)RTA_DATA(rta))[i],
                                   i < 5 ? ":" : "");
                        }
                        printf("\n");
                        break;
                    case IFLA_MTU:
                        printf("    MTU: %u\n", *(unsigned int *)RTA_DATA(rta));
                        break;
                    case IFLA_QDISC:
                        printf("    QDISC: %s\n", (char *)RTA_DATA(rta));
                        break;
                    case IFLA_IFNAME:
                        printf("    接口名: %s\n", (char *)RTA_DATA(rta));
                        break;
                    default:
                        break;
                }
            }
        }

        nlh = NLMSG_NEXT(nlh, recv_len);
    }
}

// 处理timerfd事件
void handle_timer_event() {
    uint64_t expirations;
    ssize_t s = read(g_timer_fd, &expirations, sizeof(expirations));
    if (s != sizeof(expirations)) {
        return;
    }

    g_stats.timer_ticks += expirations;  // 统计定时器触发次数

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("\n[TIMER #%lu] 触发 at %s\n", g_stats.timer_ticks, time_str);
    printf("  统计信息:\n");
    printf("    运行时间: %ld 秒\n", now - g_stats.start_time);
    printf("    链路事件: %lu 次\n", g_stats.link_events);
    printf("    定时器触发: %lu 次\n", g_stats.timer_ticks);
    printf("    信号接收: %lu 次\n", g_stats.signal_count);
}

// 处理signalfd事件
void handle_signal_event() {
    struct signalfd_siginfo siginfo;
    ssize_t s = read(g_signal_fd, &siginfo, sizeof(siginfo));
    if (s != sizeof(siginfo)) {
        return;
    }

    g_stats.signal_count++;  // 统计信号

    printf("\n[SIGNAL #%lu] 收到信号: %d\n", g_stats.signal_count, siginfo.ssi_signo);

    switch (siginfo.ssi_signo) {
        case SIGINT:
        case SIGTERM:
            printf("  >> 程序即将退出...\n");
            g_running = false;
            break;
        case SIGUSR1:
            printf("  >> 用户信号1 - 显示统计信息\n");
            printf("     链路事件: %lu 次\n", g_stats.link_events);
            printf("     定时器触发: %lu 次\n", g_stats.timer_ticks);
            printf("     信号接收: %lu 次\n", g_stats.signal_count);
            break;
        case SIGUSR2:
            printf("  >> 用户信号2 - 重置统计信息\n");
            g_stats.link_events = 0;
            g_stats.timer_ticks = 0;
            g_stats.signal_count = 0;
            g_stats.start_time = time(NULL);
            printf("     统计信息已重置\n");
            break;
        default:
            printf("  >> 未处理的信号\n");
            break;
    }
}

// 清理资源
void cleanup_resources() {
    if (g_netlink_fd >= 0) {
        close(g_netlink_fd);
        printf("已关闭netlink套接字: fd=%d\n", g_netlink_fd);
        g_netlink_fd = -1;
    }

    if (g_timer_fd >= 0) {
        close(g_timer_fd);
        printf("已关闭timerfd: fd=%d\n", g_timer_fd);
        g_timer_fd = -1;
    }

    if (g_signal_fd >= 0) {
        close(g_signal_fd);
        printf("已关闭signalfd: fd=%d\n", g_signal_fd);
        g_signal_fd = -1;
    }
}

// 主循环
void run_main_loop() {
    fd_set readfds;
    int max_fd = -1;
    int ret;

    printf("\n=== 启动主事件循环 ===\n");
    printf("文件描述符:\n");
    printf("  - Netlink (LINK): fd=%d\n", g_netlink_fd);
    printf("  - Timerfd: fd=%d (间隔%d秒)\n", g_timer_fd, TIMER_INTERVAL_SEC);
    printf("  - Signalfd: fd=%d\n", g_signal_fd);
    printf("\n等待网络链路事件... (按Ctrl+C退出)\n");
    printf("信号说明:\n");
    printf("  SIGUSR1 - 显示统计信息\n");
    printf("  SIGUSR2 - 重置统计信息\n\n");

    while (g_running) {
        FD_ZERO(&readfds);
        max_fd = -1;

        if (g_netlink_fd >= 0) {
            FD_SET(g_netlink_fd, &readfds);
            if (g_netlink_fd > max_fd) 
                max_fd = g_netlink_fd;
        }

        if (g_timer_fd >= 0) {
            FD_SET(g_timer_fd, &readfds);
            if (g_timer_fd > max_fd) 
                max_fd = g_timer_fd;
        }

        if (g_signal_fd >= 0) {
            FD_SET(g_signal_fd, &readfds);
            if (g_signal_fd > max_fd) 
                max_fd = g_signal_fd;
        }

        ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select failed");
            break;
        }

        if (g_netlink_fd >= 0 && FD_ISSET(g_netlink_fd, &readfds)) {
            handle_netlink_link_message();
        }

        if (g_timer_fd >= 0 && FD_ISSET(g_timer_fd, &readfds)) {
            handle_timer_event();
        }

        if (g_signal_fd >= 0 && FD_ISSET(g_signal_fd, &readfds)) {
            handle_signal_event();
        }
    }
}

// 主函数
int main(void) {
    printf("=== 网络链路事件监控程序 ===\n");
    printf("PID: %d\n\n", getpid());

    g_stats.start_time = time(NULL);

    if (!init_netlink_socket()) {
        fprintf(stderr, "初始化netlink套接字失败\n");
        goto cleanup;
    }

    if (!init_timerfd()) {
        fprintf(stderr, "初始化timerfd失败\n");
        goto cleanup;
    }

    if (!init_signalfd()) {
        fprintf(stderr, "初始化signalfd失败\n");
        goto cleanup;
    }

    run_main_loop();

cleanup:
    cleanup_resources();
    printf("程序正常退出\n");
    return 0;
}