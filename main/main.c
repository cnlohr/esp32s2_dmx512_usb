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

uint8_t dmx_buffer[1025];

uint16_t tud_hid_get_report_cb(uint8_t itf,
							   uint8_t report_id,
							   hid_report_type_t report_type,
							   uint8_t* buffer,
							   uint16_t reqlen)
{
	if( report_id == 0xad )
	{
		// DMX packet
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
	if( report_id == 0xad )
	{
		// DMX packet
		int offset = buffer[0] * 4;
		if( offset + bufsize > sizeof( dmx_buffer ) )
		{
			
		}
	}
	else
	{
		printf( "Custom Report Set %d %d %02x [%d]\n", itf, report_id, report_type,bufsize );
	}
}

void app_main(void)
{
	printf( "Starting\n" );
	fflush(stdout);

	/* Initialize USB peripheral */
	tinyusb_config_t tusb_cfg = {};
	tinyusb_driver_install(&tusb_cfg);

	/* Main loop */
	while(true) {
		printf( "Tick\n" );
		// Yield to let the rest of the RTOS run
		//taskYIELD();
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	return;
}
