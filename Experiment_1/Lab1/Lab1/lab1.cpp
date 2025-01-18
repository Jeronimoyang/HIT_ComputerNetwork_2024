#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#pragma comment(lib, "Ws2_32.lib")
#define MAXSIZE 65507  // 发送数据报文的最大长度
#define HTTP_PORT 80   // http 服务器端口
#define DATELENGTH 50  // 时间字节数
#define CACHE_NUM 50   // 定义最大缓存数量
// Http 重要头部数据
struct HttpHeader {
    char method[4];  // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
    char url[1024];          // 请求的 url
    char host[1024];         // 目标主机
    char cookie[1024 * 10];  // cookie
    HttpHeader() { ZeroMemory(this, sizeof(HttpHeader)); }
};
// 因为不做外部存储，所以为了节省空间，cache存储的时候
// 去掉Http头部信息中的cookie
struct cacheHttpHead {
    char method[4];  // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
    char url[1024];   // 请求的 url
    char host[1024];  // 目标主机
    cacheHttpHead() {
        ZeroMemory(this, sizeof(cacheHttpHead));
    }  // 构造函数，将结构体内存清零，确保初始化为空。
};
// 代理服务器缓存技术
struct CACHE {
    cacheHttpHead httpHead;
    char buffer[MAXSIZE];   // 储存报文返回内容
    char date[DATELENGTH];  // 缓存内容的最后修改时间
    CACHE() {
        ZeroMemory(this->buffer, MAXSIZE);
        ZeroMemory(this->date, DATELENGTH);
    }  // 构造函数，将结构体内存清零，确保初始化为空。
};
CACHE cache[CACHE_NUM];  // 缓存地址
int cache_index = 0;     // 记录当前应该将缓存放在哪个位置

