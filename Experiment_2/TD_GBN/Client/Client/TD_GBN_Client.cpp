// #include "stdafx.h"
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340      // �������ݵĶ˿ں�
#define SERVER_IP "127.0.0.1"  // �������� IP ��ַ
const int BUFFER_LENGTH = 1026;

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

char fileName[40];       // �ļ���
char filePath[50];       // �ļ�·��
char file[1024 * 1024];  // �ļ�����
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
    step =
        step >= 0
        ? step
        : step +
        SEQ_SIZE;  // ���������кţ�������ڵ���0���򲻱䣬�������
                   // SEQ_SIZE
// ���к��Ƿ��ڵ�ǰ���ʹ���֮��
// ������ڴ����ڣ����ʾδ�յ� ack
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
    Sleep(1000);                                // �ȴ� 1s
    printf("Timer out error.\n\n\n\n");         // ��ʱ�ش�
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
            ack[i] = TRUE;  // �����ߵ�ackΪTRUE
        }
        curAck = index + 1;  // ���� curAck
    }
}
/****************************************************************/
/* -time �ӷ������˻�ȡ��ǰʱ��
-quit �˳��ͻ���
-testgbn [X] ���� GBN Э��ʵ�ֿɿ����ݴ���
[X] [0,1] ģ�����ݰ���ʧ�ĸ���
[Y] [0,1] ģ�� ACK ��ʧ�ĸ���
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
        return 1;
    }
    // ���汾��
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("Could not find a usable version of Winsock.dll\n");
        WSACleanup();
    }
    else {
        printf("The Winsock 2.2 dll was found okay\n");
    }
    // �����׽���
    SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
    SOCKADDR_IN addrServer;

    // int length = sizeof(SOCKADDR);
    // struct in_addr p;
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // �������� IP
    // printf("\n%d\n\n", addrServer.sin_addr.S_un.S_addr);
    addrServer.sin_family = AF_INET;           // Э��
    addrServer.sin_port = htons(SERVER_PORT);  // �˿�
    // ���ջ�����
    char buffer[BUFFER_LENGTH];          // ������
    ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
    int len = sizeof(SOCKADDR);          // ��ַ����
    // Ϊ�˲���������������ӣ�����ʹ�� -time ����ӷ������˻�õ�ǰʱ��
    // ʹ�� -testgbn [X] [Y] ���� GBN ����[X]��ʾ���ݰ���ʧ����
    //  [Y]��ʾ ACK ��������
    printTips();  // ��ӡ��ʾ��Ϣ
    int ret;      // ����ֵ
    int interval = 1;  // �յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ��������
                       // ack��0 ���߸�������ʾ���еĶ������� ack
    char cmd[128];                // ����
    float packetLossRatio = 0.2;  // Ĭ�ϰ���ʧ�� 0.2
    float ackLossRatio = 0.2;     // Ĭ�� ACK ��ʧ�� 0.2
    // ��ʱ����Ϊ������ӣ�����ѭ����������
    // ���������ݶ����ڴ�
    std::ifstream infile;            // ������
    std::ofstream outfile;           // �����
    infile.open("./test.txt");       // ���ļ�
    char data[64 * 113];             // 113 ����
    ZeroMemory(data, sizeof(data));  // ��ʼ��
    infile.read(data, 64 * 113);     // ��ȡ�ļ�
    infile.close();                  // �ر��ļ�
    // printf("\n%s\n\n", data);
    totalPacket = sizeof(data) / 64;  // �����������
    // ZeroMemory(data, sizeof(data));
    int recvSize;  // ���յ������ݰ���С
    // ��ʼ�� ack ����
    for (int i = 0; i < SEQ_SIZE; ++i) {
        ack[i] = TRUE;
    }
    // �����������
    srand((unsigned)time(NULL));
    while (true) {
        gets_s(buffer, BUFFER_LENGTH);  // �Ӽ�����������
        // ��������
        ret = sscanf_s(buffer, "%s [%f] [%f]", &cmd,(unsigned)sizeof(cmd), &packetLossRatio, &ackLossRatio);
        // printf("\n%s\n\n", buffer);

        // ��ʼ GBN ���ԣ�ʹ�� GBN Э��ʵ�� UDP �ɿ��ļ�����
        if (!strcmp(cmd, "-testgbn")) {
            // ���� gbn ���Խ׶�
            // ��ʼ���� GBN Э�飬�벻Ҫ�жϽ���
            printf("%s\n",
                "Begin to test GBN protocol, please don't abort the "
                "process");
            // ���ݰ��Ķ�ʧ�ʺ�ack �Ķ�ʧ�ʵ����
            printf(
                "The loss ratio of packet is %.2f,the loss ratio of ack is % "
                ".2f\n",
                packetLossRatio, ackLossRatio);
            int waitCount = 0;     // �ȴ�������
            int stage = 0;         // �׶�
            BOOL b;                // ������־
            unsigned char u_code;  // ״̬��
            unsigned short seq;    // �������к�
            unsigned short recvSeq;  // ���մ��ڴ�СΪ 1����ȷ�ϵ����к�
            unsigned short waitSeq;  // �ȴ������к�
            // ���Ͳ�������
            sendto(socketClient, "-testgbn", strlen("-testgbn") + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            while (true) {
                // �ȴ� server �ظ����� UDP Ϊ����ģʽ
                // Sleep(500);
                // ѭ���ȴ����ݣ�ͨ��recvfrom�����ȴ����������͵����ݰ���ֱ�����յ����ݰ���ѭ���Ż��˳�
                // recvfrom���������������ݣ����յ����ݺ󣬽����ݴ�ŵ�buffer�У���ȡ�������ĵ�ַ��Ϣ
                while (recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrServer, &len) == -1);
                // printf("\n%s\n\n", &buffer[1]);
                // memcpy(data + 64 * totalSeq, &buffer[1], 64);
                switch (stage) {
                case 0:  // �ȴ����ֽ׶�
                    u_code = (unsigned char)buffer[0];  // ״̬��
                    // ����յ� 205�����ʾ������׼�����ˣ����Է�������
                    if ((unsigned char)buffer[0] == 205) {
                        // ������׼�����ˣ����Է�������
                        printf("Ready for file transmission\n");
                        // �ͻ��˷��� 200
                        // ״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
                        buffer[0] = 200;
                        buffer[1] = '\0';
                        // sendto���������������ݣ��������ݵ���������,���ͳɹ����ط��͵��ֽ��������򷵻�-1
                        sendto(socketClient, buffer, 2, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                        stage = 1;  // ����ȴ��������ݽ׶�
                        recvSeq = 0;  // ���մ��ڴ�СΪ 1����ȷ�ϵ����к�
                        waitSeq = 1;  // �ȴ������к�
                    }
                    break;
                case 1:  // �ȴ��������ݽ׶�
                    // ����յ������ݰ�Ϊ�գ����ʾ�ļ��������
                    if (&buffer[1] == NULL) {
                        break;
                    }
                    // ��ȡ�������к�
                    seq = (unsigned short)buffer[0];
                    // �����ģ����Ƿ�ʧ
                    b = lossInLossRatio(packetLossRatio);
                    // ����������򲻴���
                    if (b) {
                        // ������������к�
                        printf("The packet with a seq of %d loss\n", seq);
                        continue;  // �������������Ըð�
                    }
                    // ������յ��İ������к�
                    printf("recv a packet with a seq of %d\n", seq);
                    // ������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
                    if (!(waitSeq - seq)) {  // ������ڴ������к�
                        ++waitSeq;  // �����ڴ�����һ�����к�
                        if (waitSeq == 21) {  // ������������ֵ
                            waitSeq = 1;      // �ص� 1
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
                        if (!recvSeq) {  // �����δ���յ��κΰ���ֻ�ܵȴ����к�Ϊ1�İ�
                            continue;
                        }
                        // �����յ�����İ������򷵻���һ����ȷ���յ����кŵ�
                        // ACK
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';
                    }
                    // �����ģ�� ACK �Ƿ�ʧ
                    b = lossInLossRatio(ackLossRatio);
                    // ��� ACK ��ʧ���򲻴���
                    if (b) {
                        printf("The ack of %d loss\n",
                            (unsigned char)buffer[0]);
                        continue;  // ��� ACK ��ʧ������������
                    }
                    // ���ACKδ��ʧ����ͨ��sendto����
                    // ACK��������������ӡACK��־
                    while (sendto(socketClient, buffer, 2, 0,
                        (SOCKADDR*)&addrServer,
                        sizeof(SOCKADDR)) == -1);
                    // ��� ACK
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    break;
                }
                Sleep(500);  // ģ�������ӳ٣��ȴ� 500ms
            }
        }
        else if (!strcmp(cmd, "-optestgbn")) {
            // ���� gbn ���Խ׶�
            // ���� server��server ���� 0 ״̬���� client ���� 205
            // ״̬�루server���� 1 ״̬��
            //  server �ȴ� client �ظ� 200 ״̬�룬����յ���server ����
            // 2״̬������ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ
            // ���ļ�����׶Σ�server ���ʹ��ڴ�С��Ϊ
            // sendto�����������ݣ��������ݵ���������,���ͳɹ����ط��͵��ֽ��������򷵻�-1
            sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            // ZeroMemory(buffer, sizeof(buffer));
            int recvSize;       // ���յ������ݰ���С
            int waitCount = 0;  // �ȴ�������
            // �����ʾ��Ϣ
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // ������һ�����ֽ׶�
            // ���ȷ�������ͻ��˷���һ�� 205
            // ��С��״̬�루���Լ�����ģ���ʾ������׼�����ˣ����Է�������
            // �ͻ����յ� 205 ֮��ظ�һ�� 200
            // ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ��������� �������յ� 200
            // ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
            printf("Shake hands stage\n");
            int stage = 0;        // ���ڸ������ֺ����ݴ���׶�
            bool runFlag = true;  // ����whileѭ���Ƿ�����
            while (runFlag) {
                switch (stage) {
                case 0:  // ���� 205
                         // �������֣���ʾ�ͻ���׼�����˿��Կ�ʼ����
                    buffer[0] = 205;  // ״̬��
                    sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    Sleep(100);  // �ȴ� 100ms
                    stage = 1;   // ����ȴ��������ݽ׶�
                    break;
                case 1:  // �ȴ����� 200
                         // �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
                    recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH,
                        0, ((SOCKADDR*)&addrServer), &len);
                    if (recvSize < 0) {
                        ++waitCount;           // �ȴ�������+1
                        if (waitCount > 20) {  // ����ȴ����������� 20
                            runFlag = false;  // ��ʱ�������˴�����
                            printf("Timeout error\n");  // �����ʱ����
                            break;
                        }
                        Sleep(500);  // �ȴ� 500ms������
                        continue;
                    }
                    else {
                        // ���յ�200״̬��ʱ���ͻ��˽���״̬2
                        if ((unsigned char)buffer[0] == 200) {
                            // �����ʾ��Ϣ
                            printf("Begin a file transfer\n");
                            // ����ļ���С
                            printf(
                                "File size is %dB, each packet is 1024B "
                                "and packet total num is % d\n",
                                sizeof(data), totalPacket);
                            curSeq = 0;     // ��ǰ���ݰ������к�
                            curAck = 0;     // ��ǰ�ȴ�ȷ�ϵ� ack
                            totalSeq = 0;   // �յ��İ�������
                            waitCount = 0;  // �ȴ�������
                            stage = 2;      // �������ݴ���׶�
                        }
                    }
                    break;
                case 2:  // ���ݴ���׶�
                         // ����Ƿ���Է��͸����кŵ�����
                    if (seqIsAvailable()) {
                        // ���͸������������кŴ� 1 ��ʼ
                        buffer[0] = curSeq + 1;
                        // ��Ǹð�δ��ȷ��
                        ack[curSeq] = FALSE;
                        // ���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������
                        // Ϊ�򻯹��̴˴���δʵ��
                        // memcpy(&buffer[1], data + 1024 * totalSeq, 1024);

                        while (sendto(socketClient, buffer, BUFFER_LENGTH,
                            0, (SOCKADDR*)&addrServer,
                            sizeof(SOCKADDR)) == -1);
                        // ������͵İ������к�
                        printf("send a packet with a seq of %d\n", curSeq);

                        ++curSeq;            // ���µ�ǰ���к�
                        curSeq %= SEQ_SIZE;  // ���к�ѭ��ʹ��
                        ++totalSeq;  // �����ѷ��͵İ�������
                        // Sleep(500);
                        // ���� ack
                        recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrServer), &len);
                    }
                    else {  // �����ʱ�����ش�
                        timeoutHandler();
                        break;
                        // waitCount = 0;
                    }
                    // �ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1

                    // printf("\n%d\n\n", buffer[0]);
                    recvSize = buffer[0];  // ���յ��� ack
                    // ���δ�յ� ack����ȴ�������+1
                    if (recvSize < 0) {
                        waitCount++;  // �ȴ�������+1
                        // printf("waitokdqowjfq\n");
                        // 20 �εȴ� ack ��ʱ�ش�
                        if (waitCount > 20) {
                            // ��ʱ�����ط�
                            timeoutHandler();
                            // ���õȴ�������
                            waitCount = 0;
                        }
                    }
                    else {  // ����յ� ack������ ack
                     // �յ� ack
                     // printf("\n%d\n\n", buffer[0]);
                     // ����ackhandler����ack
                        ackHandler(buffer[0]);
                        // ���õȴ�������
                        waitCount = 0;
                    }
                    // Sleep(500);
                    break;
                }
            }
        }
        else if (!strcmp(cmd, "-download")) {
            // ���� download ���Խ׶�
            printf(
                "%s\n",
                "Begin to test GBN protocol, please don't abort the process");
            int waitCount = 0;     // �ȴ�������
            int stage = 0;         // �׶�
            BOOL b;                // ������־
            BOOL flag = true;      // ����ѭ���Ƿ�����
            unsigned char u_code;  // ״̬��
            unsigned short seq;    // �������к�
            unsigned short recvSeq;  // ���մ��ڴ�СΪ 1����ȷ�ϵ����к�
            unsigned short waitSeq;  // �ȴ������к�
            // ����������� -download ����
            sendto(socketClient, "-download", strlen("-download") + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            while (flag) {
                // �ȴ� server �ظ����� UDP Ϊ����ģʽ
                // Sleep(500);
                if (stage == 2) {  //
                    // �洢�ļ�
                    printf("Finish\n");  // ����ļ��������
                    printf("\ndata:\n%s\n\n", &data[0]);  // ����ļ�����
                    FILE* out;                            // ����ļ�
                    // ����ļ��򿪳ɹ�����д���ļ�
                    if (fopen_s(&out, "test_recver.txt", "wb") == 0) {
                        // д���ļ�
                        fwrite(data, sizeof(char), strlen(data), out);
                        // �ر��ļ�
                        fclose(out);
                    }
                    flag = false;  // ����ѭ��
                    break;
                }
                // ѭ���ȴ����ݣ�ͨ��recvfrom�����ȴ����������͵����ݰ�
                // ֱ�����յ����ݰ���ѭ���Ż��˳�
                while (recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                    (SOCKADDR*)&addrServer, &len) == -1);
                // �������������״̬��Ϊ 300���ͻ��˴洢���ݲ��˳�
                if (buffer[0] == 300) stage = 2;
                // printf("\nbuffer:%s\n\n", &buffer[1]);
                ZeroMemory(data + 64 * totalSeq, 64);  // ��ʼ�� data
                // �����ݴ洢�� data
                memcpy(data + 64 * totalSeq, &buffer[1], 64);
                // ��� data
                //printf("\ndata:%s\n\n", data + 64 * totalSeq);
                printf("\ndata:%s\n\n", &buffer[1]);
                switch (stage) {
                case 0:  // �ȴ����ֽ׶�
                    // ��ȡ״̬��
                    u_code = (unsigned char)buffer[0];
                    // ����յ� 205����ʾ��������׼���ý����ļ�����
                    if ((unsigned char)buffer[0] == 205) {
                        printf("Ready for file transmission\n");
                        // �ͻ��˷��� 200��ȷ�ϱ�ʾ׼���ý�������
                        buffer[0] = 200;
                        buffer[1] = '\0';  // ������
                        // ��������
                        sendto(socketClient, buffer, 2, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                        stage = 1;  // ����ȴ��������ݽ׶�
                        recvSeq = 0;  // ���մ��ڴ�СΪ 1����ȷ�ϵ����к�
                        waitSeq = 1;  // �ȴ������к�
                    }
                    break;
                    // ÿ���յ�һ�����ݰ��󣬿ͻ��˼�������к��Ƿ���Ԥ��һ��
                    // ���������к���ȷ�����Ͷ�Ӧ��
                    // ACK����������Ԥ�ڣ��ͻ��˸�����һ��ȷ�ϵ����кŷ��� ACK
                case 1:                       // �ȴ��������ݽ׶�
                    if (buffer[1] == '\0') {  // ����յ������ݰ�Ϊ��
                        stage = 2;            // ��������׶�
                        break;
                    }
                    // ��ȡ�������к�
                    seq = (unsigned short)buffer[0];
                    // �����ģ����Ƿ�ʧ
                    /*b = lossInLossRatio(packetLossRatio);
                    if (b) {
                        printf("The packet with a seq of %d loss\n", seq);
                        continue;
                    }*/
                    // ������յ��İ������к�
                    printf("recv a packet with a seq of %d\n", seq);
                    ++totalSeq;  // �����ѽ��յİ�������
                    // ������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
                    if (!(waitSeq - seq)) {  // ������ڴ������к�
                        ++waitSeq;  // �����ڴ�����һ�����к�
                        if (waitSeq == 21) {  // ������������ֵ
                            waitSeq = 1;      // �ص� 1
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
                        if (!recvSeq) {  // �����δ���յ��κΰ���ֻ�ܵȴ����к�Ϊ1�İ�
                            continue;
                        }
                        // ������򣬷�����һ����ȷ���յ����кŵ� ACK
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';  // ������
                    }
                    /*b = lossInLossRatio(ackLossRatio);
                    if (b) {
                        printf("The ack of %d loss\n", (unsigned
                    char)buffer[0]); continue;
                    }*/
                    // ���� ACK
                    while (sendto(socketClient, buffer, 2, 0,
                        (SOCKADDR*)&addrServer,
                        sizeof(SOCKADDR)) == -1);
                    // ��� ACK
                    printf("send a ack of %d\n", (unsigned char)buffer[0]);
                    break;
                case 2:                          // �����׶�
                    totalSeq -= SEND_WIND_SIZE;  // �����ѷ��͵İ�������
                    break;
                }
                Sleep(500);  // ģ�������ӳ٣��ȴ� 500ms
            }
        }
        else if (!strcmp(cmd, "-upload")) {
            // ���� gbn ���Խ׶�
            // ���� server��server ���� 0 ״̬���� client ���� 205
            // ״̬�루server���� 1 ״̬��
            // server �ȴ� client �ظ� 200 ״̬�룬����յ���server ����
            // 2״̬������ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ
            // ���ļ�����׶Σ�server ���ʹ��ڴ�С��Ϊ
            // sendto�����������ݣ��������ݵ���������,׼���������ֽ׶η��ͳɹ����ط��͵��ֽ��������򷵻�-1
            sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
            // ZeroMemory(buffer, sizeof(buffer));
            int recvSize;       // ���յ������ݰ���С
            int waitCount = 0;  // �ȴ�������
            // �����ʾ��Ϣ
            printf(
                "Begain to test GBN protocol,please don't abort the process\n");
            // ������һ�����ֽ׶�
            // ���ȷ�������ͻ��˷���һ�� 205
            // ��С��״̬�루���Լ�����ģ���ʾ������׼�����ˣ����Է�������
            // �ͻ����յ� 205 ֮��ظ�һ�� 200
            // ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ��������� �������յ� 200
            // ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
            printf("Shake hands stage\n");
            int stage = 0;        // ���ڸ������ֺ����ݴ���׶�
            bool runFlag = true;  // ����whileѭ���Ƿ�����
            while (runFlag) {
                switch (stage) {
                case 0:  // ���� 205 �׶Σ���ʾ������׼�����˿��Կ�ʼ����
                    buffer[0] = 205;
                    // ��������
                    sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    Sleep(100);  // �ȴ� 100ms
                    stage = 1;   // ����ȴ��������ݽ׶�
                    break;
                case 1:  // �ȴ����� 200
                         // �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
                    recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH,
                        0, ((SOCKADDR*)&addrServer), &len);
                    if (recvSize < 0) {        // ���û���յ�
                        ++waitCount;           // �ȴ�������+1
                        if (waitCount > 20) {  // ����ȴ����������� 20
                            runFlag = false;  // ��ʱ�������˴�����
                            printf("Timeout error\n");  // �����ʱ����
                            break;
                        }
                        Sleep(500);  // �ȴ� 500ms������
                        continue;
                    }
                    else {  // ����յ�
                     // ���յ�200״̬��ʱ������������״̬2
                        if ((unsigned char)buffer[0] == 200) {
                            // �����ʾ��Ϣ
                            printf("Begin a file transfer\n");
                            // ����ļ���С
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
                    if (seqIsAvailable()) {  // ������к�������
                        // ������ã���׼�����ݰ�
                        // ���͸������������кŴ� 1 ��ʼ
                        buffer[0] = curSeq + 1;
                        // ��Ǹð�δ��ȷ��
                        ack[curSeq] = FALSE;
                        // ���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������
                        // Ϊ�򻯹��̴˴���δʵ��
                        // �����ݴ洢�� buffer
                        memcpy(&buffer[1], data + 64 * totalSeq, 64);
                        // ���buffer
                        printf("\nbuffer:\n%s\n\n",data + 64 * totalSeq);
                        if (buffer[1] == '\0') {  // ����յ������ݰ�Ϊ��
                            printf("Finish\n");  // ����ļ��������
                            stage = 3;           // ��������׶�
                            break;
                        }
                        // �������ݰ���ֱ���ɹ�Ϊֹ������ӡ���͵����к�
                        while (sendto(socketClient, buffer, BUFFER_LENGTH,
                            0, (SOCKADDR*)&addrServer,
                            sizeof(SOCKADDR)) == -1);
                        printf("send a packet with a seq of %d\n", curSeq);

                        ++curSeq;            // ���µ�ǰ���к�
                        curSeq %= SEQ_SIZE;  // ���к�ѭ��ʹ��
                        ++totalSeq;  // �����ѷ��͵İ�������
                        // Sleep(500);
                        // ���� ack
                        recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
                            ((SOCKADDR*)&addrServer), &len);
                    }
                    else {  // ������кŲ����ã����ش�
                        timeoutHandler();  // ��ʱ����
                        break;
                        // waitCount = 0;
                    }
                    // �ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1

                    // printf("\n%d\n\n", buffer[0]);
                    recvSize = buffer[0];  // ���յ��� ack
                    if (recvSize < 0) {    // ���δ�յ� ack
                        waitCount++;       // �ȴ�������+1
                        // printf("waitokdqowjfq\n");
                        // 20 �εȴ� ack ��ʱ�ش�
                        if (waitCount > 20) {  // ����ȴ����������� 20
                            timeoutHandler();  // ��ʱ�����ط�
                            waitCount = 0;     // ���õȴ�������
                        }
                    }
                    else {  // ����յ� ack
                     // �յ� ack
                     // printf("\n%d\n\n", buffer[0]);
                        ackHandler(buffer[0]);  // ����ackhandler����ack
                        waitCount = 0;          // ���õȴ�������
                    }
                    // Sleep(500);
                    break;
                case 3:                  // �����׶�
                    printf("Finish\n");  // ����ļ��������
                    runFlag = false;     // ����ѭ��
                }
            }
        }
        // printf("\ndata:%s\n\n", data);
        // ��������
        sendto(socketClient, buffer, strlen(buffer) + 1, 0,
            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
        // ��������
        ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0,
            (SOCKADDR*)&addrServer, &len);
        // �������
        printf("%s\n", buffer);
        // �˳��ͻ���
        if (!strcmp(buffer, "Good bye!")) {
            break;
        }
        printTips();
    }
    // �ر��׽���
    closesocket(socketClient);
    WSACleanup();
    return 0;
}