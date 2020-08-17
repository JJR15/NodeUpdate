#ifndef __UPDATE_H
#define __UPDATE_H

#include "common.h"

#define APPLICATION_ADDRESS     (uint32_t)0x08004000   //16k
#define UPDATE_DATA_ADDRESS     (uint32_t)0x08025000   //20k
#define RELOCTABLE_ADDRESS      (uint32_t)0x0802A000   //24k      112k

void updateReloctable(void);
void update(void);
void application_jump(void);
int8_t page_copy(uint32_t addr, uint8_t*buffer);

uint32_t alloc_cache();
void write_cache(uint32_t dst, uint32_t src);
void free_cache(uint32_t address);

uint16_t read16(uint32_t address);
uint32_t read24(uint32_t address);
uint32_t read32(uint32_t address);

#endif
