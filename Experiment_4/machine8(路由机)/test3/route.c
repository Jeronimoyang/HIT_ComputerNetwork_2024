#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 65536
struct route_entry {
    uint32_t dest;
    uint32_t gateway;
    uint32_t netmask;
    char interface[IFNAMSIZ];
};
struct route_entry route_table[1];

int route_table_size = sizeof(route_table) / sizeof(route_table[0]);

void convert_to_ip_string(uint32_t ip_addr, char *ip_str) {
    struct in_addr addr;
    addr.s_addr =
        ip_addr;  // htonl(ip_addr); // 转换为网络字节序 inet_ntop(AF_INET,
                  // &addr, ip_str, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
}

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    for (sum = 0; len > 1; len -= 2) sum += *buf++;
    if (len == 1) sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

struct route_entry *lookup_route(uint32_t dest_ip) {
    char ip_str[32];

    for (int i = 0; i < route_table_size; i++) {
        // convert_to_ip_string(dest_ip, ip_str);
        // printf("IP Address: %s\n", ip_str);

        // convert_to_ip_string(route_table[i].dest, ip_str);
        // printf("IP Address: %s\n", ip_str);

        if ((dest_ip & route_table[i].netmask) ==
            (route_table[i].dest & route_table[i].netmask)) {
            convert_to_ip_string(dest_ip, ip_str);
            printf("-------IP Address: %s\n", ip_str);

            convert_to_ip_string(route_table[i].dest, ip_str);
            printf("--------IP Address: %s\n", ip_str);
            return &route_table[i];
        }
    }
    return NULL;
}

void initialize_route_table() {
    route_table[0].dest = inet_addr("192.168.152.128");
    route_table[0].gateway = inet_addr("192.168.109.2");
    route_table[0].netmask = inet_addr("255.255.255.0");
    strcpy(route_table[0].interface, "ens37");
}

int main() {
    int sockfd;
    struct sockaddr saddr;
    unsigned char *buffer = (unsigned char *)malloc(BUFFER_SIZE);

    initialize_route_table();

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    while (1) {
        int saddr_len = sizeof(saddr);
        int data_size = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, &saddr,
                                 (socklen_t *)&saddr_len);
        if (data_size < 0) {
            perror("Recvfrom error");
            return 1;
        }
        if (data_size == 0) continue;
        struct ethhdr *eth_header = (struct ethhdr *)buffer;

        struct iphdr *ip_header =
            (struct iphdr *)(buffer + sizeof(struct ethhdr));
        struct route_entry *route = lookup_route(ip_header->daddr);

        if (route == NULL) {
            // fprintf(stderr, "No route to host\n");
            continue;
        }
        char ip_s[32], ip_d[32];
        convert_to_ip_string(ip_header->saddr, ip_s);
        convert_to_ip_string(ip_header->daddr, ip_d);

        printf("Captured packet from %s to %s\n", ip_s, ip_d);

        // 修改 TTL
        ip_header->ttl -= 1;
        ip_header->check = 0;
        ip_header->check =
            checksum((unsigned short *)ip_header, ip_header->ihl * 4);
        // 发送数据包到目的主机
        struct ifreq ifr, ifr_mac;
        struct sockaddr_ll dest;
        // 获取网卡接口索引
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), route->interface);
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
            perror("ioctl");
            return 1;
        }
        // 获取网卡接口 MAC 地址
        memset(&ifr_mac, 0, sizeof(ifr_mac));
        snprintf(ifr_mac.ifr_name, sizeof(ifr_mac.ifr_name), route->interface);
        if (ioctl(sockfd, SIOCGIFHWADDR, &ifr_mac) < 0) {
            perror("ioctl");
            return 1;
        }
        // 设置目标 MAC
        // 地址（假设目标地址已知,此处做了简化处理，实际上，如果查找路由表后，存在“下
        // 一跳”，应该利用 ARP 协议获得 route->gateway 的 MAC
        // 地址，如果是“直接交付”的话，也应使用 ARP 协议获得 目的主机的 MAC
        // 地址。）
        unsigned char target_mac[ETH_ALEN] = {0x00, 0x0c, 0x29,
                                              0x8E, 0x33, 0x8E};  //
        // 替换为实际的目标 MAC 地址
        memset(&dest, 0, sizeof(dest));
        dest.sll_ifindex = ifr.ifr_ifindex;
        dest.sll_halen = ETH_ALEN;
        memcpy(dest.sll_addr, target_mac, ETH_ALEN);
        // 构造新的以太网帧头
        memcpy(eth_header->h_dest, target_mac, ETH_ALEN);  // 目标 MAC 地址
        memcpy(eth_header->h_source, ifr_mac.ifr_hwaddr.sa_data,
               ETH_ALEN);                       // 源 MAC 地址
        eth_header->h_proto = htons(ETH_P_IP);  // 以太网类型为 IP
        printf("Interface name: %s, index: %d\n", ifr.ifr_name,
               ifr.ifr_ifindex);

        if (sendto(sockfd, buffer, data_size, 0, (struct sockaddr *)&dest,
                   sizeof(dest)) < 0) {
            perror("Sendto error");
            return 1;
        }
    }
    close(sockfd);
    free(buffer);
    return 0;
}
