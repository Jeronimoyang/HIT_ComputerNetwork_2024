#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340    // 端口号
#define SERVER_IP "0.0.0.0"  // IP 地址
#define SEQ_SIZE 16          // 序列号个数
#define SWIN_SIZE 8          // 发送窗口大小
#define RWIN_SIZE 8          // 接收窗口大小
#define BUFFER_SIZE 1024     // 缓冲区大小
#define LOSS_RATE 0.8        // 丢包率
using namespace std;
// 定义结构体，用于存储接收窗口和发送窗口的数据
struct recv {
    bool used;                 // 标记数据包是否已经被使用
    char buffer[BUFFER_SIZE];  // 缓冲区
    recv() {                   // 构造函数
        used = false;          // 初始化数据包未被使用
        ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
    }
} recvWindow[SEQ_SIZE];  // 接收窗口
// 定义结构体，用于存储发送窗口的数据
struct send {
    clock_t start;  // 由于使用的是SR，因此每一个窗口位置都需要设置一个计时器
    char buffer[BUFFER_SIZE];                // 缓冲区
    send() {                                 // 构造函数
        start = 0;                           // 初始化计时器
        ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
    }
} sendWindow[SEQ_SIZE];      // 发送窗口
char cmdBuffer[50];          // 命令缓冲区
char buffer[BUFFER_SIZE];    // 缓冲区
char cmd[10];                // 命令
char fileName[40];           // 文件名
char filePath[50];           // 文件路径
char file[1024 * 1024];      // 文件字符串
int len = sizeof(SOCKADDR);  // 地址长度
int recvSize;                // 接收到的数据大小
int Deliver(char* file, int ack);
int Send(ifstream& infile, int seq, SOCKET socket, SOCKADDR* addr);
int MoveSendWindow(int seq);
int Read(ifstream& infile, char* buffer);
// 主函数
int main(int argc, char* argv[]) {
    // 加载套接字库
    WORD wVersionRequested;
    WSADATA wsaData;
    // 版本 2.2
    wVersionRequested = MAKEWORD(2, 2);  // 请求 2.2 版本的 Winsock 库
    int err = WSAStartup(wVersionRequested, &wsaData);  // 加载 Winsock 库
    if (err != 0) {  // 如果加载 Winsock 库失败
        printf("Winsock.dll 加载失败，错误码: %d\n", err);
        return -1;
    }
    // 判断请求的版本号是否正确
    if (LOBYTE(wsaData.wVersion) != LOBYTE(wVersionRequested) ||
        HIBYTE(wsaData.wVersion) != HIBYTE(wVersionRequested)) {
        // 如果请求的版本号不正确
        printf("找不到 %d.%d 版本的 Winsock.dll\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
        WSACleanup();  // 卸载 Winsock 库
        return -1;
    }
    else {  // 如果请求的版本号正确
        printf("Winsock %d.%d 加载成功\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
    }
    // 创建服务器套接字
    SOCKET socketServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    // 设置为非阻塞模式
    int iMode = 1;  // 1 为非阻塞，0 为阻塞
    // 设置为非阻塞模式
    ioctlsocket(socketServer, FIONBIO, (u_long FAR*) & iMode);
    SOCKADDR_IN addrServer;                               // 服务器地址
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // 设置服务器 IP 地址
    addrServer.sin_family = AF_INET;                      // 设置地址族
    addrServer.sin_port = htons(SERVER_PORT);             // 设置端口号
    // 绑定端口
    if (err = bind(socketServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR))) {
        err = GetLastError();  // 获取错误码
        printf("绑定端口 %d 失败，错误码: % d\n", SERVER_PORT, err);
        WSACleanup();  // 卸载 Winsock 库
        return -1;
    }
    else {                                      // 绑定端口成功
        printf("绑定端口 %d 成功", SERVER_PORT);  // 输出提示信息
    }
    SOCKADDR_IN addrClient;  // 客户端地址
    int status = 0;          // 状态
    clock_t start;           // 开始时间
    clock_t now;             // 当前时间
    int seq;                 // 序列号
    int ack;                 // 确认号
    ofstream outfile;        // 输出文件流
    ifstream infile;         // 输入文件流
    // 进入接收状态，注意服务器主要处理的任务是接收客户机请求，共有上载和下载两种任务
    while (true) {
        // 服务器持续接收来自客户端的数据包
        recvSize = recvfrom(socketServer, buffer, BUFFER_SIZE, 0,
            ((SOCKADDR*)&addrClient), &len);
        // 模拟丢包
        if ((float)rand() / RAND_MAX > LOSS_RATE) {
            recvSize = 0;
            buffer[0] = 0;
        }
        switch (status) {
        case 0:  // 接收请求
            // 如果接收到数据
            if (recvSize > 0 && buffer[0] == 10) {
                char addr[100];                  // 地址
                ZeroMemory(addr, sizeof(addr));  // 初始化地址
                // 获取客户端地址
                inet_ntop(AF_INET, &addrClient.sin_addr, addr,
                    sizeof(addr));
                // 获取命令和文件名
                sscanf_s(buffer + 1, "%s%s", cmd, sizeof(cmd) - 1, fileName,
                    sizeof(fileName) - 1);
                // 如果命令不是上传或下载
                if (strcmp(cmd, "upload") && strcmp(cmd, "download")) {
                    continue;  // 继续接收请求
                }
                strcpy_s(filePath, "./");      // 设置文件路径
                strcat_s(filePath, fileName);  // 连接文件名
                // 输出提示信息
                printf("收到来自客户端 %s 的请求: %s\n", addr, buffer);
                printf("是否同意该请求(Y/N)?");     // 输出提示信息
                gets_s(cmdBuffer, 50);              // 获取输入
                if (!strcmp(cmdBuffer, "Y")) {      // 如果输入为 Y
                    buffer[0] = 100;                // 设置数据帧类型
                    strcpy_s(buffer + 1, 3, "OK");  // 设置数据帧内容
                    // 如果命令是上传
                    if (!strcmp(cmd, "upload")) {
                        file[0] = 0;             // 初始化文件字符串
                        start = clock();         // 记录开始时间
                        ack = 0;                 // 初始化确认号
                        status = 1;              // 进入上传状态
                        outfile.open(filePath);  // 打开文件
                    }
                    // 如果命令是下载
                    else if (!strcmp(cmd, "download")) {
                        start = clock();        // 记录开始时间
                        seq = 0;                // 初始化序列号
                        status = -1;            // 进入下载状态
                        infile.open(filePath);  // 打开文件
                    }
                }
                else {                            // 如果输入不为 Y
                    buffer[0] = 100;                // 设置数据帧类型
                    strcpy_s(buffer + 1, 3, "NO");  // 设置数据帧内容
                }
                // 发送数据帧
                sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                    (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            }
            break;
        case 1:  // 客户机请求上传，也就是服务器端是接收方
            // 如果接收到数据
            if (recvSize > 0) {
                // 如果接收到的数据帧类型是 10
                if (buffer[0] == 10) {
                    // 如果接收到的FINISH数据帧
                    if (!strcmp(buffer + 1, "Finish")) {
                        // 输出提示信息
                        printf("传输完毕...\n");
                        start = clock();  // 记录开始时间
                        sendWindow[0].start =
                            start - 1000L;  // 设置发送窗口的开始时间
                        sendWindow[0].buffer[0] = 100;  // 设置数据帧类型
                        // 设置数据帧内容
                        strcpy_s(sendWindow[0].buffer + 1, 3, "OK");
                        // 将文件字符串写入文件流
                        outfile.write(file, strlen(file));
                        status = 2;  // 进入接收完成状态
                    }
                    buffer[0] = 100;                // 设置数据帧类型
                    strcpy_s(buffer + 1, 3, "OK");  // 设置数据帧内容
                    // 发送数据帧
                    sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                }
                // 如果接收到的数据帧类型是 20
                else if (buffer[0] == 20) {
                    seq = buffer[1];           // 获取序列号
                    int temp = seq - 1 - ack;  // 计算序列号差
                    if (temp < 0) {            // 如果序列号差小于 0
                        temp += SEQ_SIZE;  // 序列号差加上序列号个数
                    }
                    start = clock();  // 记录开始时间
                    seq--;            // 更新序列号
                    // 如果序列号差小于接收窗口大小
                    if (temp < RWIN_SIZE) {
                        // 如果接收窗口中的数据包未被使用
                        if (!recvWindow[seq].used) {
                            // 标记数据包已被使用
                            recvWindow[seq].used = true;
                            // 将数据包的内容存储到接收窗口中
                            strcpy_s(recvWindow[seq].buffer,
                                strlen(buffer + 2) + 1, buffer + 2);
                        }
                        if (ack == seq) {  // 如果确认号等于序列号
                            ack = Deliver(file, ack);  // 交付数据包
                        }
                    }
                    // 输出提示信息
                    printf(
                        "接收数据帧 seq = %d, data = %s, 发送 ack = %d, "
                        "起始 ack = %d\n",
                        seq + 1, buffer + 2, seq + 1, ack + 1);
                    buffer[0] = 101;      // 设置数据帧类型
                    buffer[1] = seq + 1;  // 设置数据帧序列号
                    buffer[2] = 0;        // 设置数据帧内容
                    // 发送数据帧
                    sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                }
            }
            break;
        case 2:  // 接收完成
                 // 如果传输成功
            if (recvSize > 0 && buffer[0] == 10 &&
                !strcmp(buffer + 1, "OK")) {
                // 输出提示信息
                printf("传输成功，结束通信\n");
                status = 0;       // 进入接收请求状态
                outfile.close();  // 关闭文件流
            }
            now = clock();  // 获取当前时间
            // 如果当前时间减去发送窗口的开始时间大于 1000 毫秒
            if (now - sendWindow[0].start >= 1000L) {
                sendWindow[0].start = now;  // 更新发送窗口的开始时间
                // 发送数据帧
                sendto(socketServer, sendWindow[0].buffer,
                    strlen(sendWindow[0].buffer) + 1, 0,
                    (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            }
            break;  // 结束通信
        case -1:  // 客户机请求下载，也就是服务器端充当发送方
            // 如果接收到数据
            if (recvSize > 0) {
                // 如果接收到的数据帧类型是 10
                if (buffer[0] == 10) {
                    // 如果接收到的数据帧内容是 OK
                    if (!strcmp(buffer + 1, "OK")) {
                        printf("开始传输...\n");  // 输出提示信息
                        start = clock();          // 记录开始时间
                        status = -2;              // 进入发送状态
                    }
                    buffer[0] = 100;                // 设置数据帧类型
                    strcpy_s(buffer + 1, 3, "OK");  // 设置数据帧内容
                    // 发送数据帧
                    sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                }
            }
            break;
        case -2:  // 服务器端发送数据
            // 如果接收到数据
            if (recvSize > 0 && buffer[0] == 11) {
                start = clock();              // 记录开始时间
                ack = buffer[1];              // 获取确认号
                ack--;                        // 更新确认号
                sendWindow[ack].start = -1L;  // 设置数据帧的开始时间
                if (ack == seq) {  // 如果确认号等于序列号
                    seq = MoveSendWindow(seq);  // 移动发送窗口
                }
                // 输出提示信息
                printf("接收 ack = %d, 当前起始 seq = %d\n", ack + 1,
                    seq + 1);
            }
            // 如果传输完毕
            if (!Send(infile, seq, socketServer, (SOCKADDR*)&addrClient)) {
                printf("传输完毕...\n");        // 输出提示信息
                status = -3;                    // 进入请求完成状态
                start = clock();                // 记录开始时间
                sendWindow[0].buffer[0] = 100;  // 设置数据帧类型
                // 设置数据帧内容
                strcpy_s(sendWindow[0].buffer + 1, 7, "Finish");
                // 设置发送窗口的开始时间
                sendWindow[0].start = start - 1000L;
            }
            break;
        case -3:  // 请求完成
            // 如果传输成功
            if (recvSize > 0 && buffer[0] == 10) {
                // 如果接收到的数据帧内容是 OK
                if (!strcmp(buffer + 1, "OK")) {
                    printf("传输成功，结束通信\n");  // 输出提示信息
                    infile.close();                  // 关闭文件流
                    status = 0;  // 进入接收请求状态
                    break;       // 结束通信
                }
            }
            now = clock();  // 获取当前时间
            // 如果当前时间减去发送窗口的开始时间大于 1000 毫秒
            if (now - sendWindow[0].start >= 1000L) {
                sendWindow[0].start = now;  // 更新发送窗口的开始时间
                // 发送数据帧
                sendto(socketServer, sendWindow[0].buffer,
                    strlen(sendWindow[0].buffer) + 1, 0,
                    (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            }
        default:
            break;
        }
        // 如果通信超时
        if (status != 0 && clock() - start > 50000L) {
            printf("通信超时, 结束通信\n");
            status = 0;       // 进入接收请求状态
            outfile.close();  // 关闭文件流
            continue;         // 继续接收请求
        }
        // 如果没有接收到数据
        if (recvSize <= 0) {
            Sleep(20);  // 休眠 20 毫秒
        }
    }
    // 关闭套接字，卸载库
    closesocket(socketServer);  // 关闭套接字
    WSACleanup();               // 卸载 Winsock 库
    return 0;
}

// 从一个输入文件流中读取数据，并将读取的内容存储到一个字符数组（缓冲区）中
int Read(ifstream& infile, char* buffer) {
    // 从文件中读取需要发送的数据
    if (infile.eof()) {  // 判断是否到达文件末尾
        return 0;
    }
    infile.read(buffer, 3);     // 从文件流中读取数据
    int cnt = infile.gcount();  // 获取读取的字节数
    buffer[cnt] = 0;            // 添加字符串结束符
    return cnt;                 // 返回读取的字节数
}
// 处理已确认的数据包，并将其内容追加到指定的文件字符串中
int Deliver(char* file, int ack) {
    while (recvWindow[ack].used) {  // 当接收窗口中的数据包已经被确认时
        recvWindow[ack].used = false;  // 将该数据包标记为未使用
        // 将数据包的内容追加到文件字符串中
        strcat_s(file, strlen(file) + strlen(recvWindow[ack].buffer) + 1,
            recvWindow[ack].buffer);
        ack++;            // 更新确认号
        ack %= SEQ_SIZE;  // 确认号取模
    }
    return ack;  // 返回更新后的确认号
}
// 从输入文件流中读取数据，并通过网络发送数据帧
int Send(ifstream& infile, int seq, SOCKET socket, SOCKADDR* addr) {
    // 发送数据
    clock_t now = clock();
    // 循环遍历发送窗口大小，计算当前发送窗口的索引，时期在序列号范围内循环
    for (int i = 0; i < SWIN_SIZE; i++) {
        int j = (seq + i) % SEQ_SIZE;      // 计算当前发送窗口的索引
        if (sendWindow[j].start == -1L) {  // 传输超时，不需要
            continue;                      // 跳过当前数据帧
        }
        if (sendWindow[j].start == 0L) {  // 开始计时,如果当前数据帧未发送
            if (Read(infile, sendWindow[j].buffer + 2)) {  // 从文件流中读取数据
                sendWindow[j].start = now;  // 更新当前数据帧的发送时间
                sendWindow[j].buffer[0] = 200;    // 设置数据帧的类型
                sendWindow[j].buffer[1] = j + 1;  // 设置数据帧的序列号
            }
            else if (i == 0) {  // 如果当前数据帧是发送窗口的第一个数据帧
                return 0;  // 返回 0，表示发送完毕
            }
            else {  // 如果当前数据帧不是发送窗口的第一个数据帧
                break;  // 跳出循环
            }
        }  // 如果当前数据帧发送超时
        else if (now - sendWindow[j].start >= 1000L) {
            sendWindow[j].start = now;  // 更新当前数据帧的发送时间
        }
        else {                        // 如果当前数据帧未发送超时
            continue;                   // 跳过当前数据帧
        }
        // 发送数据帧
        printf("发送数据帧 seq = %d, data = %s\n", j + 1,
            sendWindow[j].buffer + 2);
        // 通过网络发送数据帧
        sendto(socket, sendWindow[j].buffer, strlen(sendWindow[j].buffer) + 1,
            0, addr, sizeof(SOCKADDR));
    }
    return 1;
}
// 移动发送窗口，找到下一个可以发送的序列号
int MoveSendWindow(int seq) {
    // 循环检查发送窗口中当前序列号 seq 对应的数据包状态
    while (sendWindow[seq].start == -1L) {  // 当当前序列号的数据帧已经发送
        sendWindow[seq].start = 0L;  // 更新当前序列号的数据帧的发送时间
        seq++;                       // 更新序列号
        seq %= SEQ_SIZE;             // 序列号取模
    }
    return seq;  // 返回更新后的序列号
}