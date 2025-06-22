#include "uart_drv.h"
#include <stdbool.h>
#include <avr/io.h>

#define UBRR_MAX 4095
#define TX_IRQ_DIS(drv)	 uint8_t bee_en = (drv->_uart->U_UCSRB & (1 << UDRIE0)); drv->_uart->U_UCSRB &= ~(1 << UDRIE0); 
#define TX_IRQ_EN(drv)	 do { if(bee_en) drv->_uart->U_UCSRB |= (1 << UDRIE0); } while(0)


static uint16_t calc_speed_reg ( uint32_t baud, bool* sp_x2 )
{
	uint32_t acc_no_x2 = F_CPU % ( 16 * baud );
	uint32_t acc_x2 = F_CPU % ( 8 * baud );
	if(acc_x2 < acc_no_x2){
		uint32_t dev_x2 = ( F_CPU / ( 8 * baud ) ) - 1;
		if(dev_x2 < UBRR_MAX){
			*sp_x2 = true;
			return (uint16_t) dev_x2;
		} 
	} 
	uint32_t dev_no_x2 = ( F_CPU / ( 16 * baud ) ) - 1;
	if(dev_no_x2 < UBRR_MAX){
		return (uint16_t) dev_no_x2;
	} else {
		return UBRR_MAX + 1; // Error value;
	}
	return UBRR_MAX + 1;
}

static void check_init_state (uart_drv_st* drv)
{
	if(drv && drv->_uart && drv->_rx_cbuf._mem && drv->_tx_cbuf._mem){
		drv->_flags._inited = true;
	}
}

prj_com_op_res_et uart_drv_init_bufs(uart_drv_st* drv, uint8_t* tx_buf, uint8_t* rx_buf, cb_size_t tx_size, cb_size_t rx_size)
{
	if(!(tx_buf && rx_buf && tx_size && rx_size)){
		return OP_ERROR_NULL;
	}
	Ubuf_Init(&drv->_rx_cbuf, rx_buf, rx_size);
	Ubuf_Init(&drv->_tx_cbuf, tx_buf, tx_size);
	check_init_state (drv);
	return OP_OK;
}

prj_com_op_res_et uart_drv_rx_start(uart_drv_st* drv)
{
	if(drv){
		if(drv->_flags._inited){
			drv->_uart->U_UCSRB |=  (1 << RXCIE0) |  (1 << RXEN0);
			drv->_state = STATE_READY;
			return OP_OK;
		}
		return OP_IVALID_STATE;
	}
	return OP_ERROR_NULL;
}

prj_com_op_res_et uart_drv_rx_size ( uart_drv_st* drv, uint8_t* data, uint8_t size )
{	
	if(drv && size && data){
		if(!drv->_rxsize && drv->_state == STATE_READY){
			drv->_rxdata = data;
			drv->_state = STATE_BUSY;
			drv->_rxsize = size; // Atomic
			return OP_OK;
		}
		return OP_ERROR_FORBIDEN;
	}
	return OP_ERROR_NULL;
}

prj_com_op_res_et uart_drv_rx_abort ( uart_drv_st* drv )
{
	drv->_flags._abort = true;
	if(drv->_state == STATE_BUSY){
		drv->_rxsize = 0;
		drv->_state = STATE_READY;
		drv->_flags._abort = false;
	}
	return OP_OK;
}

prj_com_op_res_et uart_drv_init ( uart_drv_st* drv, avr_uart_st* uart, avr_uart_init_st* init )
{
	if(drv && uart && init){
		drv->_uart = uart;
		bool sp_x2 = false;
		uart->U_UBRR = calc_speed_reg(init->baud, &sp_x2);
		if(sp_x2){
			uart->U_UCSRA |= 1 << U2X0;
		}
		uart->U_UCSRC |= ((init->char_size & 0x03) << 1) | (init->mode.reverse_clock)
		| ((init->mode.double_stop) << 3) | ((init->mode.parity) << 5);
		uart->U_UCSRB |= (init->char_size & 0x04) | (1 << TXEN0) | (1 << TXCIE0);
		check_init_state (drv);
		return OP_OK;
	}
	return OP_ERROR_NULL;
}

bool uart_drv_put_char (uart_drv_st* drv, char symbol)
{
	if(Ubuf_WriteB(&drv->_tx_cbuf, symbol)){
		drv->_uart->U_UCSRB |= 1 << UDRIE0;
		return true;
	}
	return false;
}

prj_com_op_res_et uart_drv_transmit_it ( uart_drv_st* drv, const uint8_t* data, cb_size_t size )
{
	if(drv && data && size && drv->_flags._inited){
		TX_IRQ_DIS(drv);
		cb_size_t free_s = Ubuf_GetFreeSize(&drv->_tx_cbuf);
		TX_IRQ_EN(drv);
		if(free_s >= size){
			Ubuf_WriteData(&drv->_tx_cbuf, data, size);
			drv->_uart->U_UCSRB |= 1 << UDRIE0;
			return OP_OK;
		}
		drv->_evt.evt_type = UART_EVT_TRANSMIT_ERR;
		drv->_evt.err_reason = ERR_REASON_NOMEM;
		drv->_evt.data = data;
		drv->_evt.size = size;
		if(drv->event_handler){
			drv->event_handler(drv, &drv->_evt);
		}
		return OP_NO_MEM;
	}
	return OP_ERROR_NULL;
}

