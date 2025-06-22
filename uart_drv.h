#ifndef UART_DRV_H_
#define UART_DRV_H_

#include "prj_com.h"
#include "ubuf.h"

typedef struct avr_uart_s avr_uart_st;

#define RX_IRQ_DIS(drv)  drv->_uart->U_UCSRB &= ~(1 << RXCIE0);
#define RX_IRQ_EN(drv)	 drv->_uart->U_UCSRB |= (1 << RXCIE0);

typedef struct avr_uart_evt_s {
	cb_size_t size;
    const uint8_t* data;
	enum {
		ERR_REASON_OVERRUN = 1,
		ERR_REASON_PARITY,
		ERR_REASON_FRAME,
		ERR_REASON_NOMEM
	} err_reason ;
	enum {
		UART_EVT_TRANSMIT_COMPL = 1,
		UART_EVT_RECEIVE_COMPL,
		UART_EVT_TRANSMIT_ERR,
		UART_EVT_RECEIVE_ERR,
	} evt_type ;
} avr_uart_evt_st;

typedef struct uart_drv_s uart_drv_st;
typedef void (uart_drv_event_clbk_ft) ( uart_drv_st* drv, avr_uart_evt_st* evt );

struct uart_drv_s {
	ubuf_st _rx_cbuf;
	ubuf_st _tx_cbuf;
	avr_uart_st* _uart;
	avr_uart_evt_st _evt;
	uint8_t _rxsize;
	uint8_t* _rxdata;
	prj_com_state_et _state;
	uart_drv_event_clbk_ft* event_handler;
	struct {
		uint8_t _inited : 1;
		uint8_t _abort : 1;
	} _flags;
};

struct avr_uart_s {
	volatile uint8_t U_UCSRA;
	volatile uint8_t U_UCSRB;
	volatile uint8_t U_UCSRC;
	volatile const uint8_t __RESERVED; // Do not use this field!
	volatile uint16_t U_UBRR;
	volatile uint8_t UDR;
};
	
typedef struct avr_uart_init_s {
	uint32_t baud;
	struct {
		uint8_t parity :1;
		uint8_t double_stop :1;
		uint8_t reverse_clock :1;
	} mode;
	enum {
		CHAR_SIZE_5 = 0,
		CHAR_SIZE_6,
		CHAR_SIZE_7,
		CHAR_SIZE_8,
		CHAR_SIZE_9 = 7
	} char_size;
} avr_uart_init_st;	

#define UART0 ((avr_uart_st*) & UCSR0A)
#define UART1 ((avr_uart_st*) & UCSR1A)

/*------------------------------------------- Fast UART Function (No base checks) ------------------------------------------ */
#define uart_drv_write_buf(drv, data, size) CBufWrite(&drv->_tx_cbuf, data, size);
#define uart_drv_write_buf_flash(drv, data, size) CBufWrite_P(&drv->_tx_cbuf, data, size);
#define uart_drv_write_char(drv, char) Ubuf_WriteB(&drv->_tx_cbuf, char);
#define uart_drv_transmit(drv) { drv->_uart->U_UCSRB |= 1 << UDRIE0; }

prj_com_op_res_et uart_drv_transmit_it_fast ( uart_drv_st* drv, const uint8_t* data, cb_size_t size );
prj_com_op_res_et uart_drv_transmit_it_fast_P ( uart_drv_st* drv, const uint8_t* data, cb_size_t size );

/*----------------------------------------------------- Reliable function -------------------------------------------------- */
prj_com_op_res_et uart_drv_init_bufs( uart_drv_st* drv, uint8_t* tx_buf, uint8_t* rx_buf, cb_size_t tx_size, cb_size_t rx_size );
prj_com_op_res_et uart_drv_init ( uart_drv_st* drv, avr_uart_st* uart, avr_uart_init_st* init );
prj_com_op_res_et uart_drv_transmit_it ( uart_drv_st* drv, const uint8_t* data, cb_size_t size );
prj_com_op_res_et uart_drv_transmit_it_P ( uart_drv_st* drv, const uint8_t* data, cb_size_t size );

prj_com_op_res_et uart_drv_rx_start ( uart_drv_st* drv );
prj_com_op_res_et uart_drv_rx_size ( uart_drv_st* drv, uint8_t* data, uint8_t size );
prj_com_op_res_et uart_drv_rx_abort ( uart_drv_st* drv );
bool uart_drv_put_char (uart_drv_st* drv, char symbol);

cb_size_t uart_drv_read ( uart_drv_st* drv, uint8_t* data, cb_size_t size );
cb_size_t uart_drv_get_received ( uart_drv_st* drv );
void uart_drv_wait_tx (uart_drv_st* drv);

void uart_drv_tx_compl_irq ( uart_drv_st* drv );
void uart_drv_rx_byte_irq ( uart_drv_st* drv );
void uart_drv_tx_empty_irq ( uart_drv_st* drv );

#endif /* UART_DRV_H_ */