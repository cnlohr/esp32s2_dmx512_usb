/* USB console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"
#include "driver/uart.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "soc/i2s_reg.h"
#include "soc/periph_defs.h"
#include "rom/lldesc.h"
#include "private_include/regi2c_apll.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "regi2c_ctrl.h"
#include "soc/uart_reg.h"
#include "driver/periph_ctrl.h"
#include "hal/uart_ll.h"

#define DMX_UART UART_NUM_1
#define UART_EMPTY_THRESH_DEFAULT 1
#define UART_TX_BUFFER_SIZE 128

#define BUFF_SIZE_BYTES 2048

#define MAX_DMX 1024

uint8_t dmx_buffer[1024] = { 0 };
uint8_t transmit_buffer[1024] = { 0 };
volatile int dmx_buffer_snow = 0;
volatile int highest_send = 0;
volatile int transmit_total = 0;
volatile int done_send = 1;
static intr_handle_t uart_intr_handle;


int isr_countOut;

static void IRAM_ATTR uart_isr_fast(void* arg) {

    int uart_intr_status = REG_READ( UART_INT_ST_REG( DMX_UART) ) ;
    //Exit form while loop
    if (uart_intr_status == 0)
	{
		return;
	}
	if( uart_intr_status & UART_TX_BRK_DONE_INT_RAW )
	{
		done_send = 1;
		SET_PERI_REG_MASK(UART_CONF0_REG(DMX_UART), UART_TXD_BRK );
		WRITE_PERI_REG( UART_INT_CLR_REG(DMX_UART), UART_TX_BRK_DONE_INT_RAW );
	}
	if( uart_intr_status & UART_INTR_TXFIFO_EMPTY )
	{
		int local_highest = transmit_total;
		int local_snow = dmx_buffer_snow;
		int i;
		for( i = 0; i < UART_TX_BUFFER_SIZE-UART_EMPTY_THRESH_DEFAULT; i++ )
		{
			if( local_snow < local_highest )
			{
			    WRITE_PERI_REG( UART_FIFO_AHB_REG(DMX_UART), transmit_buffer[local_snow] );
				local_snow++;
			}
			else
			{
				// No data left.
			    CLEAR_PERI_REG_MASK( UART_INT_ENA_REG(DMX_UART), UART_INTR_TXFIFO_EMPTY );
			    SET_PERI_REG_MASK( UART_INT_ENA_REG(DMX_UART), UART_TX_BRK_DONE_INT_RAW );
				SET_PERI_REG_MASK(UART_CONF0_REG(DMX_UART), UART_TXD_BRK );
				WRITE_PERI_REG( UART_IDLE_CONF_REG(DMX_UART), 40<<UART_TX_BRK_NUM_S );
			}
		}
		dmx_buffer_snow = local_snow;
		WRITE_PERI_REG( UART_INT_CLR_REG(DMX_UART),UART_INTR_TXFIFO_EMPTY );
	}

	++isr_countOut;
}


uint16_t tud_hid_get_report_cb(uint8_t itf,
							   uint8_t report_id,
							   hid_report_type_t report_type,
							   uint8_t* buffer,
							   uint16_t reqlen)
{
	if( report_id == 0xad )
	{
		// DMX packet
		// No special packet supported.
	}
	else
	{
		printf( "Custom Report Get %d %d %02x [%d]\n", itf, report_id, report_type, reqlen );
	}
	return 0;
}


void tud_hid_set_report_cb(uint8_t itf,
						   uint8_t report_id,
						   hid_report_type_t report_type,
						   uint8_t const* buffer,
						   uint16_t bufsize )
{
	// Code 0xad has a special setting in TUD_HID_REPORT_DESC_GAMEPAD to allow unrestricted HIDAPI access from Windows.
	if( report_id >= 0xaa && report_id <= 0xad && buffer[1] == 0x73 )
	{
		// DMX packet
		int offset = buffer[2] * 4;
		int towrite = buffer[3];
		if( offset + towrite > sizeof( dmx_buffer ) )
			towrite = sizeof( dmx_buffer ) - offset;
		memcpy( dmx_buffer + offset, buffer + 4, towrite );

		if( offset + towrite > highest_send ) highest_send = offset + towrite;

		//printf( "%02x %02x %02x %02x %d %d %d %d %d\n", buffer[0], buffer[1], buffer[2], buffer[3], highest_send, offset, bufsize, buffer[bufsize], towrite );

		if( offset == 0 )
		{
			while( !done_send ) taskYIELD();
			memcpy( transmit_buffer, dmx_buffer, highest_send );			
			dmx_buffer_snow = 0;
			done_send = 0;
			transmit_total = highest_send;
			ets_delay_us(15);
		    WRITE_PERI_REG( UART_FIFO_AHB_REG(DMX_UART), 0x00 ); // Mark byte
		    SET_PERI_REG_MASK( UART_INT_ENA_REG(DMX_UART), UART_INTR_TXFIFO_EMPTY );	
		}
	}
	else
	{
		printf( "Custom Report Set %d %d[%02x] %02x [%d]\n", itf, report_id, buffer[0], report_type, bufsize );
	}
}


void app_main(void)
{
	printf( "Starting\n" );
	fflush(stdout);

	uart_config_t uart_config = {
		.baud_rate = 250000,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};

	ESP_ERROR_CHECK(uart_param_config(DMX_UART, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(DMX_UART, 18, -1, -1, -1));

	// Use the IO matrix to create the inverse of TX on pin 17.
	gpio_matrix_out( GPIO_NUM_17, U1TXD_OUT_IDX, 1, 0 );

	// Start returning data to the application
	esp_intr_alloc(ETS_UART1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, uart_isr_fast, (void*)REG_UART_BASE(1), &uart_intr_handle);
	esp_intr_enable(uart_intr_handle);

	// Maximize the drive strength.
	gpio_set_drive_capability( GPIO_NUM_17, GPIO_DRIVE_CAP_3 );
	gpio_set_drive_capability( GPIO_NUM_18, GPIO_DRIVE_CAP_3 );


	/* Initialize USB peripheral */
	tinyusb_config_t tusb_cfg = {};
	tinyusb_driver_install(&tusb_cfg);

	uart_enable_tx_intr( DMX_UART, 1, UART_EMPTY_THRESH_DEFAULT);

	printf( "%08x\n", REG_READ( UART_MEM_TX_STATUS_REG(1) ) );
//	vTaskSuspend( 0 );

//	uart_write_bytes_with_break(DMX_UART, "", 1, 20 ); // Can't use uart_write_bytes_with_break because break needs to go right before frame.

	/* Main loop */
	while(true)
	{
		printf( "%d\n", isr_countOut);
		vTaskDelay( 100 );
	}
	return;
}
