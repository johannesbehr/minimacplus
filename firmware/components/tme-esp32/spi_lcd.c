//Driver for the LCD on an ESP32-Wrover-Kit board
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_alloc_caps.h"
#include "mpumouse.h"
#include "../tme/mouse.h"
#include "keyboard.h"

#include "../odroid/odroid_input.h"
#include "spi_semaphore.h"

#define PIN_NUM_MISO GPIO_NUM_19 //25
#define PIN_NUM_MOSI GPIO_NUM_23 // 23
#define PIN_NUM_CLK  GPIO_NUM_18 //19
#define PIN_NUM_CS   GPIO_NUM_5 //22

#define PIN_NUM_DC   GPIO_NUM_21 //21
#define PIN_NUM_RST  GPIO_NUM_19//18
#define PIN_NUM_BCKL GPIO_NUM_14//5		//backlight enable


/*
 The ILI9341 needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} ili_init_cmd_t;

static const ili_init_cmd_t ili_init_cmds[]={
    {0xCF, {0x00, 0x83, 0X30}, 3},
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    {0xE8, {0x85, 0x01, 0x79}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x26}, 1},
    {0xC1, {0x11}, 1},
    {0xC5, {0x35, 0x3E}, 2},
    {0xC7, {0xBE}, 1},
    {0x36, {0xE8}, 1},
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},
    {0xF2, {0x08}, 1},
    {0x26, {0x01}, 1},
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4}, 
    {0x2C, {0}, 0},
    {0xB7, {0x07}, 1},
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

const int DUTY_MAX = 0x1fff;
const int LCD_BACKLIGHT_ON_VALUE = 1;

static spi_device_handle_t spi;
bool isBackLightIntialized = false;

int d_offset_x = 0;
int d_offset_y = 0;

static void backlight_init()
{
    // Note: In esp-idf v3.0, settings flash speed to 80Mhz causes the LCD controller
    // to malfunction after a soft-reset.

    // (duty range is 0 ~ ((2**bit_num)-1)


    //configure timer0
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));

    ledc_timer.bit_num = LEDC_TIMER_13_BIT; //set timer counter bit number
    ledc_timer.freq_hz = 5000;              //set frequency of pwm
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;   //timer mode,
    ledc_timer.timer_num = LEDC_TIMER_0;    //timer index


    ledc_timer_config(&ledc_timer);


    //set the configuration
    ledc_channel_config_t ledc_channel;
    memset(&ledc_channel, 0, sizeof(ledc_channel));

    //set LEDC channel 0
    ledc_channel.channel = LEDC_CHANNEL_0;
    //set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
    ledc_channel.duty = (LCD_BACKLIGHT_ON_VALUE) ? 0 : DUTY_MAX;
    //GPIO number
    ledc_channel.gpio_num = PIN_NUM_BCKL;
    //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
    ledc_channel.intr_type = LEDC_INTR_FADE_END;
    //set LEDC mode, from ledc_mode_t
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    //set LEDC timer source, if different channel use one timer,
    //the frequency and bit_num of these channels should be the same
    ledc_channel.timer_sel = LEDC_TIMER_0;


    ledc_channel_config(&ledc_channel);


    //initialize fade service.
    ledc_fade_func_install(0);

    // duty range is 0 ~ ((2**bit_num)-1)
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (LCD_BACKLIGHT_ON_VALUE) ? DUTY_MAX : 0, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);

    isBackLightIntialized = true;
}

void backlight_percentage_set(int value)
{
    int duty = DUTY_MAX * (value * 0.01f);

    // //set the configuration
    // ledc_channel_config_t ledc_channel;
    // memset(&ledc_channel, 0, sizeof(ledc_channel));
    //
    // //set LEDC channel 0
    // ledc_channel.channel = LEDC_CHANNEL_0;
    // //set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
    // ledc_channel.duty = duty;
    // //GPIO number
    // ledc_channel.gpio_num = LCD_PIN_NUM_BCKL;
    // //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
    // ledc_channel.intr_type = LEDC_INTR_FADE_END;
    // //set LEDC mode, from ledc_mode_t
    // ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    // //set LEDC timer source, if different channel use one timer,
    // //the frequency and bit_num of these channels should be the same
    // ledc_channel.timer_sel = LEDC_TIMER_0;
    //
    //
    // ledc_channel_config(&ledc_channel);

    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}

int is_backlight_initialized()
{
    return isBackLightIntialized;
}


//Send a command to the ILI9341. Uses spi_device_transmit, which waits until the transfer is complete.
void ili_cmd(spi_device_handle_t spi, const uint8_t cmd) 
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//Send data to the ILI9341. Uses spi_device_transmit, which waits until the transfer is complete.
void ili_data(spi_device_handle_t spi, const uint8_t *data, int len) 
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void ili_spi_pre_transfer_callback(spi_transaction_t *t) 
{
    int dc=(int)t->user & 0x01;
    gpio_set_level(PIN_NUM_DC, dc);
}

//Initialize the display
void ili_init(spi_device_handle_t spi) 
{
    int cmd=0;
    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
   // gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
	/*
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
*/
	
    //Send all the commands
    while (ili_init_cmds[cmd].databytes!=0xff) {
        ili_cmd(spi, ili_init_cmds[cmd].cmd);
        ili_data(spi, ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes&0x1F);
        if (ili_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

    ///Enable backlight
    //gpio_set_level(PIN_NUM_BCKL, 0);
	
	backlight_init();

	
}


static void send_header_start(spi_device_handle_t spi, int xpos, int ypos, int w, int h)
{
    esp_err_t ret;
    int x;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[5];

    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.
    for (x=0; x<5; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x&1)==0) {
            //Even transfers are commands
            trans[x].length=8;
            trans[x].user=(void*)0;
        } else {
            //Odd transfers are data
            trans[x].length=8*4;
            trans[x].user=(void*)1;
        }
        trans[x].flags=SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0]=0x2A;           //Column Address Set
    trans[1].tx_data[0]=xpos>>8;              //Start Col High
    trans[1].tx_data[1]=xpos;              //Start Col Low
    trans[1].tx_data[2]=(xpos+w-1)>>8;       //End Col High
    trans[1].tx_data[3]=(xpos+w-1)&0xff;     //End Col Low
    trans[2].tx_data[0]=0x2B;           //Page address set
    trans[3].tx_data[0]=ypos>>8;        //Start page high
    trans[3].tx_data[1]=ypos&0xff;      //start page low
    trans[3].tx_data[2]=(ypos+h-1)>>8;    //end page high
    trans[3].tx_data[3]=(ypos+h-1)&0xff;  //end page low
    trans[4].tx_data[0]=0x2C;           //memory write

    //Queue all transactions.
    for (x=0; x<5; x++) {
        ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
        assert(ret==ESP_OK);
    }

    //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
    //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
    //finish because we may as well spend the time calculating the next line. When that is done, we can call
    //send_line_finish, which will wait for the transfers to be done and check their status.
}


