#ifndef GBK_H
#define GBK_H

#include <stdio.h>
#include <windows.h>

char* utf8_to_gbk(const char* utf8_str) {
	char* gbk_str;
	wchar_t* wstr;
	int wlen;
	int mblen;

	if (!utf8_str) return NULL;

	wlen = MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8_str,
		-1,
		NULL, 0
		);
	if (wlen <= 0) return NULL;

	wstr = (wchar_t*)malloc(wlen * sizeof(wchar_t));
	if (!wstr) return NULL;

	MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, wlen);

	mblen = WideCharToMultiByte(
		CP_ACP,
		0,
		wstr,
		-1,
		NULL, 0,
		NULL, NULL
		);
	if (mblen <= 0) {
		free(wstr);
		return NULL;
	}

	gbk_str = (char*)malloc(mblen);
	if (!gbk_str) {
		free(wstr);
		return NULL;
	}

	WideCharToMultiByte(CP_ACP, 0, wstr, -1, gbk_str, mblen, NULL, NULL);

	free(wstr);
	return gbk_str;
}

char* gbk_to_utf8(const char* gbk_str) {
	int wlen = MultiByteToWideChar(936, 0, gbk_str, -1, NULL, 0);
	if (wlen == 0) return NULL;
	wchar_t* wstr = (wchar_t*)malloc(wlen * sizeof(wchar_t));
	MultiByteToWideChar(936, 0, gbk_str, -1, wstr, wlen);

	int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (len == 0) {
		free(wstr);
		return NULL;
	}

	char* utf8_str = (char*)malloc(len);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8_str, len, NULL, NULL);
	free(wstr);
	return utf8_str;
}


#endif