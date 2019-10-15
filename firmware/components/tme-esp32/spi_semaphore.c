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
#include "../odroid/odroid_input.h"
#include "spi_semaphore.h"

SemaphoreHandle_t spi_semaphore = NULL;

void spi_semaphore_give(){
	if(spi_semaphore==NULL){
		printf("Creating spi semaphore (1)\r\n");
		spi_semaphore = xSemaphoreCreateBinary();
	}
	xSemaphoreGive(spi_semaphore);	
}

void spi_semaphore_take(){
	if(spi_semaphore==NULL){
		printf("Creating spi semaphore (2)\r\n");
		spi_semaphore = xSemaphoreCreateBinary();
		xSemaphoreGive(spi_semaphore);	
	}
	xSemaphoreTake(spi_semaphore, portMAX_DELAY);
}


