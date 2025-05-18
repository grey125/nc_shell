#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       // close, read, write
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>   // sockaddr_in
#include <arpa/inet.h>    // inet_ntoa
#include <pthread.h>

int conn_fd;

// 把一个字节转成两个16进制字符，放入 buf（必须至少有3字节空间，含\0）
void byte_to_hex(unsigned char byte, char *buf) {
    const char *hex = "0123456789ABCDEF";
    buf[0] = hex[(byte >> 4) & 0xF];
    buf[1] = hex[byte & 0xF];
    buf[2] = '\0';
}

char* encrypt_to_hex(const char *input, unsigned char key, unsigned char offset) {
	size_t len = strlen(input);
	// 每个字节转成2个16进制字符，+1放\0
	char *output = (char*)malloc(len * 2 + 1);
	if (!output) return NULL;

	for (size_t i = 0; i < len; i++) {
		unsigned char c = (input[i] ^ key) + offset;
		byte_to_hex(c, &output[i * 2]);
	}
	output[len * 2] = '\0';
	return output;
}

char* decrypt_from_hex(const char *input, unsigned char key, unsigned char offset) {
    size_t len = strlen(input);
    if (len % 2 != 0) return NULL; // 非法长度

    size_t out_len = len / 2;
    char *output = (char*)malloc(out_len + 1);
    if (!output) return NULL;

    for (size_t i = 0; i < out_len; i++) {
        char hex_byte[3] = {input[i * 2], input[i * 2 + 1], 0};
        unsigned char byte = (unsigned char)strtoul(hex_byte, NULL, 16);
        output[i] = (byte - offset) ^ key;
    }
    output[out_len] = '\0';
    return output;
}

void *recv_thread(void *arg) {
    char buffer[1024];
    static char hex_buf[2048] = {0}; // 累积缓冲，放不完整的 hex 字符
    static int hex_len = 0;
    ssize_t n;

    while ((n = read(conn_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        
        // 拼接到缓冲
        if (hex_len + n < sizeof(hex_buf) - 1) {
            memcpy(hex_buf + hex_len, buffer, n);
            hex_len += n;
            hex_buf[hex_len] = '\0';
        } else {
            printf("[!] 缓冲区溢出\n");
            hex_len = 0; // 丢弃错误数据
            continue;
        }

        // 保证处理成偶数字节长度
        int process_len = (hex_len / 2) * 2;
        char temp[2048];
        memcpy(temp, hex_buf, process_len);
        temp[process_len] = '\0';

        char *decrypted = decrypt_from_hex(temp, 0x5A, 3);
        if (decrypted) {
            printf("%s", decrypted);
            fflush(stdout);
            free(decrypted);
        } else {
            printf("[!] 解密失败: %s\n", temp);
        }

        // 把剩下的残余移到头部
        int remain = hex_len - process_len;
        memmove(hex_buf, hex_buf + process_len, remain);
        hex_len = remain;
    }

    printf("\n[!] Connection closed by client.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    int listen_fd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len;
    int port = 1088;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            return 1;
        }
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket error");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind error");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 5) < 0) {
        perror("listen error");
        close(listen_fd);
        return 1;
    }

    printf("Listening on port %d, waiting for connection...\n", port);

    client_len = sizeof(client_addr);
    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) {
        perror("accept error");
        close(listen_fd);
        return 1;
    }

    printf("Connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, NULL);

    char input[1024];

    while (fgets(input, sizeof(input), stdin)) {
        unsigned char key = 0x5A;
        unsigned char offset = 3;
        char *encypted_hex = encrypt_to_hex(input, key, offset);
        //write(conn_fd, input, strlen(input));
        //printf("%s\n",encypted_hex);
        if (encypted_hex) {
            write(conn_fd, encypted_hex, strlen(encypted_hex));
            free(encypted_hex);
        }
        
    }

    close(conn_fd);
    close(listen_fd);
    return 0;
}