BOOL InitSocket();  // 声明初始化套接字（socket）函数，返回值为布尔类型（成功或失败）。
// 声明HTTP请求头解析函数，将buffer中的内容解析到`HttpHeader`结构体中。
void ParseHttpHead(char* buffer, HttpHeader* httpHeader);
// 声明连接到目标服务器的函数，返回布尔值，指示连接是否成功。
BOOL ConnectToServer(SOCKET* serverSocket, char* host);
// 声明代理服务器线程函数，用于处理客户端请求。线程入口函数采用`__stdcall`调用约定。
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
// 寻找缓存中是否存在，如果存在返回index，不存在返回-1
int isInCache(CACHE* cache, HttpHeader httpHeader);
// 声明一个函数，用于比较两个HTTP报文是否相同，主要用于判断缓存是否匹配请求。
BOOL httpEqual(cacheHttpHead http1, HttpHeader http2);
// 声明一个函数，用于修改HTTP报文中的内容，如时间字段等。
void changeHTTP(char* buffer, char* date);
// 代理相关参数
// 定义代理服务器的套接字（Socket）。
SOCKET ProxyServer;
// 定义代理服务器的地址结构体，用于存储IP地址和端口号信息。
sockaddr_in ProxyServerAddr;
// 定义代理服务器监听的端口号为10240。
const int ProxyPort = 10240;
// 由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
// 可以使用线程池技术提高服务器效率
// const int ProxyThreadMaxNum = 20;
// HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
// DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
// 定义一个结构体，用于传递客户端和服务器的套接字信息。
struct ProxyParam {
    SOCKET clientSocket;  // 客户端套接字。
    SOCKET serverSocket;  // 服务器套接字。
};
// --------------------选做功能参数定义--------------------
bool button = true;  // 取true的时候表示开始运行选做功能
// 禁止访问网站
char* invalid_website[10] = { (char*)"http://www.hit.edu.cn" };
const int invalid_website_num = 1;  // 有多少个禁止网站
// 钓鱼网站
char* fishing_src = (char*)"http://today.hit.edu.cn";  // 钓鱼网站原网址
char* fishing_dest = (char*)"http://jwes.hit.edu.cn";  // 钓鱼网站目标网址
char* fishing_dest_host = (char*)"jwes.hit.edu.cn";  // 钓鱼目的地址主机名
// 限制访问用户
char* restrict_host[10] = { (char*)"127.0.0.0" };
int main(int argc, char* argv[]) {
    printf("代理服务器正在启动\n");  // 输出启动信息。
    printf("初始化...\n");           // 输出初始化信息
    // 调用`InitSocket()`初始化套接字，如果失败则返回错误
    if (!InitSocket()) {
        printf("socket 初始化失败\n");
        return -1;
    }
    // 输出代理服务器运行和端口号信息。
    printf("代理服务器正在运行，监听端口 %d\n", ProxyPort);
    // 定义`acceptSocket`，表示客户端连接的套接字，初始值为无效套接字。
    SOCKET acceptSocket = INVALID_SOCKET;
    // 定义指向`ProxyParam`结构体的指针，用于传递客户端和服务器套接字信息
    ProxyParam* lpProxyParam;
    // 定义线程句柄，用于创建新的线程
    HANDLE hThread;
    // 定义线程ID变量（虽然未使用）
    DWORD dwThreadID;
    // 定义`addr_in`结构体，用于存储客户端的地址信息
    sockaddr_in addr_in;
    // 初始化地址长度变量`addr_len`
    int addr_len = sizeof(SOCKADDR);
    // 代理服务器不断监听
    while (true) {
        // 调用`accept()`函数等待客户端连接，成功后返回客户端的套接字
        acceptSocket = accept(ProxyServer, (SOCKADDR*)&addr_in, &(addr_len));
        // 为`ProxyParam`结构体分配内存
        lpProxyParam = new ProxyParam;
        // 如果分配失败，则跳过本次循环
        if (lpProxyParam == NULL) {
            continue;
        }
        // 受限用户,与列表中匹配上的都无法访问
        if (!strcmp(restrict_host[0], inet_ntoa(addr_in.sin_addr)) &&
            button)  // 注意比较之前将网络二进制的数字转换成网络地址
        {
            printf("该用户访问受限\n");
            continue;
        }
        // 将客户端的套接字存入`lpProxyParam`结构体
        lpProxyParam->clientSocket = acceptSocket;
        // 创建新线程调用`ProxyThread`函数，处理客户端请求
        hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread,
            (LPVOID)lpProxyParam, 0, 0);
        // 关闭线程句柄，避免资源泄露
        CloseHandle(hThread);
        // 休眠200毫秒，以防频繁创建线程
        Sleep(200);
    }
    // 关闭代理服务器的套接字
    closesocket(ProxyServer);
    // 释放Winsock资源
    WSACleanup();
    // 返回0，表示程序正常结束
    return 0;
}
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket() {
    // 加载套接字库（必须）
    // 定义请求的Winsock版本号
    WORD wVersionRequested;
    // 用于存储Winsock初始化信息的数据结构
    WSADATA wsaData;
    // 套接字加载时错误提示
    int err;
    // 版本 2.2
    wVersionRequested = MAKEWORD(2, 2);
    // 加载 dll 文件 Scoket 库
    err = WSAStartup(wVersionRequested, &wsaData);
    // 如果加载失败，输出错误信息并返回`FALSE`
    if (err != 0) {
        // 找不到 winsock.dll
        printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
        return FALSE;
    }
    // if中的语句主要用于比对是否是2.2版本
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("不能找到正确的 winsock 版本\n");
        WSACleanup();
        return FALSE;
    }
    // 创建的socket文件描述符基于IPV4，TCP
    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == ProxyServer) {
        printf("创建套接字失败，错误代码为： %d\n", WSAGetLastError());
        return FALSE;
    }
    // 设置地址族为IPv4
    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(
        ProxyPort);  // 整型变量从主机字节顺序转变成网络字节顺序,转换为大端法
    ProxyServerAddr.sin_addr.S_un.S_addr =
        INADDR_ANY;  // 泛指本机也就是表示本机的所有IP，多网卡的情况下，这个就表示所有网卡ip地址的意思
    // 将套接字与指定的IP地址和端口绑定。如果失败，输出错误信息
    if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) ==
        SOCKET_ERROR) {
        printf("绑定套接字失败\n");
        return FALSE;
    }
    // 启动监听，允许服务器接收来自客户端的连接
    if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
        printf("监听端口%d 失败", ProxyPort);
        return FALSE;
    }
    return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
