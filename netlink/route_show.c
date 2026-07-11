#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_link.h>
#include <arpa/inet.h>
#include <errno.h>

#define IFLIST_REPLY_BUFFER 8192

/* Netlink request structure */
typedef struct nl_req_s {
    struct nlmsghdr hdr;
    struct rtgenmsg gen;
} nl_req_t;

/* Function to print network interface information */
void rtnl_print_link(struct nlmsghdr *h)
{
    struct ifinfomsg *iface;
    struct rtattr *attribute;
    int len;

    iface = NLMSG_DATA(h);
    len = h->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));

    /* Loop over all attributes for the NEWLINK message */
    for (attribute = IFLA_RTA(iface); RTA_OK(attribute, len); 
         attribute = RTA_NEXT(attribute, len)) {
        switch(attribute->rta_type) {
            case IFLA_IFNAME:
                printf("Interface %d : %s\n", iface->ifi_index, 
                       (char *)RTA_DATA(attribute));
                break;
            case IFLA_ADDRESS:
                printf("  MAC Address: ");
                for (int i = 0; i < attribute->rta_len - RTA_LENGTH(0); i++) {
                    printf("%02x%s", ((unsigned char *)RTA_DATA(attribute))[i],
                           i < attribute->rta_len - RTA_LENGTH(0) - 1 ? ":" : "");
                }
                printf("\n");
                break;
            case IFLA_MTU:
                printf("  MTU: %u\n", *(unsigned int *)RTA_DATA(attribute));
                break;
            case IFLA_QDISC:
                printf("  Qdisc: %s\n", (char *)RTA_DATA(attribute));
                break;
            default:
                break;
        }
    }
}

/* Function to print route information */
void rtnl_print_route(struct nlmsghdr *nlh)
{
    struct rtmsg *route_entry;
    struct rtattr *route_attribute;
    int route_attribute_len = 0;
    unsigned char route_netmask = 0;
    unsigned char route_protocol = 0;
    char destination_address[32] = "N/A";
    char gateway_address[32] = "N/A";

    route_entry = (struct rtmsg *) NLMSG_DATA(nlh);

    /* Only show main routing table */
    if (route_entry->rtm_table != RT_TABLE_MAIN)
        return;

    route_netmask = route_entry->rtm_dst_len;
    route_protocol = route_entry->rtm_protocol;
    route_attribute = (struct rtattr *) RTM_RTA(route_entry);
    route_attribute_len = RTM_PAYLOAD(nlh);
    
    /* Loop through all attributes */
    for (; RTA_OK(route_attribute, route_attribute_len); 
         route_attribute = RTA_NEXT(route_attribute, route_attribute_len)) {
        /* Get destination address */
        if (route_attribute->rta_type == RTA_DST) {
            inet_ntop(AF_INET, RTA_DATA(route_attribute), 
                      destination_address, sizeof(destination_address));
        }
        /* Get gateway (Next hop) */
        if (route_attribute->rta_type == RTA_GATEWAY) {
            inet_ntop(AF_INET, RTA_DATA(route_attribute), 
                      gateway_address, sizeof(gateway_address));
        }
    }
    
    printf("Route to destination: %s/%d proto %d gateway %s\n", 
           destination_address, route_netmask, route_protocol, gateway_address);
}

/* Function to send netlink request */
int send_netlink_request(int fd, int msg_type, int family)
{
    struct sockaddr_nl kernel;
    struct msghdr rtnl_msg;
    struct iovec io;
    nl_req_t req;

    memset(&kernel, 0, sizeof(kernel));
    memset(&rtnl_msg, 0, sizeof(rtnl_msg));
    memset(&req, 0, sizeof(req));

    kernel.nl_family = AF_NETLINK;
    kernel.nl_groups = 0;

    req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    req.hdr.nlmsg_type = msg_type;
    req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.hdr.nlmsg_seq = 1;
    req.hdr.nlmsg_pid = getpid();
    req.gen.rtgen_family = family;

    io.iov_base = &req;
    io.iov_len = req.hdr.nlmsg_len;
    rtnl_msg.msg_iov = &io;
    rtnl_msg.msg_iovlen = 1;
    rtnl_msg.msg_name = &kernel;
    rtnl_msg.msg_namelen = sizeof(kernel);

    if (sendmsg(fd, &rtnl_msg, 0) < 0) {
        perror("sendmsg failed");
        return -1;
    }

    return 0;
}

