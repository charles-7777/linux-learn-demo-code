#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

// ========== 全局配置 ==========
#define PORT 8888
#define BUFFER_SIZE 1024

// 服务器IP地址（全局变量）
const char *SERVER_IP = "127.0.0.1";
// ===============================

int sock = 0;
volatile sig_atomic_t running = 1;

// 信号处理
void sigint_handler(int sig) {
    running = 0;
    if (sock > 0) {
        close(sock);
    }
    printf("\n客户端已退出\n");
    exit(0);
}

// 接收消息线程
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int valread;
    
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(sock, buffer, BUFFER_SIZE);
        
        if (valread > 0) {
            printf("%s", buffer);
            fflush(stdout);
        } else if (valread == 0) {
            printf("服务器已断开连接\n");
            break;
        } else {
            if (errno != EINTR) {
                perror("read error");
                break;
            }
        }
    }
    return NULL;
}

int main() {
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    pthread_t recv_thread;
    
    // 注册信号处理
    signal(SIGINT, sigint_handler);
    
    // 创建socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // 使用全局变量 SERVER_IP
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    // 连接服务器
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    printf("已连接到服务器 %s:%d\n", SERVER_IP, PORT);
    printf("输入消息 (输入 'quit' 退出):\n");
    
    // 创建接收线程
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("pthread_create");
        close(sock);
        return -1;
    }
    pthread_detach(recv_thread);
    
    // 发送消息
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // 去掉换行符
        
        if (strcmp(buffer, "quit") == 0) {
            break;
        }
        
        if (strlen(buffer) > 0) {
            if (send(sock, buffer, strlen(buffer), 0) < 0) {
                perror("send error");
                break;
            }
        }
    }
    
    close(sock);
    printf("客户端已退出\n");
    return 0;
}