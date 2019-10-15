#include "../tme/rom.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "spi_semaphore.h"

//#define ROM_CACHE_PAGE_SIZE 0x2000 // 8kb Cache page
//#define ROM_CACHE_PAGE_SLOTS 8 // Keep 8 pages in memory

#define ROM_CACHE_PAGE_SIZE 0x1000 // 4kb Cache page
#define ROM_CACHE_PAGE_SHIFT 0xc // Dividing by this is equal to a right shift by this value.
#define ROM_CACHE_PAGE_SLOTS 24 // Keep all 32 pages in memory


FILE *romfile;
uint8_t *rom_cache[ROM_CACHE_PAGE_SLOTS];
uint8_t rom_slots[ROM_CACHE_PAGE_SLOTS];
uint16_t rom_slot_age[ROM_CACHE_PAGE_SLOTS];

void change_value(int address, uint16_t value, int page, int slot){
	if((address>>ROM_CACHE_PAGE_SHIFT)==page){
		int rel_addr = address & (ROM_CACHE_PAGE_SIZE-1);
		(rom_cache[slot])[rel_addr+1] = value & 0xFF;
		(rom_cache[slot])[rel_addr] = (value>>8) & 0xFF;
	}
}

void add_value(int address, uint16_t value, int page, int slot){
	if((address>>ROM_CACHE_PAGE_SHIFT)==page){
		int rel_addr = address & (ROM_CACHE_PAGE_SIZE-1);
		uint16_t oldValue = (rom_cache[slot])[rel_addr+1]  + ((rom_cache[slot])[rel_addr]<<8);
		value = oldValue + value;
		(rom_cache[slot])[rel_addr+1] = value & 0xFF;
		(rom_cache[slot])[rel_addr] = (value>>8) & 0xFF;
	}
}

void subtract_value(int address, uint16_t value, int page, int slot){
	if((address>>ROM_CACHE_PAGE_SHIFT)==page){
		int rel_addr = address & (ROM_CACHE_PAGE_SIZE-1);
		uint16_t oldValue = (rom_cache[slot])[rel_addr+1]  + ((rom_cache[slot])[rel_addr]<<8);
		value = oldValue - value;
		(rom_cache[slot])[rel_addr+1] = value & 0xFF;
		(rom_cache[slot])[rel_addr] = (value>>8) & 0xFF;
	}
}

void apply_patches(int page,int slot){
	// Change Screen Dimension to 320x240
	change_value( 0x1e6e, 320, page,slot);  // 0x0200 => 0x0140 
	change_value( 0x1e82, 240, page,slot);  // 0x0156 => 0x00F0
	// Set Mouse Area to 320x240
	change_value( 0x0498, 320, page,slot);  // 0x0200 => 0x0140
	change_value( 0x0494, 240, page,slot);  // 0x0156 => 0x00F0
	//change_value( 0x0002, 0x7F26, page,slot); // 0x8172 => 0x7F26
	// Correct checksum
	subtract_value( 0x0002, 0x024C, page,slot); // 0x8172 => 0x7F26
}

void load_rom_page(int page_to_load, int slot){
	int address = page_to_load * ROM_CACHE_PAGE_SIZE;
	printf("Loading Rom Page %d...\r\n",page_to_load);
	spi_semaphore_take();
	fseek(romfile,address,SEEK_SET); 
	int res = fread(rom_cache[slot],1,ROM_CACHE_PAGE_SIZE,romfile);
	rom_slots[slot] = page_to_load;
	spi_semaphore_give();
	apply_patches(page_to_load, slot);
	printf("Page %d loaded to slot %d, %d bytes...\r\n",page_to_load,slot, res);
}


int load_rom_from_file(char* file_name){
	romfile=fopen(file_name, "r");
	printf("File opened: %d\r\n",(int)romfile);
	
	// Create a cache of *8kb...
	for(int i=0;i<ROM_CACHE_PAGE_SLOTS;i++){
		rom_cache[i] = malloc(ROM_CACHE_PAGE_SIZE);
		printf("Page-Slot %d:%p\r\n", i,rom_cache[i]);
		rom_slots[i]=0xff;
		rom_slot_age[i] = 0; 
	}
/*	
	for(int i=0;i<ROM_CACHE_PAGE_SLOTS;i++){
		load_rom_page(i, i);
	}*/
	
	
	return((int)romfile);
}

int get_rom_page_slot(int requested_page){
	// Is page in cache?
	for(int i=0;i<ROM_CACHE_PAGE_SLOTS;i++){
		if(rom_slots[i]==requested_page){
			return i;
		}
	}

	// Page is not in cache, so load it now...
	int slot = 0;
	int slot_age=0;
	// find oldest slot and increment age in same step.
	for(int i=0;i<ROM_CACHE_PAGE_SLOTS;i++){
		if(rom_slot_age[i]>slot_age){
			slot_age=rom_slot_age[i];
			slot = i;
		}
		rom_slot_age[i]++;
	}
	// Load page to slot
	load_rom_page(requested_page, slot);
	// Set age for new loaded slot to 0
	rom_slot_age[slot]=0;
	// Return slot
	return slot;
}

int load_rom_from_partition(char* partition_name);

unsigned char *romdata_priv;

void set_rom_data(unsigned char *romdata_pointer){
	romdata_priv = romdata_pointer;
}



unsigned int read_rom_memory_8(unsigned int address){
	// page 
	int requested_page = (address>>ROM_CACHE_PAGE_SHIFT);
	/*if(cache_page!=requested_page){
		printf("Reqested Address: %d is on page %d.", address,  requested_page);
		load_rom_page(requested_page);
	}*/
	int rel_addr = address & (ROM_CACHE_PAGE_SIZE-1);
	int slot = get_rom_page_slot(requested_page);
	return((rom_cache[slot])[rel_addr]);
/*	spi_semaphore_take();
	fseek(romfile,address,SEEK_SET); 
	spi_semaphore_give();
	return fgetc(romfile);
*/
}

unsigned int read_rom_memory_16(unsigned int address){
	unsigned int ret;	
	int requested_page = (address>>ROM_CACHE_PAGE_SHIFT);
	/*if(cache_page!=requested_page){
		printf("Reqested Address: %d is on page %d.", address,  requested_page);
		load_rom_page(requested_page);
	}*/
	int rel_addr = address & (ROM_CACHE_PAGE_SIZE-1);
	int slot = get_rom_page_slot(requested_page);
	ret=(rom_cache[slot])[rel_addr]<<8;
	ret|=(rom_cache[slot])[rel_addr+1];
	
	/*
	
	spi_semaphore_take();
	fseek(romfile,address,SEEK_SET); 
	//return fgetc(romfile);
	ret=fgetc(romfile)<<8;
	ret|=fgetc(romfile);
	spi_semaphore_give();
	//ret=romdata_priv[address]<<8;
	//ret|=romdata_priv[address+1];
	*/
	return ret;
}

unsigned int read_rom_memory_32(unsigned int address) {
	uint16_t a=read_rom_memory_16(address);
	uint16_t b=read_rom_memory_16(address+2);
	return (a<<16)|b;
}

char *get_page0_pointer(){
	int slot = get_rom_page_slot(0);
	return((char*)(rom_cache[slot]));
}

