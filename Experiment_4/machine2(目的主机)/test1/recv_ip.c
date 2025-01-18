#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
int main()
{
  int sockfd;
  struct sockaddr_in src_addr, my_addr;
  char buffer[1024];
  socklen_t addr_len;
  int port = 54321; // 修改后的接收端口号
  // 创建 UDP 套接字
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
  {
    perror("socket");
    return 1;
  }
  // 本地地址
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  // 绑定套接字到本地地址
  if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
  {
    perror("bind");
    return 1;
  }
  // 接收数据报
  addr_len = sizeof(src_addr);
  if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr,
               &addr_len) < 0)
  {
    perror("recvfrom");
    return 1;
  }
  printf("Datagram received: %s\n", buffer);
  return 0;
}