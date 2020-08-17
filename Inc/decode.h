#ifndef __DECODE_H
#define __DECODE_H

#include "common.h"

#define DECODE_DATA_ADDRESS     (uint32_t)0x08020000   //20k

void LzmaDec(void);
void flashCopy(uint8_t*arr, uint32_t address, uint16_t length);
#endif
