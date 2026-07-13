#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include <signal.h>

// ========== 全局配置 ==========
#define PORT 8888
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

// 服务器IP地址（全局变量）
const char *SERVER_IP = "127.0.0.1";
// ===============================

// 客户端信息结构
typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    char ip[INET_ADDRSTRLEN];
    int port;
} ClientInfo;

// 线程参数结构
typedef struct {
    int sockfd;
    ClientInfo *client;
} ThreadArgs;

// 全局客户端列表
ClientInfo clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;

// 函数声明
void *handle_client(void *arg);
void add_client(int sockfd, struct sockaddr_in addr);
void remove_client(int sockfd);
void broadcast_message(const char *msg, int sender_fd);
void cleanup();

// 信号处理
void sigint_handler(int sig) {
    printf("\n正在关闭服务器...\n");
    cleanup();
    exit(0);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    fd_set readfds;
    int max_sd;
    int activity;
    
    // 注册信号处理
    signal(SIGINT, sigint_handler);
    
    // 创建socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // 设置socket选项
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    // 使用全局变量 SERVER_IP
    if (inet_pton(AF_INET, SERVER_IP, &address.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(EXIT_FAILURE);
    }
    address.sin_port = htons(PORT);
    
    // 绑定
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // 监听
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("服务器启动，监听 %s:%d\n", SERVER_IP, PORT);
    printf("等待客户端连接...\n");
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
        
        // 添加所有客户端socket到fd_set
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sockfd > 0) {
                FD_SET(clients[i].sockfd, &readfds);
                if (clients[i].sockfd > max_sd) {
                    max_sd = clients[i].sockfd;
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        // select
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }
        
        // 处理新连接
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                continue;
            }
            
            // 添加客户端
            add_client(new_socket, address);
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(address.sin_port);
            printf("新客户端连接: %s:%d (socket: %d)\n", client_ip, client_port, new_socket);
            
            // 创建线程处理客户端
            ThreadArgs *args = malloc(sizeof(ThreadArgs));
            args->sockfd = new_socket;
            args->client = &clients[client_count - 1];
            
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_client, args) != 0) {
                perror("pthread_create");
                free(args);
                close(new_socket);
                remove_client(new_socket);
            } else {
                pthread_detach(thread);
            }
        }
    }
    
    cleanup();
    return 0;
}

void *handle_client(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int sockfd = args->sockfd;
    ClientInfo *client = args->client;
    char buffer[BUFFER_SIZE];
    int valread;
    
    char welcome_msg[BUFFER_SIZE];
    snprintf(welcome_msg, BUFFER_SIZE, "欢迎 %s:%d 加入聊天室！\n", client->ip, client->port);
    send(sockfd, welcome_msg, strlen(welcome_msg), 0);
    
    // 广播新用户加入
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, BUFFER_SIZE, "系统: %s:%d 加入了聊天室\n", client->ip, client->port);
    broadcast_message(join_msg, sockfd);
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(sockfd, buffer, BUFFER_SIZE);
        
        if (valread <= 0) {
            if (valread == 0) {
                printf("客户端 %s:%d 断开连接\n", client->ip, client->port);
            } else {
                perror("read error");
            }
            
            // 广播用户离开
            char leave_msg[BUFFER_SIZE];
            snprintf(leave_msg, BUFFER_SIZE, "系统: %s:%d 离开了聊天室\n", client->ip, client->port);
            broadcast_message(leave_msg, sockfd);
            
            remove_client(sockfd);
            close(sockfd);
            free(args);
            pthread_exit(NULL);
            break;
        }
        
        // 处理消息
        if (strlen(buffer) > 0) {
            buffer[strcspn(buffer, "\n")] = 0; // 去掉换行符
            char msg[BUFFER_SIZE + 50];
            snprintf(msg, sizeof(msg), "[%s:%d]: %s\n", client->ip, client->port, buffer);
            printf("%s", msg);
            broadcast_message(msg, sockfd);
        }
    }
    
    return NULL;
}

void add_client(int sockfd, struct sockaddr_in addr) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == 0) {
            clients[i].sockfd = sockfd;
            clients[i].addr = addr;
            inet_ntop(AF_INET, &addr.sin_addr, clients[i].ip, INET_ADDRSTRLEN);
            clients[i].port = ntohs(addr.sin_port);
            client_count++;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == sockfd) {
            clients[i].sockfd = 0;
            memset(clients[i].ip, 0, INET_ADDRSTRLEN);
            clients[i].port = 0;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_message(const char *msg, int sender_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 && clients[i].sockfd != sender_fd) {
            send(clients[i].sockfd, msg, strlen(msg), MSG_NOSIGNAL);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void cleanup() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0) {
            close(clients[i].sockfd);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_destroy(&clients_mutex);
    printf("服务器已关闭\n");
}