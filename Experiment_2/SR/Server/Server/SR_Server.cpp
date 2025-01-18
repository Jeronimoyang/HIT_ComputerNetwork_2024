#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#define SERVER_PORT 12340    // �˿ں�
#define SERVER_IP "0.0.0.0"  // IP ��ַ
#define SEQ_SIZE 16          // ���кŸ���
#define SWIN_SIZE 8          // ���ʹ��ڴ�С
#define RWIN_SIZE 8          // ���մ��ڴ�С
#define BUFFER_SIZE 1024     // ��������С
#define LOSS_RATE 0.8        // ������
using namespace std;
// ����ṹ�壬���ڴ洢���մ��ںͷ��ʹ��ڵ�����
struct recv {
    bool used;                 // ������ݰ��Ƿ��Ѿ���ʹ��
    char buffer[BUFFER_SIZE];  // ������
    recv() {                   // ���캯��
        used = false;          // ��ʼ�����ݰ�δ��ʹ��
        ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
    }
} recvWindow[SEQ_SIZE];  // ���մ���
// ����ṹ�壬���ڴ洢���ʹ��ڵ�����
struct send {
    clock_t start;  // ����ʹ�õ���SR�����ÿһ������λ�ö���Ҫ����һ����ʱ��
    char buffer[BUFFER_SIZE];                // ������
    send() {                                 // ���캯��
        start = 0;                           // ��ʼ����ʱ��
        ZeroMemory(buffer, sizeof(buffer));  // ��ʼ��������
    }
} sendWindow[SEQ_SIZE];      // ���ʹ���
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
// ������
int main(int argc, char* argv[]) {
    // �����׽��ֿ�
    WORD wVersionRequested;
    WSADATA wsaData;
    // �汾 2.2
    wVersionRequested = MAKEWORD(2, 2);  // ���� 2.2 �汾�� Winsock ��
    int err = WSAStartup(wVersionRequested, &wsaData);  // ���� Winsock ��
    if (err != 0) {  // ������� Winsock ��ʧ��
        printf("Winsock.dll ����ʧ�ܣ�������: %d\n", err);
        return -1;
    }
    // �ж�����İ汾���Ƿ���ȷ
    if (LOBYTE(wsaData.wVersion) != LOBYTE(wVersionRequested) ||
        HIBYTE(wsaData.wVersion) != HIBYTE(wVersionRequested)) {
        // �������İ汾�Ų���ȷ
        printf("�Ҳ��� %d.%d �汾�� Winsock.dll\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
        WSACleanup();  // ж�� Winsock ��
        return -1;
    }
    else {  // �������İ汾����ȷ
        printf("Winsock %d.%d ���سɹ�\n", LOBYTE(wVersionRequested),
            HIBYTE(wVersionRequested));
    }
    // �����������׽���
    SOCKET socketServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    // ����Ϊ������ģʽ
    int iMode = 1;  // 1 Ϊ��������0 Ϊ����
    // ����Ϊ������ģʽ
    ioctlsocket(socketServer, FIONBIO, (u_long FAR*) & iMode);
    SOCKADDR_IN addrServer;                               // ��������ַ
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);  // ���÷����� IP ��ַ
    addrServer.sin_family = AF_INET;                      // ���õ�ַ��
    addrServer.sin_port = htons(SERVER_PORT);             // ���ö˿ں�
    // �󶨶˿�
    if (err = bind(socketServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR))) {
        err = GetLastError();  // ��ȡ������
        printf("�󶨶˿� %d ʧ�ܣ�������: % d\n", SERVER_PORT, err);
        WSACleanup();  // ж�� Winsock ��
        return -1;
    }
    else {                                      // �󶨶˿ڳɹ�
        printf("�󶨶˿� %d �ɹ�", SERVER_PORT);  // �����ʾ��Ϣ
    }
    SOCKADDR_IN addrClient;  // �ͻ��˵�ַ
    int status = 0;          // ״̬
    clock_t start;           // ��ʼʱ��
    clock_t now;             // ��ǰʱ��
    int seq;                 // ���к�
    int ack;                 // ȷ�Ϻ�
    ofstream outfile;        // ����ļ���
    ifstream infile;         // �����ļ���
    // �������״̬��ע���������Ҫ����������ǽ��տͻ������󣬹������غ�������������
    while (true) {
        // �����������������Կͻ��˵����ݰ�
        recvSize = recvfrom(socketServer, buffer, BUFFER_SIZE, 0,
            ((SOCKADDR*)&addrClient), &len);
        // ģ�ⶪ��
        if ((float)rand() / RAND_MAX > LOSS_RATE) {
            recvSize = 0;
            buffer[0] = 0;
        }
        switch (status) {
        case 0:  // ��������
            // ������յ�����
            if (recvSize > 0 && buffer[0] == 10) {
                char addr[100];                  // ��ַ
                ZeroMemory(addr, sizeof(addr));  // ��ʼ����ַ
                // ��ȡ�ͻ��˵�ַ
                inet_ntop(AF_INET, &addrClient.sin_addr, addr,
                    sizeof(addr));
                // ��ȡ������ļ���
                sscanf_s(buffer + 1, "%s%s", cmd, sizeof(cmd) - 1, fileName,
                    sizeof(fileName) - 1);
                // ���������ϴ�������
                if (strcmp(cmd, "upload") && strcmp(cmd, "download")) {
                    continue;  // ������������
                }
                strcpy_s(filePath, "./");      // �����ļ�·��
                strcat_s(filePath, fileName);  // �����ļ���
                // �����ʾ��Ϣ
                printf("�յ����Կͻ��� %s ������: %s\n", addr, buffer);
                printf("�Ƿ�ͬ�������(Y/N)?");     // �����ʾ��Ϣ
                gets_s(cmdBuffer, 50);              // ��ȡ����
                if (!strcmp(cmdBuffer, "Y")) {      // �������Ϊ Y
                    buffer[0] = 100;                // ��������֡����
                    strcpy_s(buffer + 1, 3, "OK");  // ��������֡����
                    // ����������ϴ�
                    if (!strcmp(cmd, "upload")) {
                        file[0] = 0;             // ��ʼ���ļ��ַ���
                        start = clock();         // ��¼��ʼʱ��
                        ack = 0;                 // ��ʼ��ȷ�Ϻ�
                        status = 1;              // �����ϴ�״̬
                        outfile.open(filePath);  // ���ļ�
                    }
                    // �������������
                    else if (!strcmp(cmd, "download")) {
                        start = clock();        // ��¼��ʼʱ��
                        seq = 0;                // ��ʼ�����к�
                        status = -1;            // ��������״̬
                        infile.open(filePath);  // ���ļ�
                    }
                }
                else {                            // ������벻Ϊ Y
                    buffer[0] = 100;                // ��������֡����
                    strcpy_s(buffer + 1, 3, "NO");  // ��������֡����
                }
                // ��������֡
                sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                    (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            }
            break;
        case 1:  // �ͻ��������ϴ���Ҳ���Ƿ��������ǽ��շ�
            // ������յ�����
            if (recvSize > 0) {
                // ������յ�������֡������ 10
                if (buffer[0] == 10) {
                    // ������յ���FINISH����֡
                    if (!strcmp(buffer + 1, "Finish")) {
                        // �����ʾ��Ϣ
                        printf("�������...\n");
                        start = clock();  // ��¼��ʼʱ��
                        sendWindow[0].start =
                            start - 1000L;  // ���÷��ʹ��ڵĿ�ʼʱ��
                        sendWindow[0].buffer[0] = 100;  // ��������֡����
                        // ��������֡����
                        strcpy_s(sendWindow[0].buffer + 1, 3, "OK");
                        // ���ļ��ַ���д���ļ���
                        outfile.write(file, strlen(file));
                        status = 2;  // ����������״̬
                    }
                    buffer[0] = 100;                // ��������֡����
                    strcpy_s(buffer + 1, 3, "OK");  // ��������֡����
                    // ��������֡
                    sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                }
                // ������յ�������֡������ 20
                else if (buffer[0] == 20) {
                    seq = buffer[1];           // ��ȡ���к�
                    int temp = seq - 1 - ack;  // �������кŲ�
                    if (temp < 0) {            // ������кŲ�С�� 0
                        temp += SEQ_SIZE;  // ���кŲ�������кŸ���
                    }
                    start = clock();  // ��¼��ʼʱ��
                    seq--;            // �������к�
                    // ������кŲ�С�ڽ��մ��ڴ�С
                    if (temp < RWIN_SIZE) {
                        // ������մ����е����ݰ�δ��ʹ��
                        if (!recvWindow[seq].used) {
                            // ������ݰ��ѱ�ʹ��
                            recvWindow[seq].used = true;
                            // �����ݰ������ݴ洢�����մ�����
                            strcpy_s(recvWindow[seq].buffer,
                                strlen(buffer + 2) + 1, buffer + 2);
                        }
                        if (ack == seq) {  // ���ȷ�Ϻŵ������к�
                            ack = Deliver(file, ack);  // �������ݰ�
                        }
                    }
                    // �����ʾ��Ϣ
                    printf(
                        "��������֡ seq = %d, data = %s, ���� ack = %d, "
                        "��ʼ ack = %d\n",
                        seq + 1, buffer + 2, seq + 1, ack + 1);
                    buffer[0] = 101;      // ��������֡����
                    buffer[1] = seq + 1;  // ��������֡���к�
                    buffer[2] = 0;        // ��������֡����
                    // ��������֡
                    sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                }
            }
            break;
        case 2:  // �������
                 // �������ɹ�
            if (recvSize > 0 && buffer[0] == 10 &&
                !strcmp(buffer + 1, "OK")) {
                // �����ʾ��Ϣ
                printf("����ɹ�������ͨ��\n");
                status = 0;       // �����������״̬
                outfile.close();  // �ر��ļ���
            }
            now = clock();  // ��ȡ��ǰʱ��
            // �����ǰʱ���ȥ���ʹ��ڵĿ�ʼʱ����� 1000 ����
            if (now - sendWindow[0].start >= 1000L) {
                sendWindow[0].start = now;  // ���·��ʹ��ڵĿ�ʼʱ��
                // ��������֡
                sendto(socketServer, sendWindow[0].buffer,
                    strlen(sendWindow[0].buffer) + 1, 0,
                    (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            }
            break;  // ����ͨ��
        case -1:  // �ͻ����������أ�Ҳ���Ƿ������˳䵱���ͷ�
            // ������յ�����
            if (recvSize > 0) {
                // ������յ�������֡������ 10
                if (buffer[0] == 10) {
                    // ������յ�������֡������ OK
                    if (!strcmp(buffer + 1, "OK")) {
                        printf("��ʼ����...\n");  // �����ʾ��Ϣ
                        start = clock();          // ��¼��ʼʱ��
                        status = -2;              // ���뷢��״̬
                    }
                    buffer[0] = 100;                // ��������֡����
                    strcpy_s(buffer + 1, 3, "OK");  // ��������֡����
                    // ��������֡
                    sendto(socketServer, buffer, strlen(buffer) + 1, 0,
                        (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                }
            }
            break;
        case -2:  // �������˷�������
            // ������յ�����
            if (recvSize > 0 && buffer[0] == 11) {
                start = clock();              // ��¼��ʼʱ��
                ack = buffer[1];              // ��ȡȷ�Ϻ�
                ack--;                        // ����ȷ�Ϻ�
                sendWindow[ack].start = -1L;  // ��������֡�Ŀ�ʼʱ��
                if (ack == seq) {  // ���ȷ�Ϻŵ������к�
                    seq = MoveSendWindow(seq);  // �ƶ����ʹ���
                }
                // �����ʾ��Ϣ
                printf("���� ack = %d, ��ǰ��ʼ seq = %d\n", ack + 1,
                    seq + 1);
            }
            // ����������
            if (!Send(infile, seq, socketServer, (SOCKADDR*)&addrClient)) {
                printf("�������...\n");        // �����ʾ��Ϣ
                status = -3;                    // �����������״̬
                start = clock();                // ��¼��ʼʱ��
                sendWindow[0].buffer[0] = 100;  // ��������֡����
                // ��������֡����
                strcpy_s(sendWindow[0].buffer + 1, 7, "Finish");
                // ���÷��ʹ��ڵĿ�ʼʱ��
                sendWindow[0].start = start - 1000L;
            }
            break;
        case -3:  // �������
            // �������ɹ�
            if (recvSize > 0 && buffer[0] == 10) {
                // ������յ�������֡������ OK
                if (!strcmp(buffer + 1, "OK")) {
                    printf("����ɹ�������ͨ��\n");  // �����ʾ��Ϣ
                    infile.close();                  // �ر��ļ���
                    status = 0;  // �����������״̬
                    break;       // ����ͨ��
                }
            }
            now = clock();  // ��ȡ��ǰʱ��
            // �����ǰʱ���ȥ���ʹ��ڵĿ�ʼʱ����� 1000 ����
            if (now - sendWindow[0].start >= 1000L) {
                sendWindow[0].start = now;  // ���·��ʹ��ڵĿ�ʼʱ��
                // ��������֡
                sendto(socketServer, sendWindow[0].buffer,
                    strlen(sendWindow[0].buffer) + 1, 0,
                    (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            }
        default:
            break;
        }
        // ���ͨ�ų�ʱ
        if (status != 0 && clock() - start > 50000L) {
            printf("ͨ�ų�ʱ, ����ͨ��\n");
            status = 0;       // �����������״̬
            outfile.close();  // �ر��ļ���
            continue;         // ������������
        }
        // ���û�н��յ�����
        if (recvSize <= 0) {
            Sleep(20);  // ���� 20 ����
        }
    }
    // �ر��׽��֣�ж�ؿ�
    closesocket(socketServer);  // �ر��׽���
    WSACleanup();               // ж�� Winsock ��
    return 0;
}

// ��һ�������ļ����ж�ȡ���ݣ�������ȡ�����ݴ洢��һ���ַ����飨����������
int Read(ifstream& infile, char* buffer) {
    // ���ļ��ж�ȡ��Ҫ���͵�����
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
    // ��������
    clock_t now = clock();
    // ѭ���������ʹ��ڴ�С�����㵱ǰ���ʹ��ڵ�������ʱ�������кŷ�Χ��ѭ��
    for (int i = 0; i < SWIN_SIZE; i++) {
        int j = (seq + i) % SEQ_SIZE;      // ���㵱ǰ���ʹ��ڵ�����
        if (sendWindow[j].start == -1L) {  // ���䳬ʱ������Ҫ
            continue;                      // ������ǰ����֡
        }
        if (sendWindow[j].start == 0L) {  // ��ʼ��ʱ,�����ǰ����֡δ����
            if (Read(infile, sendWindow[j].buffer + 2)) {  // ���ļ����ж�ȡ����
                sendWindow[j].start = now;  // ���µ�ǰ����֡�ķ���ʱ��
                sendWindow[j].buffer[0] = 200;    // ��������֡������
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
        // ͨ�����緢������֡
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