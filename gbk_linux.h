#ifndef GBK_LINUX_H
#define GBK_LINUX_H

#include <stdio.h>      // 标准输入输出
#include <stdlib.h>     // malloc、free 等函数
#include <string.h>     // strlen 等字符串函数
#include <iconv.h>      // iconv 库用于字符编码转换
#include <errno.h>      // 错误处理

// 通用的编码转换函数：将 in_charset 编码的字符串转换为 out_charset 编码
char* convert_encoding(const char* input, const char* from_charset, const char* to_charset) {
    iconv_t cd;                            // 定义 iconv 转换描述符
    cd = iconv_open(to_charset, from_charset); // 创建转换器，从 from_charset 到 to_charset
    if (cd == (iconv_t)-1) {
        perror("iconv_open");
        return NULL;
    }

    size_t in_len = strlen(input);        // 原始字符串长度
    size_t out_len = in_len * 4;          // 分配输出缓冲区大小（预估 UTF-8 会变大）
    char* outbuf = (char*)malloc(out_len);
    if (!outbuf) {
        iconv_close(cd);
        return NULL;
    }

    char* in_ptr = (char*)input;          // 输入指针
    char* out_ptr = outbuf;               // 输出指针
    size_t in_bytes_left = in_len;        // 输入剩余字节
    size_t out_bytes_left = out_len;      // 输出剩余字节

    // 调用 iconv 进行转换
    if (iconv(cd, &in_ptr, &in_bytes_left, &out_ptr, &out_bytes_left) == (size_t)-1) {
        perror("iconv");
        free(outbuf);
        iconv_close(cd);
        return NULL;
    }

    *out_ptr = '\0';                      // 添加字符串结束符

    iconv_close(cd);                      // 关闭转换器
    return outbuf;                        // 返回转换后的字符串（记得使用者调用 free）
}

// 将 UTF-8 字符串转换为 GBK 编码
char* utf8_to_gbk(const char* utf8_str) {
    return convert_encoding(utf8_str, "UTF-8", "GBK");
}

// 将 GBK 字符串转换为 UTF-8 编码
char* gbk_to_utf8(const char* gbk_str) {
    return convert_encoding(gbk_str, "GBK", "UTF-8");
}

#endif
