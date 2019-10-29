#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"

void set_keyboard_visible(bool visible);
void mouse_on_keyboard_event(int x, int y, int button);
