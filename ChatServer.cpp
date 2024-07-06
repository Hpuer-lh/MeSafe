#include <stdio.h>
#include <winsock.h>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <time.h>
#include <string.h>
#include <openssl/sha.h>
#include <cstddef> 
#include <cstring> 
#pragma comment(lib, "ws2_32.lib") 

#define PORT 10000
#define IP "your IP address"
#define PC_number 20
#define word_size 1024
#define SQL_SERVER "your SQL SERVE"
#define SQL_DATABASE "your SQL DATABASE"
#define MAX_FILE_SIZE 10485760 //规定传输文件大小最大为10MB
#define FILE_PATH "your File Path"//文件存储路径
const char ENCRYPTION_KEY = 'X';// 定义密钥

struct UserInfo
{   //用户信息结构体
    char  UserID[20];
    char Username[255];
    int  Order;
    union
    {
        char messages[100];
        struct
        {
            char Password[255];
            bool OnlineStatus;
        };
        struct
        {
            char Usernames[PC_number][255];
            char UserIDs[PC_number][20];
        };
    };
    time_t lastHeartbeat; // 记录最后心跳时间
};

SOCKET clientSocket[PC_number];
SOCKET serverSocket;
UserInfo sbuffer[PC_number];

void sha256_hash_string(unsigned char hash[SHA256_DIGEST_LENGTH], char outputBuffer[65])
{   //将哈希值转换为字符串
    int i = 0;
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

void sha256(const char* string, char outputBuffer[65])
{   //SHA256哈希加密
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(hash, &sha256);
    sha256_hash_string(hash, outputBuffer);
}

struct SQLConnection
{   //数据库链接结构体
    SQLHENV hEnv;
    SQLHDBC hDbc;
};

SQLConnection InitializeDatabaseConnection()
{   //创建数据库链接
    SQLConnection sqlConnection;
    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLRETURN retcode;
    // 分配环境句柄
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    // 设置环境属性
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    // 分配连接句柄
    SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);

    // 构建连接字符串
    wchar_t connectionString[1024];
    swprintf(connectionString, 1024, L"Driver={SQL Server};Server=%S;Database=%S;Trusted_Connection=yes;", SQL_SERVER, SQL_DATABASE);

    // 连接到数据源
    retcode = SQLDriverConnectW(hDbc, NULL, connectionString, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        return sqlConnection = { hEnv, hDbc };
    }
    else
    {
        // 释放连接句柄
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        // 释放环境句柄
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return { NULL };
    }
}

void CloseDatabaseConnection(SQLHDBC hDbc, SQLHENV hEnv, SQLHSTMT hStmt)
{   //关闭数据库链接
    if (hStmt != NULL) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);// 释放语句句柄
    }
    if (hDbc != NULL) {
        SQLDisconnect(hDbc); // 断开连接
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc); // 释放连接句柄
    }
    if (hEnv != NULL) {
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv); // 释放环境句柄
    }
}

bool InsertData(UserInfo buffer)
{   //插入数据至数据库
    // 对用户ID、用户名和密码进行SHA256哈希加密
    char hashedUserID[65] = { 0 };
    char hashedPassword[65] = { 0 };
    sha256(buffer.UserID, hashedUserID);
    sha256(buffer.Password, hashedPassword);

    // 将加密后的哈希值转换为宽字符
    wchar_t whashedUserID[65] = { 0 };
    wchar_t whashedUsername[255] = { 0 };
    wchar_t whashedPassword[65] = { 0 };
    mbstowcs(whashedUserID, hashedUserID, 65);
    mbstowcs(whashedUsername, buffer.Username, 255);
    mbstowcs(whashedPassword, hashedPassword, 65);

    SQLConnection sqlConnection = InitializeDatabaseConnection();
    SQLHDBC hDbc = sqlConnection.hDbc;
    SQLHENV hEnv = sqlConnection.hEnv;
    SQLHSTMT hStmt = NULL;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    // 准备SQL插入语句
    SQLWCHAR sqlInsert[] = L"INSERT INTO UserInfo (UserID, Username, Password, OnlineStatus) VALUES (?, ?, ?, ?)";
    SQLPrepareW(hStmt, sqlInsert, SQL_NTS);

    // 绑定参数
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedUserID, 0, NULL);
    SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedUsername, 0, NULL);
    SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedPassword, 0, NULL);
    SQLBindParameter(hStmt, 4, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &buffer.OnlineStatus, 0, NULL);

    SQLRETURN retcode = SQLExecute(hStmt);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        CloseDatabaseConnection(hDbc, hEnv, hStmt);
        return true;
    }
    else
    {
        CloseDatabaseConnection(hDbc, hEnv, hStmt);
        return false;
    }
}