prj_com_op_res_et uart_drv_transmit_it_fast ( uart_drv_st* drv, const uint8_t* data, cb_size_t size )
{
	Ubuf_WriteData(&drv->_tx_cbuf, data, size);
	drv->_uart->U_UCSRB |= 1 << UDRIE0;
	return OP_OK;
}

prj_com_op_res_et uart_drv_transmit_it_P ( uart_drv_st* drv, const uint8_t* data, cb_size_t size )
{
	if(drv && data && size && drv->_flags._inited){
		TX_IRQ_DIS(drv);
		cb_size_t free_s = Ubuf_GetFreeSize(&drv->_tx_cbuf);
		TX_IRQ_EN(drv);
		if(free_s >= size){
			Ubuf_WriteData_P(&drv->_tx_cbuf, data, size);
			drv->_uart->U_UCSRB |= 1 << UDRIE0;
			return OP_OK;
		}
		drv->_evt.evt_type = UART_EVT_TRANSMIT_ERR;
		drv->_evt.err_reason = ERR_REASON_NOMEM;
		drv->_evt.data = data;
		drv->_evt.size = size;
		if(drv->event_handler){
			drv->event_handler(drv, &drv->_evt);
		}
		return OP_NO_MEM;
	}
	return OP_ERROR_NULL;
}

prj_com_op_res_et uart_drv_transmit_it_fast_P ( uart_drv_st* drv, const uint8_t* data, cb_size_t size )
{
	Ubuf_WriteData_P(&drv->_tx_cbuf, data, size);
	drv->_uart->U_UCSRB |= 1 << UDRIE0;
	return OP_OK;
}

void uart_drv_tx_empty_irq ( uart_drv_st* drv )
{
	// TODO Add base checks
	bool have_data = false;
	uint8_t tx_byte = Ubuf_ReadB(&drv->_tx_cbuf, &have_data);
	if(have_data){
		drv->_uart->UDR = tx_byte;
		return;
	}
	drv->_uart->U_UCSRB &= ~(1 << UDRIE0);
}

void uart_drv_rx_byte_irq ( uart_drv_st* drv )
{
	// TODO Add base checks
	if(drv->_uart->U_UCSRA & ((1 << FE0) | (1 << UPE0) | (1 << DOR0))){
		drv->_evt.evt_type = UART_EVT_RECEIVE_ERR;
		if(drv->_uart->U_UCSRA & (1 << FE0)){
			drv->_evt.err_reason = ERR_REASON_FRAME;
		} else if(drv->_uart->U_UCSRA & (1 << UPE0)){
			drv->_evt.err_reason = ERR_REASON_PARITY;
		} else { // This error blocked receive 
			drv->_evt.err_reason = ERR_REASON_OVERRUN;
		}
		Ubuf_WriteB(&drv->_rx_cbuf, drv->_uart->UDR);	
		Ubuf_GetFlatMemForRead(&drv->_rx_cbuf, (void*)&drv->_evt.data, &drv->_evt.size);
		if(drv->event_handler){
			drv->event_handler(drv, &drv->_evt);
		}
	}
	
	if(drv->_rxsize && !drv->_flags._abort){
		*(drv->_rxdata) = drv->_uart->UDR;
		drv->_rxdata++;
		drv->_rxsize--;
		if(!drv->_rxsize){
			drv->_evt.evt_type = UART_EVT_RECEIVE_COMPL;
			drv->_evt.data = drv->_rxdata;
			drv->_evt.size = drv->_rxsize;
			drv->_state = STATE_READY;
			if(drv->event_handler){
				drv->event_handler(drv, &drv->_evt);
			}
		}
	} else {
		Ubuf_WriteB(&drv->_rx_cbuf, drv->_uart->UDR);
	}	
}

void uart_drv_tx_compl_irq ( uart_drv_st* drv )
{
	drv->_evt.evt_type = UART_EVT_TRANSMIT_COMPL;
	// TODO Add event data end size params
	if(drv->event_handler){
		drv->event_handler(drv, &drv->_evt);
	}
}

cb_size_t uart_drv_get_received (uart_drv_st* drv)
{
	RX_IRQ_DIS(drv);
	cb_size_t rec = Ubuf_GetDataSize(&drv->_rx_cbuf);
	RX_IRQ_EN(drv);
	return rec;
}

cb_size_t uart_drv_read(uart_drv_st* drv, uint8_t* data, cb_size_t size)
{
	return Ubuf_ReadData(&drv->_rx_cbuf, data, size);
}

void uart_drv_wait_tx (uart_drv_st* drv)
{
	/*while(drv->_evt != UART_EVT_TRANSMIT_COMPL || drv->_evt != UART_EVT_TRANSMIT_ERR);*/
}