//************************************
// 该代码实现了一个HTTP 代理服务器线程的核心逻辑，
// 包括缓存机制、钓鱼网站重定向、以及对服务器响应的处理。
// 通过检查 Last-Modified 字段，实现了缓存的更新与使用，从而提高访问效率
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
    // 缓存客户端发来的数据
    char Buffer[MAXSIZE];
    // 用于存储接收到的数据
    char* CacheBuffer;
    // 初始化 Buffer 为 0
    ZeroMemory(Buffer, MAXSIZE);
    // 客户端地址
    SOCKADDR_IN clientAddr;
    // 地址长度
    int length = sizeof(SOCKADDR_IN);
    // 接收到的数据大小
    int recvSize;
    // 发送/接收结果
    int ret;
    // 存储 HTTP 头部信息的结构体
    HttpHeader* httpHeader = new HttpHeader();
    // 用于处理服务器返回的缓存
    char* cacheBuffer2 = NULL;
    // 分隔符
    char* delim = NULL;
    // 存储报文中的日期
    char date[DATELENGTH];
    // 记录拆分字符串后的剩余部分
    char* nextStr;
    // 用于遍历分割后的字符串
    char* p = NULL;
    // 缓存中的索引
    int index = 0;
    // 标记是否找到修改时间
    bool flag;
    recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE,
        0);  // 接收到报文

    if (recvSize <= 0) {
        goto error;
    }
    // 动态分配缓存
    CacheBuffer = new char[recvSize + 1];
    // 初始化
    ZeroMemory(CacheBuffer, recvSize + 1);
    // 拷贝客户端数据到缓存
    memcpy(CacheBuffer, Buffer, recvSize);
    // 处理HTTP头部
    ParseHttpHead(CacheBuffer, httpHeader);
    // 处理禁止访问网站
    if (strstr(httpHeader->url, invalid_website[0]) != NULL && button) {
        printf("\n=====================\n");
        printf("--------该网站已被屏蔽!----------\n");
        goto error;
    }
    // 处理钓鱼网站
    if (strstr(httpHeader->url, fishing_src) != NULL && button) {
        printf("\n=====================\n");
        printf(
            "-------------已从源网址：%s 转到 目的网址 ：%s ----------------\n",
            fishing_src, fishing_dest);
        // 修改HTTP报文
        memcpy(httpHeader->host, fishing_dest_host,
            strlen(fishing_dest_host) + 1);
        memcpy(httpHeader->url, fishing_dest, strlen(fishing_dest));
    }
    delete CacheBuffer;
    // 连接目标主机
    if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket,
        httpHeader->host)) {
        goto error;
    }
    printf("代理连接主机 %s 成功\n", httpHeader->host);

    index = isInCache(cache, *httpHeader);
    // 如果在缓存中存在
    if (index > -1) {
        char* cacheBuffer;
        char Buf[MAXSIZE];
        ZeroMemory(Buf, MAXSIZE);
        memcpy(Buf, Buffer, recvSize);
        // 插入"If-Modified-Since: "
        changeHTTP(Buf, cache[index].date);
        printf("-------------------请求报文------------------------\n%s\n",
            Buf);
        ret = send(((ProxyParam*)lpParameter)->serverSocket, Buf,
            strlen(Buf) + 1, 0);
        recvSize =
            recv(((ProxyParam*)lpParameter)->serverSocket, Buf, MAXSIZE, 0);
        printf("------------------Server返回报文-------------------\n%s\n",
            Buf);
        if (recvSize <= 0) {
            goto error;
        }
        char* No_Modified = (char*)"304";
        // 没有改变，直接返回cache中的内容
        if (!memcmp(&Buf[9], No_Modified, strlen(No_Modified))) {
            ret = send(((ProxyParam*)lpParameter)->clientSocket,
                cache[index].buffer, strlen(cache[index].buffer) + 1, 0);
            printf("将cache中的缓存返回客户端\n");
            printf("============================\n");
            goto error;
        }
    }
    // 将客户端发送的 HTTP 数据报文直接转发给目标服务器
    ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer,
        strlen(Buffer) + 1, 0);
    // 等待目标服务器返回数据
    recvSize =
        recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
    if (recvSize <= 0) {
        goto error;
    }
    // 以下部分将返回报文加入缓存
    // 从服务器返回报文中解析时间
    cacheBuffer2 = new char[MAXSIZE];
    ZeroMemory(cacheBuffer2, MAXSIZE);
    memcpy(cacheBuffer2, Buffer, MAXSIZE);
    delim = (char*)"\r\n";
    ZeroMemory(date, DATELENGTH);
    p = strtok_s(cacheBuffer2, delim, &nextStr);
    flag = false;  // 表示是否含有修改时间报文
    // 不断分行，直到分出具有修改时间的那一行
    while (p) {
        if (p[0] == 'L')  // 找到Last-Modified:那一行
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
        if (index > -1)  // 说明已经有内容存在，只要改一下时间和内容
        {
            memcpy(&(cache[index].buffer), Buffer, strlen(Buffer));
            memcpy(&(cache[index].date), date, strlen(date));
        }
        else  // 第一次访问，需要完全缓存
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
    // 将目标服务器返回的数据直接转发给客户端
    ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer),
        0);
    // 错误处理
