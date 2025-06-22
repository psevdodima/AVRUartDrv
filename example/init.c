#include <avr/interrupt.h>
#include "uart_drv.h"

uart_drv_st uart_drv;
uint8_t txBuf [128];
uint8_t rxBuf [128];

void uart_event_handler ( uart_drv_st* drv, avr_uart_evt_st* evt )
{
	switch(evt->evt_type){
		case UART_EVT_TRANSMIT_COMPL:
			// TODO
			break;
			
		case UART_EVT_RECEIVE_COMPL:
			// TODO
			break;
			
		case UART_EVT_TRANSMIT_ERR:
			// TODO
			break;
			
		case UART_EVT_RECEIVE_ERR:
			// TODO
			break;
	}
}

int main(void)
{
	avr_uart_init_st uart_init = {
		.baud = 115200,
		.char_size = CHAR_SIZE_8,
		.mode = {
			.double_stop = false,
			.parity = false,
			.reverse_clock = false
		}
	}
	uart_drv.event_handler = uart_event_handler;
	uart_drv_init_bufs (&uart_drv, txBuf, rxBuf, 128, 128);
	uart_drv_init (&uart_drv, UART1, &uart_init);
	uart_drv_rx_start (&uart_drv);
	uart_drv_transmit_it(&uart_drv, "Hello world", 12);
	
	while(1){
	}	
}

ISR (USART1_RX_vect)
{
	uart_drv_rx_byte_irq (&uart_drv);
}

ISR (USART1_UDRE_vect)
{
	uart_drv_tx_empty_irq (&uart_drv);
}

ISR (USART1_TX_vect)
{
	uart_drv_tx_compl_irq (uart_drv);
}