// #include "stdafx.h" //���� VS ��Ŀ������Ԥ����ͷ�ļ�
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340    // �˿ں�
#define SERVER_IP "0.0.0.0"  // IP ��ַ
const int BUFFER_LENGTH =
1026;  // ��������С������̫���� UDP ������֡�а�����ӦС�� 1480 �ֽڣ�
const int SEND_WIND_SIZE = 10;  // ���ʹ��ڴ�СΪ 10��GBN ��Ӧ���� W + 1 <= N��W
                                // Ϊ���ʹ��ڴ�С��N Ϊ���кŸ�����
// ����ȡ���к� 0...19 �� 20 ��
// ��������ڴ�С��Ϊ 1����Ϊͣ-��Э��
const int SEQ_SIZE = 20;  // ���кŵĸ������� 0~19 ���� 20 ��
// ���ڷ������ݵ�һ���ֽ����ֵΪ 0�������ݻᷢ��ʧ��
// ��˽��ն����к�Ϊ 1~20���뷢�Ͷ�һһ��Ӧ
BOOL ack[SEQ_SIZE];  // �յ� ack �������Ӧ 0~19 �� ack
int curSeq;          // ��ǰ���ݰ��� seq
int curAck;          // ��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;        // �յ��İ�������
int totalPacket;     // ��Ҫ���͵İ�����

