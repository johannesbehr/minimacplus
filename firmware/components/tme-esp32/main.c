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
#include <dirent.h>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"

#include "../tme/emu.h"
#include "../tme/tmeconfig.h"
#include "../tme/rtc.h"
#include "../tme/rom.h"
#include "../tme/hd.h"

#include "../odroid/odroid_settings.h"
#include "../odroid/odroid_input.h"
//#include "../components/odroid/odroid_display.h"
//#include "../components/odroid/odroid_audio.h"
#include "../odroid/odroid_system.h"
#include "../odroid/odroid_sdcard.h"
#include "spi_semaphore.h"


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

extern uint8_t patch_rom;

void app_main()
{
	//int i;
	//const esp_partition_t* part;
	//spi_flash_mmap_handle_t hrom;
	//esp_err_t err;
	//uint8_t pram[32];

	nvs_flash_init();
    
	
	odroid_system_init();
    odroid_input_gamepad_init();
	
	odroid_gamepad_state joystick;
	
	vTaskDelay(100 / portTICK_RATE_MS);
	
	odroid_input_gamepad_read(&joystick);
	if(joystick.values[ODROID_INPUT_SELECT]){
		printf("Patching rom\r\n");
	}else{
		printf("Not patching rom\r\n");
		patch_rom = 0;
	}
	
	
	
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

	
	// Search all files in Mac-Directory.
	// For each check extensioin:
	// - *.rom: Use as Bios-File
	// - *.raw, *.dsk, *.image use as Disk-Image

	// Mont-Method should autodetect Type based of content! 
	// Check first byte to see if it is a complete disk or just a prtition
	
	//if (err!=ESP_OK) printf("Couldn't map bootrom part!\n");
	//set_rom_data(romdata);*/
	
	
//	spi_semaphore_take();
	DIR* dir = opendir("/sd/roms/macplus");
	printf("Dir open done.\r\n");
    if (dir)
    {
		struct dirent* in_file;
		while ((in_file = readdir(dir))) 
		{
			char *fname = in_file->d_name;
			char fullname[128];
			int flen = strlen(fname);
			
			printf("Found file: %s\r\n",fname);
			if (!strcmp (fname, ".")||!strcmp (fname, ".."))    
				continue;
			if (fname[0] == '.' && fname[1] == '_')    
				continue;
			if(strcasecmp(".rom", &fname[flen-4]) == 0){
				sprintf(fullname, "/sd/roms/macplus/%s", fname);
				load_rom_from_file(fullname);
				printf("Found rom file!\r\n");
			}else if(strcasecmp(".raw", &fname[flen-4]) == 0||
					strcasecmp(".drv", &fname[flen-4]) == 0||
					strcasecmp(".dsk", &fname[flen-4]) == 0||
					strcasecmp(".image", &fname[flen-6]) == 0||
					strcasecmp(".ima", &fname[flen-4]) == 0||
					strcasecmp(".img", &fname[flen-4]) == 0||
					strcasecmp(".hfv", &fname[flen-4]) == 0){
				sprintf(fullname, "/sd/roms/macplus/%s", fname);
				load_image_file(fullname);
				printf("Found image file!\r\n");
			}else{
				printf("Unknown file type.\r\n");
			}
		}
		closedir(dir);
	}else{
		printf("Error opening Dir...\r\n");
		
	}
//	spi_semaphore_give();

	
	//romdata = NULL; // Rom is not passed directly to emulator any more.
	//load_rom_from_file("/sd/roms/macplus/mac.rom");
	

	
		
	/*
	// Search for files. 
	// If file ends with "image" or "drv" add it as floppy
	// if file ends with "raw" add it as drive image
    // Count drives, add maximal n devices...
	
	int scsi_addr = 6;

	// Try to read hd drive
	SCSIDevice *device;
	
	device=hdCreate("/sd/roms/macplus/mac_hd.raw");
	if(device!=NULL){
		ncrRegisterDevice(scsi_addr, device);
		scsi_addr--;
	}

	// Try to read up to 3 floppy drives
	device=fdCreate("/sd/roms/macplus/mac_fd1.dsk");
	if(device!=NULL){
		ncrRegisterDevice(scsi_addr, device);
		scsi_addr--;
	}

	device=fdCreate("/sd/roms/macplus/mac_fd2.dsk");
	if(device!=NULL){
		ncrRegisterDevice(scsi_addr, device);
		scsi_addr--;
	}

	*/
	

	printf("Starting emu...\n");
	xTaskCreatePinnedToCore(&emuTask, "emu", 6*1024, NULL, 5, NULL, 0);
}

