#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 8192
#define ERR_RET(msg) do { perror(msg); return -1; } while(0)

/* Flag to control program running */
static volatile int running = 1;

/* Signal handler for graceful shutdown */
void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, shutting down...\n", sig);
        running = 0;
    }
}

/* Function to print route information */
void print_route_info(struct nlmsghdr *nlh, const char *action)
{
    struct rtmsg *route_entry;
    struct rtattr *route_attribute;
    int route_attribute_len = 0;
    unsigned char route_netmask = 0;
    unsigned char route_protocol = 0;
    char destination_address[32] = "N/A";
    char gateway_address[32] = "N/A";
    char src_address[32] = "N/A";
    int table = 0;

    route_entry = (struct rtmsg *) NLMSG_DATA(nlh);

    /* Get basic route information */
    route_netmask = route_entry->rtm_dst_len;
    route_protocol = route_entry->rtm_protocol;
    table = route_entry->rtm_table;

    /* Get attributes */
    route_attribute = (struct rtattr *) RTM_RTA(route_entry);
    route_attribute_len = RTM_PAYLOAD(nlh);

    /* Loop through all attributes */
    for (; RTA_OK(route_attribute, route_attribute_len); 
         route_attribute = RTA_NEXT(route_attribute, route_attribute_len)) {
        
        switch (route_attribute->rta_type) {
            case RTA_DST:
                inet_ntop(AF_INET, RTA_DATA(route_attribute), 
                         destination_address, sizeof(destination_address));
                break;
            case RTA_GATEWAY:
                inet_ntop(AF_INET, RTA_DATA(route_attribute), 
                         gateway_address, sizeof(gateway_address));
                break;
            case RTA_PREFSRC:
                inet_ntop(AF_INET, RTA_DATA(route_attribute), 
                         src_address, sizeof(src_address));
                break;
            case RTA_OIF:
                /* Output interface index */
                break;
            default:
                break;
        }
    }

    /* Print route information with timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("[%s] %s route: %s/%d proto %d", 
           time_str, action, destination_address, route_netmask, route_protocol);
    
    if (strcmp(gateway_address, "N/A") != 0) {
        printf(" via %s", gateway_address);
    }
    if (strcmp(src_address, "N/A") != 0) {
        printf(" src %s", src_address);
    }
    if (table != RT_TABLE_MAIN) {
        printf(" table %d", table);
    }
    printf("\n");
}

/* Main loop to monitor route changes */
int loop(int sock, struct sockaddr_nl *addr)
{
    int received_bytes = 0;
    struct nlmsghdr *nlh;
    char buffer[BUFFER_SIZE];

    /* Zero out buffers */
    memset(buffer, 0, sizeof(buffer));

    /* Receiving netlink socket data */
    received_bytes = recv(sock, buffer, sizeof(buffer), 0);
    if (received_bytes < 0) {
        if (errno == EINTR) {
            return 0;  /* Interrupted by signal */
        }
        ERR_RET("recv");
    }

    if (received_bytes == 0) {
        return 0;
    }

    /* Parse the received messages */
    nlh = (struct nlmsghdr *) buffer;

    /* Check if we received all data */
    if (nlh->nlmsg_type == NLMSG_DONE) {
        return 0;
    }

    /* Handle error messages */
    if (nlh->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(nlh);
        fprintf(stderr, "Netlink error: %s\n", strerror(-err->error));
        return -1;
    }

    /* Loop through all entries */
    for (; NLMSG_OK(nlh, received_bytes); nlh = NLMSG_NEXT(nlh, received_bytes)) {
        
        /* Only interested in Routing information */
        if (nlh->nlmsg_type != RTM_NEWROUTE && nlh->nlmsg_type != RTM_DELROUTE) {
            continue;
        }

        /* Print route information based on message type */
        if (nlh->nlmsg_type == RTM_NEWROUTE) {
            print_route_info(nlh, "ADDED");
        } else if (nlh->nlmsg_type == RTM_DELROUTE) {
            print_route_info(nlh, "DELETED");
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    int sock = -1;
    struct sockaddr_nl addr;
    int opt = 0;

    /* Register signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Parse command line options */
    printf("Route Monitor Tool\n");
    printf("==================\n");
    printf("Monitoring IPv4 route changes...\n");
    printf("Press Ctrl+C to exit\n\n");

    /* Zeroing addr */
    memset(&addr, 0, sizeof(addr));

    /* Create netlink socket */
    if ((sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
        ERR_RET("socket");
    }

    /* Set socket options to receive notifications */
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_ROUTE;  /* Subscribe to IPv4 route changes */

    /* Bind socket */
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        ERR_RET("bind");
    }

    /* Main monitoring loop */
    while (running) {
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        /* Wait for data with timeout */
        ret = select(sock + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted by signal */
            }
            perror("select");
            break;
        }

        if (ret > 0 && FD_ISSET(sock, &fds)) {
            if (loop(sock, &addr) < 0) {
                break;
            }
        }
    }

    /* Cleanup */
    if (sock >= 0) {
        close(sock);
    }

    printf("\nRoute monitor stopped.\n");
    return 0;
}
