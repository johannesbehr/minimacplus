//Stuff for a host build of TME
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../tme/ncr.h"
#include "../tme/hd.h"
#include "esp_system.h"
#include "hexdump.h"
#include "spi_semaphore.h"
#include "fd.h"

typedef struct {
	FILE *f;
	int size;
	uint8_t isFlopy;
	uint8_t floppyOffset;
} HdPriv;

const uint8_t inq_resp[95]={
	0, //HD
	0, //0x80 if removable
	0x49, //Obsolete SCSI standard 1 all the way
	0, //response version etc
	31, //extra data
	0,0, //reserved
	0, //features
	'A','P','P','L','E',' ',' ',' ', //vendor id
	'2','0','S','C',' ',' ',' ',' ', //prod id
	'1','.','0',' ',' ',' ',' ',' ', //prod rev lvl
};

static void readBlock(uint8_t *data,unsigned int len, unsigned int  block, HdPriv *hd){

	if(hd->isFlopy){
		if(block<74){
			
			// Take data from header (74 Blocks)
			memcpy(data, disk_header + block*512, len*512);
			
			// Inject information about size in header-data:
			if(block==0){
				// At pos 0x06 the full size of the image has to be injected.
				int blockcnt = hd->size / 512;
				data[0x6] = blockcnt>>8;
				data[0x7] = blockcnt&0xFF;
				
			}
			if(block==1){
				int blockcnt = (hd->size / 512) - 96;
				// At pos 0x0e and 0x56 of block 1 the size of the disk image has to be injected. 
				data[0x0e] = data[0x56] = blockcnt>>8;
				data[0x0f] = data[0x57] = blockcnt&0xFF;
			}
			if(block==0&&len>1){
				printf("Unexpected read at block %d len %d",block, len );
				abort();
			}
		}else if(block <96){
			// This blocks are just empty, so set to zero
			memset(data,0,len*512);
		}else{
			// All Blocks > 96 belong to disk, so read disk content
			spi_semaphore_take();
			//printf("HD: Got semaphore.\r\n");
			fseek(hd->f, (block-96)*512 + hd->floppyOffset, SEEK_SET);
			fread(data, 512, len, hd->f);
			//printf("HD: Giving semaphore..\r\n");
			spi_semaphore_give();
		}
		
	//	printf("FD: Read %d bytes from block %d.\n", len*512, block);
	//	hexdump(data, len*512);
		
		
	}
	else{
		//printf("HD: Read %d bytes from LBA %d.\n", len*512, lba);
		//printf("HD: Taking semaphore..\r\n");
		spi_semaphore_take();
		//printf("HD: Got semaphore.\r\n");
		fseek(hd->f, block*512, SEEK_SET);
		fread(data, 512, len, hd->f);
		//printf("HD: Giving semaphore..\r\n");
		spi_semaphore_give();
		//printf("HD: Semaphore given.\r\n");
		//int cnt = fread(data->data, 1, 512, hd->f);
		
		//printf("HD: Read %dx%d => %d bytes.\n",cnt,512, len*512);
		//hexdump(data->data, len*512);
//		printf("HD: Read %d bytes.\n", len*512);
	}

}

static void writeBlock(uint8_t *data,unsigned int len, unsigned int  block, HdPriv *hd){
	if(hd->isFlopy){
		if(block>96){
			spi_semaphore_take();
			fseek(hd->f, (block-96)*512 + hd->floppyOffset, SEEK_SET);
			fwrite(data, 512, len, hd->f);
			spi_semaphore_give();
		}
		else{
		}
	}
	else{
		spi_semaphore_take();
		fseek(hd->f, block*512, SEEK_SET);
		fwrite(data, 512, len, hd->f);
		spi_semaphore_give();
	}
//		printf("HD: Write %d bytes\n", len*512);
}