void send_header_cleanup(spi_device_handle_t spi) 
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 5 transactions to be done and get back the results.
    for (int x=0; x<5; x++) {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}


volatile static uint8_t *currFbPtr=NULL;
SemaphoreHandle_t dispSem = NULL;

#define NO_SIM_TRANS 5
#define MEM_PER_TRANS 1024
extern char *img_keyboard;
extern char img_cursor[];
extern char img_cursor_mask[];
extern bool keyboard_visible;
extern unsigned char *macRam;
extern uint8_t key_inversion;
extern uint16_t  key_inversion_top;
extern uint16_t  key_inversion_left;
extern uint16_t  key_inversion_width;
extern uint16_t  key_inversion_height;


int last_mouse_y=0;

void get_mouse(int *x, int *y){
	*y = (macRam[0x840] << 8) + (macRam[0x841]);
	if(*y==0){
		// Mouse is hidden, take last known position
		*y=last_mouse_y;
	}else{
		*y = *y-15; 
		last_mouse_y = *y;
	}
	*x = (macRam[0x82A] << 8) + (macRam[0x82B]);
}

void IRAM_ATTR displayTask(void *arg) {
	int x, i;
	int idx=0;
	int inProgress=0;
	static uint16_t *dmamem[NO_SIM_TRANS];
	spi_transaction_t trans[NO_SIM_TRANS];
	spi_transaction_t *rtrans;

	esp_err_t ret;
	spi_bus_config_t buscfg={
		.miso_io_num=PIN_NUM_MISO,
		.mosi_io_num=PIN_NUM_MOSI,
		.sclk_io_num=PIN_NUM_CLK,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};
	spi_device_interface_config_t devcfg={
		.clock_speed_hz=26000000,               //Clock out at 40 MHz. Yes, that's heavily overclocked.
		.mode=0,                                //SPI mode 0
		.spics_io_num=PIN_NUM_CS,               //CS pin
		.queue_size=10,                          //We want to be able to queue 7 transactions at a time
		.pre_cb=ili_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
		.flags = SPI_DEVICE_NO_DUMMY,
	};

	printf("*** Display task starting.\n");

	spi_semaphore_take();
	
	//Initialize the SPI bus
	//ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
	//assert(ret==ESP_OK);
	//Attach the LCD to the SPI bus
	ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
	assert(ret==ESP_OK);
	//Initialize the LCD
	ili_init(spi);

	for (x=0; x<NO_SIM_TRANS; x++) {
		dmamem[x]=pvPortMallocCaps(MEM_PER_TRANS*2, MALLOC_CAP_DMA);
		assert(dmamem[x]);
		memset(&trans[x], 0, sizeof(spi_transaction_t));
		trans[x].length=MEM_PER_TRANS*4;
		trans[x].user=(void*)1;
		trans[x].tx_buffer=&dmamem[x];
	}
	
	spi_semaphore_give();
	
	
	
	while(1) {
		xSemaphoreTake(dispSem, portMAX_DELAY); // Wait until Displaydata is available
		//printf("Display: Taking semaphore..\r\n");
		spi_semaphore_take();
		//printf("Display: Got semaphore.\r\n");

		int mouse_y, mouse_x;
		get_mouse(&mouse_x, &mouse_y);
		
		if(mouse_x<512 && mouse_y<342){
			// Virtual scrolling/Follow mouse, but not on the virtual keyboard
			if(!keyboard_visible||mouse_y+16<=159){
				if(mouse_x> (305 + d_offset_x)){
					d_offset_x = (mouse_x-305);
					if(d_offset_x>512-320){
						d_offset_x = 512-320;
					}
				}else if(mouse_x -15 < d_offset_x && d_offset_x>0){
					d_offset_x = mouse_x - 15;
					if(d_offset_x <0) d_offset_x = 0;
				}
				if(mouse_y> (225 + d_offset_y)){
					d_offset_y = (mouse_y-225);
					if(d_offset_y>342-240){
						d_offset_y = 342-240;
					}
				}else if(mouse_y -15 < d_offset_y && d_offset_y>0){
					d_offset_y = mouse_y - 15;
					if(d_offset_y <0) d_offset_y = 0;
				}
			}
		}
		
		
		
		mouse_y-= d_offset_y;
		mouse_x-= d_offset_x;
		
		uint8_t *myData=(uint8_t*)currFbPtr + (d_offset_x/8) + (d_offset_y * 64) ;

		send_header_start(spi, 0, 0, 320, 240);
		send_header_cleanup(spi);
		int xpos=0;
		int ypos=0;
		int omx, omy;
		int rowOffset = ((512-320)/8);
		for (x=0; x<320*240; x+=MEM_PER_TRANS) {
			i=0; 
			while (i<MEM_PER_TRANS) {
				
				// Copy 8 bit
				for (int j=0x80; j!=0; j>>=1) {
					//dmamem[idx][i]=(*myData&j)?0:0xffff;
					
					if(keyboard_visible && 
						key_inversion &&
						xpos>key_inversion_left && 
						xpos<(key_inversion_left + key_inversion_width) &&
						ypos>key_inversion_top + 159 && 
						ypos<(key_inversion_top + key_inversion_height + 159)){
						dmamem[idx][i]=(*myData&j)?0xffff:0;
					}else{
						dmamem[idx][i]=(*myData&j)?0:0xffff;
					}

					omx = xpos - mouse_x +1;
					omy = ypos - mouse_y +1;
						
					if(keyboard_visible){
						if((mouse_y+16>=159)){
							if(omx>= 0 && omx<10 && omy >=0 && omy <16){
								int c_index = (omx + (omy*10))/8;
								int c_bit = (omx + (omy*10)) % 8;
								if(img_cursor_mask[c_index] & (0x80>>c_bit)){
									dmamem[idx][i]=(img_cursor[c_index] & (0x80>>c_bit))?0:0xffff;
								}
							}
						}
					}
					
					// Mouse rect is 10x16
					// x-mx 
					
					
					i++;
					xpos++;
				}
				myData++;
				//xpos+=8;
				
				if (xpos>=320) {
					xpos=0;
					ypos++;			
					myData+=rowOffset;
					
					// Display virtual Keyboard
					if(ypos==240-81 && keyboard_visible){
						myData = (uint8_t *)img_keyboard;
						rowOffset = 0;
					}

				}

				// Copy mouse to this fragment
				
				
			}
			trans[idx].length=MEM_PER_TRANS*16;
			trans[idx].user=(void*)1;
			trans[idx].tx_buffer=dmamem[idx];
			ret=spi_device_queue_trans(spi, &trans[idx], portMAX_DELAY);
			assert(ret==ESP_OK);

			idx++;
			if (idx>=NO_SIM_TRANS) idx=0;

			if (inProgress==NO_SIM_TRANS-1) {
				ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
				assert(ret==ESP_OK);
			} else {
				inProgress++;
			}
		}
		while(inProgress) {
			ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
			assert(ret==ESP_OK);
			inProgress--;
			//vTaskDelay(1);
		}
				
		//printf("Display: Giving semaphore..\r\n");
		spi_semaphore_give();
		//printf("Display: Semaphore given.\r\n");
		vTaskDelay(10 / portTICK_RATE_MS);
		
	}
}

