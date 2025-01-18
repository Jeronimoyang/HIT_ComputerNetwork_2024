#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340      // 接收数据的端口号
#define SERVER_IP "127.0.0.1"  // 服务器的 IP 地址
#define BUFFER_SIZE 1024       // 缓冲区大小
#define SEQ_SIZE 16            // 序列号个数
#define SWIN_SIZE 8            // 发送窗口大小
#define RWIN_SIZE 8            // 接收窗口大小
#define LOSS_RATE 0.8          // 丢包率
using namespace std;

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
// 缓存结构体
struct Cache {
    bool used;                               // 是否使用
    char buffer[BUFFER_SIZE];                // 缓冲区
    Cache() {                                // 构造函数
        used = false;                        // 初始化为未使用
        ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
    }
} recvWindow[SEQ_SIZE];  // 接收窗口
// 数据帧结构体
struct DataFrame {
    clock_t start;                           // 发送时间
    char buffer[BUFFER_SIZE];                // 缓冲区
    DataFrame() {                            // 构造函数
        start = 0;                           // 初始化发送时间
        ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
    }
} sendWindow[SEQ_SIZE];  // 发送窗口
int main(int argc, char* argv[]) {
    // 加载套接字库
    WORD wVersionRequested;
    WSADATA wsaData;
    // 版本 2.2
    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);  // 加载 Winsock.dll
    if (err != 0) {                                     // 加载失败
        // 输出错误码
        printf("Winsock.dll 加载失败，错误码: %d\n", err);
        return -1;
    }
    // 判断 Winsock.dll 的版本
    if (LOBYTE(wsaData.wVersion) != LOBYTE(wVersionRequested) ||
        HIBYTE(wsaData.wVersion) != HIBYTE(wVersionRequested)) {
        // 如果版本不符合要求，输出错误信息
        printf("找不到 %d.%d 版本的 Winsock.dll\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
        WSACleanup();  // 清理 Winsock.dll
        return -1;
    }
    else {  // 加载成功
        printf("Winsock %d.%d 加载成功\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
    }
    // 创建客户端套接字
    SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
    // 设置为非阻塞模式
    int iMode = 1;                                            // 非阻塞模式
    ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);  // 设置非阻塞
    SOCKADDR_IN addrServer;                                   // 服务器地址
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // 设置服务器 IP
    addrServer.sin_family = AF_INET;                      // 设置地址族
    addrServer.sin_port = htons(SERVER_PORT);             // 设置端口号
    srand((unsigned)time(NULL));  // 设置随机数种子
    int status = 0;               // 状态
    clock_t start;                // 开始时间
    clock_t now;                  // 当前时间
    int seq;                      // 序列号
    int ack;                      // 确认号
    while (true) {
        gets_s(cmdBuffer, 50);  // 从控制台读取命令
        // 解析命令
        sscanf_s(cmdBuffer, "%s%s", cmd, sizeof(cmd) - 1, fileName,
            sizeof(fileName) - 1);
        // 如果命令式 -upload
        if (!strcmp(cmd, "upload")) {
            // 打印上传文件
            printf("申请上传文件: %s\n", fileName);
            strcpy_s(filePath, "./");      // 设置文件路径
            strcat_s(filePath, fileName);  // 连接文件名
            ifstream infile(filePath);     // 打开文件
            start = clock();               // 获取当前时间
            seq = 0;                       // 序列号
            status = 0;                    // 状态
            sendWindow[0].buffer[0] = 10;  // 设置缓冲区
            // 设置缓冲区
            strcpy_s(sendWindow[0].buffer + 1, strlen(cmdBuffer) + 1,
                cmdBuffer);
            sendWindow[0].start = start - 1000L;  // 设置发送时间
            while (true) {
                // 接收数据
                recvSize = recvfrom(socketClient, buffer, BUFFER_SIZE, 0,
                    (SOCKADDR*)&addrServer, &len);
                switch (status) {  // 根据状态处理数据
                case 0:        // 请求上传
                    // 如果接收到数据，并且数据类型为 100
                    if (recvSize > 0 && buffer[0] == 100) {
                        // 如果数据内容为 OK
                        if (!strcmp(buffer + 1, "OK")) {
                            // 打印申请通过
                            printf("申请通过, 开始上传...\n");
                            start = clock();           // 获取当前时间
                            status = 1;                // 设置状态为 1
                            sendWindow[0].start = 0L;  // 设置发送时间
                            continue;                  // 跳过当前循环
                        }
                        // 如果数据内容为NO
                        else if (!strcmp(buffer + 1, "NO")) {
                            status = -1;  // 设置状态为 -1
                            break;        // 跳出循环
                        }
                    }
                    now = clock();  // 获取当前时间
                    // 如果当前时间减去发送时间大于 1000 毫秒
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // 更新发送时间
                        // 发送数据
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                    break;
                case 1:  // 发送文件数据
                    // 如果接收到数据，并且数据类型为 101
                    if (recvSize > 0 && buffer[0] == 101) {
                        start = clock();              // 获取当前时间
                        ack = buffer[1];              // 获取确认号
                        ack--;                        // 确认号减一
                        sendWindow[ack].start = -1L;  // 设置发送时间
                        if (ack == seq) {  // 如果确认号等于序列号
                            seq = MoveSendWindow(seq);  // 交付数据
                        }
                        // 打印接收数据
                        printf("接收 ack = %d, 当前起始 seq = %d\n",
                            ack + 1, seq + 1);
                    }
                    // 如果文件已发送完毕
                    if (!Send(infile, seq, socketClient,
                        (SOCKADDR*)&addrServer)) {
                        // 打印上传完毕
                        printf("上传完毕...\n");
                        status = 2;                    // 设置状态为 2
                        start = clock();               // 获取当前时间
                        sendWindow[0].buffer[0] = 10;  // 设置缓冲区
                        // 设置缓冲区
                        strcpy_s(sendWindow[0].buffer + 1, 7, "Finish");
                        // 设置发送时间
                        sendWindow[0].start = start - 1000L;
                        continue;  // 跳过当前循环
                    }
                    break;
                case 2:  // 等待完成确认
                    // 如果接收到数据，并且数据类型为 100
                    if (recvSize > 0 && buffer[0] == 100) {
                        // 如果数据内容为 OK
                        if (!strcmp(buffer + 1, "OK")) {
                            buffer[0] = 10;  // 设置缓冲区
                            strcpy_s(buffer + 1, 3, "OK");  // 设置缓冲区
                            // 发送数据
                            sendto(socketClient, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrServer,
                                sizeof(SOCKADDR));
                            status = 3;  // 设置状态为 3
                            break;       // 跳出循环
                        }
                    }
                    now = clock();  // 获取当前时间
                    // 如果当前时间减去发送时间大于 1000 毫秒
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // 更新发送时间
                        // 发送数据
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                default:
                    break;
                }
                // 如果状态为 -1
                if (status == -1) {
                    printf("服务器拒绝请求\n");
                    infile.close();
                    break;
                }
                // 如果状态为 3
                if (status == 3) {
                    printf("上传成功，结束通信\n");
                    infile.close();
                    break;
                }
                // 如果当前时间减去开始时间大于 5000 毫秒
                if (clock() - start >= 5000L) {
                    printf("通信超时，结束通信\n");
                    infile.close();
                    break;
                }
                // 如果接收到的数据大小小于等于 0
                if (recvSize <= 0) {
                    Sleep(200);
                }
            }
        }
        // 如果命令式 -download
        else if (!strcmp(cmd, "download")) {
            // 打印下载文件
            printf("申请下载文件 %s\n", fileName);
            strcpy_s(filePath, "./");      // 设置文件路径
            strcat_s(filePath, fileName);  // 连接文件名
            ofstream outfile(filePath);    // 打开文件
            start = clock();               // 获取当前时间
            ack = 0;                       // 确认号
            status = 0;                    // 状态
            sendWindow[0].buffer[0] = 10;  // 设置缓冲区
            // 设置缓冲区
            strcpy_s(sendWindow[0].buffer + 1, strlen(cmdBuffer) + 1,
                cmdBuffer);
            sendWindow[0].start = start - 1000L;  // 设置发送时间
            while (true) {                        // 循环处理数据
                recvSize = recvfrom(socketClient, buffer, BUFFER_SIZE, 0,
                    (SOCKADDR*)&addrServer, &len);
                // 模拟丢包,若随机数大于丢包率则丢包
                if ((float)rand() / RAND_MAX > LOSS_RATE) {
                    recvSize = 0;   // 设置接收数据大小为 0
                    buffer[0] = 0;  // 设置缓冲区
                }
                switch (status) {
                case 0:  // 请求下载
                    // 如果接收到数据，并且数据类型为 100
                    if (recvSize > 0 && buffer[0] == 100) {
                        // 如果数据内容为 OK
                        if (!strcmp(buffer + 1, "OK")) {
                            // 打印申请通过
                            printf("申请通过, 准备下载...\n");
                            start = clock();  // 获取当前时间
                            status = 1;       // 设置状态为 1
                            sendWindow[0].buffer[0] = 10;  // 设置缓冲区
                            // 设置缓冲区
                            strcpy_s(sendWindow[0].buffer + 1, 3, "OK");
                            // 设置发送时间
                            sendWindow[0].start = start - 1000L;
                            continue;  // 跳过当前循环
                        }
                        // 如果数据内容为 NO
                        else if (!strcmp(buffer + 1, "NO")) {
                            status = -1;  // 设置状态为 -1
                            break;        // 跳出循环
                        }
                    }
                    now = clock();  // 获取当前时间
                    // 如果当前时间减去发送时间大于 1000 毫秒
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // 更新发送时间
                        // 发送数据
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                    break;
                case 1:  // 开始下载数据
                    // 如果接收到数据，并且数据类型为 200
                    if (recvSize > 0 && (unsigned char)buffer[0] == 200) {
                        // 打印开始下载
                        printf("开始下载...\n");
                        start = clock();  // 获取当前时间
                        seq = buffer[1];  // 获取序列号
                        // 输出接收数据
                        printf(
                            "接收数据帧 seq = %d, data = %s, 发送ack = "
                            "%d\n",
                            seq, buffer + 2, seq);
                        seq--;                        // 序列号减一
                        recvWindow[seq].used = true;  // 设置为已使用
                        // 设置缓冲区
                        strcpy_s(recvWindow[seq].buffer,
                            strlen(buffer + 2) + 1, buffer + 2);
                        if (ack == seq) {  // 如果确认号等于序列号
                            ack = Deliver(file, ack);  // 交付数据
                            status = 2;                // 设置状态为 2
                            buffer[0] = 11;            // 设置缓冲区
                            buffer[1] = seq + 1;       // 设置缓冲区
                            buffer[2] = 0;             // 设置缓冲区
                        }
                        sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                        continue;  // 跳过当前循环
                    }
                    now = clock();  // 获取当前时间
                    // 如果当前时间减去开始时间大于 5000 毫秒
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // 更新发送时间
                        // 发送数据
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                    break;
                case 2:                  // 处理数据帧
                    if (recvSize > 0) {  // 如果接收到数据
                        // 如果数据类型为 200
                        if ((unsigned char)buffer[0] == 200) {
                            seq = buffer[1];           // 获取序列号
                            int temp = seq - 1 - ack;  // 计算序列号差
                            if (temp < 0) {  // 如果序列号差小于 0
                                temp += SEQ_SIZE;  // 序列号差加上序列号个数
                            }
                            start = clock();  // 获取当前时间
                            seq--;            // 序列号减一
                            // 如果序列号差小于接收窗口大小
                            if (temp < RWIN_SIZE) {
                                // 如果接收窗口中的数据包未使用
                                if (!recvWindow[seq].used) {
                                    // 设置为已使用
                                    recvWindow[seq].used = true;
                                    // 设置缓冲区
                                    strcpy_s(recvWindow[seq].buffer,
                                        strlen(buffer + 2) + 1,
                                        buffer + 2);
                                }
                                // 如果确认号等于序列号
                                if (ack == seq) {
                                    ack = Deliver(file, ack);  // 交付数据
                                }
                            }
                            // 输出接收数据
                            printf(
                                "接收数据帧 seq = %d, data = %s, 发送 ack "
                                "= %d, 起始 ack = %d\n",
                                seq + 1, buffer + 2, seq + 1, ack + 1);
                            buffer[0] = 11;       // 设置缓冲区
                            buffer[1] = seq + 1;  // 设置缓冲区
                            buffer[2] = 0;        // 设置缓冲区
                            // 发送数据
                            sendto(socketClient, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrServer,
                                sizeof(SOCKADDR));
                        }
                        // 如果数据类型为 100
                        else if (buffer[0] == 100 &&
                            !strcmp(buffer + 1, "Finish")) {
                            status = 3;  // 设置状态为 3
                            outfile.write(file, strlen(file));  // 写入文件
                            buffer[0] = 10;  // 设置缓冲区
                            strcpy_s(buffer + 1, 3, "OK");  // 设置缓冲区
                            // 发送数据
                            sendto(socketClient, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrServer,
                                sizeof(SOCKADDR));
                            continue;  // 跳过当前循环
                        }
                    }
                    break;  // 跳出循环
                default:
                    break;
                }
                // 如果状态为 -1
                if (status == -1) {
                    printf("服务器拒绝请求\n");
                    outfile.close();
                    break;
                }
                // 如果状态为 3
                if (status == 3) {
                    printf("下载成功, 结束通信\n");
                    outfile.close();
                    break;
                }
                // 如果当前时间减去开始时间大于 5000 毫秒
                if (clock() - start >= 50000L) {
                    printf("通信超时, 结束通信\n");
                    outfile.close();
                    break;
                }
                // 如果接收到的数据大小小于等于 0
                if (recvSize <= 0) {
                    Sleep(20);
                }
            }
        }
        else if (!strcmp(cmd, "quit")) {  // 如果命令式 -quit
            break;                          // 退出循环
        }
    }
    closesocket(socketClient);  // 关闭套接字
    printf("关闭套接字\n");     // 打印关闭套接字
    WSACleanup();               // 清理 Winsock.dll
    return 0;
}

// 从一个输入文件流中读取数据，并将读取的内容存储到一个字符数组（缓冲区）中
int Read(ifstream& infile, char* buffer) {
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
    clock_t now = clock();  // 获取当前时间
    // 循环遍历发送窗口大小，计算当前发送窗口的索引，时期在序列号范围内循环
    for (int i = 0; i < SWIN_SIZE; i++) {
        int j = (seq + i) % SEQ_SIZE;      // 计算当前发送窗口的索引
        if (sendWindow[j].start == -1L) {  // 如果当前数据帧已经发送
            continue;                      // 跳过当前数据帧
        }
        if (sendWindow[j].start == 0L) {  // 如果当前数据帧未发送
            if (Read(infile, sendWindow[j].buffer + 2)) {  // 从文件流中读取数据
                sendWindow[j].start = now;  // 更新当前数据帧的发送时间
                sendWindow[j].buffer[0] = 20;     // 设置数据帧的类型
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
        // 发送数据帧到指定的地址
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