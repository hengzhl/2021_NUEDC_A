/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "ti/devices/msp/peripherals/hw_rtc.h"
#include "ti/driverlib/dl_uart_main.h"
#include <stdio.h>
#include <stdlib.h>  // 核心：提供 malloc, free 等堆内存管理函数
#include <stdint.h>  // 核心：提供 uint16_t, uint32_t 等标准整数类型定义
#include <stddef.h>  // 建议：提供 NULL 的宏定义

#include "thd_m0.h"
#include "zhl_io.h"
#include "adc_processor.h"


// 宏定义
#define SAMPLE_RATE  1250000.0f
#define sampleLEN 1024 // 采样点数
#define showPoints 120 // 设定发送点数，通过平均降采样实现平滑

// 变量声明
uint16_t ADC_Buffer[sampleLEN];
uint8_t  gDisplayBuffer[showPoints]; // 处理后给屏幕的数据

DIFFT_Result difft;
WaveMetrics gMetrics;                    // 信号特征结构体

volatile uint8_t gEchoData = 0; // 1字节数据，两位十六进制数
uint8_t ADCFlag = 0; // 0-ADC停止工作；1-ADC正在工作
uint8_t SysFlag = 0; // 0-停止；1-工作



// 声明函数
static void ADC_DMA_init(void);
void process_data(void);
void transmit(void);
void toShow(uint16_t *ADCBuffer, uint16_t LEN);



int main(void)
{
    
    SYSCFG_DL_init();
    
    NVIC_ClearPendingIRQ(UART_TJC_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_TJC_INST_INT_IRQN);

    ADC_DMA_init();

    delay_cycles(CPUCLK_FREQ / 2); // 500ms延时

    
    
    //uart_printf(UART_TJC_INST,"b6.txt=\"方波\"\xff\xff\xff");

   
    
    while (1) {
 //uart_printf(UART_TJC_INST,"b6.txt=\"三角波\"\xff\xff\xff" );
        if(SysFlag==1){
            ADCFlag = 1;

            DL_TimerA_startCounter(TIMER_0_INST);// 第一次开启定时器
            while(ADCFlag)
              ;
            
            process_data();
        }
    }
}


/** 辅助函数 */
static void ADC_DMA_init(void)
{
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t) 0x40556280); // DMA传输源地址--ADC0地址
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&ADC_Buffer);// DMA传输目标地址--ADC_Buffer[]地址
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);   // 使能DMA通道

    ADCFlag=0;
    NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);  //使能DMA中断
}

void process_data(void)
{
    //__BKPT();
    difft_run_adc(ADC_Buffer, ADC_SAMPLES, SAMPLE_RATE, 0.1, &difft);
    transmit();
}

void transmit(void){
  // 发送串口屏
  if(difft.wavetype==0){
    uart_printf(UART_TJC_INST,"b6.txt=\"周期波\"\xff\xff\xff" );
  }else if(difft.wavetype==1){
    uart_printf(UART_TJC_INST,"b6.txt=\"正弦波\"\xff\xff\xff" );
  }else if(difft.wavetype==2){
    uart_printf(UART_TJC_INST,"b6.txt=\"三角波\"\xff\xff\xff" );
  }else if(difft.wavetype==3){
    uart_printf(UART_TJC_INST,"b6.txt=\"方波\"\xff\xff\xff" );
  }

  
  
  uart_printf(UART_TJC_INST,"b4.txt=\"%d Hz\"\xff\xff\xff",(uint16_t)difft.f );
  uart_printf(UART_TJC_INST,"b5.txt=\"%d mV\"\xff\xff\xff",(uint16_t)(difft.vpp_mv) );

  uart_printf(UART_TJC_INST, "t7.txt=\"%d\"\xff\xff\xff", 1 );
  uart_printf(UART_TJC_INST, "t8.txt=\"%.4f\"\xff\xff\xff", difft.harmonicNorm.h2 );
  uart_printf(UART_TJC_INST, "t9.txt=\"%.4f\"\xff\xff\xff", difft.harmonicNorm.h3 );
  uart_printf(UART_TJC_INST, "t10.txt=\"%.4f\"\xff\xff\xff", difft.harmonicNorm.h4 );
  uart_printf(UART_TJC_INST, "t11.txt=\"%.4f\"\xff\xff\xff", difft.harmonicNorm.h5 );
  uart_printf(UART_TJC_INST, "t12.txt=\"%.4f\"\xff\xff\xff", difft.harmonicNorm.thd );

//   //发送蓝牙上位机
//   uart_printf(UART_HC08_INST, "t0.txt=\"%d\"\xff\xff\xff", (uint16_t)difft.f);      // 信号基频,hz
//   uart_printf(UART_HC08_INST, "t1.txt=\"%d\"\xff\xff\xff", (uint16_t)(difft.vpp_mv)); // 基波幅值,mv

//   uart_printf(UART_HC08_INST, "t2.txt=\"%.2f\"\xff\xff\xff", difft.harmonicNorm.h2*100);// 单位%
//   uart_printf(UART_HC08_INST, "t3.txt=\"%.2f\"\xff\xff\xff", difft.harmonicNorm.h3*100);
//   uart_printf(UART_HC08_INST, "t4.txt=\"%.2f\"\xff\xff\xff", difft.harmonicNorm.h4*100);
//   uart_printf(UART_HC08_INST, "t5.txt=\"%.2f\"\xff\xff\xff", difft.harmonicNorm.h5*100);
//   uart_printf(UART_HC08_INST, "t6.txt=\"%.2f\"\xff\xff\xff", difft.harmonicNorm.thd*100);

  toShow(ADC_Buffer,sampleLEN);
}

void toShow(uint16_t *ADCBuffer, uint16_t LEN)
{
    ADC_Filter_Smooth(ADCBuffer,LEN);
    gMetrics = ADC_Analyze_Waveform(ADCBuffer, LEN);
    float step = (SAMPLE_RATE /difft.f  * 1.0f) /showPoints ;
    ADC_Normalize_For_Display(ADC_Buffer, gDisplayBuffer, &gMetrics, step);

    for (int i = 0; i < showPoints; i++) {
        uart_printf(UART_TJC_INST, "add s0.id,0,%d\xff\xff\xff", gDisplayBuffer[i]); 
        //uart_printf(UART_HC08_INST, "add s0.id,0,%d\xff\xff\xff", gDisplayBuffer[i]); 
    }
}

/** 中断函数 */
// ADC中断关闭TIM驱动,标志位置0
void ADC12_0_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) { // ADC采样结束中断
        case DL_ADC12_IIDX_DMA_DONE:
            ADCFlag = 0;
            DL_TimerA_stopCounter(TIMER_0_INST); // 关定时器，停止采样
            

            break;
        default:
            break;
    }
}

// UART中断函数处理,改变系统工作标志位
void UART_TJC_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_TJC_INST)) {
        case DL_UART_MAIN_IIDX_RX:
            gEchoData = DL_UART_Main_receiveData(UART_TJC_INST); // 接收一个字节数据

            if (gEchoData==0x01){
                SysFlag=1;
            
            }else if (gEchoData==0x00){
                SysFlag=0;
            }

            break;
        default:
            break;
    }
}