UserInfo QueryData(UserInfo buffer)
{   //查询数据至数据库
    // 对用户ID、用户名和密码进行SHA256哈希加密
    char hashedUserID[65] = { 0 };
    sha256(buffer.UserID, hashedUserID);

    // 将加密后的哈希值转换为宽字符
    wchar_t whashedUserID[65] = { 0 };
    mbstowcs(whashedUserID, hashedUserID, 65);

    SQLConnection sqlConnection = InitializeDatabaseConnection();
    SQLHDBC hDbc = sqlConnection.hDbc;
    SQLHENV hEnv = sqlConnection.hEnv;
    SQLHSTMT hStmt = NULL;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    // 准备SQL查询语句
    SQLWCHAR sqlQuery[] = L"SELECT UserID, Username, Password, OnlineStatus FROM UserInfo WHERE UserID = ?";
    SQLPrepareW(hStmt, sqlQuery, SQL_NTS);

    // 绑定参数
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedUserID, 0, NULL);

    SQLRETURN retcode = SQLExecute(hStmt);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        SQLWCHAR UserID[255];
        SQLWCHAR Username[255];
        SQLWCHAR Password[255];
        SQLCHAR OnlineStatus;
        SQLLEN cbUserID = SQL_NULL_DATA, cbUsername = 0, cbPassword = 0, cbOnlineStatus = 0;

        // 绑定列到变量
        SQLBindCol(hStmt, 1, SQL_C_WCHAR, UserID, sizeof(UserID), &cbUserID);
        SQLBindCol(hStmt, 2, SQL_C_WCHAR, Username, sizeof(Username), &cbUsername);
        SQLBindCol(hStmt, 3, SQL_C_WCHAR, Password, sizeof(Password), &cbPassword);
        SQLBindCol(hStmt, 4, SQL_C_BIT, &OnlineStatus, sizeof(OnlineStatus), &cbOnlineStatus);

        if (SQLFetch(hStmt) == SQL_SUCCESS)
        {
            UserInfo queriedUser;
            // 将查询结果复制到结构体变量中
            wcstombs(queriedUser.UserID, UserID, sizeof(queriedUser.UserID));
            wcstombs(queriedUser.Username, Username, sizeof(queriedUser.Username));
            wcstombs(queriedUser.Password, Password, sizeof(queriedUser.Password));
            queriedUser.OnlineStatus = OnlineStatus;
            queriedUser.Order = 0;
            CloseDatabaseConnection(hDbc, hEnv, hStmt);
            return queriedUser;
        }
        else
        {
            CloseDatabaseConnection(hDbc, hEnv, hStmt);
            return { 0 };
        }
    }
    else
    {
        CloseDatabaseConnection(hDbc, hEnv, hStmt);
        return { 0 };
    }
}

bool ModifyData(UserInfo buffer)
{   //修改数据至数据库
    // 对用户ID、用户名和密码进行SHA256哈希加密

    char hashedUserID[65] = { 0 };
    char hashedPassword[65] = { 0 };
    sha256(buffer.UserID, hashedUserID);
    sha256(buffer.Password, hashedPassword);

    // 将加密后的哈希值转换为宽字符
    wchar_t whashedUserID[65] = { 0 };
    wchar_t whashedUsername[255] = { 0 };
    wchar_t whashedPassword[65] = { 0 };
    mbstowcs(whashedUserID, hashedUserID, 65);
    mbstowcs(whashedUsername, buffer.Username, 65);
    mbstowcs(whashedPassword, hashedPassword, 65);

    // 初始化数据库连接
    SQLConnection sqlConnection;
    sqlConnection = InitializeDatabaseConnection();
    SQLHDBC hDbc = sqlConnection.hDbc;
    SQLHENV hEnv = sqlConnection.hEnv;
    SQLHSTMT hStmt = NULL;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    // 准备SQL更新语句，用于更新用户名、密码、在线状态和Socket
    SQLWCHAR sqlUpdate[] = L"UPDATE UserInfo SET Username = ?, Password = ?, OnlineStatus = ?, Socket = ? WHERE UserID = ?";
    SQLPrepareW(hStmt, sqlUpdate, SQL_NTS);

    // 绑定参数到更新语句
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedUsername, 0, NULL);
    SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedPassword, 0, NULL);
    SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &buffer.OnlineStatus, 0, NULL);
    SQLBindParameter(hStmt, 4, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &buffer.Order, 0, NULL); // 使用Order作为Socket的值
    SQLBindParameter(hStmt, 5, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedUserID, 0, NULL);

    // 执行SQL更新语句
    SQLRETURN retcode = SQLExecute(hStmt);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        CloseDatabaseConnection(hDbc, hEnv, hStmt);
        return true;
    }
    else
    {
        CloseDatabaseConnection(hDbc, hEnv, hStmt);
        return false;
    }
}

