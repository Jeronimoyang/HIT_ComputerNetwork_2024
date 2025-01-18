#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340      // �������ݵĶ˿ں�
#define SERVER_IP "127.0.0.1"  // �������� IP ��ַ
#define BUFFER_SIZE 1024       // ��������С
#define SEQ_SIZE 16            // ���кŸ���
#define SWIN_SIZE 8            // ���ʹ��ڴ�С
#define RWIN_SIZE 8            // ���մ��ڴ�С
#define LOSS_RATE 0.8          // ������
using namespace std;

char cmdBuffer[50];          // �������
char buffer[BUFFER_SIZE];    // ������
char cmd[10];                // ����
char fileName[40];           // �ļ���
char filePath[50];           // �ļ�·��
char file[1024 * 1024];      // �ļ��ַ���
int len = sizeof(SOCKADDR);  // ��ַ����
int recvSize;                // ���յ������ݴ�С
int Deliver(char* file, int ack);
int Send(ifstream& infile, int seq, SOCKET socket, SOCKADDR* addr);
int MoveSendWindow(int seq);
int Read(ifstream& infile, char* buffer);
// ����ṹ��
struct Cache {
    bool used;                               // �Ƿ�ʹ��
    char buffer[BUFFER_SIZE];                // ������
    Cache() {                                // ���캯��
        used = false;                        // ��ʼ��Ϊδʹ��
        ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
    }
} recvWindow[SEQ_SIZE];  // ���մ���
// ����֡�ṹ��
struct DataFrame {
    clock_t start;                           // ����ʱ��
    char buffer[BUFFER_SIZE];                // ������
    DataFrame() {                            // ���캯��
        start = 0;                           // ��ʼ������ʱ��
        ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
    }
} sendWindow[SEQ_SIZE];  // ���ʹ���
int main(int argc, char* argv[]) {
    // �����׽��ֿ�
    WORD wVersionRequested;
    WSADATA wsaData;
    // �汾 2.2
    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);  // ���� Winsock.dll
    if (err != 0) {                                     // ����ʧ��
        // ���������
        printf("Winsock.dll ����ʧ�ܣ�������: %d\n", err);
        return -1;
    }
    // �ж� Winsock.dll �İ汾
    if (LOBYTE(wsaData.wVersion) != LOBYTE(wVersionRequested) ||
        HIBYTE(wsaData.wVersion) != HIBYTE(wVersionRequested)) {
        // ����汾������Ҫ�����������Ϣ
        printf("�Ҳ��� %d.%d �汾�� Winsock.dll\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
        WSACleanup();  // ���� Winsock.dll
        return -1;
    }
    else {  // ���سɹ�
        printf("Winsock %d.%d ���سɹ�\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
    }
    // �����ͻ����׽���
    SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
    // ����Ϊ������ģʽ
    int iMode = 1;                                            // ������ģʽ
    ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);  // ���÷�����
    SOCKADDR_IN addrServer;                                   // ��������ַ
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // ���÷����� IP
    addrServer.sin_family = AF_INET;                      // ���õ�ַ��
    addrServer.sin_port = htons(SERVER_PORT);             // ���ö˿ں�
    srand((unsigned)time(NULL));  // �������������
    int status = 0;               // ״̬
    clock_t start;                // ��ʼʱ��
    clock_t now;                  // ��ǰʱ��
    int seq;                      // ���к�
    int ack;                      // ȷ�Ϻ�
    while (true) {
        gets_s(cmdBuffer, 50);  // �ӿ���̨��ȡ����
        // ��������
        sscanf_s(cmdBuffer, "%s%s", cmd, sizeof(cmd) - 1, fileName,
            sizeof(fileName) - 1);
        // �������ʽ -upload
        if (!strcmp(cmd, "upload")) {
            // ��ӡ�ϴ��ļ�
            printf("�����ϴ��ļ�: %s\n", fileName);
            strcpy_s(filePath, "./");      // �����ļ�·��
            strcat_s(filePath, fileName);  // �����ļ���
            ifstream infile(filePath);     // ���ļ�
            start = clock();               // ��ȡ��ǰʱ��
            seq = 0;                       // ���к�
            status = 0;                    // ״̬
            sendWindow[0].buffer[0] = 10;  // ���û�����
            // ���û�����
            strcpy_s(sendWindow[0].buffer + 1, strlen(cmdBuffer) + 1,
                cmdBuffer);
            sendWindow[0].start = start - 1000L;  // ���÷���ʱ��
            while (true) {
                // ��������
                recvSize = recvfrom(socketClient, buffer, BUFFER_SIZE, 0,
                    (SOCKADDR*)&addrServer, &len);
                switch (status) {  // ����״̬��������
                case 0:        // �����ϴ�
                    // ������յ����ݣ�������������Ϊ 100
                    if (recvSize > 0 && buffer[0] == 100) {
                        // �����������Ϊ OK
                        if (!strcmp(buffer + 1, "OK")) {
                            // ��ӡ����ͨ��
                            printf("����ͨ��, ��ʼ�ϴ�...\n");
                            start = clock();           // ��ȡ��ǰʱ��
                            status = 1;                // ����״̬Ϊ 1
                            sendWindow[0].start = 0L;  // ���÷���ʱ��
                            continue;                  // ������ǰѭ��
                        }
                        // �����������ΪNO
                        else if (!strcmp(buffer + 1, "NO")) {
                            status = -1;  // ����״̬Ϊ -1
                            break;        // ����ѭ��
                        }
                    }
                    now = clock();  // ��ȡ��ǰʱ��
                    // �����ǰʱ���ȥ����ʱ����� 1000 ����
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // ���·���ʱ��
                        // ��������
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                    break;
                case 1:  // �����ļ�����
                    // ������յ����ݣ�������������Ϊ 101
                    if (recvSize > 0 && buffer[0] == 101) {
                        start = clock();              // ��ȡ��ǰʱ��
                        ack = buffer[1];              // ��ȡȷ�Ϻ�
                        ack--;                        // ȷ�Ϻż�һ
                        sendWindow[ack].start = -1L;  // ���÷���ʱ��
                        if (ack == seq) {  // ���ȷ�Ϻŵ������к�
                            seq = MoveSendWindow(seq);  // ��������
                        }
                        // ��ӡ��������
                        printf("���� ack = %d, ��ǰ��ʼ seq = %d\n",
                            ack + 1, seq + 1);
                    }
                    // ����ļ��ѷ������
                    if (!Send(infile, seq, socketClient,
                        (SOCKADDR*)&addrServer)) {
                        // ��ӡ�ϴ����
                        printf("�ϴ����...\n");
                        status = 2;                    // ����״̬Ϊ 2
                        start = clock();               // ��ȡ��ǰʱ��
                        sendWindow[0].buffer[0] = 10;  // ���û�����
                        // ���û�����
                        strcpy_s(sendWindow[0].buffer + 1, 7, "Finish");
                        // ���÷���ʱ��
                        sendWindow[0].start = start - 1000L;
                        continue;  // ������ǰѭ��
                    }
                    break;
                case 2:  // �ȴ����ȷ��
                    // ������յ����ݣ�������������Ϊ 100
                    if (recvSize > 0 && buffer[0] == 100) {
                        // �����������Ϊ OK
                        if (!strcmp(buffer + 1, "OK")) {
                            buffer[0] = 10;  // ���û�����
                            strcpy_s(buffer + 1, 3, "OK");  // ���û�����
                            // ��������
                            sendto(socketClient, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrServer,
                                sizeof(SOCKADDR));
                            status = 3;  // ����״̬Ϊ 3
                            break;       // ����ѭ��
                        }
                    }
                    now = clock();  // ��ȡ��ǰʱ��
                    // �����ǰʱ���ȥ����ʱ����� 1000 ����
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // ���·���ʱ��
                        // ��������
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                default:
                    break;
                }
                // ���״̬Ϊ -1
                if (status == -1) {
                    printf("�������ܾ�����\n");
                    infile.close();
                    break;
                }
                // ���״̬Ϊ 3
                if (status == 3) {
                    printf("�ϴ��ɹ�������ͨ��\n");
                    infile.close();
                    break;
                }
                // �����ǰʱ���ȥ��ʼʱ����� 5000 ����
                if (clock() - start >= 5000L) {
                    printf("ͨ�ų�ʱ������ͨ��\n");
                    infile.close();
                    break;
                }
                // ������յ������ݴ�СС�ڵ��� 0
                if (recvSize <= 0) {
                    Sleep(200);
                }
            }
        }
        // �������ʽ -download
        else if (!strcmp(cmd, "download")) {
            // ��ӡ�����ļ�
            printf("���������ļ� %s\n", fileName);
            strcpy_s(filePath, "./");      // �����ļ�·��
            strcat_s(filePath, fileName);  // �����ļ���
            ofstream outfile(filePath);    // ���ļ�
            start = clock();               // ��ȡ��ǰʱ��
            ack = 0;                       // ȷ�Ϻ�
            status = 0;                    // ״̬
            sendWindow[0].buffer[0] = 10;  // ���û�����
            // ���û�����
            strcpy_s(sendWindow[0].buffer + 1, strlen(cmdBuffer) + 1,
                cmdBuffer);
            sendWindow[0].start = start - 1000L;  // ���÷���ʱ��
            while (true) {                        // ѭ����������
                recvSize = recvfrom(socketClient, buffer, BUFFER_SIZE, 0,
                    (SOCKADDR*)&addrServer, &len);
                // ģ�ⶪ��,����������ڶ������򶪰�
                if ((float)rand() / RAND_MAX > LOSS_RATE) {
                    recvSize = 0;   // ���ý������ݴ�СΪ 0
                    buffer[0] = 0;  // ���û�����
                }
                switch (status) {
                case 0:  // ��������
                    // ������յ����ݣ�������������Ϊ 100
                    if (recvSize > 0 && buffer[0] == 100) {
                        // �����������Ϊ OK
                        if (!strcmp(buffer + 1, "OK")) {
                            // ��ӡ����ͨ��
                            printf("����ͨ��, ׼������...\n");
                            start = clock();  // ��ȡ��ǰʱ��
                            status = 1;       // ����״̬Ϊ 1
                            sendWindow[0].buffer[0] = 10;  // ���û�����
                            // ���û�����
                            strcpy_s(sendWindow[0].buffer + 1, 3, "OK");
                            // ���÷���ʱ��
                            sendWindow[0].start = start - 1000L;
                            continue;  // ������ǰѭ��
                        }
                        // �����������Ϊ NO
                        else if (!strcmp(buffer + 1, "NO")) {
                            status = -1;  // ����״̬Ϊ -1
                            break;        // ����ѭ��
                        }
                    }
                    now = clock();  // ��ȡ��ǰʱ��
                    // �����ǰʱ���ȥ����ʱ����� 1000 ����
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // ���·���ʱ��
                        // ��������
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                    break;
                case 1:  // ��ʼ��������
                    // ������յ����ݣ�������������Ϊ 200
                    if (recvSize > 0 && (unsigned char)buffer[0] == 200) {
                        // ��ӡ��ʼ����
                        printf("��ʼ����...\n");
                        start = clock();  // ��ȡ��ǰʱ��
                        seq = buffer[1];  // ��ȡ���к�
                        // �����������
                        printf(
                            "��������֡ seq = %d, data = %s, ����ack = "
                            "%d\n",
                            seq, buffer + 2, seq);
                        seq--;                        // ���кż�һ
                        recvWindow[seq].used = true;  // ����Ϊ��ʹ��
                        // ���û�����
                        strcpy_s(recvWindow[seq].buffer,
                            strlen(buffer + 2) + 1, buffer + 2);
                        if (ack == seq) {  // ���ȷ�Ϻŵ������к�
                            ack = Deliver(file, ack);  // ��������
                            status = 2;                // ����״̬Ϊ 2
                            buffer[0] = 11;            // ���û�����
                            buffer[1] = seq + 1;       // ���û�����
                            buffer[2] = 0;             // ���û�����
                        }
                        sendto(socketClient, buffer, strlen(buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                        continue;  // ������ǰѭ��
                    }
                    now = clock();  // ��ȡ��ǰʱ��
                    // �����ǰʱ���ȥ��ʼʱ����� 5000 ����
                    if (now - sendWindow[0].start >= 1000L) {
                        sendWindow[0].start = now;  // ���·���ʱ��
                        // ��������
                        sendto(socketClient, sendWindow[0].buffer,
                            strlen(sendWindow[0].buffer) + 1, 0,
                            (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                    }
                    break;
                case 2:                  // ��������֡
                    if (recvSize > 0) {  // ������յ�����
                        // �����������Ϊ 200
                        if ((unsigned char)buffer[0] == 200) {
                            seq = buffer[1];           // ��ȡ���к�
                            int temp = seq - 1 - ack;  // �������кŲ�
                            if (temp < 0) {  // ������кŲ�С�� 0
                                temp += SEQ_SIZE;  // ���кŲ�������кŸ���
                            }
                            start = clock();  // ��ȡ��ǰʱ��
                            seq--;            // ���кż�һ
                            // ������кŲ�С�ڽ��մ��ڴ�С
                            if (temp < RWIN_SIZE) {
                                // ������մ����е����ݰ�δʹ��
                                if (!recvWindow[seq].used) {
                                    // ����Ϊ��ʹ��
                                    recvWindow[seq].used = true;
                                    // ���û�����
                                    strcpy_s(recvWindow[seq].buffer,
                                        strlen(buffer + 2) + 1,
                                        buffer + 2);
                                }
                                // ���ȷ�Ϻŵ������к�
                                if (ack == seq) {
                                    ack = Deliver(file, ack);  // ��������
                                }
                            }
                            // �����������
                            printf(
                                "��������֡ seq = %d, data = %s, ���� ack "
                                "= %d, ��ʼ ack = %d\n",
                                seq + 1, buffer + 2, seq + 1, ack + 1);
                            buffer[0] = 11;       // ���û�����
                            buffer[1] = seq + 1;  // ���û�����
                            buffer[2] = 0;        // ���û�����
                            // ��������
                            sendto(socketClient, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrServer,
                                sizeof(SOCKADDR));
                        }
                        // �����������Ϊ 100
                        else if (buffer[0] == 100 &&
                            !strcmp(buffer + 1, "Finish")) {
                            status = 3;  // ����״̬Ϊ 3
                            outfile.write(file, strlen(file));  // д���ļ�
                            buffer[0] = 10;  // ���û�����
                            strcpy_s(buffer + 1, 3, "OK");  // ���û�����
                            // ��������
                            sendto(socketClient, buffer, strlen(buffer) + 1,
                                0, (SOCKADDR*)&addrServer,
                                sizeof(SOCKADDR));
                            continue;  // ������ǰѭ��
                        }
                    }
                    break;  // ����ѭ��
                default:
                    break;
                }
                // ���״̬Ϊ -1
                if (status == -1) {
                    printf("�������ܾ�����\n");
                    outfile.close();
                    break;
                }
                // ���״̬Ϊ 3
                if (status == 3) {
                    printf("���سɹ�, ����ͨ��\n");
                    outfile.close();
                    break;
                }
                // �����ǰʱ���ȥ��ʼʱ����� 5000 ����
                if (clock() - start >= 50000L) {
                    printf("ͨ�ų�ʱ, ����ͨ��\n");
                    outfile.close();
                    break;
                }
                // ������յ������ݴ�СС�ڵ��� 0
                if (recvSize <= 0) {
                    Sleep(20);
                }
            }
        }
        else if (!strcmp(cmd, "quit")) {  // �������ʽ -quit
            break;                          // �˳�ѭ��
        }
    }
    closesocket(socketClient);  // �ر��׽���
    printf("�ر��׽���\n");     // ��ӡ�ر��׽���
    WSACleanup();               // ���� Winsock.dll
    return 0;
}

// ��һ�������ļ����ж�ȡ���ݣ�������ȡ�����ݴ洢��һ���ַ����飨����������
int Read(ifstream& infile, char* buffer) {
    if (infile.eof()) {  // �ж��Ƿ񵽴��ļ�ĩβ
        return 0;
    }
    infile.read(buffer, 3);     // ���ļ����ж�ȡ����
    int cnt = infile.gcount();  // ��ȡ��ȡ���ֽ���
    buffer[cnt] = 0;            // ����ַ���������
    return cnt;                 // ���ض�ȡ���ֽ���
}
// ������ȷ�ϵ����ݰ�������������׷�ӵ�ָ�����ļ��ַ�����
int Deliver(char* file, int ack) {
    while (recvWindow[ack].used) {  // �����մ����е����ݰ��Ѿ���ȷ��ʱ
        recvWindow[ack].used = false;  // �������ݰ����Ϊδʹ��
        // �����ݰ�������׷�ӵ��ļ��ַ�����
        strcat_s(file, strlen(file) + strlen(recvWindow[ack].buffer) + 1,
            recvWindow[ack].buffer);
        ack++;            // ����ȷ�Ϻ�
        ack %= SEQ_SIZE;  // ȷ�Ϻ�ȡģ
    }
    return ack;  // ���ظ��º��ȷ�Ϻ�
}
// �������ļ����ж�ȡ���ݣ���ͨ�����緢������֡
int Send(ifstream& infile, int seq, SOCKET socket, SOCKADDR* addr) {
    clock_t now = clock();  // ��ȡ��ǰʱ��
    // ѭ���������ʹ��ڴ�С�����㵱ǰ���ʹ��ڵ�������ʱ�������кŷ�Χ��ѭ��
    for (int i = 0; i < SWIN_SIZE; i++) {
        int j = (seq + i) % SEQ_SIZE;      // ���㵱ǰ���ʹ��ڵ�����
        if (sendWindow[j].start == -1L) {  // �����ǰ����֡�Ѿ�����
            continue;                      // ������ǰ����֡
        }
        if (sendWindow[j].start == 0L) {  // �����ǰ����֡δ����
            if (Read(infile, sendWindow[j].buffer + 2)) {  // ���ļ����ж�ȡ����
                sendWindow[j].start = now;  // ���µ�ǰ����֡�ķ���ʱ��
                sendWindow[j].buffer[0] = 20;     // ��������֡������
                sendWindow[j].buffer[1] = j + 1;  // ��������֡�����к�
            }
            else if (i == 0) {  // �����ǰ����֡�Ƿ��ʹ��ڵĵ�һ������֡
                return 0;  // ���� 0����ʾ�������
            }
            else {  // �����ǰ����֡���Ƿ��ʹ��ڵĵ�һ������֡
                break;  // ����ѭ��
            }
        }  // �����ǰ����֡���ͳ�ʱ
        else if (now - sendWindow[j].start >= 1000L) {
            sendWindow[j].start = now;  // ���µ�ǰ����֡�ķ���ʱ��
        }
        else {                        // �����ǰ����֡δ���ͳ�ʱ
            continue;                   // ������ǰ����֡
        }
        // ��������֡
        printf("��������֡ seq = %d, data = %s\n", j + 1,
            sendWindow[j].buffer + 2);
        // ��������֡��ָ���ĵ�ַ
        sendto(socket, sendWindow[j].buffer, strlen(sendWindow[j].buffer) + 1,
            0, addr, sizeof(SOCKADDR));
    }
    return 1;
}
// �ƶ����ʹ��ڣ��ҵ���һ�����Է��͵����к�
int MoveSendWindow(int seq) {
    // ѭ����鷢�ʹ����е�ǰ���к� seq ��Ӧ�����ݰ�״̬
    while (sendWindow[seq].start == -1L) {  // ����ǰ���кŵ�����֡�Ѿ�����
        sendWindow[seq].start = 0L;  // ���µ�ǰ���кŵ�����֡�ķ���ʱ��
        seq++;                       // �������к�
        seq %= SEQ_SIZE;             // ���к�ȡģ
    }
    return seq;  // ���ظ��º�����к�
}