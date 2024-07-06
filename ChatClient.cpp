#include <stdio.h>
#include <winsock.h>
#include <graphics.h>
#include <time.h>
#include "zip.h" //自定义压缩解压头文件
#pragma comment(lib, "ws2_32.lib") // 链接到Winsock库

#define PORT 10000
#define IP "your Server IP address"
#define PC_number 20
#define word_size 1024
#define MAX_FILE_SIZE 10485760 //规定传输文件大小最大为10MB
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

struct ChatThreadParams 
{   //聊天线程参数结构体
    SOCKET clientSocket;
    UserInfo* userInfo; // 修改为指针
};

UserInfo unvalue = {};
time_t lastHeartbeatResponse = 0;

void menu(int x, UserInfo cbuffer)
{   //菜单
    if (x == 1)
    {
        printf("欢迎使用MeSafe——安全通信！\n");
        printf("\t1.登录\n");
        printf("\t2.注册\n");
        printf("\t3.退出\n");
        printf("请输入您的选择：");
    }
    else if (x == 2)
    {
        printf("欢迎使用MeSafe——安全通信！\n");
        printf("1.交流沟通\n");
        printf("2.文件传输\n");
        printf("3.修改信息\n");
        printf("4.注销账号\n");
        printf("5.退出登录\n");
        printf("请输入您的选择：");
    }
    else if (x == 3)
    {
        printf("欢迎使用MeSafe——交流沟通！\n");
        printf("好友列表：\n");
        for (int i = 0; i < cbuffer.Order; i++)
        {
            printf("%d. %s（%s）\n", i + 1, cbuffer.Usernames[i], cbuffer.UserIDs[i]);
        }
        printf("%d. 群聊（与所有好友共同交流）\n", cbuffer.Order + 1);
        printf("%d. 退出聊天室\n", cbuffer.Order + 2);
        printf("请输入您的选择：");
    }
    else if (x == 4)
    {
        printf("欢迎使用MeSafe——文件传输！\n");
        printf("1.发送文件\n");
        printf("2.接收文件\n");
        printf("3.退出登录\n");
        printf("请输入您的选择：");
    }
}

void xorEncryptDecrypt(UserInfo* userInfo, size_t size)
{   // 异或加密解密
    char* data = reinterpret_cast<char*>(userInfo); // 将结构体指针转换为char指针
    for (size_t i = 0; i < size; ++i)
    {
        data[i] ^= ENCRYPTION_KEY; // 对每个字节进行异或操作
    }
}

void drawTextAutoWrap(int x, int& y, const char* text, int maxWidth, int textAreaHeight) {
    int lineHeight = 20; // 假设每行文本的高度
    const int charWidth = 8; // 假设每个字符的平均宽度
    int maxCharsPerLine = maxWidth / charWidth; // 每行最多的字符数
    char lineBuffer[1024]; // 存储分割后的行
    int bufferIndex = 0;
    int textLength = strlen(text);
    for (int i = 0; i < textLength; ++i) {
        lineBuffer[bufferIndex++] = text[i];
        if (bufferIndex == maxCharsPerLine || text[i] == '\n' || i == textLength - 1) {
            lineBuffer[bufferIndex] = '\0'; // 结束当前行
            if (y + lineHeight > textAreaHeight) 
            {
                // 如果绘制下一行会超出文本区域，则向上滚动
                y = 0; // 重置y坐标为文本区域顶部
                cleardevice(); // 清除文本区域的内容，准备重新绘制
            }
            outtextxy(x, y, lineBuffer); // 绘制当前行
            y += lineHeight; // 移动到下一行的位置
            bufferIndex = 0; // 重置行缓冲区索引
        }
    }
}

void SendHeartbeat(SOCKET clientSocket)
{
    UserInfo heartbeatMsg;
    memset(&heartbeatMsg, 0, sizeof(heartbeatMsg));
    heartbeatMsg.Order = -6;
    while (true)
    {
        xorEncryptDecrypt(&heartbeatMsg, sizeof(heartbeatMsg));
        send(clientSocket, (char*)&heartbeatMsg, sizeof(heartbeatMsg), 0);
        xorEncryptDecrypt(&heartbeatMsg, sizeof(heartbeatMsg));
        Sleep(900000); // 每90秒发送一次心跳包
    }
}

