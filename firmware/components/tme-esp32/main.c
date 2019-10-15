/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include "esp_attr.h"

#include "rom/cache.h"
#include "rom/ets_sys.h"
#include "rom/spi_flash.h"
#include "rom/crc.h"

#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_cntl_reg.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"

#include "../tme/emu.h"
#include "../tme/tmeconfig.h"
#include "../tme/rtc.h"
#include "../tme/rom.h"

#include "../odroid/odroid_settings.h"
#include "../odroid/odroid_input.h"
//#include "../components/odroid/odroid_display.h"
//#include "../components/odroid/odroid_audio.h"
#include "../odroid/odroid_system.h"
#include "../odroid/odroid_sdcard.h"

unsigned char *romdata;
nvs_handle nvs;
const char* SD_BASE_PATH = "/sd";


void emuTask(void *pvParameters)
{
	tmeStartEmu(romdata);
}

void saveRtcMem(char *data) {
/*
	esp_err_t err;
	err=nvs_set_blob(nvs, "pram", data, 32);
	if (err!=ESP_OK) {
		printf("NVS: Saving to PRAM failed!");
	}
	*/
}


void app_main()
{
	int i;
	const esp_partition_t* part;
	spi_flash_mmap_handle_t hrom;
	esp_err_t err;
	uint8_t pram[32];

	nvs_flash_init();
    odroid_system_init();
    odroid_input_gamepad_init();
	
	esp_err_t r = odroid_sdcard_open(SD_BASE_PATH);
	if (r != ESP_OK)
	{
		printf("Error opening SD-Card! (Err:%d)\r\n", r);
		abort();
	}
	
	/*
	nvs_flash_init();
	err=nvs_open("pram", NVS_READWRITE, &nvs);
	if (err!=ESP_OK) {
		printf("NVS: Try erase\n");
		nvs_flash_erase();
		err=nvs_open("pram", NVS_READWRITE, &nvs);
	}
	
	unsigned int sz=32;
	err = nvs_get_blob(nvs, "pram", pram, &sz);
	if (err == ESP_OK) {
		rtcInit((char*)pram);
	} else {
		printf("NVS: Cannot load pram!\n");
	}
	*/

//	part=esp_partition_find_first(0x40, 0x1, "vmac.rom");
/*
	part=esp_partition_find_first(	ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "vmac.rom");
	if (part==0){
		printf("Couldn't find bootrom part!\n");
		abort();
	}else{
		printf("Found partition at: %d\n",part->address);
		
	}
	
	//unsigned char *tmp;
	
	//err=esp_partition_mmap(part, 0, 128*1024, SPI_FLASH_MMAP_DATA, (const void**)&romdata, &hrom);
	err=esp_partition_mmap(part, 0, 128*1024, SPI_FLASH_MMAP_DATA, (const void**)&romdata, &hrom);
	
	
	//tmp = malloc(128*1024);
	printf("Rom mapped at: %p\r\n",romdata);
	//memcpy(romdata,tmp,128*1024);


	
	//if (err!=ESP_OK) printf("Couldn't map bootrom part!\n");
	//set_rom_data(romdata);*/
	
	romdata = NULL; // Rom is not passed directly to emulator any more.
	load_rom_from_file("/sd/roms/macplus/mac.rom");

	printf("Starting emu...\n");
	xTaskCreatePinnedToCore(&emuTask, "emu", 6*1024, NULL, 5, NULL, 0);
}

