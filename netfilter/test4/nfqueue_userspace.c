#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

static int running = 1;

void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, exiting...\n", sig);
        running = 0;
    }
}

/* Callback function for processing queued packets */
static int packet_cb(struct nfq_q_handle *qh, 
                     struct nfgenmsg *nfmsg,
                     struct nfq_data *nfad, 
                     void *data)
{
    struct nfqnl_msg_packet_hdr *ph;
    struct iphdr *iph;
    struct icmphdr *icmph;
    unsigned char *payload;
    int id;
    int ret;
    struct in_addr src, dst;
    
    ph = nfq_get_msg_packet_hdr(nfad);
    if (ph) {
        id = ntohl(ph->packet_id);
        printf("[Packet ID: %u] ", id);
    } else {
        id = -1;
        printf("[Packet] ");
    }
    
    ret = nfq_get_payload(nfad, &payload);
    if (ret >= 0) {
        iph = (struct iphdr *)payload;
        
        if (iph->protocol == IPPROTO_ICMP) {
            icmph = (struct icmphdr *)(payload + (iph->ihl * 4));
            
            src.s_addr = iph->saddr;
            dst.s_addr = iph->daddr;
            
            printf("ICMP Packet - Type: %d, Code: %d, ", 
                   icmph->type, icmph->code);
            printf("SRC: %s -> DST: %s\n", 
                   inet_ntoa(src), inet_ntoa(dst));
            
            /* You can choose to drop or accept the packet */
            /* return nfq_set_verdict(qh, id, NF_DROP, 0, NULL); */
            return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
        }
    }
    
    /* Accept the packet */
    return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
}

int main(int argc, char **argv)
{
    struct nfq_handle *h;
    struct nfq_q_handle *qh;
    int fd;
    int rv;
    char buf[4096] __attribute__ ((aligned));
    int queue_num = 0;
    
    /* Parse command line arguments */
    if (argc > 1) {
        queue_num = atoi(argv[1]);
    }
    
    printf("NFQUEUE Userspace Program\n");
    printf("==========================\n");
    printf("Queue number: %d\n", queue_num);
    printf("Press Ctrl+C to stop\n\n");
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Open netfilter queue */
    h = nfq_open();
    if (!h) {
        fprintf(stderr, "Error: nfq_open() failed\n");
        exit(EXIT_FAILURE);
    }
    
    /* Unbind existing queue handler (if any) */
    if (nfq_unbind_pf(h, AF_INET) < 0) {
        fprintf(stderr, "Warning: nfq_unbind_pf() failed\n");
    }
    
    /* Bind to IPv4 */
    if (nfq_bind_pf(h, AF_INET) < 0) {
        fprintf(stderr, "Error: nfq_bind_pf() failed\n");
        exit(EXIT_FAILURE);
    }
    
    /* Create queue handler */
    qh = nfq_create_queue(h, queue_num, &packet_cb, NULL);
    if (!qh) {
        fprintf(stderr, "Error: nfq_create_queue() failed\n");
        exit(EXIT_FAILURE);
    }
    
    /* Set queue mode - copy packet payload */
    if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
        fprintf(stderr, "Error: nfq_set_mode() failed\n");
        exit(EXIT_FAILURE);
    }
    
    fd = nfq_fd(h);
    printf("Listening for queued packets...\n\n");
    
    /* Main loop */
    while (running && (rv = recv(fd, buf, sizeof(buf), 0)) >= 0) {
        nfq_handle_packet(h, buf, rv);
    }
    
    printf("\nExiting...\n");
    
    /* Cleanup */
    nfq_destroy_queue(qh);
    nfq_close(h);
    
    return 0;
}
