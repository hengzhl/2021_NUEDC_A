#ifndef __ZHL_IO_H__
#define __ZHL_IO_H__

#include "ti_msp_dl_config.h" // 确保能识别 UART_Regs 类型
#include <stdio.h>

/**
 * @brief 自定义串口打印函数，支持指定串口实例
 * @param UART_INST 串口实例 (如 UART_0_INST, UART_1_INST)
 * @param format 格式化字符串，用法同 printf
 */
void uart_printf(UART_Regs *UART_INST, const char *format, ...);

/* * 注意：fputc, fputs, puts 不需要在这里声明。
 * 因为它们是标准库函数的重定向，链接器会自动寻找实现。
 */

#endif /* __ZHL_IO_H__ */