odroid_gamepad_state joystick, last_joystick;

int mv_x = 0;
int mv_y = 0;
int v_max = 10;

extern int dumpBlock;
extern int dumpRequest;

void dispDraw(uint8_t *mem) {
	int dx=0, dy=0, btn=0;
	currFbPtr=mem;
	xSemaphoreGive(dispSem);

	odroid_input_gamepad_read(&joystick);

	if(joystick.values[ODROID_INPUT_B]&&!last_joystick.values[ODROID_INPUT_B]){
		// Toggle Keyboard
		set_keyboard_visible(!keyboard_visible);
	}
	
	/*
	if(joystick.values[ODROID_INPUT_START]&&!last_joystick.values[ODROID_INPUT_START]){
		dumpRequest = true;
	}

	if(joystick.values[ODROID_INPUT_MENU]&&!last_joystick.values[ODROID_INPUT_MENU]){
		if(dumpBlock>0){
			dumpBlock--;
			printf("Selecting Mem Block:%d\r\n", dumpBlock);
		}
	}
	
	if(joystick.values[ODROID_INPUT_VOLUME]&&!last_joystick.values[ODROID_INPUT_VOLUME]){
		if(dumpBlock<31){
			dumpBlock++;
			printf("Selecting Mem Block:%d\r\n", dumpBlock);
		}
	}
*/


	if(joystick.values[ODROID_INPUT_SELECT]){
		if(joystick.values[ODROID_INPUT_DOWN]){
			mv_y++;
			d_offset_y+=mv_y>>2;
			if(d_offset_y>102)d_offset_y=102;
		}else{
			if(mv_y>0){
				mv_y = 0;
			}
		}
		
		if(joystick.values[ODROID_INPUT_UP]){
			mv_y--;
			d_offset_y+=mv_y>>2;
			if(d_offset_y<0)d_offset_y=0;
		}else{
			if(mv_y<0){
				mv_y = 0;
			}
		}
		
		
		if(joystick.values[ODROID_INPUT_RIGHT]){
			mv_x++;
			d_offset_x+=mv_x>>2;
			if(d_offset_x>192)d_offset_x=192;
		}else{
			if(mv_x>0){
				mv_x = 0;
			}
		}
		if(joystick.values[ODROID_INPUT_LEFT]){
			mv_x--;
			d_offset_x+=mv_x>>2;
			if(d_offset_x<0)d_offset_x=0;
		}else{
			if(mv_x<0){
				mv_x = 0;
			}
		}
	}else{
		if(joystick.values[ODROID_INPUT_DOWN]){
			mv_y++;
			dy=mv_y>>2;
		}else{
			if(mv_y>0){
				mv_y = 0;
			}
		}
		
		if(joystick.values[ODROID_INPUT_UP]){
			mv_y--;
			dy=mv_y>>2;
		}else{
			if(mv_y<0){
				mv_y = 0;
			}
		}
		
		if(joystick.values[ODROID_INPUT_RIGHT]){
			mv_x++;
			dx=mv_x>>2;
		}else{
			if(mv_x>0){
				mv_x = 0;
			}
		}
		if(joystick.values[ODROID_INPUT_LEFT]){
			mv_x--;
			dx=mv_x>>2;
		}else{
			if(mv_x<0){
				mv_x = 0;
			}
		}
	}

	int mouse_x=0, mouse_y=0;
	if(keyboard_visible){
		get_mouse(&mouse_x, &mouse_y);
		mouse_y-= d_offset_y;
		mouse_x-= d_offset_x;
	}
	
	if(mouse_y>=159){
		mouse_on_keyboard_event(mouse_x, mouse_y-159, joystick.values[ODROID_INPUT_A]);
	}else{
		if(joystick.values[ODROID_INPUT_A]){
			btn =1;
		}else if(last_joystick.values[ODROID_INPUT_A]){
			// Key might still be pressed, so notify Keyboard that key was released.
			mouse_on_keyboard_event(mouse_x, mouse_y-159, joystick.values[ODROID_INPUT_A]);
		}
	}
	
/*	
	if(joystick.values[ODROID_INPUT_A]){
	
		
		if(keyboard_visible){
			mouseY = (macRam[0x840] << 8) + (macRam[0x841])- d_offset_y - 15;
			mouseX = (macRam[0x82A] << 8) + (macRam[0x82B])- d_offset_x;
		}
		
		if(mouseY>=159){
			if(!last_joystick.values[ODROID_INPUT_A]){
				printf("Virtual Keyboard klicked at: (%d/%d)", mouseX, mouseY);
				viaSendKeyTransision();
			}		
		}else{
			btn =1;
		}
	}
*/	
	
//	if(joystick.values[ODROID_INPUT_B])js +=0x20;
//	if(joystick.values[ODROID_INPUT_SELECT])js +=0x40;
//	if(joystick.values[ODROID_INPUT_START])js +=0x80;
	
	last_joystick = joystick;
	
	
	//mpuMouseGetDxDyBtn(&dx, &dy, &btn);
	mouseMove(dx, dy, btn);
}

void dispInit() {
	printf("spi_lcd_init()\n");
    dispSem=xSemaphoreCreateBinary();
	/*
#if CONFIG_FREERTOS_UNICORE
	xTaskCreatePinnedToCore(&displayTask, "display", 3000, NULL, 5, NULL, 0);
#else*/
	xTaskCreatePinnedToCore(&displayTask, "display", 3000, NULL, 6, NULL, 1);
//#endif
}
