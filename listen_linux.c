#include <stdio.h>          // 标准输入输出函数，如 printf, fprintf
#include <stdlib.h>         // 标准库函数，如 atoi, exit
#include <string.h>         // 字符串处理函数，如 memset, strlen
#include <unistd.h>         // POSIX API，如 close, read, write
#include <sys/types.h>      // 基本类型定义，如 ssize_t
#include <sys/socket.h>     // 套接字函数，如 socket, bind, accept
#include <netinet/in.h>     // sockaddr_in 结构体定义（IPV4 地址族）
#include <arpa/inet.h>      // IP 地址转换函数，如 inet_ntoa
#include <pthread.h>        // POSIX 线程库

int conn_fd;  // 用于保存与客户端建立连接后的套接字描述符

// 接收线程函数，用于从客户端不断接收数据并输出到控制台
void *recv_thread(void *arg) {
    char buffer[1024];           // 接收缓冲区
    ssize_t n;                   // 实际读取的字节数

    // 循环接收客户端发来的数据
    while ((n = read(conn_fd, buffer, sizeof(buffer)-1)) > 0) {
        buffer[n] = 0;           // 添加字符串结束符，确保可打印
        printf("%s", buffer);    // 输出到终端
        fflush(stdout);          // 强制刷新标准输出，立即显示内容
    }

    // 如果读取失败或客户端关闭连接
    printf("\nConnection closed by client.\n");
    _exit(0);                     // 主动退出程序
    return NULL;                 // 返回空指针（虽然不会执行到这）
}

int main(int argc, char *argv[]) {
    int listen_fd;                     // 监听套接字描述符
    struct sockaddr_in serv_addr;      // 服务端地址结构体
    struct sockaddr_in client_addr;    // 客户端地址结构体
    socklen_t client_len;              // 客户端地址长度
    int port = 1088;                   // 默认监听端口

    // 如果命令行传入了端口号，就使用该端口
    if (argc > 1) {
        port = atoi(argv[1]);  // 将字符串转换为整数
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            return 1;          // 非法端口，退出程序
        }
    }

    // 创建监听套接字（TCP）
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket error"); // 输出错误信息
        return 1;
    }

    // 清空地址结构体，并设置监听信息
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;         // 地址族：IPv4
    serv_addr.sin_addr.s_addr = INADDR_ANY; // 接受任意 IP 地址连接
    serv_addr.sin_port = htons(port);       // 设置监听端口（主机字节序转网络字节序）

    // 将套接字与本地地址绑定
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind error");    // 输出绑定错误信息
        close(listen_fd);        // 关闭套接字
        return 1;
    }

    // 开始监听（最多排队 5 个连接）
    if (listen(listen_fd, 5) < 0) {
        perror("listen error");  // 输出监听错误
        close(listen_fd);
        return 1;
    }

    printf("Listening on port %d, waiting for connection...\n", port);

    client_len = sizeof(client_addr);  // 初始化地址结构体长度

    // 接受来自客户端的连接请求
    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) {
        perror("accept error");  // 输出错误信息
        close(listen_fd);        // 关闭监听套接字
        return 1;
    }

    // 打印连接的客户端信息（IP + 端口）
    printf("Connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    pthread_t tid;  // 线程 ID 变量
    // 创建接收线程，用于异步接收客户端数据
    pthread_create(&tid, NULL, recv_thread, NULL);

    // 主线程用于读取用户输入并发送给客户端
    char input[1024];
    while (fgets(input, sizeof(input), stdin)) {
        write(conn_fd, input, strlen(input));  // 发送数据到客户端
    }

    // 用户退出，关闭连接
    close(conn_fd);
    close(listen_fd);
    return 0;
}
