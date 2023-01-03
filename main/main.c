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

#define DMX_UART UART_NUM_1

#define MAX_DMX 1024
uint8_t dmx_buffer[1024 + 4] = { 0 };
uint8_t send_buffer[1024];
volatile int highest_send = 0;
volatile int do_send;

SemaphoreHandle_t DoneSendMutex;
SemaphoreHandle_t PleaseSendMutex;

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
	if( report_id == 0xad && buffer[1] == 0x73 )
	{
		// DMX packet
		int offset = buffer[2] * 4;
		int towrite = buffer[3];
		if( offset + towrite > sizeof( dmx_buffer ) )
			towrite = sizeof( dmx_buffer ) - offset;
		memcpy( dmx_buffer + offset + 4, buffer + 4, towrite );

		if( offset + towrite > highest_send ) highest_send = offset + towrite;

		//printf( "%02x %02x %02x %02x %d %d %d %d %d\n", buffer[0], buffer[1], buffer[2], buffer[3], highest_send, offset, bufsize, buffer[bufsize], towrite );

		if( offset == 0 )
		{
			// Actually send.
			xSemaphoreTake( DoneSendMutex, portMAX_DELAY );
			do_send = highest_send;
			memcpy( send_buffer, dmx_buffer, highest_send );
			highest_send = 0;
			xSemaphoreGive( PleaseSendMutex );
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

	PleaseSendMutex = xSemaphoreCreateBinary();
	DoneSendMutex = xSemaphoreCreateBinary();

	uart_config_t uart_config = {
		.baud_rate = 250000,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};

	ESP_ERROR_CHECK(uart_param_config(DMX_UART, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(DMX_UART, 18, -1, -1, -1));

	QueueHandle_t uart_queue;
	// Install UART driver using an event queue here
	ESP_ERROR_CHECK(uart_driver_install(DMX_UART, 256, 256, 10, &uart_queue, 0));

	// Use the IO matrix to create the inverse of TX on pin 17.
	gpio_matrix_out( GPIO_NUM_17, U1TXD_OUT_IDX, 1, 0 );

	// Maximize the drive strength.
	gpio_set_drive_capability( GPIO_NUM_17, GPIO_DRIVE_CAP_3 );
	gpio_set_drive_capability( GPIO_NUM_18, GPIO_DRIVE_CAP_3 );


	/* Initialize USB peripheral */
	tinyusb_config_t tusb_cfg = {};
	tinyusb_driver_install(&tusb_cfg);

//	vTaskSuspend( 0 );

//	uart_write_bytes_with_break(DMX_UART, "", 1, 20 ); // Can't use uart_write_bytes_with_break because break needs to go right before frame.

	/* Main loop */
	while(true)
	{
		xSemaphoreGive( DoneSendMutex );
		xSemaphoreTake( PleaseSendMutex, portMAX_DELAY );
		uart_wait_tx_done(DMX_UART, portMAX_DELAY);
		ets_delay_us( 140 );
		// I have literally no idea why this needs to be called from the main task instead of the USB task.
		uart_write_bytes_with_break(DMX_UART, send_buffer+3, do_send+1, 20 );
		do_send = 0;
	}
	return;
}