SOCKET InitializeSocket(const char* ip, int port)
{   // 创建socket链接
    // 初始化Winsock
    WSADATA wsaData;
    int wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    // 创建socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // 填充sockaddr_in结构
    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr(ip);//服务器的ip地址
    clientService.sin_port = htons(port);//服务器的端口号
    printf("正在连接到服务器...\n");
    // 连接到服务器
    int connResult = connect(clientSocket, (SOCKADDR*)&clientService, sizeof(clientService));
    if (connResult == SOCKET_ERROR) {
        printf("连接到服务器失败。\n");
        closesocket(clientSocket);
        WSACleanup();
        return INVALID_SOCKET;
    }
    printf("连接到服务器成功。\n");
    return clientSocket; // 连接成功，返回socket
}

bool deletedate(SOCKET clientSocket, UserInfo databuffer)
{   //注销账号 操作码为7
	UserInfo cbuffer = databuffer;
    char choice;
    printf("您正在注销账号，是否继续？（Y/N）\n");
    getchar();choice = getchar(); getchar();
    if (choice == 'Y')
	{
        printf("您正在注销账号，是您本人操作吗？（y/N）\n");
        choice = getchar(); getchar();
        if (choice == 'y')
        {
            cbuffer.Order = 7;
            xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
            send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);

            recv(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
            xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
            if (cbuffer.Order == 0)
			{
				printf("账号注销成功，欢迎您的下次使用！\n");
                return true;
            }
			else
			{
				printf("注销账号失败，感谢您愿意再次使用MeSafe！\n");
				return false;
			}
        }
        else
        {
            printf("您已取消注销账号！感谢您愿意再次使用MeSafe！\n");
            return false;
        }
	}
	else
	{
		printf("您已取消注销账号！感谢您愿意再次使用MeSafe！\n");
        return false;
	}
}

