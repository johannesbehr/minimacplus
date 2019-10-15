int load_rom_from_file(char* file_name);
int load_rom_from_partition(char* partition_name);
void set_rom_data(unsigned char *romdata_pointer);

unsigned int read_rom_memory_8(unsigned int address);
unsigned int read_rom_memory_16(unsigned int address);
unsigned int read_rom_memory_32(unsigned int address);
char *get_page0_pointer();