static int hdScsiCmd(SCSITransferData *data, unsigned int cmd, unsigned int len, unsigned int lba, void *arg) {
	int ret=0;
	HdPriv *hd=(HdPriv*)arg;
//	for (int x=0; x<32; x++) printf("%02X ", data->cmd[x]);
//	printf("\n");
	if (cmd==0x8 || cmd==0x28) { //read
		readBlock(data->data, len,lba, hd);
		ret=len*512;
	} else if (cmd==0xA || cmd==0x2A) { //write
		writeBlock(data->data, len,lba, hd);
		ret=0;
	} else if (cmd==0x12) { //inquiry
		printf("HD: Inquery\n");
		
		memset(data->data, 0, 54);
		data->data[0] = 0x00; // device is direct-access (e.g. hard disk)
		data->data[1] = 0x00; // media is not removable
		data->data[2] = 0x05; // device complies with SPC-3 standard
		data->data[3] = 0x01; // response data format = CCS
		// Apple HD SC setup utility needs to see this
		strcpy((char *)&data->data[8], " SEAGATE");
		strcpy((char *)&data->data[15], "          ST225N");
		strcpy((char *)&data->data[31], "1.00");
		data->data[36] = 0x00; // # of extents high
		data->data[37] = 0x08; // # of extents low
		data->data[38] = 0x00; // group 0 commands 0-1f
		data->data[39] = 0x99; // commands 0,3,4,7
		data->data[40] = 0xa0; // commands 8, a
		data->data[41] = 0x27; // commands 12,15,16,17
		data->data[42] = 0x34; // commands 1a,1b,1d
		data->data[43] = 0x01; // group 1 commands 20-3f
		data->data[44] = 0x04;
		data->data[45] = 0xa0;
		data->data[46] = 0x01;
		data->data[47] = 0x18;
		data->data[48] = 0x07; // group 7 commands e0-ff
		data->data[49] = 0x00;
		data->data[50] = 0xa0; // commands 8, a
		data->data[51] = 0x00;
		data->data[52] = 0x00;
		data->data[53] = 0xff; // end of list
		return 54;
		
		
		//memcpy(data->data, inq_resp, sizeof(inq_resp));		
		//return 95;
	} else if (cmd==0x25) { //read capacity
		int lbacnt=hd->size/512;
		data->data[0]=(lbacnt>>24);
		data->data[1]=(lbacnt>>16);
		data->data[2]=(lbacnt>>8);
		data->data[3]=(lbacnt>>0);
		data->data[4]=0;
		data->data[5]=0;
		data->data[6]=2; //512
		data->data[7]=0;
		ret=8;
		printf("HD: Read capacity (%d)\n", lbacnt);
	} else {
		printf("********** hdScsiCmd: unrecognized command %x\n", cmd);
	}
	data->cmd[0]=0; //status
	data->msg[0]=0;
	return ret;
}

SCSIDevice *fdCreate(char *file) {
	FILE *f;
	printf("Open FD-File: %s\r\n",file);
	
	spi_semaphore_take();
	f=fopen(file, "r+");
	if(f<=0){
		printf("File not found.\r\n");
		return NULL;
	}

	SCSIDevice *ret=malloc(sizeof(SCSIDevice));
	memset(ret, 0, sizeof(SCSIDevice));
	HdPriv *hd=malloc(sizeof(HdPriv));
	memset(hd, 0, sizeof(HdPriv));
	hd->f=f;

	int c1,c2,c3;


	// First check: is Partition or full disk image?
	// Get first two bytes
	c1 = fgetc(hd->f);
	c2 = fgetc(hd->f);
	
	// Seek to end to determine size
	fseek(hd->f, 0, SEEK_END);
	hd->size=ftell(hd->f);
	
	if(c1==0x45 && c2==0x52){
		// Image is full drive
		hd->isFlopy = 0;
		printf ("Image is full drive.\r\n");

	}else{
		// Image is partition, so construct "frameset"
		hd->size += 0xc000; // Plus header size (96 Blocks * 512 Byte)
		// Size is ok.

		// Check if FD-Image has Header or not
		fseek(hd->f, 0x51, SEEK_SET);
		
		c1 = fgetc(hd->f);
		c2 = fgetc(hd->f);
		c3 = fgetc(hd->f);
		printf ("Diskheader: %x %x %x\r\n", c1,c2,c3);
	
		if(c1==0x22 && c2==0x01&& c3==0x00){
			// Disk has header, so offset is 0x54 byte.
			printf("Disk has  header.\r\n");
			hd->size-=0x54;
			hd->floppyOffset = 0x54;
		}else{
			printf("Disk has no header.\r\n");
			hd->floppyOffset = 0x00;
		}
		
		// Make shure size is multiple of 512
		int remainder = (hd->size % 512);
		if(remainder!=0){
			hd->size += (512-remainder);
		}
		hd->isFlopy = 1;
	}
	spi_semaphore_give();

	ret->arg=hd;
	ret->scsiCmd=hdScsiCmd;
	
	printf("Created Disk, size is %d\r\n",hd->size);
	return ret;
}



SCSIDevice *hdCreate(char *file) {
	FILE *f;
	printf("Open HD-File: %s\r\n",file);
	spi_semaphore_take();
	f=fopen(file, "r+");
	if(f<=0){
		printf("File not found.");
		return NULL;
	}

	SCSIDevice *ret=malloc(sizeof(SCSIDevice));
	memset(ret, 0, sizeof(SCSIDevice));
	HdPriv *hd=malloc(sizeof(HdPriv));
	memset(hd, 0, sizeof(HdPriv));
	hd->f=f;
	hd->isFlopy = 0;
	fseek(hd->f, 0, SEEK_END);
	hd->size=ftell(hd->f);//2097152
	spi_semaphore_give();
	ret->arg=hd;
	ret->scsiCmd=hdScsiCmd;
	
	printf("Created HD, size is %d\r\n",hd->size);
	return ret;
}
