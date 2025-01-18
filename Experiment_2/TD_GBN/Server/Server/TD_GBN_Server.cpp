// #include "stdafx.h" //创建 VS 项目包含的预编译头文件
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340    // 端口号
#define SERVER_IP "0.0.0.0"  // IP 地址
const int BUFFER_LENGTH =
1026;  // 缓冲区大小，（以太网中 UDP 的数据帧中包长度应小于 1480 字节）
const int SEND_WIND_SIZE = 10;  // 发送窗口大小为 10，GBN 中应满足 W + 1 <= N（W
                                // 为发送窗口大小，N 为序列号个数）
// 本例取序列号 0...19 共 20 个
// 如果将窗口大小设为 1，则为停-等协议
const int SEQ_SIZE = 20;  // 序列号的个数，从 0~19 共计 20 个
// 由于发送数据第一个字节如果值为 0，则数据会发送失败
// 因此接收端序列号为 1~20，与发送端一一对应
BOOL ack[SEQ_SIZE];  // 收到 ack 情况，对应 0~19 的 ack
int curSeq;          // 当前数据包的 seq
int curAck;          // 当前等待确认的 ack
int totalSeq;        // 收到的包的总数
int totalPacket;     // 需要发送的包总数

//************************************
// Method: lossInLossRatio
// FullName: lossInLossRatio
// Access: public
// Returns: BOOL
// Qualifier: 根据丢失率随机生成一个数字，判断是否丢失,丢失则返回TRUE，否则返回
// FALSE Parameter: float lossRatio [0,1]
//************************************
BOOL lossInLossRatio(float lossRatio) {
    // 将传入的丢失率转换为 0~100的整数
    int lossBound = (int)(lossRatio * 100);
    int r = rand() % 101;  // 生成 0~100 的随机数
    if (r <= lossBound) {  // 如果随机数小于等于丢失率
        return TRUE;
    }
    return FALSE;  // 否则不丢失
}
//************************************
// Method: getCurTime
// FullName: getCurTime
// Access: public
// Returns: void
// Qualifier: 获取当前系统时间，结果存入 ptime 中
// Parameter: char * ptime
//************************************
void getCurTime(char* ptime) {
    char buffer[128];                   // 缓冲区
    memset(buffer, 0, sizeof(buffer));  // 初始化
    time_t c_time;                      // 时间
    struct tm* p = NULL;                // 时间结构体
    time(&c_time);                      // 获取当前时间
    localtime_s(p, &c_time);            // 转换为本地时间
    printf("\n%d\n\n", p->tm_year);     // 输出年份
    // 格式化时间
    sprintf_s(buffer, "%d/%d/%d %d:%d:%d", p->tm_year + 1900, p->tm_mon,
        p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
    // 将格式化后的时间拷贝到 ptime 中
    strcpy_s(ptime, sizeof(buffer), buffer);
}
//************************************
// Method: seqIsAvailable
// FullName: seqIsAvailable
// Access: public
// Returns: bool
// Qualifier: 当前序列号 curSeq 是否可用
//************************************
bool seqIsAvailable() {
    int step;                // 当前序列号与当前确认号的差值
    step = curSeq - curAck;  // 当前序列号与当前确认号的差值
    step = step >= 0 ? step : step + SEQ_SIZE;  // 处理环形序列号
    // 序列号是否在当前发送窗口之内
    if (step >= SEND_WIND_SIZE) {
        return false;
    }
    // 如果收到 ack，则表示当前序列号可用
    if (ack[curSeq]) {
        return true;
    }
    // 如果未收到 ack，则表示当前序列号不可用
    return false;
}
//************************************
// Method: timeoutHandler
// FullName: timeoutHandler
// Access: public
// Returns: void
// Qualifier: 超时重传处理函数，滑动窗口内的数据帧都要重传
//************************************
void timeoutHandler() {
    printf("Timer out error.\n");               // 超时重传
    int index;                                  // 序列号
    for (int i = 0; i < SEND_WIND_SIZE; ++i) {  // 遍历滑动窗口内的数据帧
        index = (i + curAck) % SEQ_SIZE;        // 计算序列号
        ack[index] = TRUE;                      // 标记为重传
    }
    totalSeq -= SEND_WIND_SIZE;  // 更新已发送的总序列号数
    curSeq = curAck;             // 重置当前发送序列号
}
//************************************
// Method: ackHandler
// FullName: ackHandler
// Access: public
// Returns: void
// Qualifier: 收到 ack，累积确认，取数据帧的第一个字节
// 由于发送数据时，第一个字节（序列号）为
// 0（ASCII）时发送失败，因此加一了，此处需要减一还原
// Parameter: char c
//************************************
void ackHandler(char c) {
    unsigned char index = (unsigned char)c - 1;  // 序列号减一
    printf("Recv a ack of %d\n", index);  // 输出收到的 ack 序列号
    // 当curAck小于等于index时，表示ack没有回到curAck的左边
    if (curAck <= index) {
        // 依次标记curAck到index之间的ack为TRUE
        for (int i = curAck; i <= index; ++i) {
            ack[i] = TRUE;
        }
        // 更新curAck
        curAck = (index + 1) % SEQ_SIZE;
    }
    else {
        // ack 超过了最大值，回到了 curAck 的左边
        for (int i = curAck; i < SEQ_SIZE; ++i) {
            ack[i] = TRUE;  // 标记右边的ack为TRUE
        }
        for (int i = 0; i <= index; ++i) {
            ack[i] = TRUE;  // 标记右边的ack为TRUE
        }
        curAck = index + 1;  // 更新 curAck
    }
}
// 主函数
int main(int argc, char* argv[]) {
    // 加载套接字库（必须）
    WORD wVersionRequested;
    WSADATA wsaData;
    // 套接字加载时错误提示
    int err;
    // 版本 2.2
    wVersionRequested = MAKEWORD(2, 2);
    // 加载 dll 文件 Scoket 库
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        // 找不到 winsock.dll
        printf("WSAStartup failed with error: %d\n", err);
        return -1;
    }
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("Could not find a usable version of Winsock.dll\n");
        WSACleanup();
    }
    else {
        printf("The Winsock 2.2 dll was found okay\n");
    }
    SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    // 设置套接字为非阻塞模式
    int iMode = 1;  // 1：非阻塞，0：阻塞
    ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);  // 非阻塞设置
    SOCKADDR_IN addrServer;                                 // 服务器地址
    // addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    // addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//两者均可
    // printf("\n%d\n\n", addrServer.sin_addr.S_un.S_addr);
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // 服务器 IP 地址
    addrServer.sin_family = AF_INET;                      // 协议族
    addrServer.sin_port = htons(SERVER_PORT);             // 端口号
    // 绑定端口
    err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    if (err) {                 // 绑定失败
        err = GetLastError();  // 获取错误代码
        // 输出错误信息
        printf("Could not bind the port %d for socket.Error code is % d\n",
            SERVER_PORT, err);
        WSACleanup();  // 卸载库
        return -1;     // 返回错误
    }
    SOCKADDR_IN addrClient;              // 客户端地址
    int length = sizeof(SOCKADDR);       // 地址长度
    char buffer[BUFFER_LENGTH];          // 数据发送接收缓冲区
    ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
    // int len = sizeof(SOCKADDR);
    // 将测试数据读入内存
    std::ifstream infile;            // 读取文件
    std::ofstream outfile;           // 写入文件
    infile.open("./test.txt");       // 打开文件
    char data[64 * 113];             // 读取数据
    ZeroMemory(data, sizeof(data));  // 初始化
    infile.read(data, 64 * 113);     // 读取数据
    infile.close();                  // 关闭文件
    // printf("\n%c\n\n", data[1]);
    totalPacket = sizeof(data) / 64;  // 计算包的总数
    int recvSize;                     // 接收到的数据大小
    // 初始化 ack 数组
    for (int i = 0; i < SEQ_SIZE; ++i) {
        ack[i] = TRUE;
    }
    int ret;  // 返回值
    int interval = 1;  // 收到数据包之后返回 ack 的间隔，默认为 1 表示每个都返回
                       // ack，0 或者负数均表示所有的都不返回 ack
    char cmd[128];
    float packetLossRatio = 0.2;  // 默认包丢失率 0.2
    float ackLossRatio = 0.2;     // 默认 ACK 丢失率 0.2
    //
    while (true) {
        // 非阻塞接收，若没有收到数据，返回值为-1
        recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
            ((SOCKADDR*)&addrClient), &length);
        // 解析接收到的数据
        ret = sscanf_s(buffer, "%s [%f] [%f]", &cmd, (unsigned)sizeof(cmd), &packetLossRatio, &ackLossRatio);
        // printf("\nhaved receviced!\n\n");
        if (recvSize < 0) {  // 没有收到数据
            Sleep(200);      // 延时等待
            continue;
        }
        printf("recv from client: %s\n", buffer);  // 输出接收到的数据
        if (strcmp(buffer, "-time") == 0) {  // 如果接收到的数据是-time
            time_t timep;                    // 时间
            time(&timep);  // 获取从1970至今过了多少秒，存入time_t类型的timep
            ctime_s(buffer, BUFFER_LENGTH, &timep);  // 将时间转换为字符串
            // printf("time is ready:%s\n", buffer);
        }
        else if (strcmp(buffer, "-quit") == 0) {  // 如果接收到的数据是-quit
         // 退出程序
            strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
        }
        else if (strcmp(buffer, "-testgbn") == 0) {
            // 进入 gbn 测试阶段
            // 首先 server（server 处于 0 状态）向 client 发送 205
            // 状态码（server进入 1 状态）
            // server 等待 client 回复 200 状态码，如果收到（server 进入
            // 2状态），则开始传输文件，否则延时等待直至超时
            // 在文件传输阶段，server 发送窗口大小设为
            ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
            int recvSize;                        // 接收到的数据大小
            int waitCount = 0;                   // 等待计数器
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // 加入了一个握手阶段
            // 首先服务器向客户端发送一个 205
            // 大小的状态码（我自己定义的）表示服务器准备好了，可以发送数据
            // 客户端收到 205 之后回复一个 200
            // 大小的状态码，表示客户端准备好了，可以接收数据了 服务器收到 200
            // 状态码之后，就开始使用 GBN 发送数据了
            printf("Shake hands stage\n");
            int stage = 0;        // 阶段
            bool runFlag = true;  // 运行标志
            while (runFlag) {
                switch (stage) {
                case 0:  // 发送 205 阶段，表示服务器准备好了
                    buffer[0] = 205;  // 状态码 205
                    sendto(sockServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                    Sleep(100);  // 延时等待
                    stage = 1;   // 进入下一个阶段
                    break;
                case 1:  // 等待接收 200
                         // 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    if (recvSize < 0) {        // 没有收到数据
                        ++waitCount;           // 计数器+1
                        if (waitCount > 20) {  // 如果等待次数超过 20 次
                            runFlag = false;  // 超时，放弃此次连接
                            printf("Timeout error\n");  // 输出错误信息
                            break;
                        }
                        Sleep(500);  // 等待 500ms后重试
                        continue;
                    }
                    else {
                        // 当收到 200 状态码时，表示客户端准备好了
                        if ((unsigned char)buffer[0] == 200) {
                            // 输出信息
                            printf("Begin a file transfer\n");
                            // 输出文件大小信息
                            printf(
                                "File size is %dB, each packet is 64B and "
                                "packet total num is % d\n",
                                sizeof(data), totalPacket);
                            curSeq = 0;     // 当前序列号
                            curAck = 0;     // 当前确认号
                            totalSeq = 0;   // 总序列号
                            waitCount = 0;  // 等待计数器
                            stage = 2;      // 进入下一个阶段
                        }
                    }
                    break;
                case 2:  // 数据传输阶段
                         // 检查是否有可用的序列号的数据
                    if (seqIsAvailable()) {
                        // 发送给客户端的序列号从 1 开始
                        buffer[0] = curSeq + 1;
                        // 标记该包未被确认
                        ack[curSeq] = FALSE;
                        // 数据发送的过程中应该判断是否传输完成
                        // 为简化过程此处并未实现
                        // 拷贝数据
                        memcpy(&buffer[1], data + 64 * totalSeq, 64);
                        // printf("\n%s\n\n", &buffer[1]);
                        // 如果数据为空，则表示传输完成
                        while (sendto(sockServer, buffer, BUFFER_LENGTH, 0,
                            (SOCKADDR*)&addrClient,
                            sizeof(SOCKADDR)) == -1);
                        // 输出信息
                        printf("send a packet with a seq of %d\n", curSeq);
                        ++curSeq;            // 序列号+1
                        curSeq %= SEQ_SIZE;  // 环形序列号
                        ++totalSeq;          // 总序列号+1
                        Sleep(500);          // 延时等待
                    }
                    // 等待 Ack，若没有收到，则返回值为-1，计数器+1
                    // Sleep(500);
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    // printf("\n%d\n\n", recvSize);
                    if (recvSize < 0) {  // 没有收到数据
                        waitCount++;     // 计数器+1
                        // 20 次等待 ack 则超时重传
                        if (waitCount > 20) {  // 如果等待次数超过 20 次
                            timeoutHandler();  // 超时重传
                            buffer[0] = 300;   // 超时重传状态码
                            sendto(sockServer, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrClient,
                                sizeof(SOCKADDR));
                            waitCount = 0;  // 重置计数器
                        }
                    }
                    else {  // 如果收到数据
                     // 收到 ack
                        ackHandler(buffer[0]);  // 处理 ack
                        waitCount = 0;          // 重置计数器
                    }
                    Sleep(500);  // 延时等待
                    break;
                }
            }
        }
        else if (strcmp(cmd, "-optestgbn") == 0) {
            // 进入 gbn 测试阶段
            printf(
                "%s\n",
                "Begin to test GBN protocol, please don't abort the process");
            // 读取丢包率和 ACK 丢包率
            printf(
                "The loss ratio of packet is %.2f,the loss ratio of ack is % "
                ".2f\n",
                packetLossRatio, ackLossRatio);
            int waitCount = 0;     // 等待计数器
            int stage = 0;         // 阶段
            int counter = 0;       // 计数器
            BOOL b;                // 丢包标志
            unsigned char u_code;  // 状态码
            unsigned short seq;    // 包的序列号
            unsigned short recvSeq;  // 接收窗口大小为 1，已确认的序列号
            unsigned short waitSeq;  // 等待的序列号
            // sendto(sockServer, "-testgbn", strlen("-testgbn") + 1, 0,
            // (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            while (true) {
                // 等待 server 回复设置 UDP 为阻塞模式
                // Sleep(500);
                while (recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrClient, &length) == -1);

                switch (stage) {
                case 0:  // 等待握手阶段
                    u_code = (unsigned char)buffer[0];  // 状态码
                    if ((unsigned char)buffer[0] == 205) {
                        // 如果收到 205 状态码，表示客户端准备好了
                        printf("Ready for file transmission\n");
                        buffer[0] = 200;   // 回复 200 状态码
                        buffer[1] = '\0';  // 结束符
                        // sendto函数用来发送数据，参数分别为：套接字，发送缓冲区，发送缓冲区大小，标志位，目的地址，目的地址大小
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        stage = 1;    // 进入接收数据阶段
                        recvSeq = 0;  // 接收窗口大小为 1
                        waitSeq = 1;  // 等待的序列号
                    }
                    break;
                case 1:  // 等待接收数据阶段
                         // 如果数据为空，则表示传输完成
                    seq = (unsigned short)buffer[0];
                    // printf("\n%d\n\n", buffer[0]);
                    // 随机法模拟包是否丢失
                    b = lossInLossRatio(packetLossRatio);
                    if (b) {  // 如果丢包
                              // 输出信息
                        printf("The packet with a seq of %d loss\n", seq);
                        counter = 1;  // 计数器+1

                        buffer[0] = -1;  // 丢包状态码
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        // Sleep(2000);
                        continue;
                    }
                    // 输出接收到的包的序列号
                    printf("recv a packet with a seq of %d\n", seq);
                    // 如果是期待的包，正确接收，正常确认即可
                    if (!(waitSeq - seq)) {  // 如果是期待的序列号
                        ++waitSeq;           // 等待序列号+1
                        if (waitSeq == 21) {  // 如果等待序列号超过 20
                            waitSeq = 1;      // 重置为 1
                        }
                        // 输出数据
                        // printf("%s\n",&buffer[1]);
                        buffer[0] = seq;   // 序列号
                        recvSeq = seq;     // 更新已接收序列号
                        buffer[1] = '\0';  // 结束符
                    }
                    else {
                        // 如果当前一个包都没有收到，则等待 Seq 为 1
                        // 的数据包，不是则不返回
                        // ACK（因为并没有上一个正确的 ACK）
                        if (!recvSeq) {  // 如果尚未接收到任何包，只能等待序列号为
                                         // 1 的包
                            continue;
                        }
                        // 否则（收到乱序的包），则返回上一次正确接收的序列号的ack
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';
                    }
                    // 随机法模拟 ack 是否丢失
                    b = lossInLossRatio(ackLossRatio);
                    if (b) {  // 如果 ack 丢失
                        // 输出信息
                        printf("The ack of %d loss\n",
                            (unsigned char)buffer[0]);
                        counter = 1;  // 计数器+1

                        buffer[0] = -1;  // 丢包状态码
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        // Sleep(2000);
                        continue;
                    }
                    // printf("\n%d\n\n",buffer[0]);
                    while (sendto(sockServer, buffer, 2, 0,
                        (SOCKADDR*)&addrClient,
                        sizeof(SOCKADDR)) == -1);
                    // 输出信息
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    counter = 0;  // 计数器清零
                    break;
                }
                Sleep(500);
            }
        }
        else if (strcmp(cmd, "-download") == 0) {
            // 进入 gbn 测试阶段
            // 首先 server（server 处于 0 状态）向 client 发送 205
            // 状态码（server进入 1 状态）
            // server 等待 client 回复 200 状态码，如果收到（server 进入
            // 2状态），则开始传输文件，否则延时等待直至超时
            // 在文件传输阶段，server 发送窗口大小设为
            ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
            int recvSize;                        // 接收到的数据大小
            int waitCount = 0;                   // 等待计数器
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // 加入了一个握手阶段
            // 首先服务器向客户端发送一个 205
            // 大小的状态码（我自己定义的）表示服务器准备好了，可以发送数据
            // 客户端收到 205 之后回复一个 200
            // 大小的状态码，表示客户端准备好了，可以接收数据了 服务器收到 200
            // 状态码之后，就开始使用 GBN 发送数据了
            printf("Shake hands stage\n");
            int stage = 0;        // 阶段
            bool runFlag = true;  // 运行标志
            while (runFlag) {
                switch (stage) {
                case 0:  // 发送 205 阶段，表示客户端准备好了
                    buffer[0] = 205;
                    // 发送数据
                    sendto(sockServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                    Sleep(100);  // 延时等待
                    stage = 1;   // 进入等待接收数据阶段
                    break;
                case 1:  // 等待接收 200
                         // 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    if (recvSize < 0) {        // 如果没有收到
                        ++waitCount;           // 等待计数器+1
                        if (waitCount > 20) {  // 如果等待次数超过 20 次
                            runFlag = false;  // 超时，放弃此次连接
                            printf("Timeout error\n");  // 输出错误信息
                            break;
                        }
                        Sleep(500);  // 等待 500ms后重试
                        continue;
                    }
                    else {  // 如果收到数据
                     // 当收到 200 状态码时，表示客户端准备好了
                        if ((unsigned char)buffer[0] == 200) {
                            // 输出信息
                            printf("Begin a file transfer\n");
                            // 输出文件大小信息
                            printf(
                                "File size is %dB, each packet is 64B and "
                                "packet total num is % d\n",
                                sizeof(data), totalPacket);
                            curSeq = 0;     // 当前数据包的序列号
                            curAck = 0;     // 当前等待确认的 ack
                            totalSeq = 0;   // 收到的包的总数
                            waitCount = 0;  // 等待计数器
                            stage = 2;      // 进入数据传输阶段
                        }
                    }
                    break;
                case 2:                      // 数据传输阶段
                    if (seqIsAvailable()) {  // 检查是否有可用的序列号的数据
                        // 如果有可用的序列号的数据，则准备数据包
                        // 发送给客户端的序列号从 1 开始
                        buffer[0] = curSeq + 1;
                        // 标记该包未被确认
                        ack[curSeq] = FALSE;
                        // 数据发送的过程中应该判断是否传输完成
                        // 为简化过程此处并未实现
                        // 拷贝数据
                        memcpy(&buffer[1], data + 64 * totalSeq, 64);
                        // 输出数据
                        printf("\nbuffer:%s\n\n", &buffer[1]);
                        // 如果数据为空，则表示传输完成
                        if (buffer[1] == '\0') {
                            // printf("Finish\n");
                            stage = 3;  // 进入结束阶段
                            break;
                        }
                        // 发送数据包，直到成功为止，并打印发送的序列号
                        while (sendto(sockServer, buffer, BUFFER_LENGTH, 0,
                            (SOCKADDR*)&addrClient,
                            sizeof(SOCKADDR)) == -1);
                        printf("send a packet with a seq of %d\n", curSeq);
                        ++curSeq;            // 更新当前序列号
                        curSeq %= SEQ_SIZE;  // 环形序列号
                        ++totalSeq;          // 更新总序列号
                        Sleep(500);          // 延时等待
                    }
                    // 等待 Ack，若没有收到，则返回值为-1，计数器+1
                    // Sleep(500);
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    // printf("\n%d\n\n", recvSize);
                    if (recvSize < 0) {  // 如果没有收到数据
                        waitCount++;     // 等待计数器+1
                        // 20 次等待 ack 则超时重传
                        if (waitCount > 20) {  // 如果等待次数超过 20 次
                            timeoutHandler();  // 超时重传
                            waitCount = 0;     // 重置计数器
                        }
                    }
                    else {  // 如果收到数据
                     // 收到 ack
                        ackHandler(buffer[0]);  // 调用ackhandler处理ack
                        waitCount = 0;          // 重置等待计数器
                    }
                    Sleep(500);  // 延时等待
                    break;
                case 3:                  // 结束阶段
                    printf("Finish\n");  // 输出信息
                    runFlag = false;     // 结束循环
                }
            }
        }
        else if (strcmp(buffer, "-upload") == 0) {
            ZeroMemory(data, sizeof(data));  // 初始化数据
            // 进入 upload阶段
            printf(
                "%s\n",
                "Begin to test GBN protocol, please don't abort the process");
            int waitCount = 0;     // 等待计数器
            int stage = 0;         // 阶段
            int counter = 0;       // 计数器
            BOOL flag = true;      // 运行标志
            BOOL b;                // 丢包标志
            unsigned char u_code;  // 状态码
            unsigned short seq;    // 包的序列号
            unsigned short recvSeq;  // 接收窗口大小为 1，已确认的序列号
            unsigned short waitSeq;  // 等待的序列号
            // sendto(sockServer, "-testgbn", strlen("-testgbn") + 1, 0,
            // (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            while (flag) {
                // 等待 server 回复设置 UDP 为阻塞模式
                // Sleep(500);

                if (stage == 2) {
                    // 存储文件
                    printf("Finish\n");                   // 输出信息
                    printf("\ndata:\n%s\n\n", &data[0]);  // 输出数据
                    FILE* out;
                    // 如果文件打开成功，则写入数据
                    if (fopen_s(&out, "test_recver.txt", "wb") == 0) {
                        // 写入数据
                        fwrite(data, sizeof(char), strlen(data), out);
                        // 关闭文件
                        fclose(out);
                    }
                    flag = false;  // 结束循环
                    break;
                }
                // 循环等待数据，通过recvfrom函数阻塞等待客户端发送数据
                // 直到接收到数据为止，循环才会退出
                while (recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrClient, &length) == -1);
                // 拷贝数据
                memcpy(data + 64 * totalSeq, &buffer[1], 64);
                // 输出数据
                printf("\ndata:%s\n\n", data + 64 * totalSeq);
                switch (stage) {
                case 0:  // 等待握手阶段
                         // 获取状态码
                    u_code = (unsigned char)buffer[0];
                    if ((unsigned char)buffer[0] == 205) {
                        printf("Ready for file transmission\n");
                        // 服务器发送 200 以确认准备好接收数据
                        buffer[0] = 200;
                        buffer[1] = '\0';  // 结束符
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        stage = 1;    // 进入接收数据阶段
                        recvSeq = 0;  // 接收窗口大小为 1
                        waitSeq = 1;  // 等待的序列号
                    }
                    break;
                    // 每次收到一个数据包后，客户端检查其序列号是否与预期一致
                    // 若包的序列号正确，发送对应的
                    // ACK。若不符合预期，客户端根据上一个确认的序列号发送
                    // ACK
                case 1:                       // 等待接收数据阶段
                    if (buffer[1] == '\0') {  // 如果收到的数据为空
                        stage = 2;            // 进入结束阶段
                        break;
                    }
                    // 获取包的序列号
                    seq = (unsigned short)buffer[0];
                    // 输出接收到的包的序列号
                    printf("recv a packet with a seq of %d\n", seq);
                    ++totalSeq;  // 更新已接收的包的总数
                    // 如果是期待的包，正确接收，正常确认即可
                    if (!(waitSeq - seq)) {  // 如果是期待的序列号
                        ++waitSeq;  // 更新期待的下一个序列号
                        if (waitSeq == 21) {  // 如果等待序列号超过 20
                            waitSeq = 1;      // 重置为 1
                        }
                        // 输出数据
                        // printf("%s\n",&buffer[1]);
                        buffer[0] = seq;   // 序列号
                        recvSeq = seq;     // 更新已接收的序列号
                        buffer[1] = '\0';  // 结束符
                    }
                    else {
                        // 如果当前一个包都没有收到，则等待 Seq 为 1
                        // 的数据包，不是则不返回
                        // ACK（因为并没有上一个正确的 ACK）
                        // 如果尚未接收到任何包，只能等待序列号为1的包
                        if (!recvSeq) {
                            continue;
                        }
                        // 否则（收到乱序的包），则返回上一次正确接收的序列号的ack
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';  // 结束符
                    }
                    // 发送 ack
                    while (sendto(sockServer, buffer, 2, 0,
                        (SOCKADDR*)&addrClient,
                        sizeof(SOCKADDR)) == -1);
                    // 输出信息
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    counter = 0;  // 计数器清零
                    break;
                }
                Sleep(500);  // 延时等待
            }
        }
        // 发送数据
        sendto(sockServer, buffer, strlen(buffer) + 1, 0,
            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
        Sleep(500);  // 延时等待
    }
    // 关闭套接字，卸载库
    closesocket(sockServer);
    WSACleanup();
    return 0;
}