bool DeleteData(UserInfo buffer)
{   //删除数据至数据库
    char hashedUserID[65] = { 0 };
    sha256(buffer.UserID, hashedUserID); // 对用户ID进行SHA256哈希加密

    // 将加密后的哈希值转换为宽字符
    wchar_t whashedUserID[65] = { 0 };
    mbstowcs(whashedUserID, hashedUserID, 65);

    SQLConnection sqlConnection = InitializeDatabaseConnection();
    SQLHDBC hDbc = sqlConnection.hDbc;
    SQLHENV hEnv = sqlConnection.hEnv;
    SQLHSTMT hStmt = NULL;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    SQLWCHAR sqlDelete[] = L"DELETE FROM UserInfo WHERE UserID = ?";
    SQLPrepareW(hStmt, sqlDelete, SQL_NTS);

    // 绑定参数
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, 255, 0, whashedUserID, 0, NULL);

    SQLRETURN retcode = SQLExecute(hStmt);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        CloseDatabaseConnection(hDbc, hEnv, hStmt);
        return true;
    }
    else
    {
        CloseDatabaseConnection(hDbc, hEnv, hStmt);
        return false;
    }
}

void GenerateID(int number, char* idBuffer, int bufferSize)
{   // 随机生成ID
    // 获取当前时间的时间戳
    time_t now = time(NULL);
    // 将时间戳和给定的数结合
    snprintf(idBuffer, bufferSize, "%ld%d", now, number);
}

void xorEncryptDecrypt(UserInfo* userInfo, size_t size)
{   //异或加密解密
    char* data = reinterpret_cast<char*>(userInfo); // 将结构体指针转换为char指针
    for (size_t i = 0; i < size; ++i)
    {
        data[i] ^= ENCRYPTION_KEY; // 对每个字节进行异或操作
    }
}

SOCKET InitializeSocket(const char* ip, int port)
{
    // 创建socket链接
    //C/S通信
    // 确定协议版本信息
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    // 创建socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // 确定服务器协议地址簇
    SOCKADDR_IN sAddr = { 0 };
    sAddr.sin_family = AF_INET;//socket地址簇
    sAddr.sin_addr.S_un.S_addr = inet_addr(ip);//服务器IP地址
    sAddr.sin_port = htons(port);//服务器端口号 65536
    // 绑定
    int ret = bind(serverSocket, (sockaddr*)&sAddr, sizeof(sAddr));
    // 监听
    ret = listen(serverSocket, 10);

    return serverSocket;
}

DWORD WINAPI CheckHeartbeat(LPVOID lpParam) 
{
    Sleep(60000);
    while (true)
    {
        time_t now;
        time(&now);
        bool allInvalid = true; // 假设所有客户端都是INVALID_SOCKET
        for (int i = 0; i < PC_number; i++)
        {
            if (clientSocket[i] != INVALID_SOCKET)
            {
                allInvalid = false; // 如果找到一个有效的socket，标记为false
                if (difftime(now, sbuffer[i].lastHeartbeat) > 200)
                {
                    sbuffer[i].OnlineStatus = false;
                    sbuffer[i].Order = -1;
                    ModifyData(sbuffer[i]);
                    closesocket(clientSocket[i]);
                    printf("\n客户端%d已断开连接！\n", i);
                    clientSocket[i] = INVALID_SOCKET;
                }
            }
        }
        if (allInvalid)
        {
            // 如果所有客户端都是INVALID_SOCKET，关闭服务器
            printf("所有客户端均已断开连接，服务器即将关闭。\n");
            Sleep(3000);
            closesocket(serverSocket);
            WSACleanup(); 
            exit(0); 
        }
        Sleep(10000); 
    }
    return 0;
}