//************************************
// Method: lossInLossRatio
// FullName: lossInLossRatio
// Access: public
// Returns: BOOL
// Qualifier: ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻�TRUE�����򷵻�
// FALSE Parameter: float lossRatio [0,1]
//************************************
BOOL lossInLossRatio(float lossRatio) {
    // ������Ķ�ʧ��ת��Ϊ 0~100������
    int lossBound = (int)(lossRatio * 100);
    int r = rand() % 101;  // ���� 0~100 �������
    if (r <= lossBound) {  // ��������С�ڵ��ڶ�ʧ��
        return TRUE;
    }
    return FALSE;  // ���򲻶�ʧ
}
//************************************
// Method: getCurTime
// FullName: getCurTime
// Access: public
// Returns: void
// Qualifier: ��ȡ��ǰϵͳʱ�䣬������� ptime ��
// Parameter: char * ptime
//************************************
void getCurTime(char* ptime) {
    char buffer[128];                   // ������
    memset(buffer, 0, sizeof(buffer));  // ��ʼ��
    time_t c_time;                      // ʱ��
    struct tm* p = NULL;                // ʱ��ṹ��
    time(&c_time);                      // ��ȡ��ǰʱ��
    localtime_s(p, &c_time);            // ת��Ϊ����ʱ��
    printf("\n%d\n\n", p->tm_year);     // ������
    // ��ʽ��ʱ��
    sprintf_s(buffer, "%d/%d/%d %d:%d:%d", p->tm_year + 1900, p->tm_mon,
        p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
    // ����ʽ�����ʱ�俽���� ptime ��
    strcpy_s(ptime, sizeof(buffer), buffer);
}
//************************************
// Method: seqIsAvailable
// FullName: seqIsAvailable
// Access: public
// Returns: bool
// Qualifier: ��ǰ���к� curSeq �Ƿ����
//************************************
bool seqIsAvailable() {
    int step;                // ��ǰ���к��뵱ǰȷ�ϺŵĲ�ֵ
    step = curSeq - curAck;  // ��ǰ���к��뵱ǰȷ�ϺŵĲ�ֵ
    step = step >= 0 ? step : step + SEQ_SIZE;  // ���������к�
    // ���к��Ƿ��ڵ�ǰ���ʹ���֮��
    if (step >= SEND_WIND_SIZE) {
        return false;
    }
    // ����յ� ack�����ʾ��ǰ���кſ���
    if (ack[curSeq]) {
        return true;
    }
    // ���δ�յ� ack�����ʾ��ǰ���кŲ�����
    return false;
}
//************************************
// Method: timeoutHandler
// FullName: timeoutHandler
// Access: public
// Returns: void
// Qualifier: ��ʱ�ش������������������ڵ�����֡��Ҫ�ش�
//************************************
void timeoutHandler() {
    printf("Timer out error.\n");               // ��ʱ�ش�
    int index;                                  // ���к�
    for (int i = 0; i < SEND_WIND_SIZE; ++i) {  // �������������ڵ�����֡
        index = (i + curAck) % SEQ_SIZE;        // �������к�
        ack[index] = TRUE;                      // ���Ϊ�ش�
    }
    totalSeq -= SEND_WIND_SIZE;  // �����ѷ��͵������к���
    curSeq = curAck;             // ���õ�ǰ�������к�
}
//************************************
// Method: ackHandler
// FullName: ackHandler
// Access: public
// Returns: void
// Qualifier: �յ� ack���ۻ�ȷ�ϣ�ȡ����֡�ĵ�һ���ֽ�
// ���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ
// 0��ASCII��ʱ����ʧ�ܣ���˼�һ�ˣ��˴���Ҫ��һ��ԭ
// Parameter: char c
//************************************
void ackHandler(char c) {
    unsigned char index = (unsigned char)c - 1;  // ���кż�һ
    printf("Recv a ack of %d\n", index);  // ����յ��� ack ���к�
    // ��curAckС�ڵ���indexʱ����ʾackû�лص�curAck�����
    if (curAck <= index) {
        // ���α��curAck��index֮���ackΪTRUE
        for (int i = curAck; i <= index; ++i) {
            ack[i] = TRUE;
        }
        // ����curAck
        curAck = (index + 1) % SEQ_SIZE;
    }
    else {
        // ack ���������ֵ���ص��� curAck �����
        for (int i = curAck; i < SEQ_SIZE; ++i) {
            ack[i] = TRUE;  // ����ұߵ�ackΪTRUE
        }
        for (int i = 0; i <= index; ++i) {
            ack[i] = TRUE;  // ����ұߵ�ackΪTRUE
        }
        curAck = index + 1;  // ���� curAck
    }
}
// ������
int main(int argc, char* argv[]) {
    // �����׽��ֿ⣨���룩
    WORD wVersionRequested;
    WSADATA wsaData;
    // �׽��ּ���ʱ������ʾ
    int err;
    // �汾 2.2
    wVersionRequested = MAKEWORD(2, 2);
    // ���� dll �ļ� Scoket ��
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        // �Ҳ��� winsock.dll
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
    // �����׽���Ϊ������ģʽ
    int iMode = 1;  // 1����������0������
    ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);  // ����������
    SOCKADDR_IN addrServer;                                 // ��������ַ
    // addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    // addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//���߾���
    // printf("\n%d\n\n", addrServer.sin_addr.S_un.S_addr);
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // ������ IP ��ַ
    addrServer.sin_family = AF_INET;                      // Э����
    addrServer.sin_port = htons(SERVER_PORT);             // �˿ں�
    // �󶨶˿�
    err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    if (err) {                 // ��ʧ��
        err = GetLastError();  // ��ȡ�������
        // ���������Ϣ
        printf("Could not bind the port %d for socket.Error code is % d\n",
            SERVER_PORT, err);
        WSACleanup();  // ж�ؿ�
        return -1;     // ���ش���
    }
    SOCKADDR_IN addrClient;              // �ͻ��˵�ַ
    int length = sizeof(SOCKADDR);       // ��ַ����
    char buffer[BUFFER_LENGTH];          // ���ݷ��ͽ��ջ�����
    ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
    // int len = sizeof(SOCKADDR);
    // ���������ݶ����ڴ�
    std::ifstream infile;            // ��ȡ�ļ�
    std::ofstream outfile;           // д���ļ�
    infile.open("./test.txt");       // ���ļ�
    char data[64 * 113];             // ��ȡ����
    ZeroMemory(data, sizeof(data));  // ��ʼ��
    infile.read(data, 64 * 113);     // ��ȡ����
    infile.close();                  // �ر��ļ�
    // printf("\n%c\n\n", data[1]);
    totalPacket = sizeof(data) / 64;  // �����������
    int recvSize;                     // ���յ������ݴ�С
    // ��ʼ�� ack ����
    for (int i = 0; i < SEQ_SIZE; ++i) {
        ack[i] = TRUE;
    }
    int ret;  // ����ֵ
    int interval = 1;  // �յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ��������
                       // ack��0 ���߸�������ʾ���еĶ������� ack
    char cmd[128];
    float packetLossRatio = 0.2;  // Ĭ�ϰ���ʧ�� 0.2
    float ackLossRatio = 0.2;     // Ĭ�� ACK ��ʧ�� 0.2
    //
    while (true) {
        // ���������գ���û���յ����ݣ�����ֵΪ-1
        recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
            ((SOCKADDR*)&addrClient), &length);
        // �������յ�������
        ret = sscanf_s(buffer, "%s [%f] [%f]", &cmd, (unsigned)sizeof(cmd), &packetLossRatio, &ackLossRatio);
        // printf("\nhaved receviced!\n\n");
        if (recvSize < 0) {  // û���յ�����
            Sleep(200);      // ��ʱ�ȴ�
            continue;
        }
        printf("recv from client: %s\n", buffer);  // ������յ�������
        if (strcmp(buffer, "-time") == 0) {  // ������յ���������-time
            time_t timep;                    // ʱ��
            time(&timep);  // ��ȡ��1970������˶����룬����time_t���͵�timep
            ctime_s(buffer, BUFFER_LENGTH, &timep);  // ��ʱ��ת��Ϊ�ַ���
            // printf("time is ready:%s\n", buffer);
        }
        else if (strcmp(buffer, "-quit") == 0) {  // ������յ���������-quit
         // �˳�����
            strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
        }
        else if (strcmp(buffer, "-testgbn") == 0) {
            // ���� gbn ���Խ׶�
            // ���� server��server ���� 0 ״̬���� client ���� 205
            // ״̬�루server���� 1 ״̬��
            // server �ȴ� client �ظ� 200 ״̬�룬����յ���server ����
            // 2״̬������ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ
            // ���ļ�����׶Σ�server ���ʹ��ڴ�С��Ϊ
            ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
            int recvSize;                        // ���յ������ݴ�С
            int waitCount = 0;                   // �ȴ�������
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // ������һ�����ֽ׶�
            // ���ȷ�������ͻ��˷���һ�� 205
            // ��С��״̬�루���Լ�����ģ���ʾ������׼�����ˣ����Է�������
            // �ͻ����յ� 205 ֮��ظ�һ�� 200
            // ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ��������� �������յ� 200
            // ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
            printf("Shake hands stage\n");
            int stage = 0;        // �׶�
            bool runFlag = true;  // ���б�־
            while (runFlag) {
                switch (stage) {
                case 0:  // ���� 205 �׶Σ���ʾ������׼������
                    buffer[0] = 205;  // ״̬�� 205
                    sendto(sockServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                    Sleep(100);  // ��ʱ�ȴ�
                    stage = 1;   // ������һ���׶�
                    break;
                case 1:  // �ȴ����� 200
                         // �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    if (recvSize < 0) {        // û���յ�����
                        ++waitCount;           // ������+1
                        if (waitCount > 20) {  // ����ȴ��������� 20 ��
                            runFlag = false;  // ��ʱ�������˴�����
                            printf("Timeout error\n");  // ���������Ϣ
                            break;
                        }
                        Sleep(500);  // �ȴ� 500ms������
                        continue;
                    }
                    else {
                        // ���յ� 200 ״̬��ʱ����ʾ�ͻ���׼������
                        if ((unsigned char)buffer[0] == 200) {
                            // �����Ϣ
                            printf("Begin a file transfer\n");
                            // ����ļ���С��Ϣ
                            printf(
                                "File size is %dB, each packet is 64B and "
                                "packet total num is % d\n",
                                sizeof(data), totalPacket);
                            curSeq = 0;     // ��ǰ���к�
                            curAck = 0;     // ��ǰȷ�Ϻ�
                            totalSeq = 0;   // �����к�
                            waitCount = 0;  // �ȴ�������
                            stage = 2;      // ������һ���׶�
                        }
                    }
                    break;
                case 2:  // ���ݴ���׶�
                         // ����Ƿ��п��õ����кŵ�����
                    if (seqIsAvailable()) {
                        // ���͸��ͻ��˵����кŴ� 1 ��ʼ
                        buffer[0] = curSeq + 1;
                        // ��Ǹð�δ��ȷ��
                        ack[curSeq] = FALSE;
                        // ���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������
                        // Ϊ�򻯹��̴˴���δʵ��
                        // ��������
                        memcpy(&buffer[1], data + 64 * totalSeq, 64);
                        // printf("\n%s\n\n", &buffer[1]);
                        // �������Ϊ�գ����ʾ�������
                        while (sendto(sockServer, buffer, BUFFER_LENGTH, 0,
                            (SOCKADDR*)&addrClient,
                            sizeof(SOCKADDR)) == -1);
                        // �����Ϣ
                        printf("send a packet with a seq of %d\n", curSeq);
                        ++curSeq;            // ���к�+1
                        curSeq %= SEQ_SIZE;  // �������к�
                        ++totalSeq;          // �����к�+1
                        Sleep(500);          // ��ʱ�ȴ�
                    }
                    // �ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
                    // Sleep(500);
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    // printf("\n%d\n\n", recvSize);
                    if (recvSize < 0) {  // û���յ�����
                        waitCount++;     // ������+1
                        // 20 �εȴ� ack ��ʱ�ش�
                        if (waitCount > 20) {  // ����ȴ��������� 20 ��
                            timeoutHandler();  // ��ʱ�ش�
                            buffer[0] = 300;   // ��ʱ�ش�״̬��
                            sendto(sockServer, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrClient,
                                sizeof(SOCKADDR));
                            waitCount = 0;  // ���ü�����
                        }
                    }
                    else {  // ����յ�����
                     // �յ� ack
                        ackHandler(buffer[0]);  // ���� ack
                        waitCount = 0;          // ���ü�����
                    }
                    Sleep(500);  // ��ʱ�ȴ�
                    break;
                }
            }
        }
        else if (strcmp(cmd, "-optestgbn") == 0) {
            // ���� gbn ���Խ׶�
            printf(
                "%s\n",
                "Begin to test GBN protocol, please don't abort the process");
            // ��ȡ�����ʺ� ACK ������
            printf(
                "The loss ratio of packet is %.2f,the loss ratio of ack is % "
                ".2f\n",
                packetLossRatio, ackLossRatio);
            int waitCount = 0;     // �ȴ�������
            int stage = 0;         // �׶�
            int counter = 0;       // ������
            BOOL b;                // ������־
            unsigned char u_code;  // ״̬��
            unsigned short seq;    // �������к�
            unsigned short recvSeq;  // ���մ��ڴ�СΪ 1����ȷ�ϵ����к�
            unsigned short waitSeq;  // �ȴ������к�
            // sendto(sockServer, "-testgbn", strlen("-testgbn") + 1, 0,
            // (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            while (true) {
                // �ȴ� server �ظ����� UDP Ϊ����ģʽ
                // Sleep(500);
                while (recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrClient, &length) == -1);

                switch (stage) {
                case 0:  // �ȴ����ֽ׶�
                    u_code = (unsigned char)buffer[0];  // ״̬��
                    if ((unsigned char)buffer[0] == 205) {
                        // ����յ� 205 ״̬�룬��ʾ�ͻ���׼������
                        printf("Ready for file transmission\n");
                        buffer[0] = 200;   // �ظ� 200 ״̬��
                        buffer[1] = '\0';  // ������
                        // sendto���������������ݣ������ֱ�Ϊ���׽��֣����ͻ����������ͻ�������С����־λ��Ŀ�ĵ�ַ��Ŀ�ĵ�ַ��С
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        stage = 1;    // ����������ݽ׶�
                        recvSeq = 0;  // ���մ��ڴ�СΪ 1
                        waitSeq = 1;  // �ȴ������к�
                    }
                    break;
                case 1:  // �ȴ��������ݽ׶�
                         // �������Ϊ�գ����ʾ�������
                    seq = (unsigned short)buffer[0];
                    // printf("\n%d\n\n", buffer[0]);
                    // �����ģ����Ƿ�ʧ
                    b = lossInLossRatio(packetLossRatio);
                    if (b) {  // �������
                              // �����Ϣ
                        printf("The packet with a seq of %d loss\n", seq);
                        counter = 1;  // ������+1

                        buffer[0] = -1;  // ����״̬��
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        // Sleep(2000);
                        continue;
                    }
                    // ������յ��İ������к�
                    printf("recv a packet with a seq of %d\n", seq);
                    // ������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
                    if (!(waitSeq - seq)) {  // ������ڴ������к�
                        ++waitSeq;           // �ȴ����к�+1
                        if (waitSeq == 21) {  // ����ȴ����кų��� 20
                            waitSeq = 1;      // ����Ϊ 1
                        }
                        // �������
                        // printf("%s\n",&buffer[1]);
                        buffer[0] = seq;   // ���к�
                        recvSeq = seq;     // �����ѽ������к�
                        buffer[1] = '\0';  // ������
                    }
                    else {
                        // �����ǰһ������û���յ�����ȴ� Seq Ϊ 1
                        // �����ݰ��������򲻷���
                        // ACK����Ϊ��û����һ����ȷ�� ACK��
                        if (!recvSeq) {  // �����δ���յ��κΰ���ֻ�ܵȴ����к�Ϊ
                                         // 1 �İ�
                            continue;
                        }
                        // �����յ�����İ������򷵻���һ����ȷ���յ����кŵ�ack
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';
                    }
                    // �����ģ�� ack �Ƿ�ʧ
                    b = lossInLossRatio(ackLossRatio);
                    if (b) {  // ��� ack ��ʧ
                        // �����Ϣ
                        printf("The ack of %d loss\n",
                            (unsigned char)buffer[0]);
                        counter = 1;  // ������+1

                        buffer[0] = -1;  // ����״̬��
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        // Sleep(2000);
                        continue;
                    }
                    // printf("\n%d\n\n",buffer[0]);
                    while (sendto(sockServer, buffer, 2, 0,
                        (SOCKADDR*)&addrClient,
                        sizeof(SOCKADDR)) == -1);
                    // �����Ϣ
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    counter = 0;  // ����������
                    break;
                }
                Sleep(500);
            }
        }
        else if (strcmp(cmd, "-download") == 0) {
            // ���� gbn ���Խ׶�
            // ���� server��server ���� 0 ״̬���� client ���� 205
            // ״̬�루server���� 1 ״̬��
            // server �ȴ� client �ظ� 200 ״̬�룬����յ���server ����
            // 2״̬������ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ
            // ���ļ�����׶Σ�server ���ʹ��ڴ�С��Ϊ
            ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
            int recvSize;                        // ���յ������ݴ�С
            int waitCount = 0;                   // �ȴ�������
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // ������һ�����ֽ׶�
            // ���ȷ�������ͻ��˷���һ�� 205
            // ��С��״̬�루���Լ�����ģ���ʾ������׼�����ˣ����Է�������
            // �ͻ����յ� 205 ֮��ظ�һ�� 200
            // ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ��������� �������յ� 200
            // ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
            printf("Shake hands stage\n");
            int stage = 0;        // �׶�
            bool runFlag = true;  // ���б�־
            while (runFlag) {
                switch (stage) {
                case 0:  // ���� 205 �׶Σ���ʾ�ͻ���׼������
                    buffer[0] = 205;
                    // ��������
                    sendto(sockServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                    Sleep(100);  // ��ʱ�ȴ�
                    stage = 1;   // ����ȴ��������ݽ׶�
                    break;
                case 1:  // �ȴ����� 200
                         // �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    if (recvSize < 0) {        // ���û���յ�
                        ++waitCount;           // �ȴ�������+1
                        if (waitCount > 20) {  // ����ȴ��������� 20 ��
                            runFlag = false;  // ��ʱ�������˴�����
                            printf("Timeout error\n");  // ���������Ϣ
                            break;
                        }
                        Sleep(500);  // �ȴ� 500ms������
                        continue;
                    }
                    else {  // ����յ�����
                     // ���յ� 200 ״̬��ʱ����ʾ�ͻ���׼������
                        if ((unsigned char)buffer[0] == 200) {
                            // �����Ϣ
                            printf("Begin a file transfer\n");
                            // ����ļ���С��Ϣ
                            printf(
                                "File size is %dB, each packet is 64B and "
                                "packet total num is % d\n",
                                sizeof(data), totalPacket);
                            curSeq = 0;     // ��ǰ���ݰ������к�
                            curAck = 0;     // ��ǰ�ȴ�ȷ�ϵ� ack
                            totalSeq = 0;   // �յ��İ�������
                            waitCount = 0;  // �ȴ�������
                            stage = 2;      // �������ݴ���׶�
                        }
                    }
                    break;
                case 2:                      // ���ݴ���׶�
                    if (seqIsAvailable()) {  // ����Ƿ��п��õ����кŵ�����
                        // ����п��õ����кŵ����ݣ���׼�����ݰ�
                        // ���͸��ͻ��˵����кŴ� 1 ��ʼ
                        buffer[0] = curSeq + 1;
                        // ��Ǹð�δ��ȷ��
                        ack[curSeq] = FALSE;
                        // ���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������
                        // Ϊ�򻯹��̴˴���δʵ��
                        // ��������
                        memcpy(&buffer[1], data + 64 * totalSeq, 64);
                        // �������
                        printf("\nbuffer:%s\n\n", &buffer[1]);
                        // �������Ϊ�գ����ʾ�������
                        if (buffer[1] == '\0') {
                            // printf("Finish\n");
                            stage = 3;  // ��������׶�
                            break;
                        }
                        // �������ݰ���ֱ���ɹ�Ϊֹ������ӡ���͵����к�
                        while (sendto(sockServer, buffer, BUFFER_LENGTH, 0,
                            (SOCKADDR*)&addrClient,
                            sizeof(SOCKADDR)) == -1);
                        printf("send a packet with a seq of %d\n", curSeq);
                        ++curSeq;            // ���µ�ǰ���к�
                        curSeq %= SEQ_SIZE;  // �������к�
                        ++totalSeq;          // ���������к�
                        Sleep(500);          // ��ʱ�ȴ�
                    }
                    // �ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
                    // Sleep(500);
                    recvSize =
                        recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrClient), &length);
                    // printf("\n%d\n\n", recvSize);
                    if (recvSize < 0) {  // ���û���յ�����
                        waitCount++;     // �ȴ�������+1
                        // 20 �εȴ� ack ��ʱ�ش�
                        if (waitCount > 20) {  // ����ȴ��������� 20 ��
                            timeoutHandler();  // ��ʱ�ش�
                            waitCount = 0;     // ���ü�����
                        }
                    }
                    else {  // ����յ�����
                     // �յ� ack
                        ackHandler(buffer[0]);  // ����ackhandler����ack
                        waitCount = 0;          // ���õȴ�������
                    }
                    Sleep(500);  // ��ʱ�ȴ�
                    break;
                case 3:                  // �����׶�
                    printf("Finish\n");  // �����Ϣ
                    runFlag = false;     // ����ѭ��
                }
            }
        }
        else if (strcmp(buffer, "-upload") == 0) {
            ZeroMemory(data, sizeof(data));  // ��ʼ������
            // ���� upload�׶�
            printf(
                "%s\n",
                "Begin to test GBN protocol, please don't abort the process");
            int waitCount = 0;     // �ȴ�������
            int stage = 0;         // �׶�
            int counter = 0;       // ������
            BOOL flag = true;      // ���б�־
            BOOL b;                // ������־
            unsigned char u_code;  // ״̬��
            unsigned short seq;    // �������к�
            unsigned short recvSeq;  // ���մ��ڴ�СΪ 1����ȷ�ϵ����к�
            unsigned short waitSeq;  // �ȴ������к�
            // sendto(sockServer, "-testgbn", strlen("-testgbn") + 1, 0,
            // (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            while (flag) {
                // �ȴ� server �ظ����� UDP Ϊ����ģʽ
                // Sleep(500);

                if (stage == 2) {
                    // �洢�ļ�
                    printf("Finish\n");                   // �����Ϣ
                    printf("\ndata:\n%s\n\n", &data[0]);  // �������
                    FILE* out;
                    // ����ļ��򿪳ɹ�����д������
                    if (fopen_s(&out, "test_recver.txt", "wb") == 0) {
                        // д������
                        fwrite(data, sizeof(char), strlen(data), out);
                        // �ر��ļ�
                        fclose(out);
                    }
                    flag = false;  // ����ѭ��
                    break;
                }
                // ѭ���ȴ����ݣ�ͨ��recvfrom���������ȴ��ͻ��˷�������
                // ֱ�����յ�����Ϊֹ��ѭ���Ż��˳�
                while (recvfrom(sockServer, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrClient, &length) == -1);
                // ��������
                memcpy(data + 64 * totalSeq, &buffer[1], 64);
                // �������
                printf("\ndata:%s\n\n", data + 64 * totalSeq);
                switch (stage) {
                case 0:  // �ȴ����ֽ׶�
                         // ��ȡ״̬��
                    u_code = (unsigned char)buffer[0];
                    if ((unsigned char)buffer[0] == 205) {
                        printf("Ready for file transmission\n");
                        // ���������� 200 ��ȷ��׼���ý�������
                        buffer[0] = 200;
                        buffer[1] = '\0';  // ������
                        sendto(sockServer, buffer, 2, 0,
                            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                        stage = 1;    // ����������ݽ׶�
                        recvSeq = 0;  // ���մ��ڴ�СΪ 1
                        waitSeq = 1;  // �ȴ������к�
                    }
                    break;
                    // ÿ���յ�һ�����ݰ��󣬿ͻ��˼�������к��Ƿ���Ԥ��һ��
                    // ���������к���ȷ�����Ͷ�Ӧ��
                    // ACK����������Ԥ�ڣ��ͻ��˸�����һ��ȷ�ϵ����кŷ���
                    // ACK
                case 1:                       // �ȴ��������ݽ׶�
                    if (buffer[1] == '\0') {  // ����յ�������Ϊ��
                        stage = 2;            // ��������׶�
                        break;
                    }
                    // ��ȡ�������к�
                    seq = (unsigned short)buffer[0];
                    // ������յ��İ������к�
                    printf("recv a packet with a seq of %d\n", seq);
                    ++totalSeq;  // �����ѽ��յİ�������
                    // ������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
                    if (!(waitSeq - seq)) {  // ������ڴ������к�
                        ++waitSeq;  // �����ڴ�����һ�����к�
                        if (waitSeq == 21) {  // ����ȴ����кų��� 20
                            waitSeq = 1;      // ����Ϊ 1
                        }
                        // �������
                        // printf("%s\n",&buffer[1]);
                        buffer[0] = seq;   // ���к�
                        recvSeq = seq;     // �����ѽ��յ����к�
                        buffer[1] = '\0';  // ������
                    }
                    else {
                        // �����ǰһ������û���յ�����ȴ� Seq Ϊ 1
                        // �����ݰ��������򲻷���
                        // ACK����Ϊ��û����һ����ȷ�� ACK��
                        // �����δ���յ��κΰ���ֻ�ܵȴ����к�Ϊ1�İ�
                        if (!recvSeq) {
                            continue;
                        }
                        // �����յ�����İ������򷵻���һ����ȷ���յ����кŵ�ack
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';  // ������
                    }
                    // ���� ack
                    while (sendto(sockServer, buffer, 2, 0,
                        (SOCKADDR*)&addrClient,
                        sizeof(SOCKADDR)) == -1);
                    // �����Ϣ
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    counter = 0;  // ����������
                    break;
                }
                Sleep(500);  // ��ʱ�ȴ�
            }
        }
        // ��������
        sendto(sockServer, buffer, strlen(buffer) + 1, 0,
            (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
        Sleep(500);  // ��ʱ�ȴ�
    }
    // �ر��׽��֣�ж�ؿ�
    closesocket(sockServer);
    WSACleanup();
    return 0;
}