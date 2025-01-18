#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#pragma comment(lib, "Ws2_32.lib")
#define MAXSIZE 65507  // �������ݱ��ĵ���󳤶�
#define HTTP_PORT 80   // http �������˿�
#define DATELENGTH 50  // ʱ���ֽ���
#define CACHE_NUM 50   // ������󻺴�����
// Http ��Ҫͷ������
struct HttpHeader {
    char method[4];  // POST ���� GET��ע����ЩΪ CONNECT����ʵ���ݲ�����
    char url[1024];          // ����� url
    char host[1024];         // Ŀ������
    char cookie[1024 * 10];  // cookie
    HttpHeader() { ZeroMemory(this, sizeof(HttpHeader)); }
};
// ��Ϊ�����ⲿ�洢������Ϊ�˽�ʡ�ռ䣬cache�洢��ʱ��
// ȥ��Httpͷ����Ϣ�е�cookie
struct cacheHttpHead {
    char method[4];  // POST ���� GET��ע����ЩΪ CONNECT����ʵ���ݲ�����
    char url[1024];   // ����� url
    char host[1024];  // Ŀ������
    cacheHttpHead() {
        ZeroMemory(this, sizeof(cacheHttpHead));
    }  // ���캯�������ṹ���ڴ����㣬ȷ����ʼ��Ϊ�ա�
};
// ������������漼��
struct CACHE {
    cacheHttpHead httpHead;
    char buffer[MAXSIZE];   // ���汨�ķ�������
    char date[DATELENGTH];  // �������ݵ�����޸�ʱ��
    CACHE() {
        ZeroMemory(this->buffer, MAXSIZE);
        ZeroMemory(this->date, DATELENGTH);
    }  // ���캯�������ṹ���ڴ����㣬ȷ����ʼ��Ϊ�ա�
};
CACHE cache[CACHE_NUM];  // �����ַ
int cache_index = 0;     // ��¼��ǰӦ�ý���������ĸ�λ��

