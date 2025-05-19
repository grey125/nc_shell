#include <stdio.h>              // 标准输入输出函数
#include <stdlib.h>             // 标准库函数，如 atoi, exit
#include <string.h>             // 字符串处理函数
#include <winsock2.h>           // Windows 套接字库
#include <windows.h>            // Windows API
#include <process.h>            // _beginthread 函数所在头文件
#include "gbk.h"

#pragma comment(lib, "ws2_32.lib")  // 链接 ws2_32 库

SOCKET conn_fd;  // 用于保存客户端连接的套接字

				 // 接收线程函数（注意参数必须为 void* 类型）
void recv_thread(void *arg) {
	char buffer[1024];      // 接收缓冲区
	int n;                  // 实际接收字节数

							// 不断从客户端接收数据
	while ((n = recv(conn_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
		buffer[n] = 0;       // 末尾加 \0，确保字符串格式
		char *gbk_buf = utf8_to_gbk(buffer);
		if (gbk_buf) {
			printf("%s", gbk_buf);  // 输出内容
			free(gbk_buf);
			fflush(stdout);       // 立即刷新输出缓冲区
		}
		else {
			printf("%s", buffer);  // 输出内容
			fflush(stdout);       // 立即刷新输出缓冲区
		}

	}

	// 如果客户端断开连接或出错
	printf("\nConnection closed by client.\n");
	_exit(0);                 // 退出程序
}

int main(int argc, char *argv[]) {
	WSADATA wsaData;               // 存储 WSA 启动信息
	SOCKET listen_fd;              // 监听套接字
	struct sockaddr_in serv_addr;  // 服务器地址结构体
	struct sockaddr_in client_addr;// 客户端地址结构体
	int client_len;                // 客户端地址结构体长度
	int port = 1088;               // 默认端口

								   // 如果命令行传入了端口号，就使用用户指定的端口
	if (argc > 1) {
		port = atoi(argv[1]);      // 将字符串转为整数
		if (port <= 0 || port > 65535) {
			fprintf(stderr, "Invalid port number: %s\n", argv[1]);
			return 1;
		}
	}

	// 初始化 Winsock 库
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup failed.\n");
		return 1;
	}

	// 创建 TCP 套接字
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == INVALID_SOCKET) {
		printf("Socket creation failed.\n");
		WSACleanup();       // 清理资源
		return 1;
	}

	// 设置服务器地址结构体
	memset(&serv_addr, 0, sizeof(serv_addr));         // 清零结构体
	serv_addr.sin_family = AF_INET;                   // IPv4 地址族
	serv_addr.sin_addr.s_addr = INADDR_ANY;           // 接收任意 IP 地址连接
	serv_addr.sin_port = htons(port);                 // 设置监听端口（转换为网络字节序）

													  // 绑定套接字与地址
	if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
		printf("Bind failed.\n");
		closesocket(listen_fd);   // 关闭套接字
		WSACleanup();             // 清理 Winsock
		return 1;
	}

	// 开始监听端口
	if (listen(listen_fd, 5) == SOCKET_ERROR) {
		printf("Listen failed.\n");
		closesocket(listen_fd);
		WSACleanup();
		return 1;
	}

	printf("Listening on port %d, waiting for connection...\n", port);

	client_len = sizeof(client_addr);  // 设置结构体长度
									   // 等待客户端连接
	conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
	if (conn_fd == INVALID_SOCKET) {
		printf("Accept failed.\n");
		closesocket(listen_fd);
		WSACleanup();
		return 1;
	}

	// 显示客户端 IP 和端口信息
	printf("Connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

	// 创建接收线程，使用 _beginthread 而不是 pthread
	_beginthread(recv_thread, 0, NULL);

	// 主线程用于读取用户输入并发送给客户端
	char input[1024];
	while (fgets(input, sizeof(input), stdin)) {
		send(conn_fd, input, strlen(input), 0);  // 发送数据
	}

	// 用户退出，关闭套接字
	closesocket(conn_fd);
	closesocket(listen_fd);
	WSACleanup();  // 清理 Winsock 库
	return 0;
}