/* Function to receive and process netlink responses */
void process_netlink_responses(int fd, int show_routes, int show_interfaces)
{
    char reply[IFLIST_REPLY_BUFFER];
    int end = 0;

    while (!end) {
        int len;
        struct nlmsghdr *msg_ptr;
        struct msghdr rtnl_reply;
        struct iovec io_reply;
        struct sockaddr_nl kernel;

        memset(&io_reply, 0, sizeof(io_reply));
        memset(&rtnl_reply, 0, sizeof(rtnl_reply));
        memset(&kernel, 0, sizeof(kernel));

        io_reply.iov_base = reply;
        io_reply.iov_len = IFLIST_REPLY_BUFFER;
        rtnl_reply.msg_iov = &io_reply;
        rtnl_reply.msg_iovlen = 1;
        rtnl_reply.msg_name = &kernel;
        rtnl_reply.msg_namelen = sizeof(kernel);

        len = recvmsg(fd, &rtnl_reply, 0);
        if (len < 0) {
            perror("recvmsg failed");
            break;
        }

        if (len == 0)
            break;

        for (msg_ptr = (struct nlmsghdr *)reply; 
             NLMSG_OK(msg_ptr, len); 
             msg_ptr = NLMSG_NEXT(msg_ptr, len)) {
            
            switch (msg_ptr->nlmsg_type) {
                case NLMSG_DONE:
                    end = 1;
                    break;

                case RTM_NEWROUTE:
                    if (show_routes)
                        rtnl_print_route(msg_ptr);
                    break;

                case RTM_NEWLINK:
                    if (show_interfaces)
                        rtnl_print_link(msg_ptr);
                    break;

                case NLMSG_ERROR:
                {
                    struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(msg_ptr);
                    fprintf(stderr, "Netlink error: %s\n", strerror(-err->error));
                    end = 1;
                    break;
                }

                default:
                    if (show_interfaces || show_routes)
                        printf("Unexpected message type: %d, length: %d\n", 
                               msg_ptr->nlmsg_type, msg_ptr->nlmsg_len);
                    break;
            }
        }
    }
}

/* Function to show usage information */
void show_usage(const char *progname)
{
    printf("Usage: %s [options]\n", progname);
    printf("Options:\n");
    printf("  -r, --routes        Show routing table\n");
    printf("  -i, --interfaces    Show network interfaces\n");
    printf("  -a, --all           Show all information (default)\n");
    printf("  -h, --help          Show this help message\n");
}

int main(int argc, char **argv)
{
    int fd;
    struct sockaddr_nl local;
    pid_t pid = getpid();
    int show_routes = 0;
    int show_interfaces = 0;
    int show_all = 0;

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--routes") == 0) {
            show_routes = 1;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interfaces") == 0) {
            show_interfaces = 1;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            show_all = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            show_usage(argv[0]);
            return -1;
        }
    }

    /* If no specific options, show everything */
    if (!show_routes && !show_interfaces && !show_all) {
        show_all = 1;
    }

    /* Create netlink socket */
    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        printf("Error creating socket: %s\n", strerror(errno));
        return -1;
    }

    /* Set up local address and bind */
    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_pid = pid;
    local.nl_groups = RTMGRP_LINK;  /* Subscribe to link notifications */

    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("Cannot bind - are you root? Check netlink/rtnetlink support");
        close(fd);
        return -1;
    }

    printf("=== Network Information ===\n\n");

    /* Request and display network interfaces if requested */
    if (show_all || show_interfaces) {
        printf("--- Network Interfaces ---\n");
        if (send_netlink_request(fd, RTM_GETLINK, AF_UNSPEC) == 0) {
            process_netlink_responses(fd, 0, 1);
        }
        printf("\n");
    }

    /* Request and display routing table if requested */
    if (show_all || show_routes) {
        printf("--- Routing Table ---\n");
        if (send_netlink_request(fd, RTM_GETROUTE, AF_INET) == 0) {
            process_netlink_responses(fd, 1, 0);
        }
        printf("\n");
    }

    close(fd);
    return 0;
}
