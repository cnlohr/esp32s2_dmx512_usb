#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hidapi.h"
#include "hidapi.c"

#define VID 0x303a
#define PID 0x4004

hid_device * hd;

int tries = 0;
int alignlen = 4;

int req_sandbox = 1024;


uint32_t EHSVtoHEX( uint8_t hue, uint8_t sat, uint8_t val );

int main( int argc, char ** argv )
{
	int r;
	hid_init();
	hd = hid_open( VID, PID, L"cndmx512v001");
	if( !hd ) { fprintf( stderr, "Could not open USB\n" ); return -94; }
	// Disable mode.
	uint8_t colodata[800] = { 0 };
	int channels = 512;
	int frame = 0;
	do
	{
		frame++;
		int i;
		int idlight = frame%(512/3);
		for( i = 0; i < 512/3+1; i++ )
		{
/*
			int color = EHSVtoHEX( i*10 + frame, 0xff, 0xff );
			colodata[i*3+0] = color & 0xff;
			colodata[i*3+1] = (color>>8) & 0xff;
			colodata[i*3+2] = (color>>16) & 0xff;
*/
			colodata[i*3+0] = 
			colodata[i*3+1] = 
			colodata[i*3+2] = (idlight==i)?200:0;
		}
	

		int chunk;
		int chunks = (channels + 247) / 248;
		for( chunk = chunks-1; chunk >= 0; chunk-- )
		{
			// Upon sending 0th chunk, it will transmit the acutal signal.
			uint8_t rdata[256] = { 0 };

			int offset = chunk * 248;
			int remain = channels - offset;
			if( remain > 248 ) remain = 248;

			rdata[0] = 0xad;  // Feature Report ID
			rdata[1] = 0x73;
			rdata[2] = offset/4;
			rdata[3] = remain;
			memcpy( rdata+4, colodata + chunk * 248, remain );
			//printf( "%d %d rdata[10] = %02x\n", offset, remain, rdata[15] );
			do
			{
				r = hid_send_feature_report( hd, rdata, 255 );
				if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
			} while ( r < 6 );
			tries = 0;
		}
		printf( "." ); fflush( stdout );
	} while( 1 );
	
}




uint32_t EHSVtoHEX( uint8_t hue, uint8_t sat, uint8_t val )
{
	#define SIXTH1 43
	#define SIXTH2 85
	#define SIXTH3 128
	#define SIXTH4 171
	#define SIXTH5 213

	uint16_t or = 0, og = 0, ob = 0;

	hue -= SIXTH1; //Off by 60 degrees.

	//TODO: There are colors that overlap here, consider 
	//tweaking this to make the best use of the colorspace.

	if( hue < SIXTH1 ) //Ok: Yellow->Red.
	{
		or = 255;
		og = 255 - ((uint16_t)hue * 255) / (SIXTH1);
	}
	else if( hue < SIXTH2 ) //Ok: Red->Purple
	{
		or = 255;
		ob = (uint16_t)hue*255 / SIXTH1 - 255;
	}
	else if( hue < SIXTH3 )  //Ok: Purple->Blue
	{
		ob = 255;
		or = ((SIXTH3-hue) * 255) / (SIXTH1);
	}
	else if( hue < SIXTH4 ) //Ok: Blue->Cyan
	{
		ob = 255;
		og = (hue - SIXTH3)*255 / SIXTH1;
	}
	else if( hue < SIXTH5 ) //Ok: Cyan->Green.
	{
		og = 255;
		ob = ((SIXTH5-hue)*255) / SIXTH1;
	}
	else //Green->Yellow
	{
		og = 255;
		or = (hue - SIXTH5) * 255 / SIXTH1;
	}

	uint16_t rv = val;
	if( rv > 128 ) rv++;
	uint16_t rs = sat;
	if( rs > 128 ) rs++;

	//or, og, ob range from 0...255 now.
	//Need to apply saturation and value.

	or = (or * val)>>8;
	og = (og * val)>>8;
	ob = (ob * val)>>8;

	//OR..OB == 0..65025
	or = or * rs + 255 * (256-rs);
	og = og * rs + 255 * (256-rs);
	ob = ob * rs + 255 * (256-rs);
//printf( "__%d %d %d =-> %d\n", or, og, ob, rs );

	or >>= 8;
	og >>= 8;
	ob >>= 8;

	return or | (og<<8) | ((uint32_t)ob<<16);
}