BOOL InitSocket();  // ������ʼ���׽��֣�socket������������ֵΪ�������ͣ��ɹ���ʧ�ܣ���
// ����HTTP����ͷ������������buffer�е����ݽ�����`HttpHeader`�ṹ���С�
void ParseHttpHead(char* buffer, HttpHeader* httpHeader);
// �������ӵ�Ŀ��������ĺ��������ز���ֵ��ָʾ�����Ƿ�ɹ���
BOOL ConnectToServer(SOCKET* serverSocket, char* host);
// ��������������̺߳��������ڴ���ͻ��������߳���ں�������`__stdcall`����Լ����
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
// Ѱ�һ������Ƿ���ڣ�������ڷ���index�������ڷ���-1
int isInCache(CACHE* cache, HttpHeader httpHeader);
// ����һ�����������ڱȽ�����HTTP�����Ƿ���ͬ����Ҫ�����жϻ����Ƿ�ƥ������
BOOL httpEqual(cacheHttpHead http1, HttpHeader http2);
// ����һ�������������޸�HTTP�����е����ݣ���ʱ���ֶεȡ�
void changeHTTP(char* buffer, char* date);
// ������ز���
// ���������������׽��֣�Socket����
SOCKET ProxyServer;
// �������������ĵ�ַ�ṹ�壬���ڴ洢IP��ַ�Ͷ˿ں���Ϣ��
sockaddr_in ProxyServerAddr;
// �����������������Ķ˿ں�Ϊ10240��
const int ProxyPort = 10240;
// �����µ����Ӷ�ʹ�����߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ
// ����ʹ���̳߳ؼ�����߷�����Ч��
// const int ProxyThreadMaxNum = 20;
// HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
// DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
// ����һ���ṹ�壬���ڴ��ݿͻ��˺ͷ��������׽�����Ϣ��
struct ProxyParam {
    SOCKET clientSocket;  // �ͻ����׽��֡�
    SOCKET serverSocket;  // �������׽��֡�
};
// --------------------ѡ�����ܲ�������--------------------
bool button = true;  // ȡtrue��ʱ���ʾ��ʼ����ѡ������
// ��ֹ������վ
char* invalid_website[10] = { (char*)"http://www.hit.edu.cn" };
const int invalid_website_num = 1;  // �ж��ٸ���ֹ��վ
// ������վ
char* fishing_src = (char*)"http://today.hit.edu.cn";  // ������վԭ��ַ
char* fishing_dest = (char*)"http://jwes.hit.edu.cn";  // ������վĿ����ַ
char* fishing_dest_host = (char*)"jwes.hit.edu.cn";  // ����Ŀ�ĵ�ַ������
// ���Ʒ����û�
char* restrict_host[10] = { (char*)"127.0.0.0" };
int main(int argc, char* argv[]) {
    printf("�����������������\n");  // ���������Ϣ��
    printf("��ʼ��...\n");           // �����ʼ����Ϣ
    // ����`InitSocket()`��ʼ���׽��֣����ʧ���򷵻ش���
    if (!InitSocket()) {
        printf("socket ��ʼ��ʧ��\n");
        return -1;
    }
    // ���������������кͶ˿ں���Ϣ��
    printf("����������������У������˿� %d\n", ProxyPort);
    // ����`acceptSocket`����ʾ�ͻ������ӵ��׽��֣���ʼֵΪ��Ч�׽��֡�
    SOCKET acceptSocket = INVALID_SOCKET;
    // ����ָ��`ProxyParam`�ṹ���ָ�룬���ڴ��ݿͻ��˺ͷ������׽�����Ϣ
    ProxyParam* lpProxyParam;
    // �����߳̾�������ڴ����µ��߳�
    HANDLE hThread;
    // �����߳�ID��������Ȼδʹ�ã�
    DWORD dwThreadID;
    // ����`addr_in`�ṹ�壬���ڴ洢�ͻ��˵ĵ�ַ��Ϣ
    sockaddr_in addr_in;
    // ��ʼ����ַ���ȱ���`addr_len`
    int addr_len = sizeof(SOCKADDR);
    // ������������ϼ���
    while (true) {
        // ����`accept()`�����ȴ��ͻ������ӣ��ɹ��󷵻ؿͻ��˵��׽���
        acceptSocket = accept(ProxyServer, (SOCKADDR*)&addr_in, &(addr_len));
        // Ϊ`ProxyParam`�ṹ������ڴ�
        lpProxyParam = new ProxyParam;
        // �������ʧ�ܣ�����������ѭ��
        if (lpProxyParam == NULL) {
            continue;
        }
        // �����û�,���б���ƥ���ϵĶ��޷�����
        if (!strcmp(restrict_host[0], inet_ntoa(addr_in.sin_addr)) &&
            button)  // ע��Ƚ�֮ǰ����������Ƶ�����ת���������ַ
        {
            printf("���û���������\n");
            continue;
        }
        // ���ͻ��˵��׽��ִ���`lpProxyParam`�ṹ��
        lpProxyParam->clientSocket = acceptSocket;
        // �������̵߳���`ProxyThread`����������ͻ�������
        hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread,
            (LPVOID)lpProxyParam, 0, 0);
        // �ر��߳̾����������Դй¶
        CloseHandle(hThread);
        // ����200���룬�Է�Ƶ�������߳�
        Sleep(200);
    }
    // �رմ�����������׽���
    closesocket(ProxyServer);
    // �ͷ�Winsock��Դ
    WSACleanup();
    // ����0����ʾ������������
    return 0;
}
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: ��ʼ���׽���
//************************************
BOOL InitSocket() {
    // �����׽��ֿ⣨���룩
    // ���������Winsock�汾��
    WORD wVersionRequested;
    // ���ڴ洢Winsock��ʼ����Ϣ�����ݽṹ
    WSADATA wsaData;
    // �׽��ּ���ʱ������ʾ
    int err;
    // �汾 2.2
    wVersionRequested = MAKEWORD(2, 2);
    // ���� dll �ļ� Scoket ��
    err = WSAStartup(wVersionRequested, &wsaData);
    // �������ʧ�ܣ����������Ϣ������`FALSE`
    if (err != 0) {
        // �Ҳ��� winsock.dll
        printf("���� winsock ʧ�ܣ��������Ϊ: %d\n", WSAGetLastError());
        return FALSE;
    }
    // if�е������Ҫ���ڱȶ��Ƿ���2.2�汾
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("�����ҵ���ȷ�� winsock �汾\n");
        WSACleanup();
        return FALSE;
    }
    // ������socket�ļ�����������IPV4��TCP
    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == ProxyServer) {
        printf("�����׽���ʧ�ܣ��������Ϊ�� %d\n", WSAGetLastError());
        return FALSE;
    }
    // ���õ�ַ��ΪIPv4
    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(
        ProxyPort);  // ���ͱ����������ֽ�˳��ת��������ֽ�˳��,ת��Ϊ��˷�
    ProxyServerAddr.sin_addr.S_un.S_addr =
        INADDR_ANY;  // ��ָ����Ҳ���Ǳ�ʾ����������IP��������������£�����ͱ�ʾ��������ip��ַ����˼
    // ���׽�����ָ����IP��ַ�Ͷ˿ڰ󶨡����ʧ�ܣ����������Ϣ
    if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) ==
        SOCKET_ERROR) {
        printf("���׽���ʧ��\n");
        return FALSE;
    }
    // ��������������������������Կͻ��˵�����
    if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
        printf("�����˿�%d ʧ��", ProxyPort);
        return FALSE;
    }
    return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: �߳�ִ�к���
// Parameter: LPVOID lpParameter
//************************************
// �ô���ʵ����һ��HTTP ����������̵߳ĺ����߼���
// ����������ơ�������վ�ض����Լ��Է�������Ӧ�Ĵ���
// ͨ����� Last-Modified �ֶΣ�ʵ���˻���ĸ�����ʹ�ã��Ӷ���߷���Ч��
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
    // ����ͻ��˷���������
    char Buffer[MAXSIZE];
    // ���ڴ洢���յ�������
    char* CacheBuffer;
    // ��ʼ�� Buffer Ϊ 0
    ZeroMemory(Buffer, MAXSIZE);
    // �ͻ��˵�ַ
    SOCKADDR_IN clientAddr;
    // ��ַ����
    int length = sizeof(SOCKADDR_IN);
    // ���յ������ݴ�С
    int recvSize;
    // ����/���ս��
    int ret;
    // �洢 HTTP ͷ����Ϣ�Ľṹ��
    HttpHeader* httpHeader = new HttpHeader();
    // ���ڴ�����������صĻ���
    char* cacheBuffer2 = NULL;
    // �ָ���
    char* delim = NULL;
    // �洢�����е�����
    char date[DATELENGTH];
    // ��¼����ַ������ʣ�ಿ��
    char* nextStr;
    // ���ڱ����ָ����ַ���
    char* p = NULL;
    // �����е�����
    int index = 0;
    // ����Ƿ��ҵ��޸�ʱ��
    bool flag;
    recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE,
        0);  // ���յ�����

    if (recvSize <= 0) {
        goto error;
    }
    // ��̬���仺��
    CacheBuffer = new char[recvSize + 1];
    // ��ʼ��
    ZeroMemory(CacheBuffer, recvSize + 1);
    // �����ͻ������ݵ�����
    memcpy(CacheBuffer, Buffer, recvSize);
    // ����HTTPͷ��
    ParseHttpHead(CacheBuffer, httpHeader);
    // �����ֹ������վ
    if (strstr(httpHeader->url, invalid_website[0]) != NULL && button) {
        printf("\n=====================\n");
        printf("--------����վ�ѱ�����!----------\n");
        goto error;
    }
    // ���������վ
    if (strstr(httpHeader->url, fishing_src) != NULL && button) {
        printf("\n=====================\n");
        printf(
            "-------------�Ѵ�Դ��ַ��%s ת�� Ŀ����ַ ��%s ----------------\n",
            fishing_src, fishing_dest);
        // �޸�HTTP����
        memcpy(httpHeader->host, fishing_dest_host,
            strlen(fishing_dest_host) + 1);
        memcpy(httpHeader->url, fishing_dest, strlen(fishing_dest));
    }
    delete CacheBuffer;
    // ����Ŀ������
    if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket,
        httpHeader->host)) {
        goto error;
    }
    printf("������������ %s �ɹ�\n", httpHeader->host);

    index = isInCache(cache, *httpHeader);
    // ����ڻ����д���
    if (index > -1) {
        char* cacheBuffer;
        char Buf[MAXSIZE];
        ZeroMemory(Buf, MAXSIZE);
        memcpy(Buf, Buffer, recvSize);
        // ����"If-Modified-Since: "
        changeHTTP(Buf, cache[index].date);
        printf("-------------------������------------------------\n%s\n",
            Buf);
        ret = send(((ProxyParam*)lpParameter)->serverSocket, Buf,
            strlen(Buf) + 1, 0);
        recvSize =
            recv(((ProxyParam*)lpParameter)->serverSocket, Buf, MAXSIZE, 0);
        printf("------------------Server���ر���-------------------\n%s\n",
            Buf);
        if (recvSize <= 0) {
            goto error;
        }
        char* No_Modified = (char*)"304";
        // û�иı䣬ֱ�ӷ���cache�е�����
        if (!memcmp(&Buf[9], No_Modified, strlen(No_Modified))) {
            ret = send(((ProxyParam*)lpParameter)->clientSocket,
                cache[index].buffer, strlen(cache[index].buffer) + 1, 0);
            printf("��cache�еĻ��淵�ؿͻ���\n");
            printf("============================\n");
            goto error;
        }
    }
    // ���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
    ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer,
        strlen(Buffer) + 1, 0);
    // �ȴ�Ŀ���������������
    recvSize =
        recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
    if (recvSize <= 0) {
        goto error;
    }
    // ���²��ֽ����ر��ļ��뻺��
    // �ӷ��������ر����н���ʱ��
    cacheBuffer2 = new char[MAXSIZE];
    ZeroMemory(cacheBuffer2, MAXSIZE);
    memcpy(cacheBuffer2, Buffer, MAXSIZE);
    delim = (char*)"\r\n";
    ZeroMemory(date, DATELENGTH);
    p = strtok_s(cacheBuffer2, delim, &nextStr);
    flag = false;  // ��ʾ�Ƿ����޸�ʱ�䱨��
    // ���Ϸ��У�ֱ���ֳ������޸�ʱ�����һ��
    while (p) {
        if (p[0] == 'L')  // �ҵ�Last-Modified:��һ��
        {
            if (strlen(p) > 15) {
                char header[15];
                ZeroMemory(header, sizeof(header));
                memcpy(header, p, 14);
                if (!(strcmp(header, "Last-Modified:"))) {
                    memcpy(date, &p[15], strlen(p) - 15);
                    flag = true;
                    break;
                }
            }
        }
        p = strtok_s(NULL, delim, &nextStr);
    }
    if (flag) {
        if (index > -1)  // ˵���Ѿ������ݴ��ڣ�ֻҪ��һ��ʱ�������
        {
            memcpy(&(cache[index].buffer), Buffer, strlen(Buffer));
            memcpy(&(cache[index].date), date, strlen(date));
        }
        else  // ��һ�η��ʣ���Ҫ��ȫ����
        {
            memcpy(&(cache[cache_index % CACHE_NUM].httpHead.host),
                httpHeader->host, strlen(httpHeader->host));
            memcpy(&(cache[cache_index % CACHE_NUM].httpHead.method),
                httpHeader->method, strlen(httpHeader->method));
            memcpy(&(cache[cache_index % CACHE_NUM].httpHead.url),
                httpHeader->url, strlen(httpHeader->url));
            memcpy(&(cache[cache_index % CACHE_NUM].buffer), Buffer,
                strlen(Buffer));
            memcpy(&(cache[cache_index % CACHE_NUM].date), date, strlen(date));
            cache_index++;
        }
    }
    // ��Ŀ����������ص�����ֱ��ת�����ͻ���
    ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer),
        0);
    // ������
error:
    printf("�ر��׽���\n");
    Sleep(200);
    closesocket(((ProxyParam*)lpParameter)->clientSocket);
    closesocket(((ProxyParam*)lpParameter)->serverSocket);
    delete lpParameter;
    _endthreadex(0);
    return 0;
}
//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: ���� TCP �����е� HTTP ͷ��
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
// ParseHttpHead ������ HTTP ����ͷ����ȡ�ؼ��ֶΣ�
// �� GET/POST ������URL��Host
// �� Cookie�� ������Щ��Ϣ�洢�� HttpHeader
// �ṹ���С�����ͨ��ѭ�����н�������ͷ��
// ���ÿ���ֶν���ƥ��ʹ���ȷ����ȷ��ȡ��Ϣ
void ParseHttpHead(char* buffer, HttpHeader* httpHeader) {
    char* p;
    char* ptr;
    const char* delim = "\r\n";
    p = strtok_s(buffer, delim, &ptr);  // ��ȡ��һ��
    printf("%s\n", p);
    if (p[0] == 'G') {  // GET ��ʽ
        memcpy(httpHeader->method, "GET", 3);
        memcpy(httpHeader->url, &p[4], strlen(p) - 13);
    }
    else if (p[0] == 'P') {  // POST ��ʽ
        memcpy(httpHeader->method, "POST", 4);
        memcpy(httpHeader->url, &p[5], strlen(p) - 14);
    }
    printf("%s\n", httpHeader->url);
    p = strtok_s(NULL, delim, &ptr);
    while (p) {
        switch (p[0]) {
        case 'H':  // Host
            memcpy(httpHeader->host, &p[6], strlen(p) - 6);
            break;
        case 'C':  // Cookie
            if (strlen(p) > 8) {
                char header[8];
                ZeroMemory(header, sizeof(header));
                memcpy(header, p, 6);
                if (!strcmp(header, "Cookie")) {
                    memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
                }
            }
            break;
        default:
            break;
        }
        p = strtok_s(NULL, delim, &ptr);
    }
}
//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: ������������Ŀ��������׽��֣�������
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
// ���� TCP �׽��֣�ʹ�� IPv4 �� TCP Э�鴴���׽��֡�
// ������������ͨ�� gethostbyname ������������Ϊ IP ��ַ��
// �������ӣ�ʹ�� connect
// �����������ӵ����������������ʧ�ܣ���ر��׽��ֲ�����ʧ��״̬������ɹ�������
// TRUE��
BOOL ConnectToServer(SOCKET* serverSocket, char* host) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT);
    HOSTENT* hostent = gethostbyname(host);
    if (!hostent) {
        return FALSE;
    }
    in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
    serverAddr.sin_addr.s_addr =
        inet_addr(inet_ntoa(Inaddr));  // ��һ���������ַת����һ������������
    *serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (*serverSocket == INVALID_SOCKET) {
        return FALSE;
    }
    if (connect(*serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) ==
        SOCKET_ERROR) {
        closesocket(*serverSocket);
        return FALSE;
    }
    return TRUE;
}
// �ú�����һ�Ƚ� ���󷽷��������� �� URL �Ƿ���ͬ��
// �����һ�ֶβ�ͬ������������ false�����ȫ����ͬ���򷵻� true��
// ����һ���򵥵ıȽ��߼��������ж����� HTTP �����Ƿ���ͬһ������
BOOL httpEqual(cacheHttpHead http1, HttpHeader http2) {
    if (strcmp(http1.method, http2.method)) return false;
    if (strcmp(http1.host, http2.host)) return false;
    if (strcmp(http1.url, http2.url)) return false;
    return true;
}
// �ú��������ڻ����в���ĳ�� HTTP ����ͷ�Ƿ���ڡ�
// �����߼�����һ�������������е�ÿ���ʹ�� httpEqual ���бȽϡ�
// ����ֵ��
// ����ҵ�ƥ����򷵻ظ����������
// ���δ�ҵ����򷵻� -1 ��ʾδ���л��档
// ��δ���ʵ����һ���򵥵� ������ҹ��ܡ�
int isInCache(CACHE* cache, HttpHeader httpHeader) {
    int index = 0;
    for (; index < CACHE_NUM; index++) {
        if (httpEqual(cache[index].httpHead, httpHeader)) return index;
    }
    return -1;
}
// �ú�������Ҫ�������� HTTP �����ĵ� "Host" �ֶ�֮ǰ���� "If-Modified-Since:
// <����>\r\n"�� �߼����̣� ���� Host �ֶε�λ�á� �� Host
// ���������ݱ��浽��ʱ������ temp�� �ض�ԭ���ģ������� "If-Modified-Since"
// �ֶμ����ڡ� ����ƴ�� Host ������ԭʼ���ݡ� ���ֲ���������ʵ�� HTTP
// ������ƣ�ͨ�� "If-Modified-Since" �ֶμ����Դ�Ƿ���¡�
void changeHTTP(char* buffer, char* date) {
    // �˺�����HTTP�м����"If-Modified-Since: "
    const char* strHost = "Host";
    const char* inputStr = "If-Modified-Since: ";
    char temp[MAXSIZE];
    ZeroMemory(temp, MAXSIZE);
    char* pos = strstr(buffer, strHost);  // �ҵ�Hostλ��
    int i = 0;
    // ��host��֮��Ĳ���д��temp
    for (i = 0; i < strlen(pos); i++) {
        temp[i] = pos[i];
    }
    *pos = '\0';
    while (*inputStr != '\0') {  // ����If-Modified-Since�ֶ�
        *pos++ = *inputStr++;
    }
    while (*date != '\0') {
        *pos++ = *date++;
    }
    *pos++ = '\r';
    *pos++ = '\n';
    // ��host֮����ֶθ��Ƶ�buffer��
    for (i = 0; i < strlen(temp); i++) {
        *pos++ = temp[i];
    }
}