error:
    printf("关闭套接字\n");
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
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
// ParseHttpHead 函数从 HTTP 请求头中提取关键字段，
// 如 GET/POST 方法、URL、Host
// 和 Cookie， 并将这些信息存储在 HttpHeader
// 结构体中。代码通过循环逐行解析请求头，
// 针对每个字段进行匹配和处理，确保正确提取信息
void ParseHttpHead(char* buffer, HttpHeader* httpHeader) {
    char* p;
    char* ptr;
    const char* delim = "\r\n";
    p = strtok_s(buffer, delim, &ptr);  // 提取第一行
    printf("%s\n", p);
    if (p[0] == 'G') {  // GET 方式
        memcpy(httpHeader->method, "GET", 3);
        memcpy(httpHeader->url, &p[4], strlen(p) - 13);
    }
    else if (p[0] == 'P') {  // POST 方式
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
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
// 创建 TCP 套接字：使用 IPv4 和 TCP 协议创建套接字。
// 主机名解析：通过 gethostbyname 将主机名解析为 IP 地址。
// 建立连接：使用 connect
// 函数尝试连接到服务器。如果连接失败，则关闭套接字并返回失败状态；如果成功，返回
// TRUE。
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
        inet_addr(inet_ntoa(Inaddr));  // 将一个将网络地址转换成一个长整数型数
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
// 该函数逐一比较 请求方法、主机名 和 URL 是否相同。
// 如果任一字段不同，则立即返回 false；如果全部相同，则返回 true。
// 这是一个简单的比较逻辑，用于判断两个 HTTP 请求是否是同一个请求
BOOL httpEqual(cacheHttpHead http1, HttpHeader http2) {
    if (strcmp(http1.method, http2.method)) return false;
    if (strcmp(http1.host, http2.host)) return false;
    if (strcmp(http1.url, http2.url)) return false;
    return true;
}
// 该函数用于在缓存中查找某个 HTTP 请求头是否存在。
// 遍历逻辑：逐一遍历缓存数组中的每个项，使用 httpEqual 进行比较。
// 返回值：
// 如果找到匹配项，则返回该项的索引；
// 如果未找到，则返回 -1 表示未命中缓存。
// 这段代码实现了一个简单的 缓存查找功能。
int isInCache(CACHE* cache, HttpHeader httpHeader) {
    int index = 0;
    for (; index < CACHE_NUM; index++) {
        if (httpEqual(cache[index].httpHead, httpHeader)) return index;
    }
    return -1;
}
// 该函数的主要作用是在 HTTP 请求报文的 "Host" 字段之前插入 "If-Modified-Since:
// <日期>\r\n"。 逻辑流程： 查找 Host 字段的位置。 将 Host
// 及其后的内容保存到临时缓冲区 temp。 截断原报文，并插入 "If-Modified-Since"
// 字段及日期。 重新拼接 Host 及其后的原始内容。 这种操作常用于实现 HTTP
// 缓存机制，通过 "If-Modified-Since" 字段检查资源是否更新。
void changeHTTP(char* buffer, char* date) {
    // 此函数在HTTP中间插入"If-Modified-Since: "
    const char* strHost = "Host";
    const char* inputStr = "If-Modified-Since: ";
    char temp[MAXSIZE];
    ZeroMemory(temp, MAXSIZE);
    char* pos = strstr(buffer, strHost);  // 找到Host位置
    int i = 0;
    // 将host与之后的部分写入temp
    for (i = 0; i < strlen(pos); i++) {
        temp[i] = pos[i];
    }
    *pos = '\0';
    while (*inputStr != '\0') {  // 插入If-Modified-Since字段
        *pos++ = *inputStr++;
    }
    while (*date != '\0') {
        *pos++ = *date++;
    }
    *pos++ = '\r';
    *pos++ = '\n';
    // 将host之后的字段复制到buffer中
    for (i = 0; i < strlen(temp); i++) {
        *pos++ = temp[i];
    }
}