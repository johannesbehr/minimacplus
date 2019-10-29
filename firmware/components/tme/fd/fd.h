#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>


#define UINT8 uint8_t
#define UINT32 uint32_t
#define emu_file FILE
#define size_t int
#define device_t int
#define legacy_floppy_image_device int
#define floppy_image_legacy int
#define FLOPPY_TYPE_SONY 1
#define running_machine int
#define auto_free(m,v) free(v)
#define auto_alloc_array(m,t,s)  malloc(s)



UINT32 floppy_get_track_size(floppy_image_legacy *floppy, int head, int track);
legacy_floppy_image_device *floppy_get_device_by_type(running_machine &machine,int ftype,int drive);
void sony_filltrack(UINT8 *buffer, size_t buffer_len, size_t *pos, UINT8 data);
UINT8 sony_fetchtrack(const UINT8 *buffer, size_t buffer_len, size_t *pos);
int apple35_sectors_per_track(floppy_image_legacy *image, int track);