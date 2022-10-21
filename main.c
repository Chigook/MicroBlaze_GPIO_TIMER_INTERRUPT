/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"
#include "sleep.h"
#include "xil_exception.h"
#include "xintc.h"
#include "xtmrctr.h"

#define CHANNEL_1   			   1
#define GPIO_BTN_DEVICE_ID         XPAR_AXI_GPIO_1_DEVICE_ID
#define BTN_INT_MASK_IR_CH1_MASK   XGPIO_IR_CH1_MASK

#define GPIO_LED_DEVICE_ID         XPAR_AXI_GPIO_0_DEVICE_ID

#define INTC_DEVICE_ID             XPAR_INTC_0_DEVICE_ID
#define INTC_BTN_INT_VEC_ID        XPAR_INTC_0_GPIO_1_VEC_ID

#define TMRCTR_DEVICE_ID		   XPAR_TMRCTR_0_DEVICE_ID
#define TMRCTR_INT_VECTOR_ID	   XPAR_INTC_0_TMRCTR_0_VEC_ID
#define TIMER_CNTR_0			   0

uint32_t resetValue = 0xffffffff - 100000000;		//1sec //ff까지 counter
//uint32_t resetValue = 0xffffffff - 1000000;		//10msec
//uint32_t resetValue = 0xffffffff - 100000;		//1msec

void TimerCounterHandler(void *CallBackRef, uint8_t TmrCtrNumber);

void GpioHandler(void* CallBackRef);

XTmrCtr TimerCounterInst;

XGpio Gpio_Led;
XGpio Gpio_Button;

XIntc Intc;

void Button_Init()
{
    // Button Init
    XGpio_Initialize(&Gpio_Button, GPIO_BTN_DEVICE_ID);
    XGpio_SetDataDirection(&Gpio_Button, CHANNEL_1, 0x0f);
}

void Led_Init()
{
    // LED Init
    XGpio_Initialize(&Gpio_Led, GPIO_LED_DEVICE_ID);
    XGpio_SetDataDirection(&Gpio_Led, CHANNEL_1, 0x00);
}

void INTTERUPT_Init()
{
    // Interrupt setup --> Device Connect
    XIntc_Initialize(&Intc, INTC_DEVICE_ID);

    // Interrupt Controller의  Vector Table에 Jump할 함수 할당
    // 함수 이름만 사용하면 함수 주소를 불러오는 것
    // 해당되는 모든 Device의 Interrupt를 전부 감시 --> GPIO1인 button의 입력을 전부 다 감시
    XIntc_Connect(&Intc, INTC_BTN_INT_VEC_ID, (Xil_ExceptionHandler)GpioHandler, &Gpio_Button);

    // Enable the Interrupt vector at the interrupt controller
    XIntc_Enable(&Intc, INTC_BTN_INT_VEC_ID);

    // Start the Interrupt controller such that interrupts recognized
    // and handled by the processor
    XIntc_Start(&Intc, XIN_REAL_MODE);

    //GPIO Interrupt Enable
    XGpio_InterruptEnable(&Gpio_Button, CHANNEL_1);
    XGpio_InterruptGlobalEnable(&Gpio_Button);

    /*Timer Interrupt Setup*/
    //Vector Table Connect
    XIntc_Connect(&Intc, TMRCTR_INT_VECTOR_ID, (XInterruptHandler)XTmrCtr_InterruptHandler,
    		&TimerCounterInst);

    XIntc_Enable(&Intc, TMRCTR_INT_VECTOR_ID);

    //Handler Connect
    XTmrCtr_SetHandler(&TimerCounterInst, TimerCounterHandler, &TimerCounterInst);

    //INTERRUPT가 걸리면 자동으로 resetValue값을 다시 읽겠다
    XTmrCtr_SetOptions(&TimerCounterInst, TIMER_CNTR_0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_SetResetValue(&TimerCounterInst, TIMER_CNTR_0, resetValue);

    //Interrupt Start
    XTmrCtr_Start(&TimerCounterInst, TIMER_CNTR_0);

    // Exception Setup
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XIntc_InterruptHandler, &Intc);
    Xil_ExceptionEnable();
}

int main()
{
    init_platform();
    Button_Init();
    Led_Init();

    XTmrCtr_Initialize(&TimerCounterInst, TMRCTR_DEVICE_ID);
    XTmrCtr_SelfTest(&TimerCounterInst, TIMER_CNTR_0);

    INTTERUPT_Init();





    while(1)
    {
    	XGpio_DiscreteWrite(&Gpio_Led, CHANNEL_1, 0x00);
    	usleep(500000);
    	XGpio_DiscreteWrite(&Gpio_Led, CHANNEL_1, 0xff);
		usleep(500000);
    }

    cleanup_platform();
    return 0;
}

void GpioHandler(void *CallBackRef) //void *CallBackRef --> void pointer 자료형을 지금 정하지 않고 나중에 정하겠다
{
   XGpio *pGpio = (XGpio *)CallBackRef;

   if((XGpio_DiscreteRead(pGpio, CHANNEL_1)&0x01)){
	   printf("Pushed Button_1!\n");
   }
   else if((XGpio_DiscreteRead(pGpio, CHANNEL_1)&0x02)){
	   printf("Pushed Button_2!\n");
   }
   else if((XGpio_DiscreteRead(pGpio, CHANNEL_1)&0x04)){
   	   printf("Pushed Button_3!\n");
      }
   else if((XGpio_DiscreteRead(pGpio, CHANNEL_1)&0x08)){
   	   printf("Pushed Button_4!\n");
      }

   XGpio_InterruptClear(pGpio, CHANNEL_1); //Clear 꼭 선언 해야함 안하면 Interrupt후에 오동작함
}

void TimerCounterHandler(void *CallBackRef, uint8_t TmrCtrNumber)
{
	static int counter = 0;
	printf("Timer/Counter Interrupt : %d\n", counter++);

}
