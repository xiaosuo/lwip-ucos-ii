#ifndef __SIO_CPU_H__
#define __SIO_CPU_H__

#include "stm32f10x_usart.h"

#define sio_rx_ok(fd) (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == SET)
#define sio_rx(fd) USART_ReceiveData(USART2)
#define sio_tx_ok(fd) (USART_GetFlagStatus(USART2, USART_FLAG_TC) == SET)
#define sio_tx(fd, c) USART_SendData(USART2, c)
#define sio_enable_tx_irq(fd) USART_ITConfig(USART2, USART_IT_TC, ENABLE)
#define sio_disable_tx_irq(fd) USART_ITConfig(USART2, USART_IT_TC, DISABLE)
#define sio_enable_rx_irq(fd) USART_ITConfig(USART2, USART_IT_RXNE, ENABLE)
#define sio_disable_rx_irq(fd) USART_ITConfig(USART2, USART_IT_RXNE, DISABLE)

#endif /* __SIO_CPU_H__ */
