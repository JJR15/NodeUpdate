#ifndef __UPDATE_H
#define __UPDATE_H

#define APPLICATION_ADDRESS     (uint32_t)0x08003000   //12k
#define UPDATE_DATA_ADDRESS     (uint32_t)0x08025000   //20k
#define RELOCTABLE_ADDRESS      (uint32_t)0x0802A000   //24k      136k

void updateReloctable(void);
void update(void);
void application_jump(void);
#endif
