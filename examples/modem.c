#include "stm32f10x_usart.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "misc.h"
#include "ucos_ii.h"
#include "ppp.h"

#include "lwip/tcpip.h"
#include "lwip/err.h"
#include "lwip/dns.h"

static OS_EVENT *__sem;

static void tcpip_init_done(void *arg)
{
	OSSemPost(__sem);
}

void modem_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	USART_InitTypeDef USART_InitStruct;
	NVIC_InitTypeDef NVIC_InitStruct;
	INT8U err;

	__sem = OSSemCreate(0);
	LWIP_ASSERT("OSSemCreate", __sem);

	tcpip_init(tcpip_init_done, NULL);
	OSSemPend(__sem, 0, &err);
	OSSemPost(__sem);

	pppInit();
	pppSetAuth(PPPAUTHTYPE_ANY, "cmnet", "cmnet");

	sio_open(0);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStruct); 

	NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 5;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 5;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);

	USART_StructInit(&USART_InitStruct);
	USART_InitStruct.USART_BaudRate = 115200;
	USART_Init(USART2, &USART_InitStruct);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART2, USART_IT_TC, ENABLE);

	USART_Cmd(USART2, ENABLE);
}

void USART2_IRQHandler(void)
{
	OSIntEnter();
	sio_tx_complete(NULL);
	sio_rx_complete(NULL);
	OSIntExit();
}

static INT32U read_line(INT8U *buf, INT32U size)
{
	INT8U c;
	INT32U len = 0;

	while (size-- > 0) {
		c = sio_recv(NULL);
		*buf++ = c;
		len++;
		if (c == '\n')
			break;
	}

	return len;
}

static void write_str(const INT8U *str)
{
	sio_write(NULL, (u8_t *)str, strlen((const char *)str));
}

static void link_status_cb(void *ctx, int errCode, void *arg)
{
	if (errCode == PPPERR_NONE) {
		struct ppp_addrs *addrs = arg;

		if (addrs->dns1.addr)
			dns_setserver(0, &addrs->dns1);
		if (addrs->dns2.addr)
			dns_setserver(1, &addrs->dns2);
	} else {
		OSSemPost(__sem);
		sys_thread_free(PPP_THREAD_PRIO);
	}
}

void modem_task(void *p_arg)
{
	INT8U err;
	int pd = -1;
	INT8U buf[80];
	INT32U len;

	write_str("AT+IPR=115200\r\n");
	OSTimeDly(OS_TICKS_PER_SEC);
	while (sio_tryread(NULL, buf, sizeof(buf)) > 0)
		;

	while (1) {
again:
		OSSemPend(__sem, 0, &err);
		LWIP_ASSERT("OSSemPend", err == OS_ERR_NONE);
		if (pd >= 0) {
			pppClose(pd);
			pd = -1;
		}

		write_str("AT+CGDCONT=1,\"IP\",\"CMNET\"\r\n");
		while (1) {
			len = read_line(buf, sizeof(buf));
			if (len > 2 && memcmp(buf, "OK", 2) == 0) {
				break;
			} else if (len > 5 && memcmp(buf, "ERROR", 5) == 0) {
				err = OSSemPost(__sem);
				OSTimeDly(OS_TICKS_PER_SEC * 3);
				goto again;
			}
		}

		write_str("ATD*99***1#\r\n");
		while (1) {
			len = read_line(buf, sizeof(buf));
			LWIP_ASSERT("read_line", len < sizeof(buf));
			if (len > 7 && memcmp(buf, "CONNECT", 7) == 0) {
				break;
			} else if ((len > 10 &&
				    memcmp(buf, "NO CARRIER", 10) == 0) ||
				   (len > 4 && memcmp(buf, "BUSY", 4) == 0) ||
				   (len > 7 && memcmp(buf, "DELAYED", 7) == 0) ||
				   (len > 5 && memcmp(buf, "ERROR", 5) == 0) ||
				   (len > 11 &&
				    memcmp(buf, "NO DIALTONE", 11) == 0) ||
				   (len > 9 &&
				    memcmp(buf, "NO ANSWER", 9) == 0)) {
				err = OSSemPost(__sem);
				OSTimeDly(OS_TICKS_PER_SEC * 3);
				goto again;
			} else {
				continue;
			}
		}

		pd = pppOverSerialOpen(NULL, link_status_cb, NULL);
		LWIP_ASSERT("pppOverSerialOpen", pd >= 0);
	}
}