void deletedata(int idx, UserInfo buffer)
{
	if (DeleteData(buffer))
	{
		buffer.Order = 0;
		sbuffer[idx].OnlineStatus = false;
		sbuffer[idx].Order = -1;
	}
	else
	{
		buffer.Order = 7;
	}
	xorEncryptDecrypt(&buffer, sizeof(buffer));
	send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
}

void SendFile(int idx, UserInfo buffer)
{
    char fileCode[20];
    char filePath[1024];
    strcpy(filePath, FILE_PATH);
    recv(clientSocket[idx], fileCode, sizeof(fileCode), 0);
    sprintf(filePath + strlen(filePath), "%s.txt", fileCode);
    FILE* file = fopen(filePath, "rb");
    if (file == NULL)
    {
        buffer.Order = 0;
        xorEncryptDecrypt(&buffer, sizeof(buffer));
        send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
        return;
    }
    buffer.Order = 6;
    xorEncryptDecrypt(&buffer, sizeof(buffer));
    send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (fileSize > MAX_FILE_SIZE)
    {
        fclose(file); 
        buffer.Order = 0;
    }
    // 发送文件大小
    send(clientSocket[idx], (char*)&fileSize, sizeof(fileSize), 0);
    char sbuffer[1024];
    while (!feof(file)) {
        int bytesRead = fread(sbuffer, 1, sizeof(buffer), file);
        send(clientSocket[idx], sbuffer, bytesRead, 0);
    }
    fclose(file);
    buffer.Order = 6;
    xorEncryptDecrypt(&buffer, sizeof(buffer));
    send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
}

void RecvFile(int idx, UserInfo buffer)
{
    char filePath[1024];
    strcpy(filePath, FILE_PATH);
    GenerateID(idx, buffer.UserID, sizeof(buffer.UserID));
    sprintf(filePath + strlen(filePath), "%s.txt", buffer.UserID);
    char fileCode[20];
    strcpy(fileCode, buffer.UserID);
    FILE* file = fopen(filePath, "wb");
    if (file == NULL)
    {
        buffer.Order = 0;
    }
    long fileSize;
    recv(clientSocket[idx], (char*)&fileSize, sizeof(fileSize), 0);
    char sbuffer[1024];
    long bytesReceived = 0;
    while (bytesReceived < fileSize)
    {
        int bytesRead = recv(clientSocket[idx], sbuffer, sizeof(sbuffer), 0);
        fwrite(sbuffer, 1, bytesRead, file);
        bytesReceived += bytesRead;
    }
    fclose(file);
    buffer.Order = 5;
    strcpy(buffer.UserID, fileCode);
    xorEncryptDecrypt(&buffer, sizeof(buffer));
    send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
}

void sign_in(int idx, UserInfo buffer)
{   //处理注册操作
    GenerateID(idx, buffer.UserID, sizeof(buffer.UserID));//生成ID
    buffer.OnlineStatus = false;
    if(InsertData(buffer))
	{
        buffer.Order = idx+1;
	}
	else
	{
        buffer.Order = 0;
	}
    xorEncryptDecrypt(&buffer, sizeof(buffer));
	send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
}

void log_in(int idx, UserInfo buffer)
{   //处理登录操作
    const int maxRetryCount = 2; // 最大重试次数
    bool loginSuccess = false; // 登录成功标志
    for (int retryCount = 0; retryCount < maxRetryCount; ++retryCount)
    {
        UserInfo queriedUser = QueryData(buffer);
        char HashPassword[65];
        sha256(buffer.Password, HashPassword);
        if (strcmp(queriedUser.Password, HashPassword) == 0)
        {
            buffer.OnlineStatus = true;
            buffer.Order = idx;
            strcpy(buffer.Username, queriedUser.Username);
            if (ModifyData(buffer))
            {
                loginSuccess = true;
                buffer.Order = 2;            
                sbuffer[idx] = buffer;
                break; // 登录成功，退出循环
            }
            else
            {
                Sleep(6000); // 重试前等待6秒
            }
        }
    }
    if (!loginSuccess)
    {
        // 所有重试尝试后仍未成功登录
        buffer.Order = 0;
    }
    xorEncryptDecrypt(&buffer, sizeof(buffer));
    send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
}

