#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "7zTypes.h"
#include "LzmaDec.h"
#include "decode.h"
#include "update.h"

#define IN_BUF_SIZE 256
#define OUT_BUF_SIZE 256
#define UNUSED_VAR(x) (void)x;
#define M_MIN(x,y) x<y?x:y

char buf2[256];

void *SzAlloc(void *p,size_t size) { p = p; return malloc(size); }
void SzFree(void *p, void *address) { p = p; free(address); }
static const ISzAlloc g_Alloc = { SzAlloc, SzFree };



void flashCopy(uint8_t*arr, uint32_t address, uint16_t length);

SRes decode2(CLzmaDec *state, uint32_t outData, uint32_t inData, size_t inLength,size_t inOffset, UInt64 unpackSize)
{
	Byte inBuf[IN_BUF_SIZE];
	Byte outBuf[OUT_BUF_SIZE];
	int thereIsSize = (unpackSize != (UInt64)(Int64)-1);
	unsigned __int64 inPos = 0, inSize = 0, outPos = 0;
	int outOffset = 0;

	LzmaDec_Init(state);

	for (;;)
	{
		if (inPos == inSize)
		{
			inSize = M_MIN(IN_BUF_SIZE, inLength - inOffset);
			flashCopy(inBuf, inData + inOffset, inSize);
			inOffset += inSize;
			inPos = 0;
		}
		int res;
		SizeT inProcessed = inSize - inPos;
		SizeT outProcessed = OUT_BUF_SIZE - outPos;
		ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
		ELzmaStatus status;
		if (thereIsSize && outProcessed > unpackSize)
		{
			outProcessed = (SizeT)unpackSize;
			finishMode = LZMA_FINISH_END;
		}
		res = LzmaDec_DecodeToBuf(state, outBuf + outPos, &outProcessed,
			inBuf + inPos, &inProcessed, finishMode, &status);
		
		inPos += inProcessed;
		outPos += outProcessed;
		unpackSize -= outProcessed;

		if(outPos>=128){
			page_copy(outData + outOffset, outBuf);
			outOffset += 128;
			for(uint16_t i=0;i<128;i++){
				outBuf[i]=outBuf[i+128];
			}
			outPos -= 128;
		}

		if (res != SZ_OK || (thereIsSize && unpackSize == 0)){
			page_copy(outData + outOffset, outBuf);
			return res;
		}
			

		if (inProcessed == 0 && outProcessed == 0)
		{
			page_copy(outData + outOffset, outBuf);
			if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
				return SZ_ERROR_DATA;
			return res;
		}
	}
}

SRes decode(uint32_t outData, uint32_t inData, uint16_t inLength)
{
	unsigned char header[LZMA_PROPS_SIZE+8];
	CLzmaDec state;

	flashCopy(header, inData, LZMA_PROPS_SIZE+8);
	uint16_t inOffset = LZMA_PROPS_SIZE + 8;

	uint64_t unpackSize = 0;
	for (int i = 0; i < 8; i++)
		unpackSize += (uint64_t)header[LZMA_PROPS_SIZE + i] << (i * 8);
	
	LzmaDec_Construct(&state);
	RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
	
	SRes res = 0;
	res = decode2(&state, outData, inData,inLength, inOffset, unpackSize);
	LzmaDec_Free(&state, &g_Alloc);
	return res;
}

void LzmaDec()
{
	uint32_t inData = DECODE_DATA_ADDRESS;
	uint32_t outData = UPDATE_DATA_ADDRESS;
	uint16_t inLength = read16(DECODE_DATA_ADDRESS+2);
	Serial_PutString("start LzmaDec\r\n");
	
	SRes res =  decode(outData, inData+4, inLength);
	
	Serial_PutString("end LzmaDec\r\n");
}

void flashCopy(uint8_t*arr, uint32_t address, uint16_t length){
	for(uint16_t i=0; i< length;i++){
		arr[i]=*(uint8_t*)(address+i);
	}
}
