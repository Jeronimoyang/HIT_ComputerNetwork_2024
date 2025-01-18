#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define DEST_IP "192.168.152.128"
#define DEST_PORT 12345
#define MESSAGE "Hello, this is a test message."
int main() {
    int sockfd;
    struct sockaddr_in dest_addr;
    // 创建 UDP 套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    // 设置目的地址
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest_addr.sin_addr);
    // 发送数据包
    if (sendto(sockfd, MESSAGE, strlen(MESSAGE), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("Sendto failed");
        return 1;
    }
    printf("Message sent to %s:%d\n", DEST_IP, DEST_PORT);
    close(sockfd);
    return 0;
}