void modify(int idx, UserInfo buffer)
{   //处理修改操作
    strcpy(buffer.UserID, sbuffer[idx].UserID);
	if (!ModifyData(buffer))
	{
        buffer.Order = 0;
	}
	else
	{
		sbuffer[idx] = buffer;
	}
    xorEncryptDecrypt(&buffer, sizeof(buffer));
    send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
}

void GS_chat(int idx, UserInfo buffer)
{
    // 初始化用户信息
    buffer.Order = 0;
    int k = 0; // 用于记录有效用户数量
    int flag[PC_number]; // 用于记录有效用户的索引

    // 遍历所有用户，收集在线用户信息
    for (int i = 0; i < PC_number; i++)
    {
        if (i != idx && sbuffer[i].OnlineStatus) // 检查用户是否在线
        {
            strncpy(buffer.UserIDs[k], sbuffer[i].UserID, 19);
            buffer.UserIDs[k][19] = '\0';
            strncpy(buffer.Usernames[k], sbuffer[i].Username, 19);
            buffer.Usernames[k][254] = '\0';
            flag[k] = i; // 记录有效用户的索引
            k++;
        }
    }
    buffer.Order = k; // 更新有效用户数量
    xorEncryptDecrypt(&buffer, sizeof(buffer));
    send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);

    recv(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
    xorEncryptDecrypt(&buffer, sizeof(buffer));

    if (buffer.Order == k)
    {
        while(1)
        {
            memset(&buffer, 0, sizeof(buffer));
            int ret = recv(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
            xorEncryptDecrypt(&buffer, sizeof(buffer));
            if (buffer.Order == 0)
            {
                break;
            }
            if (ret > 0)
            {
                buffer.messages[ret] = '\0';
                for (int i = 0; i < PC_number; i++)
                {
                    if (sbuffer[i].OnlineStatus)
                    {
                        xorEncryptDecrypt(&buffer, sizeof(buffer));
                        send(clientSocket[i], (char*)&buffer, sizeof(buffer), 0);
                        xorEncryptDecrypt(&buffer, sizeof(buffer));
                    }
                }
            }
        }
        xorEncryptDecrypt(&buffer, sizeof(buffer));
        send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
    }
    else if(buffer.Order == -1)
	{
		return;
	}
    else if(buffer.Order>-1&&buffer.Order < k)
    {
        int cache = flag[buffer.Order];
        while (1)
        {
            memset(&buffer, 0, sizeof(buffer));
            int ret = recv(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
            xorEncryptDecrypt(&buffer, sizeof(buffer));
            if (buffer.Order == 0)
            {
                break;
            }
            if (ret > 0)
            {
                buffer.messages[ret] = '\0';
                for (int i = 0; i < PC_number; i++)
                {
                    if (i == idx || i == cache)
                    {
                        if (!sbuffer[i].OnlineStatus)
                        {
                            buffer.Order = 0;
                            xorEncryptDecrypt(&buffer, sizeof(buffer));
                            send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
                            return;
                        }
                        xorEncryptDecrypt(&buffer, sizeof(buffer));
                        send(clientSocket[i], (char*)&buffer, sizeof(buffer), 0);
                        xorEncryptDecrypt(&buffer, sizeof(buffer));
                    }
                }
            }
        }
        xorEncryptDecrypt(&buffer, sizeof(buffer));
        send(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
    }
}

void Chat(int idx)
{   //处理客户端请求
    UserInfo buffer;
    buffer.Order = -1;
    printf("等待客户端%d发送消息...\n", idx);
    while (1)
    {
        int ret = recv(clientSocket[idx], (char*)&buffer, sizeof(buffer), 0);
        xorEncryptDecrypt(&buffer, sizeof(buffer));
        if (ret <= 0)
		{
            sbuffer[idx].OnlineStatus = false;
            sbuffer[idx].Order = -1;
            ModifyData(sbuffer[idx]);
            closesocket(clientSocket[idx]);
            printf("客户端%d已断开连接！\n", idx);
            break;
		}
        if(buffer.Order == 0)
		{
            sbuffer[idx].OnlineStatus = false;
            sbuffer[idx].Order = -1;
            ModifyData(sbuffer[idx]);
            closesocket(clientSocket[idx]);
            printf("客户端%d已断开连接！\n", idx);
            break;
		}
        else if (buffer.Order == 1)
        {
            printf("客户端%d发送注册请求...\n", idx);
            sign_in(idx, buffer);
            printf("客户端%d结束注册请求！\n", idx);
        }
        else if (buffer.Order == 2)
        {
            printf("客户端%d发送登录请求...\n", idx);
            log_in(idx, buffer);
            printf("客户端%d结束登录请求！\n", idx);
        }
        else if (buffer.Order == 3)
        {
            printf("客户端%d发送修改请求...\n", idx);
            modify(idx, buffer);
            printf("客户端%d结束修改请求！\n", idx);
        }
        else if (buffer.Order == 4)
        {
            printf("客户端%d发送聊天请求...\n", idx);
            GS_chat(idx, buffer);
            printf("客户端%d结束聊天请求！\n", idx);
        }
        else if (buffer.Order == 5)
        {
            printf("客户端%d发送上传文件请求...\n", idx);
            RecvFile(idx, buffer);
            printf("客户端%d结束上传文件请求！\n", idx);
        }
        else if (buffer.Order == 6)
        {
            printf("客户端%d发送接收文件请求...\n", idx);
            SendFile(idx, buffer);
            printf("客户端%d结束接收文件请求！\n", idx);
        }
        else if (buffer.Order == 7)
        {
            printf("客户端%d发送注销账号请求...\n", idx);
            deletedata(idx, buffer);
            printf("客户端%d结束注销账号请求！\n", idx);
        }
        else if (buffer.Order == -6) //心跳测试
        {
            printf("客户端%d心跳测试...\n", idx);
            time(&sbuffer[idx].lastHeartbeat); // 更新心跳时间戳
        }
    }
}

int main() 
{
    // 初始化serverSocket
    serverSocket = INVALID_SOCKET;
    // 初始化clientSocket数组
    for (int i = 0; i < PC_number; i++)
    {
        clientSocket[i] = INVALID_SOCKET;
    }
    // 初始化sbuffer数组
    for (int i = 0; i < PC_number; i++)
    {
        memset(&sbuffer[i], 0, sizeof(UserInfo)); // 将每个UserInfo结构体的内存设置为0
        sbuffer[i].OnlineStatus = false; // 显式设置在线状态为false
        sbuffer[i].Order = -1; // 初始化Order为-1，表示未使用或未登录
    }
    // 创建socket
    serverSocket = InitializeSocket(IP, PORT);
    if (serverSocket == INVALID_SOCKET)
	{
		printf("服务器创建失败！\n");
		WSACleanup();
		return -1;
	}
    printf("服务器创建成功！\n");
    // 创建心跳检测线程
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckHeartbeat, NULL, 0, NULL);
    // 客户端地址
    sockaddr_in cAddr;
    int cAddrLen = sizeof(cAddr);
    printf("等待客户端连接...\n");
    // 循环接受连接
    while (1)
    {
        for (int i = 0; i < PC_number; i++)
        {
            if(sbuffer[i].OnlineStatus==false)
			{
                clientSocket[i] = accept(serverSocket, (sockaddr*)&cAddr, &cAddrLen);
                if (SOCKET_ERROR == clientSocket[i])
                {
                    printf("客户端连接错误！\n");
                    closesocket(serverSocket);
                    WSACleanup();
                    return -1;
                }
                // 获取客户端IP地址并打印
                char* clientIP = inet_ntoa(cAddr.sin_addr);
                int clientPort = ntohs(cAddr.sin_port);
                printf("客户端%d连接成功！\tIP地址：%s\t端口号：%d\n",i, clientIP, clientPort);
                // 创建通信线程
                CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Chat, (LPVOID)i, 0, NULL);
			}           
        }
    }
    // 关闭socket
    closesocket(serverSocket);
    // 清理协议版本信息
    WSACleanup();
    return 0;
}