void RecvFile(SOCKET clientSocket, UserInfo databuffer)
{   //接收文件 操作码为6
    char filePath[1024];
    char fileCode[20];
    UserInfo cbuffer = databuffer;

    printf("请输入文件提取码：");
    scanf("%s", fileCode);
    printf("请输入您要接收的文件夹路径：");
    scanf("%s", filePath);
    cbuffer.Order = 6;
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    strcat(filePath, "\\"); 
    sprintf(filePath + strlen(filePath), "%s.txt", fileCode); 
    send(clientSocket, fileCode, sizeof(fileCode), 0);
    FILE* file = fopen(filePath, "wb");
    if (file == NULL)
    {
       printf("文件打开失败，请重新接收！\n");
       return;
    }
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    recv(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    if (cbuffer.Order == 0 || cbuffer.Order == -6)
	{
		printf("文件提取失败，请稍后重试！\n");
		fclose(file);
		return;
	}
    long fileSize;
    recv(clientSocket, (char*)&fileSize, sizeof(fileSize), 0);
    char buffer[1024];
    long bytesReceived = 0;
    while (bytesReceived < fileSize)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        fwrite(buffer, 1, bytesRead, file);
        bytesReceived += bytesRead;
    }
    fclose(file);    
    recv(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    if (cbuffer.Order == 0)
    {
        printf("文件接收失败，请重新发送！\n");
    }
    else
    {
        printf("文件接收成功！\n");
    }
    if (dezip(filePath) == 999)
    {
        printf("文件解压成功！\n");
        remove(filePath);
    }
    else
    {
        printf("文件解压失败！\n");
    }
}

void SendFile(SOCKET clientSocket, UserInfo databuffer)
{   //发送文件 操作码为5
    char filePath[1024];
    char fileCode[20];
    int flag = 0;
    UserInfo cbuffer = databuffer;

    printf("请输入您要上传的文件夹路径：");
    scanf("%s", filePath);
    printf("请输入文件名称（包括后缀名）：");
    scanf("%s", fileCode);
    strcat(filePath, "\\");
    char bfilepath[1024];
    strcpy(bfilepath, filePath);
    sprintf(filePath + strlen(filePath), "%s", fileCode);
    FILE* fileCheck = fopen(filePath, "rb");
    if (fileCheck == NULL)
    {
        printf("文件打开失败，请重新发送！\n");
        return;
    }
    fseek(fileCheck, 0, SEEK_END);
    long fileSizes = ftell(fileCheck);
    fseek(fileCheck, 0, SEEK_SET);
    // 检查文件大小是否超过限制
    if (fileSizes > MAX_FILE_SIZE)
    {
		printf("文件大小超过限制（%d字节），无法发送。\n", MAX_FILE_SIZE);
		fclose(fileCheck); // 关闭文件
		return;
	}
    if (zip(filePath) == 999)
    {
        flag = 1;
        printf("文件压缩成功！\n正在发送...\n");
        sprintf(bfilepath + strlen(bfilepath), "Zip.txt");
        strcpy(filePath, bfilepath);
    }
    else
    {
        printf("文件压缩失败！\n正在发送源文件...\n");
    }
    FILE* file = fopen(filePath, "rb");
    if (file == NULL)
    {
        printf("文件打开失败，请重新发送！\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    // 检查文件大小是否超过限制
    cbuffer.Order = 5;
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    // 发送文件大小
    send(clientSocket, (char*)&fileSize, sizeof(fileSize), 0);
    char buffer[1024];
    while (!feof(file)) {
        int bytesRead = fread(buffer, 1, sizeof(buffer), file);
        send(clientSocket, buffer, bytesRead, 0);
    }
    fclose(file);
    recv(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    if (cbuffer.Order == 0)
    {
        printf("文件发送失败，请重新发送！\n");
    }
    else
    {
        printf("文件发送成功！\n文件提取码为：%s\n", cbuffer.UserID);
    }
    if (flag == 1)
    {
        remove(filePath);
    }
}

void File_trans(SOCKET clientSocket, UserInfo databuffer)
{
    menu(4, unvalue);
    int choice;
    scanf("%d", &choice);
    while (choice != 3)
    {
        if (choice == 1)
        {
            SendFile(clientSocket, databuffer);
            printf("成功退出发送界面！\n");
        }
        else if (choice == 2)
        {
            RecvFile(clientSocket, databuffer);
            printf("成功退出接收界面！\n");
        }
        else
        {
            printf("输入错误，请重新输入！\n");
        }
        menu(4, unvalue);
        scanf("%d", &choice);
    }


}

void Chatsend(ChatThreadParams* params)
{   //信息发送
   //信息发送
    char message[word_size*2];
    SOCKET clientSocket = params->clientSocket;
    UserInfo cbuffer = *params->userInfo;
    getchar();
    while (1)
    {
        memset(cbuffer.messages, 0, sizeof(cbuffer.messages));
        memset(message, 0, sizeof(message));
        printf("请输入要发送的信息：");
        fgets(message, sizeof(message), stdin); // 使用fgets读取一行输入
        // 移除输入字符串末尾的换行符
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') 
        {
            message[len - 1] = '\0';
        }
        if (len > word_size-1)
		{
			printf("输入信息过长，请重新输入！\n");
			continue;
		}
        else
        {
            strcpy(cbuffer.messages,message);
            if (strcmp(cbuffer.messages, "exit") == 0)
            {
                cbuffer.Order = 0;
                xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
                send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
                return;
            }
            else
            {
                cbuffer.Order = 1;
            }
            xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
            send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
            xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
        }
    }
}

void GS_chat(SOCKET clientSocket, UserInfo databuffer)
{   //群聊和单聊 操作码为4 
    UserInfo cbuffer[2];//cbuffer[0]为发送信息，cbuffer[1]为接收信息
    cbuffer[0] = databuffer;
	cbuffer[0].Order = 4;
    xorEncryptDecrypt(&cbuffer[0], sizeof(cbuffer[0]));
    send(clientSocket, (char*)&cbuffer[0], sizeof(cbuffer[0]), 0);

    recv(clientSocket, (char*)&cbuffer[1], sizeof(cbuffer[1]), 0);
    xorEncryptDecrypt(&cbuffer[1], sizeof(cbuffer[1]));
    //选择聊天对象
    while (cbuffer[1].Order)
	{
        printf("好友当前在线人数为：%d人\n请选择您的聊天对象...\n", cbuffer[1].Order);
        menu(3, cbuffer[1]);
        int choice;
        scanf("%d", &choice);
        if (choice > 0 && choice <= cbuffer[1].Order + 1)
        {
            strcpy(cbuffer[0].UserID, cbuffer[0].Username);
            cbuffer[0].messages[0] = '\0';
            if(choice==cbuffer[1].Order + 1)
            {
                cbuffer[0].Order = cbuffer[1].Order;
            }
            else if(choice > 0 && choice <= cbuffer[1].Order)
			{
                
                cbuffer[0].Order = choice-1;
			}
            xorEncryptDecrypt(&cbuffer[0], sizeof(cbuffer[0]));
            send(clientSocket, (char*)&cbuffer[0], sizeof(cbuffer[0]), 0);

            UserInfo* userInfoCopy = new UserInfo; 
            *userInfoCopy = cbuffer[0]; 
            ChatThreadParams* params = new ChatThreadParams();
            params->clientSocket = clientSocket;
            params->userInfo = userInfoCopy;
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Chatsend, (LPVOID)params, 0, NULL);//创建线程
            initgraph(300, 600, 1);
            int n = 0;
            while (cbuffer[1].Order)
            {
                int ret = recv(clientSocket, (char*)&cbuffer[1], sizeof(cbuffer[1]), 0);
                xorEncryptDecrypt(&cbuffer[1], sizeof(cbuffer[1]));
                if (ret > 0)
                {
                    cbuffer[1].messages[ret] = '\0';
                    if(strcmp(cbuffer[1].UserID,databuffer.Username)==0)
					{
						settextcolor(YELLOW);
					}
					else
					{
						settextcolor(GREEN);
					}
                    char displayMessage[word_size + 20];
                    sprintf(displayMessage, "%s: %s", cbuffer[1].UserID, cbuffer[1].messages);
                    drawTextAutoWrap(3, n, displayMessage, 300, 600);
                }
            }
            closegraph();
            delete params->userInfo;
            delete params;
        }
        else if (choice == cbuffer[1].Order + 2)
		{
            break;
        }
        else
        {
            printf("好友当前在线人数为：%d人\n请选择您的聊天对象...\n", cbuffer[1].Order);
            menu(3, cbuffer[1]);
            scanf("%d", &choice);
        }
	}
    cbuffer[0].Order = -1;
    xorEncryptDecrypt(&cbuffer[0], sizeof(cbuffer[0]));
    send(clientSocket, (char*)&cbuffer[0], sizeof(cbuffer[0]), 0);
	printf("聊天功能暂时关闭！请稍后重试。\n");
}

void modify(SOCKET clientSocket,UserInfo databuffer)
{   //修改信息 操作码为3
    UserInfo cbuffer;
    cbuffer.Order = 3;
    printf("欢迎使用MeSafe——信息修改！\n");
    printf("请您按照提示完成修改信息！对不更改的信息可用“*”代替\n");
    printf("新昵称：");
    scanf("%254s", cbuffer.Username); getchar();
    printf("新密码：");
    scanf("%254s", cbuffer.Password); getchar();
    if(strcmp(cbuffer.Username,"*") == 0)
	{
		strcpy(cbuffer.Username,databuffer.Username);
	}
    if(strcmp(cbuffer.Password,"*") == 0)
    {
        strcpy(cbuffer.Password,databuffer.Password);
	}
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    recv(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    if (cbuffer.Order == 0)
    {
        printf("修改失败，请重新修改！\n");
    }
    else
    {
        printf("修改成功！\n");
    }
}

void log_in(SOCKET clientSocket)
{   //登录 操作码为2
	UserInfo cbuffer;
    UserInfo databuffer;
    printf("请您按照提示完成登录！\n");
    printf("账号ID：");
    scanf("%19s", cbuffer.UserID); getchar();
    printf("密码：");
    scanf("%254s", cbuffer.Password); getchar();
    cbuffer.Order = 2;
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
	send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    recv(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
    xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
    if(cbuffer.Order == 0)
	{
		printf("登录失败，请重新登录！\n");
	}
	else
	{
		printf("登录成功！\n");
        databuffer = cbuffer;
        menu(2,unvalue);
        int choice;
        scanf("%d", &choice);
        while (choice != 5)
        {
            if (choice == 1 )
            {
                GS_chat(clientSocket, databuffer);
                printf("成功退出聊天室！\n");
            }
            else if (choice == 2)
            {
                File_trans(clientSocket,databuffer);
                printf("成功退出文件传输界面！\n");
            }
            else if (choice == 3)
            {
                modify(clientSocket, databuffer);
                printf("成功退出信息修改界面！\n");
            }
            else if (choice == 4)
			{
                unvalue.Order=deletedate(clientSocket, databuffer);
				printf("成功退出账号注销界面！！\n");
                if (unvalue.Order)
                {return;}
			}
            else
            {
                printf("输入错误，请重新输入！\n");
            }
            menu(2, unvalue);
            scanf("%d", &choice);
        }
	}
}

void sign_in(SOCKET clientSocket)
{   //注册 操作码为1
    UserInfo cbuffer;
    char Password[255];
    cbuffer.Order = 1;
    printf("请您按照提示完成注册！\n");
    printf("昵称：");
    scanf("%254s", cbuffer.Username); getchar();
    printf("密码：");
    scanf("%254s", Password); getchar();
    printf("确认密码：");
    scanf("%254s", cbuffer.Password); getchar();
    if (strcmp(Password, cbuffer.Password) != 0)
    {
        printf("两次密码不一致，请重新注册！\n");
        sign_in(clientSocket);
    }
    else
    {
        xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
        send(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
        recv(clientSocket, (char*)&cbuffer, sizeof(cbuffer), 0);
        xorEncryptDecrypt(&cbuffer, sizeof(cbuffer));
        if (cbuffer.Order == 0)
        {
            printf("注册失败，请重新注册！\n");
        }
        else
        {
            printf("MeSafe——安全通信！\n您的注册ID为：%s\n登陆需要ID，请您妥善保管！\n",cbuffer.UserID);
        }
    }
}

int main() 
{
    // 创建socket链接
    SOCKET clientSocket = InitializeSocket(IP, PORT);
    if (clientSocket == INVALID_SOCKET) 
	{
        Sleep(3000);
		return 1;
	}
    //创建心跳包线程
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SendHeartbeat, (LPVOID)clientSocket, 0, NULL);
     //双向通信
    menu(1, unvalue);
    int choice;
    scanf("%d", &choice);
    while(choice != 3)
	{   
		if (choice == 1)
		{
			log_in(clientSocket);
            printf("成功退出登录界面！\n");
		}
		else if (choice == 2)
		{
			sign_in(clientSocket);
            printf("成功退出注册界面！\n");
		}
        else
		{
			printf("输入错误，请重新输入！\n");
		}
        menu(1, unvalue);
        scanf("%d", &choice);
	}
    unvalue.Order = 0;
    xorEncryptDecrypt(&unvalue, sizeof(unvalue));
    send(clientSocket, (char*)&unvalue, sizeof(unvalue), 0);
    // 关闭socket
    closesocket(clientSocket);
    // 清理协议版本信息
    WSACleanup();
    printf("账号退出成功！\n");
    Sleep(3000);
    return 0;
}