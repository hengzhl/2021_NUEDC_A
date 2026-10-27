/**
 * @file    zhl_io.c
 * @author  ZHL
 * @brief   串口重定向与自定义多串口打印工具库
 */

#include "zhl_io.h"
#include <stdarg.h>
#include <string.h>

/* ------------------- 标准库重定向 (针对 UART_0) ------------------- */

/** 串口重定向：底层输出字符函数 */
int fputc(int ch, FILE *f) {
    while (DL_UART_Main_isBusy(UART_TJC_INST))
        ;
    DL_UART_Main_transmitData(UART_TJC_INST, ch);
    return ch;
}

/** 串口重定向：输出字符串函数 */
int fputs(const char *_ptr, register FILE *_fp) {
    unsigned int len = strlen(_ptr);
    for (unsigned int i = 0; i < len; i++) {
        while (DL_UART_Main_isBusy(UART_TJC_INST))
            ;
        DL_UART_Main_transmitData(UART_TJC_INST, (uint8_t)_ptr[i]);
    }
    return (int)len;
}

/** 串口重定向：标准 puts 函数 */
int puts(const char *_ptr) {
    int count = fputs(_ptr, stdout);
    count += fputs("\n", stdout);
    return count;
}

/* ------------------- 自定义多串口打印实现 ------------------- */

void uart_printf(UART_Regs *UART_INST, const char *format, ...) {
    char buffer[128]; // 缓冲区：如果打印信息过长，请调大此值
    va_list args;

    va_start(args, format);
    // 使用 vsnprintf 限制长度，防止 buffer 溢出导致系统崩溃
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    char *p = buffer;
    while (*p != '\0') {
        while (DL_UART_Main_isBusy(UART_INST))
            ;
        DL_UART_Main_transmitData(UART_INST, (uint8_t)(*p));
        p++;
    }
}