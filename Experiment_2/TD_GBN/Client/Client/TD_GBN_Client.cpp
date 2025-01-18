// #include "stdafx.h"
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340      // 接收数据的端口号
#define SERVER_IP "127.0.0.1"  // 服务器的 IP 地址
const int BUFFER_LENGTH = 1026;

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

char fileName[40];       // 文件名
char filePath[50];       // 文件路径
char file[1024 * 1024];  // 文件内容
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
    step =
        step >= 0
        ? step
        : step +
        SEQ_SIZE;  // 处理环形序列号，如果大于等于0，则不变，否则加上
                   // SEQ_SIZE
// 序列号是否在当前发送窗口之内
// 如果不在窗口内，则表示未收到 ack
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
    Sleep(1000);                                // 等待 1s
    printf("Timer out error.\n\n\n\n");         // 超时重传
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
            ack[i] = TRUE;  // 标记左边的ack为TRUE
        }
        curAck = index + 1;  // 更新 curAck
    }
}
/****************************************************************/
/* -time 从服务器端获取当前时间
-quit 退出客户端
-testgbn [X] 测试 GBN 协议实现可靠数据传输
[X] [0,1] 模拟数据包丢失的概率
[Y] [0,1] 模拟 ACK 丢失的概率
*/
/****************************************************************/
void printTips() {
    printf_s("*****************************************\n");
    printf_s("| -time to get current time |\n");
    printf_s("| -quit to exit client |\n");
    printf_s("| -testgbn [X] [Y] to test the gbn |\n");
    printf_s("| -optestgbn [X] [Y] to test the gbnop |\n");
    printf_s("| -download to test the download |\n");
    printf_s("| -upload to test the upload |\n");
    printf_s("*****************************************\n");
}
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
        return 1;
    }
    // 检测版本号
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("Could not find a usable version of Winsock.dll\n");
        WSACleanup();
    }
    else {
        printf("The Winsock 2.2 dll was found okay\n");
    }
    // 创建套接字
    SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
    SOCKADDR_IN addrServer;

    // int length = sizeof(SOCKADDR);
    // struct in_addr p;
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // 服务器的 IP
    // printf("\n%d\n\n", addrServer.sin_addr.S_un.S_addr);
    addrServer.sin_family = AF_INET;           // 协议
    addrServer.sin_port = htons(SERVER_PORT);  // 端口
    // 接收缓冲区
    char buffer[BUFFER_LENGTH];          // 缓冲区
    ZeroMemory(buffer, sizeof(buffer));  // 初始化缓冲区
    int len = sizeof(SOCKADDR);          // 地址长度
    // 为了测试与服务器的连接，可以使用 -time 命令从服务器端获得当前时间
    // 使用 -testgbn [X] [Y] 测试 GBN 其中[X]表示数据包丢失概率
    //  [Y]表示 ACK 丢包概率
    printTips();  // 打印提示信息
    int ret;      // 返回值
    int interval = 1;  // 收到数据包之后返回 ack 的间隔，默认为 1 表示每个都返回
                       // ack，0 或者负数均表示所有的都不返回 ack
    char cmd[128];                // 命令
    float packetLossRatio = 0.2;  // 默认包丢失率 0.2
    float ackLossRatio = 0.2;     // 默认 ACK 丢失率 0.2
    // 用时间作为随机种子，放在循环的最外面
    // 将测试数据读入内存
    std::ifstream infile;            // 输入流
    std::ofstream outfile;           // 输出流
    infile.open("./test.txt");       // 打开文件
    char data[64 * 113];             // 113 个包
    ZeroMemory(data, sizeof(data));  // 初始化
    infile.read(data, 64 * 113);     // 读取文件
    infile.close();                  // 关闭文件
    // printf("\n%s\n\n", data);
    totalPacket = sizeof(data) / 64;  // 计算包的总数
    // ZeroMemory(data, sizeof(data));
    int recvSize;  // 接收到的数据包大小
    // 初始化 ack 数组
    for (int i = 0; i < SEQ_SIZE; ++i) {
        ack[i] = TRUE;
    }
    // 设置随机种子
    srand((unsigned)time(NULL));
    while (true) {
        gets_s(buffer, BUFFER_LENGTH);  // 从键盘输入命令
        // 解析命令
        ret = sscanf_s(buffer, "%s [%f] [%f]", &cmd,(unsigned)sizeof(cmd), &packetLossRatio, &ackLossRatio);
        // printf("\n%s\n\n", buffer);

        // 开始 GBN 测试，使用 GBN 协议实现 UDP 可靠文件传输
        if (!strcmp(cmd, "-testgbn")) {
            // 进入 gbn 测试阶段
            // 开始测试 GBN 协议，请不要中断进程
            printf("%s\n",
                "Begin to test GBN protocol, please don't abort the "
                "process");
            // 数据包的丢失率和ack 的丢失率的输出
            printf(
                "The loss ratio of packet is %.2f,the loss ratio of ack is % "
                ".2f\n",
                packetLossRatio, ackLossRatio);
            int waitCount = 0;     // 等待计数器
            int stage = 0;         // 阶段
            BOOL b;                // 丢包标志
            unsigned char u_code;  // 状态码
            unsigned short seq;    // 包的序列号
            unsigned short recvSeq;  // 接收窗口大小为 1，已确认的序列号
            unsigned short waitSeq;  // 等待的序列号
            // 发送测试命令
            sendto(socketClient, "-testgbn", strlen("-testgbn") + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            while (true) {
                // 等待 server 回复设置 UDP 为阻塞模式
                // Sleep(500);
                // 循环等待数据，通过recvfrom阻塞等待服务器发送的数据包，直到接收到数据包，循环才会退出
                // recvfrom函数用来接收数据，接收到数据后，将数据存放到buffer中，获取服务器的地址信息
                while (recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrServer, &len) == -1);
                // printf("\n%s\n\n", &buffer[1]);
                // memcpy(data + 64 * totalSeq, &buffer[1], 64);
                switch (stage) {
                case 0:  // 等待握手阶段
                    u_code = (unsigned char)buffer[0];  // 状态码
                    // 如果收到 205，则表示服务器准备好了，可以发送数据
                    if ((unsigned char)buffer[0] == 205) {
                        // 服务器准备好了，可以发送数据
                        printf("Ready for file transmission\n");
                        // 客户端发送 200
                        // 状态码，表示客户端准备好了，可以接收数据了
                        buffer[0] = 200;
                        buffer[1] = '\0';
                        // sendto函数用来发送数据，发送数据到服务器端,发送成功返回发送的字节数，否则返回-1
                        sendto(socketClient, buffer, 2, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                        stage = 1;  // 进入等待接收数据阶段
                        recvSeq = 0;  // 接收窗口大小为 1，已确认的序列号
                        waitSeq = 1;  // 等待的序列号
                    }
                    break;
                case 1:  // 等待接收数据阶段
                    // 如果收到的数据包为空，则表示文件传输结束
                    if (&buffer[1] == NULL) {
                        break;
                    }
                    // 获取包的序列号
                    seq = (unsigned short)buffer[0];
                    // 随机法模拟包是否丢失
                    b = lossInLossRatio(packetLossRatio);
                    // 如果丢包，则不处理
                    if (b) {
                        // 输出丢包的序列号
                        printf("The packet with a seq of %d loss\n", seq);
                        continue;  // 如果丢包，则忽略该包
                    }
                    // 输出接收到的包的序列号
                    printf("recv a packet with a seq of %d\n", seq);
                    // 如果是期待的包，正确接收，正常确认即可
                    if (!(waitSeq - seq)) {  // 如果是期待的序列号
                        ++waitSeq;  // 更新期待的下一个序列号
                        if (waitSeq == 21) {  // 如果超过了最大值
                            waitSeq = 1;      // 回到 1
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
                        if (!recvSeq) {  // 如果尚未接收到任何包，只能等待序列号为1的包
                            continue;
                        }
                        // 否则（收到乱序的包），则返回上一次正确接收的序列号的
                        // ACK
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';
                    }
                    // 随机法模拟 ACK 是否丢失
                    b = lossInLossRatio(ackLossRatio);
                    // 如果 ACK 丢失，则不处理
                    if (b) {
                        printf("The ack of %d loss\n",
                            (unsigned char)buffer[0]);
                        continue;  // 如果 ACK 丢失，则跳过发送
                    }
                    // 如果ACK未丢失，则通过sendto发送
                    // ACK给服务器，并打印ACK日志
                    while (sendto(socketClient, buffer, 2, 0,
                        (SOCKADDR*)&addrServer,
                        sizeof(SOCKADDR)) == -1);
                    // 输出 ACK
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    break;
                }
                Sleep(500);  // 模拟网络延迟，等待 500ms
            }
        }
        else if (!strcmp(cmd, "-optestgbn")) {
            // 进入 gbn 测试阶段
            // 首先 server（server 处于 0 状态）向 client 发送 205
            // 状态码（server进入 1 状态）
            //  server 等待 client 回复 200 状态码，如果收到（server 进入
            // 2状态），则开始传输文件，否则延时等待直至超时
            // 在文件传输阶段，server 发送窗口大小设为
            // sendto用来发送数据，发送数据到服务器端,发送成功返回发送的字节数，否则返回-1
            sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            // ZeroMemory(buffer, sizeof(buffer));
            int recvSize;       // 接收到的数据包大小
            int waitCount = 0;  // 等待计数器
            // 输出提示信息
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // 加入了一个握手阶段
            // 首先服务器向客户端发送一个 205
            // 大小的状态码（我自己定义的）表示服务器准备好了，可以发送数据
            // 客户端收到 205 之后回复一个 200
            // 大小的状态码，表示客户端准备好了，可以接收数据了 服务器收到 200
            // 状态码之后，就开始使用 GBN 发送数据了
            printf("Shake hands stage\n");
            int stage = 0;        // 用于跟踪握手和数据传输阶段
            bool runFlag = true;  // 控制while循环是否运行
            while (runFlag) {
                switch (stage) {
                case 0:  // 发送 205
                         // 进行握手，表示客户端准备好了可以开始传输
                    buffer[0] = 205;  // 状态码
                    sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    Sleep(100);  // 等待 100ms
                    stage = 1;   // 进入等待接收数据阶段
                    break;
                case 1:  // 等待接收 200
                         // 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
                    recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH,
                        0, ((SOCKADDR*)&addrServer), &len);
                    if (recvSize < 0) {
                        ++waitCount;           // 等待计数器+1
                        if (waitCount > 20) {  // 如果等待计数器超过 20
                            runFlag = false;  // 超时，放弃此次连接
                            printf("Timeout error\n");  // 输出超时错误
                            break;
                        }
                        Sleep(500);  // 等待 500ms后重试
                        continue;
                    }
                    else {
                        // 当收到200状态码时，客户端进入状态2
                        if ((unsigned char)buffer[0] == 200) {
                            // 输出提示信息
                            printf("Begin a file transfer\n");
                            // 输出文件大小
                            printf(
                                "File size is %dB, each packet is 1024B "
                                "and packet total num is % d\n",
                                sizeof(data), totalPacket);
                            curSeq = 0;     // 当前数据包的序列号
                            curAck = 0;     // 当前等待确认的 ack
                            totalSeq = 0;   // 收到的包的总数
                            waitCount = 0;  // 等待计数器
                            stage = 2;      // 进入数据传输阶段
                        }
                    }
                    break;
                case 2:  // 数据传输阶段
                         // 检查是否可以发送该序列号的数据
                    if (seqIsAvailable()) {
                        // 发送给服务器的序列号从 1 开始
                        buffer[0] = curSeq + 1;
                        // 标记该包未被确认
                        ack[curSeq] = FALSE;
                        // 数据发送的过程中应该判断是否传输完成
                        // 为简化过程此处并未实现
                        // memcpy(&buffer[1], data + 1024 * totalSeq, 1024);

                        while (sendto(socketClient, buffer, BUFFER_LENGTH,
                            0, (SOCKADDR*)&addrServer,
                            sizeof(SOCKADDR)) == -1);
                        // 输出发送的包的序列号
                        printf("send a packet with a seq of %d\n", curSeq);

                        ++curSeq;            // 更新当前序列号
                        curSeq %= SEQ_SIZE;  // 序列号循环使用
                        ++totalSeq;  // 更新已发送的包的总数
                        // Sleep(500);
                        // 接收 ack
                        recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrServer), &len);
                    }
                    else {  // 如果超时，则重传
                        timeoutHandler();
                        break;
                        // waitCount = 0;
                    }
                    // 等待 Ack，若没有收到，则返回值为-1，计数器+1

                    // printf("\n%d\n\n", buffer[0]);
                    recvSize = buffer[0];  // 接收到的 ack
                    // 如果未收到 ack，则等待计数器+1
                    if (recvSize < 0) {
                        waitCount++;  // 等待计数器+1
                        // printf("waitokdqowjfq\n");
                        // 20 次等待 ack 则超时重传
                        if (waitCount > 20) {
                            // 超时处理并重发
                            timeoutHandler();
                            // 重置等待计数器
                            waitCount = 0;
                        }
                    }
                    else {  // 如果收到 ack，则处理 ack
                     // 收到 ack
                     // printf("\n%d\n\n", buffer[0]);
                     // 调用ackhandler处理ack
                        ackHandler(buffer[0]);
                        // 重置等待计数器
                        waitCount = 0;
                    }
                    // Sleep(500);
                    break;
                }
            }
        }
        else if (!strcmp(cmd, "-download")) {
            // 进入 download 测试阶段
            printf(
                "%s\n",
                "Begin to test GBN protocol, please don't abort the process");
            int waitCount = 0;     // 等待计数器
            int stage = 0;         // 阶段
            BOOL b;                // 丢包标志
            BOOL flag = true;      // 控制循环是否运行
            unsigned char u_code;  // 状态码
            unsigned short seq;    // 包的序列号
            unsigned short recvSeq;  // 接收窗口大小为 1，已确认的序列号
            unsigned short waitSeq;  // 等待的序列号
            // 向服务器发送 -download 命令
            sendto(socketClient, "-download", strlen("-download") + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            while (flag) {
                // 等待 server 回复设置 UDP 为阻塞模式
                // Sleep(500);
                if (stage == 2) {  //
                    // 存储文件
                    printf("Finish\n");  // 输出文件传输结束
                    printf("\ndata:\n%s\n\n", &data[0]);  // 输出文件内容
                    FILE* out;                            // 输出文件
                    // 如果文件打开成功，则写入文件
                    if (fopen_s(&out, "test_recver.txt", "wb") == 0) {
                        // 写入文件
                        fwrite(data, sizeof(char), strlen(data), out);
                        // 关闭文件
                        fclose(out);
                    }
                    flag = false;  // 结束循环
                    break;
                }
                // 循环等待数据，通过recvfrom阻塞等待服务器发送的数据包
                // 直到接收到数据包，循环才会退出
                while (recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrServer, &len) == -1);
                // 如果服务器发送状态码为 300，客户端存储数据并退出
                if (buffer[0] == 300) stage = 2;
                // printf("\nbuffer:%s\n\n", &buffer[1]);
                ZeroMemory(data + 64 * totalSeq, 64);  // 初始化 data
                // 将数据存储到 data
                memcpy(data + 64 * totalSeq, &buffer[1], 64);
                // 输出 data
                //printf("\ndata:%s\n\n", data + 64 * totalSeq);
                printf("\ndata:%s\n\n", &buffer[1]);
                switch (stage) {
                case 0:  // 等待握手阶段
                    // 获取状态码
                    u_code = (unsigned char)buffer[0];
                    // 如果收到 205，表示服务器已准备好进行文件传输
                    if ((unsigned char)buffer[0] == 205) {
                        printf("Ready for file transmission\n");
                        // 客户端发送 200以确认表示准备好接收数据
                        buffer[0] = 200;
                        buffer[1] = '\0';  // 结束符
                        // 发送数据
                        sendto(socketClient, buffer, 2, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                        stage = 1;  // 进入等待接收数据阶段
                        recvSeq = 0;  // 接收窗口大小为 1，已确认的序列号
                        waitSeq = 1;  // 等待的序列号
                    }
                    break;
                    // 每次收到一个数据包后，客户端检查其序列号是否与预期一致
                    // 若包的序列号正确，发送对应的
                    // ACK。若不符合预期，客户端根据上一个确认的序列号发送 ACK
                case 1:                       // 等待接收数据阶段
                    if (buffer[1] == '\0') {  // 如果收到的数据包为空
                        stage = 2;            // 进入结束阶段
                        break;
                    }
                    // 获取包的序列号
                    seq = (unsigned short)buffer[0];
                    // 随机法模拟包是否丢失
                    /*b = lossInLossRatio(packetLossRatio);
                    if (b) {
                        printf("The packet with a seq of %d loss\n", seq);
                        continue;
                    }*/
                    // 输出接收到的包的序列号
                    printf("recv a packet with a seq of %d\n", seq);
                    ++totalSeq;  // 更新已接收的包的总数
                    // 如果是期待的包，正确接收，正常确认即可
                    if (!(waitSeq - seq)) {  // 如果是期待的序列号
                        ++waitSeq;  // 更新期待的下一个序列号
                        if (waitSeq == 21) {  // 如果超过了最大值
                            waitSeq = 1;      // 回到 1
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
                        if (!recvSeq) {  // 如果尚未接收到任何包，只能等待序列号为1的包
                            continue;
                        }
                        // 如果乱序，返回上一次正确接收的序列号的 ACK
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';  // 结束符
                    }
                    /*b = lossInLossRatio(ackLossRatio);
                    if (b) {
                        printf("The ack of %d loss\n", (unsigned
                    char)buffer[0]); continue;
                    }*/
                    // 发送 ACK
                    while (sendto(socketClient, buffer, 2, 0,
                        (SOCKADDR*)&addrServer,
                        sizeof(SOCKADDR)) == -1);
                    // 输出 ACK
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    break;
                case 2:                          // 结束阶段
                    totalSeq -= SEND_WIND_SIZE;  // 更新已发送的包的总数
                    break;
                }
                Sleep(500);  // 模拟网络延迟，等待 500ms
            }
        }
        else if (!strcmp(cmd, "-upload")) {
            // 进入 gbn 测试阶段
            // 首先 server（server 处于 0 状态）向 client 发送 205
            // 状态码（server进入 1 状态）
            // server 等待 client 回复 200 状态码，如果收到（server 进入
            // 2状态），则开始传输文件，否则延时等待直至超时
            // 在文件传输阶段，server 发送窗口大小设为
            // sendto用来发送数据，发送数据到服务器端,准备进入握手阶段发送成功返回发送的字节数，否则返回-1
            sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            // ZeroMemory(buffer, sizeof(buffer));
            int recvSize;       // 接收到的数据包大小
            int waitCount = 0;  // 等待计数器
            // 输出提示信息
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // 加入了一个握手阶段
            // 首先服务器向客户端发送一个 205
            // 大小的状态码（我自己定义的）表示服务器准备好了，可以发送数据
            // 客户端收到 205 之后回复一个 200
            // 大小的状态码，表示客户端准备好了，可以接收数据了 服务器收到 200
            // 状态码之后，就开始使用 GBN 发送数据了
            printf("Shake hands stage\n");
            int stage = 0;        // 用于跟踪握手和数据传输阶段
            bool runFlag = true;  // 控制while循环是否运行
            while (runFlag) {
                switch (stage) {
                case 0:  // 发送 205 阶段，表示服务器准备好了可以开始传输
                    buffer[0] = 205;
                    // 发送数据
                    sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    Sleep(100);  // 等待 100ms
                    stage = 1;   // 进入等待接收数据阶段
                    break;
                case 1:  // 等待接收 200
                         // 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
                    recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH,
                        0, ((SOCKADDR*)&addrServer), &len);
                    if (recvSize < 0) {        // 如果没有收到
                        ++waitCount;           // 等待计数器+1
                        if (waitCount > 20) {  // 如果等待计数器超过 20
                            runFlag = false;  // 超时，放弃此次连接
                            printf("Timeout error\n");  // 输出超时错误
                            break;
                        }
                        Sleep(500);  // 等待 500ms后重试
                        continue;
                    }
                    else {  // 如果收到
                     // 当收到200状态码时，服务器进入状态2
                        if ((unsigned char)buffer[0] == 200) {
                            // 输出提示信息
                            printf("Begin a file transfer\n");
                            // 输出文件大小
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
                    if (seqIsAvailable()) {  // 检查序列号书否可用
                        // 如果可用，则准备数据包
                        // 发送给服务器的序列号从 1 开始
                        buffer[0] = curSeq + 1;
                        // 标记该包未被确认
                        ack[curSeq] = FALSE;
                        // 数据发送的过程中应该判断是否传输完成
                        // 为简化过程此处并未实现
                        // 将数据存储到 buffer
                        memcpy(&buffer[1], data + 64 * totalSeq, 64);
                        // 输出buffer
                        printf("\nbuffer:\n%s\n\n",data + 64 * totalSeq);
                        if (buffer[1] == '\0') {  // 如果收到的数据包为空
                            printf("Finish\n");  // 输出文件传输结束
                            stage = 3;           // 进入结束阶段
                            break;
                        }
                        // 发送数据包，直到成功为止，并打印发送的序列号
                        while (sendto(socketClient, buffer, BUFFER_LENGTH,
                            0, (SOCKADDR*)&addrServer,
                            sizeof(SOCKADDR)) == -1);
                        printf("send a packet with a seq of %d\n", curSeq);

                        ++curSeq;            // 更新当前序列号
                        curSeq %= SEQ_SIZE;  // 序列号循环使用
                        ++totalSeq;  // 更新已发送的包的总数
                        // Sleep(500);
                        // 接收 ack
                        recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrServer), &len);
                    }
                    else {  // 如果序列号不可用，则重传
                        timeoutHandler();  // 超时处理
                        break;
                        // waitCount = 0;
                    }
                    // 等待 Ack，若没有收到，则返回值为-1，计数器+1

                    // printf("\n%d\n\n", buffer[0]);
                    recvSize = buffer[0];  // 接收到的 ack
                    if (recvSize < 0) {    // 如果未收到 ack
                        waitCount++;       // 等待计数器+1
                        // printf("waitokdqowjfq\n");
                        // 20 次等待 ack 则超时重传
                        if (waitCount > 20) {  // 如果等待计数器超过 20
                            timeoutHandler();  // 超时处理并重发
                            waitCount = 0;     // 重置等待计数器
                        }
                    }
                    else {  // 如果收到 ack
                     // 收到 ack
                     // printf("\n%d\n\n", buffer[0]);
                        ackHandler(buffer[0]);  // 调用ackhandler处理ack
                        waitCount = 0;          // 重置等待计数器
                    }
                    // Sleep(500);
                    break;
                case 3:                  // 结束阶段
                    printf("Finish\n");  // 输出文件传输结束
                    runFlag = false;     // 结束循环
                }
            }
        }
        // printf("\ndata:%s\n\n", data);
        // 发送数据
        sendto(socketClient, buffer, strlen(buffer) + 1, 0,
            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
        // 接收数据
        ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
            (SOCKADDR*)&addrServer, &len);
        // 输出数据
        printf("%s\n", buffer);
        // 退出客户端
        if (!strcmp(buffer, "Good bye!")) {
            break;
        }
        printTips();
    }
    // 关闭套接字
    closesocket(socketClient);
    WSACleanup();
    return 0;
}