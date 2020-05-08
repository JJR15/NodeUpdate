#include "update.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

#define max(A,B)  A>B?A:B
#define min(A,B)  A<B?A:B

typedef  void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

char buf[20];
uint8_t flash_buf[150] = {0};
uint16_t flash_buf_length = 0;
uint16_t reloc_map[150]={0};
uint8_t reloc_map_len = 0;

uint8_t page_update_map[1200]={0};

typedef union{
	struct {
		uint16_t list_start;
		uint8_t to_length;
	}edges;
	uint8_t*page;
}page_t;

page_t graph_to[1200]={{0,0}};
uint8_t graph_from[1200]={0};

int16_t node_to_list[3500]={0};
uint16_t node_to_list_len = 0;

uint32_t d_start, d_end, r_start, r_end, f_start, f_end;

uint16_t read16(uint32_t address);
uint32_t read24(uint32_t address);
uint32_t read32(uint32_t address);
void write_to_buf16(uint16_t word);
void write_to_buf32(uint32_t dword);
int8_t move_flash(uint32_t move_page, uint32_t start_page,uint32_t end_page);
int8_t page_copy(uint32_t addr);
void update_bin2(void);
void update_page(uint32_t page_index);


void application_jump(void);	
extern void FLASH_PageErase(uint32_t PageAddress);
void findNext(uint32_t diff_start, uint32_t diff_end, uint32_t*index,uint32_t*len, uint32_t*x_off_addr,uint32_t*y_off_addr);

void updateReloctable(){
	
	uint32_t reloc_diff_start,reloc_diff_end;
	uint32_t diff_start ,diff_end , add_start;
	uint16_t reloc_new_len, reloc_move_len;
	uint16_t reloc_old_len0, reloc_old_len01, reloc_old_len012;
	uint16_t reloc_add_len0, reloc_add_len01, reloc_add_len012;
	
	reloc_diff_start = read32(UPDATE_DATA_ADDRESS + 16);
	reloc_diff_end = read32(UPDATE_DATA_ADDRESS + 20);

	diff_start = UPDATE_DATA_ADDRESS + reloc_diff_start;
	diff_end = UPDATE_DATA_ADDRESS + reloc_diff_end;
	add_start = UPDATE_DATA_ADDRESS + reloc_diff_end;
	
	reloc_new_len = read16(UPDATE_DATA_ADDRESS + 24);
	reloc_move_len = read16(UPDATE_DATA_ADDRESS + 26);
	
	reloc_old_len0 = read16(UPDATE_DATA_ADDRESS + 28);
	reloc_old_len01 = read16(UPDATE_DATA_ADDRESS + 30);
	reloc_old_len012 = read16(UPDATE_DATA_ADDRESS + 32);
	
	reloc_add_len0 = read16(add_start);
	reloc_add_len01 = read16(add_start + 2);
	reloc_add_len012 = read16(add_start + 4);
	
	uint32_t move_page, end_page;
	move_page=(reloc_move_len*6+127)&0xffffff80;
	end_page = (RELOCTABLE_ADDRESS + reloc_old_len012*6)&0xFFFFFF80;
	
	//sprintf(buf, "move %d %d\r\n",move_page, end_page);
	//Serial_PutString(buf);
	
	move_flash(move_page,RELOCTABLE_ADDRESS, end_page);
	

	uint32_t write_to_addr = RELOCTABLE_ADDRESS;
	flash_buf_length = 0;
	
	uint32_t i, len = 0,x_off = 0,y_off=0;
	//int32_t x_off;
	uint32_t index = 0;
	uint32_t reloc_addr,reloc_addr_diff,reloc_addr_add,reloc_add_i = 0,data;
	uint16_t r_addr;
	uint8_t flag_diff=0, flag_add=0;
	
	//sprintf(buf, "reloc_add_len012 %d %d %d %d\r\n",reloc_add_len0,reloc_add_len01,reloc_add_len012 , reloc_new_len);
	//Serial_PutString(buf);
	uint32_t count = 0;		
	for(i = 0;i<reloc_new_len;i++){
		if(len == 0){
			findNext(diff_start, diff_end, &index, &len, &x_off, &y_off);
			count += len;
			//sprintf(buf, "findNext %d %d %d\r\n",index, len, count);
			//Serial_PutString(buf);
		}
		if(flag_diff==0){
			flag_diff = 1;
			if(index < reloc_old_len012){		
				r_addr = read16(RELOCTABLE_ADDRESS + 6 * index + move_page);
				
				//sprintf(buf, "reloc_old %d %d %x %x\r\n",index, len,r_addr, RELOCTABLE_ADDRESS + 6 * index + move_page);
				//Serial_PutString(buf);
				
				reloc_addr_diff = r_addr + 0x08000000;
				if(index>=reloc_old_len0){
					reloc_addr_diff += 0x10000;
					if(index >= reloc_old_len01){
						reloc_addr_diff += 0x10000;
					}
				}
				reloc_addr_diff = (((reloc_addr_diff&0xffffff) + x_off)&0xffffff) + (reloc_addr_diff&0xff000000);
			}
			else{
				reloc_addr_diff = 0xffffffff;
			}				
		}
		if(flag_add==0){
			flag_add = 1;
			if(reloc_add_i<reloc_add_len012){
				r_addr = read16(add_start + 6 + 6*reloc_add_i);
				reloc_addr_add = r_addr + 0x08000000;
				if(reloc_add_i>=reloc_add_len0){
					reloc_addr_add += 0x10000;
					if(reloc_add_i >= reloc_add_len01){
						reloc_addr_add += 0x10000;
					}
				}
			}
			else{
				reloc_addr_add = 0xffffffff;
			}
		}
		
		if(reloc_addr_diff < reloc_addr_add){
			reloc_addr=reloc_addr_diff;
			data = read32(RELOCTABLE_ADDRESS + 6 * index + 2 + move_page);
			data += y_off;
			write_to_buf16(reloc_addr_diff&0xffff);
			
			//sprintf(buf, "reloc_diff %d %x %x \r\n",i,reloc_addr_diff, data);
			//Serial_PutString(buf);
			
			flag_diff = 0;
			index++;
			len--;
		}
		else{
			reloc_addr = reloc_addr_add;
			data = read32(add_start + 8 + 6*reloc_add_i);
			write_to_buf16(reloc_addr_add&0xffff);
				
			//sprintf(buf, "reloc_add %d %x %x \r\n",i,reloc_addr_add, data);
			//Serial_PutString(buf);
			
			flag_add = 0;
			reloc_add_i++;
			
		}
		while(reloc_addr>=reloc_map_len*1024+APPLICATION_ADDRESS){
			reloc_map[reloc_map_len++] = i;
		}
		
		write_to_buf32(data);

		
		if(flash_buf_length >= 128){
			page_copy(write_to_addr);
			write_to_addr +=128;
			flash_buf_length -=128;
			
			flash_buf[0] = flash_buf[128];
			flash_buf[1] = flash_buf[129];
			flash_buf[2] = flash_buf[130];
			flash_buf[3] = flash_buf[131];
		}
	}                                                                         
	page_copy(write_to_addr);
	
	reloc_map[reloc_map_len++] = reloc_new_len;
}

void findNext(uint32_t diff_start, uint32_t diff_end, uint32_t*index,uint32_t*len, uint32_t*x_off_addr,uint32_t*y_off_addr){
	//sprintf(buf, "findNext %d\r\n",*index);
	//Serial_PutString(buf);
	
	uint32_t last_index = *index;
	*index = 0xffffffff;
	uint32_t x_length,x_off,x_end,y_length,y_off,y_end,n;
	int32_t s;
	while(diff_start<diff_end){
		x_off = read24(diff_start);
		x_length = read16(diff_start+3);
		
		//sprintf(buf, "x %d %d \r\n",x_off, x_length);
		//Serial_PutString(buf);
		
		x_end = diff_start+x_length;
		diff_start += 5;
		
		while(diff_start<x_end){
			y_off = read32(diff_start);
			y_length = read16(diff_start+4);
			
			//sprintf(buf, "y %d %d \r\n",y_off, y_length);
			//Serial_PutString(buf);
			
			y_end = diff_start+y_length;
			diff_start += 6;
			
			while(diff_start<y_end){
				s = read16(diff_start);
				if(s>>15==1){
					s = s &0x7fff;
					n = 1;
					diff_start += 2;
				}
				else {
					n = read16(diff_start + 2);
					diff_start += 4;
				}
				
				//sprintf(buf, "find %d %d \r\n",s, n);
				//Serial_PutString(buf);
				
				if(s>=last_index){
					if(s<*index){
						*index = s;
						*x_off_addr = x_off;
						*y_off_addr = y_off;
						*len = n;
						
						//sprintf(buf, "find %d %d %x\r\n",s, n, diff_start);
						//Serial_PutString(buf);
					}
					diff_start = y_end;
					break;	
				}
			}
		}
	}
}

//void update_bin2(void){
//	uint32_t move_len = read32(UPDATE_DATA_ADDRESS + 4);
//	uint32_t old_bin_len = read32(UPDATE_DATA_ADDRESS + 8);
//	uint32_t delta_end = read32(UPDATE_DATA_ADDRESS + 12);
//	uint32_t r_fix_end = read32(UPDATE_DATA_ADDRESS + 16);
//		
//	uint32_t reloc_new_len = read16(UPDATE_DATA_ADDRESS + 24);
//	
//	d_start = UPDATE_DATA_ADDRESS + 34;
//	d_end = UPDATE_DATA_ADDRESS + delta_end;
//	
//	f_start = UPDATE_DATA_ADDRESS + delta_end;
//	f_end = UPDATE_DATA_ADDRESS + r_fix_end;
//	
//	r_start = RELOCTABLE_ADDRESS;
//	r_end = RELOCTABLE_ADDRESS + reloc_new_len * 6;
//	
//	uint32_t addr = d_start,n,type,bin_len=0;
//	uint32_t old_start;
//	

//	/*uint32_t base = 0x08000000;
//	uint32_t last=0;
//	uint32_t r;
//	for(r=0;r<reloc_new_len;r++){
//		uint32_t reloc_addr=read16(RELOCTABLE_ADDRESS + r * 6)+base;
//		if(reloc_addr<last){
//			base+=0x10000;
//			reloc_addr+=0x10000;
//		}
//		last = reloc_addr;
//		while(reloc_addr>=reloc_map_len*1024+APPLICATION_ADDRESS){
//			reloc_map[reloc_map_len++] = r;
//		}
//	}
//	reloc_map[reloc_map_len++] = r;*/
//	
//	//build graph
//	while(addr < d_end){
//		n = read24(addr);
//		type = n >> 23;
//		n = n&0x007fffff;
//	
//		//sprintf(buf, "n type %d %d \r\n",n, type);
//		//Serial_PutString(buf);
//		
//		if(type==0){
//			old_start = read24(addr+3)+0x08000000-APPLICATION_ADDRESS;
//			addr+=6;
//			//sprintf(buf, "bin_len n old_start %d %d %d \r\n",bin_len,n, old_start);
//			//Serial_PutString(buf);
//			
//			uint32_t st,ed;
//			for(uint32_t i=(bin_len>>7);i<=(bin_len+n-1)>>7;i++){
//				st = max(i<<7,bin_len);
//				ed = min((i<<7)+127,bin_len+n-1);
//				st=(st-bin_len+old_start)>>7;
//				ed=(ed-bin_len+old_start)>>7;
//				
//				//sprintf(buf, "i st ed %d %d %d\r\n",i,st,ed);
//				//Serial_PutString(buf);
//				
//				if(st!=i){
//					graph_from[st]++;
//					
//					if(graph_to[i].edges.to_length==0){
//						graph_to[i].edges.list_start = node_to_list_len;
//					}
//					graph_to[i].edges.to_length++;
//					node_to_list[node_to_list_len++]=st;
//					
//					//sprintf(buf, "st %d %d %d %d %d\r\n",i,st,node_to_list_len,graph_to[i].edges.list_start,graph_to[i].edges.to_length );
//					//Serial_PutString(buf);
//				}
//				if(ed!=st&&ed!=i){
//					graph_from[ed]++;
//					
//					if(graph_to[i].edges.to_length==0){
//						graph_to[i].edges.list_start = node_to_list_len;
//					}
//					graph_to[i].edges.to_length++;
//					node_to_list[node_to_list_len++]=ed;
//					
//					//sprintf(buf, "ed %d %d %d %d %d\r\n",i,ed,node_to_list_len,graph_to[i].edges.list_start,graph_to[i].edges.to_length );
//					//Serial_PutString(buf);
//				}
//			}
//		}
//		else{
//			addr+=3+n;
//		}
//		bin_len+=n;	
//	}
//	
//	//sprintf(buf, "node_to_list_len %d\r\n",node_to_list_len);
//	//Serial_PutString(buf);
//	
//	//update page
//	for(uint16_t i=0;i<=(bin_len-1)>>7;i++){
//		uint16_t page = 0;
//		uint8_t depend_n=0xff;
//		for(uint16_t j=0;j<=(bin_len-1)>>7;j++){
//			if(page_update_map[j]==0){
//				if(graph_from[j]<depend_n){
//					page = j;
//					depend_n = graph_from[j];
//				}
//			}
//		}
//		
//		
//		uint8_t*saved_page=0; 
//		if(depend_n!=0){
//			//save page
//			saved_page = malloc(128*sizeof(uint8_t));
//			for(uint8_t j=0;j<128;j++){
//				saved_page[j] = *(uint8_t*)(APPLICATION_ADDRESS + page*128 +j);
//			}
//			
//			//sprintf(buf, "save %d\r\n",page);
//			//Serial_PutString(buf);
//		}
//		
//		
//		sprintf(buf, "update %d %d\r\n",i,page);
//		Serial_PutString(buf);
//		update_page(page);
//		//page_update_map[page]= 1;
//		
//		//update graph	
//		for(uint16_t j=0;j<graph_to[page].edges.to_length;j++){
//			uint16_t from_page = node_to_list[graph_to[page].edges.list_start+j];
//			graph_from[from_page]--;
//			
//			//sprintf(buf, "del %d %d %d\r\n",page,from_page,graph_from[from_page]);
//			//Serial_PutString(buf);
//			
//			
//			if(graph_from[from_page]==0&&page_update_map[from_page]==1){
//				free(graph_to[from_page].page);
//				
//				//sprintf(buf, "free %d\r\n",from_page);
//				//Serial_PutString(buf);
//			}
//		}
//		graph_to[page].page=saved_page;
//	}
//	
//}


void update_page(uint32_t page_index){
	uint32_t page_addr = APPLICATION_ADDRESS+(page_index<<7);
	
	flash_buf[0]=0;
	
	uint32_t addr = d_start;
	uint32_t n, type, write_addr=0;
	int32_t start_n,end_n;
	
	while(addr < d_end){
		n = read24(addr);
		type = n >> 23;
		n = n&0x007fffff;
		write_addr+=n;
		
		//sprintf(buf, "read %d %d %d\r\n",n,type,write_addr);
		//Serial_PutString(buf);
		
		if(write_addr > page_index<<7){
			//apply command
			start_n =(page_index<<7)-write_addr+n;
			end_n = ((page_index+1)<<7)- write_addr + n;
			if(start_n<0){
				start_n = 0;
			}
			if(end_n>n){
				end_n = n;
			}
			
			//sprintf(buf, "range %d %d\r\n",start_n,end_n);
			//Serial_PutString(buf);
			
			if(type==0){
				uint32_t copy_addr = read24(addr+3) + 0x08000000;
				uint32_t p_index;
				
				//sprintf(buf, "copy %d %d %d %d %d\r\n",start_n,end_n,copy_addr,n,write_addr);
				//Serial_PutString(buf);
				
				//copy
				for(;start_n<end_n;start_n++){
					p_index=(copy_addr+start_n-APPLICATION_ADDRESS)>>7;
					
					if(page_update_map[p_index]==0){
						flash_buf[write_addr-n+start_n-(page_index<<7)]=*(uint8_t*)(copy_addr+start_n);
					}
					else{
						//sprintf(buf, "copy save %d\r\n",p_index);
						//Serial_PutString(buf);
						flash_buf[write_addr-n+start_n-((page_index<<7))] = graph_to[p_index].page[copy_addr+start_n-APPLICATION_ADDRESS-(p_index<<7)];
					}
				}
			}
			else{
				//add
				for(;start_n<end_n;start_n++){
					flash_buf[write_addr-n+start_n-((page_index<<7))]=*(uint8_t*)(addr+3+start_n);
				}
			}
		}
		
		if(write_addr >= (page_index+1)<<7){
			break;
		}
		
		if(type==0){
			addr+=6;
		}
		else{
			addr+=3+n;
		}
	}
	
	//apply reloc, fix zero
	uint32_t i,r_addr;
	i = reloc_map[page_index>>3];
	if(i>0 && (page_index&63)==0){
		r_addr = read16(RELOCTABLE_ADDRESS+6*i-6)+(page_addr&0xffff0000)-0x10000;
		if(r_addr+4>page_addr){
			flash_buf[r_addr-page_addr+2]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i-2);
			flash_buf[r_addr-page_addr+3]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i-1);
		}
	}
	for(;i<reloc_map[(page_index>>3)+1];i++){
		r_addr = read16(RELOCTABLE_ADDRESS+6*i)+(page_addr&0xffff0000);
		if(r_addr>=page_addr){
			if(r_addr<page_addr+128){
				flash_buf[r_addr-page_addr]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i+2);
				flash_buf[r_addr-page_addr+1]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i+3);
				flash_buf[r_addr-page_addr+2]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i+4);
				flash_buf[r_addr-page_addr+3]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i+5);
			}
			else{
				break;
			}
		}
		else if(r_addr+4>page_addr){
			flash_buf[r_addr-page_addr+2]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i+4);
			flash_buf[r_addr-page_addr+3]=*(uint8_t*)(RELOCTABLE_ADDRESS+6*i+5);
		}
	}
	
	for(i=f_start;i<f_end;i+=3){	
		r_addr = read24(i)+0x08000000;
		if(r_addr>=page_addr){
			if(r_addr<page_addr+128){
				flash_buf[r_addr-page_addr]=0;
			}
			else{
				break;
			}
		}
	}
	
	page_copy(page_addr);
	page_update_map[page_index]= 1;
}


void update_bin(){
	uint32_t move_len = read32(UPDATE_DATA_ADDRESS + 4);
	uint32_t old_bin_len = read32(UPDATE_DATA_ADDRESS + 8);
	uint32_t delta_end = read32(UPDATE_DATA_ADDRESS + 12);
	uint32_t r_fix_end = read32(UPDATE_DATA_ADDRESS + 16);
		
	uint32_t reloc_new_len = read16(UPDATE_DATA_ADDRESS + 24);
	
	
	d_start = UPDATE_DATA_ADDRESS + 34;
	d_end = UPDATE_DATA_ADDRESS + delta_end;
	
	f_start = UPDATE_DATA_ADDRESS + delta_end;
	f_end = UPDATE_DATA_ADDRESS + r_fix_end;
	
	r_start = RELOCTABLE_ADDRESS;
	r_end = RELOCTABLE_ADDRESS + reloc_new_len * 6;
	
	uint32_t move_page, end_page;
	move_page=(move_len+127)&0xffffff80;
	end_page = (APPLICATION_ADDRESS + old_bin_len)&0xFFFFFF80;
	//sprintf(buf, "move_page %d \r\n",move_page);
	//Serial_PutString(buf);
	move_flash(move_page,APPLICATION_ADDRESS, end_page);
	
	uint32_t i,type,n,copy_addr,delta_addr, reloc_addr,reloc_last, fix_addr;
	uint16_t r_addr;
	copy_addr = APPLICATION_ADDRESS;
	
	reloc_addr = 0;
	if(r_start<r_end){
		r_addr=read16(r_start);
		reloc_last = reloc_addr;
		reloc_addr  = r_addr + 0x08000000;
	}
	
	if(f_start<f_end){
		fix_addr = read24(f_start)+0x08000000;
	}
	
	flash_buf_length = 0;
	
	while(d_start<d_end){
		
		n = read24(d_start);
		type = n >> 23;
		n = n&0x007fffff;

		//sprintf(buf, "d_start %d %d %d\r\n",d_start,n,type);
		//Serial_PutString(buf);
		
		if(type==0){
			delta_addr = read24(d_start + 3)+ 0x08000000+ move_page;
			d_start+=6;
		}
		else{
			delta_addr = d_start + 3;
			d_start+=3+n;
		}
		
		for(i=0;i<n;i++){
			flash_buf[flash_buf_length++] = (*(uint8_t*)(delta_addr + i));
			
			if(reloc_addr+4==copy_addr+flash_buf_length){
				flash_buf[flash_buf_length-4]=*(uint8_t*)(r_start+2);
				flash_buf[flash_buf_length-3]=*(uint8_t*)(r_start+3);
				flash_buf[flash_buf_length-2]=*(uint8_t*)(r_start+4);
				flash_buf[flash_buf_length-1]=*(uint8_t*)(r_start+5);
					
				
				
				r_start += 6;			
				reloc_last = reloc_addr;
				if(r_start<r_end){
					r_addr=read16(r_start);
					reloc_addr  = r_addr | (reloc_last&0xffff0000);
					if(reloc_addr<reloc_last){
						reloc_addr += 0x10000;
					}
					//sprintf(buf, "reloc_addr %d\r\n",reloc_addr);
					//Serial_PutString(buf);
				}
			}
			
			if(fix_addr + 1== copy_addr+flash_buf_length){
				flash_buf[flash_buf_length-1]=0;
				f_start += 3;
				if(f_start<f_end){
					fix_addr = read24(f_start)+0x08000000;
					//sprintf(buf, "fix_addr %d\r\n",fix_addr);
					//Serial_PutString(buf);
				}
			}

			if(flash_buf_length ==130){
				page_copy(copy_addr);
				flash_buf_length = 2;
				flash_buf[0] = flash_buf[128];
				flash_buf[1] = flash_buf[129];
				copy_addr+=128;
			}
		}
	}
	page_copy(copy_addr);
}

void update(void){
	
	uint32_t flag = read32(UPDATE_DATA_ADDRESS);
	application_jump();
	if(flag!=1){
		Serial_PutString("already updated\r\n");
		application_jump();
		return;
	}
	Serial_PutString("start update\r\n");
	HAL_FLASH_Unlock();
	
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR |
                         FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | FLASH_FLAG_FWWERR |
                         FLASH_FLAG_NOTZEROERR);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, UPDATE_DATA_ADDRESS, 0xffffffff);
	
	updateReloctable();
	Serial_PutString("update reloctable end\r\n");
	update_bin();
	
	HAL_FLASH_Lock();
	
	application_jump();
	//__set_FAULTMASK(1);
  //NVIC_SystemReset();
}

int8_t move_flash(uint32_t move_page, uint32_t start_page,uint32_t end_page){
	if(move_page==0)
		return 0;
	FLASH_EraseInitTypeDef desc;
  desc.TypeErase = FLASH_TYPEERASE_PAGES;
	desc.NbPages = 1;
	uint32_t result;
	
	Serial_PutString("move_flash start\r\n");
	uint8_t i;
	uint32_t *p=(uint32_t*) & flash_buf[0];
	while(end_page>=start_page){
		for(i=0;i<128;i++){
			flash_buf[i] = *(uint8_t*)(end_page+i);
		}
		
		desc.PageAddress = end_page + move_page;
		if (HAL_FLASHEx_Erase(&desc, &result) != HAL_OK){
			return 1;
		}

		if(HAL_FLASHEx_HalfPageProgram(end_page + move_page,p)!=HAL_OK){
			return 2;
		}
		if(HAL_FLASHEx_HalfPageProgram(end_page + move_page + 64,p+16)!=HAL_OK){
			return 2;
		}
		/*i=0;
		for(i=0;i<32;i++){
			if( HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, end_page + move_page + 4*i, *(p+i)) != HAL_OK){
				return 2;
			}
		}*/
		end_page-=128;
	}
	Serial_PutString(buf);
	Serial_PutString("move_flash end\r\n");
	return 0;
}

int8_t page_copy(uint32_t addr){
	uint8_t i,flag=0;
	for(i=0;i<128;i++){
		if(flash_buf[i]!=*(uint8_t*)(addr+i)){
			flag = 1;
		}
	}
	if(flag==0){
		return 0;
	}
	FLASH_EraseInitTypeDef desc;
	uint32_t result;
	uint32_t *p=(uint32_t*) & flash_buf[0];
	
  desc.TypeErase = FLASH_TYPEERASE_PAGES;
	desc.NbPages = 1;
	desc.PageAddress = addr;
	if (HAL_FLASHEx_Erase(&desc, &result) != HAL_OK){
		return 1;
	}

	if(HAL_FLASHEx_HalfPageProgram(addr,p)!=HAL_OK){
		return 2;
	}
	if(HAL_FLASHEx_HalfPageProgram(addr + 64,p+16)!=HAL_OK){
		return 2;
	}
		
	/*for(i=0;i<32;i++){
		if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4*i, *(p+i)) != HAL_OK){
			return 2;
		}
	}*/
	return 0;
}


void application_jump(){
	if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
	{
		Serial_PutString("Start program execution......\r\n\n");
			
		/* execute the new program */
		JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
		/* Jump to user application */
		JumpToApplication = (pFunction) JumpAddress;
		/* Initialize user application's Stack Pointer */
		__set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
		JumpToApplication();
	}
	else{
		Serial_PutString("Start program execution error\r\n\n");
	}
}

uint16_t read16(uint32_t address){
	uint16_t r = 0;
	r += *(uint8_t*)address;
	r += (*(uint8_t*)(address+1))<<8;
	return r;
}

uint32_t read24(uint32_t address){
	uint32_t r = 0;
	r += *(uint8_t*)address;
	r += (*(uint8_t*)(address+1))<<8;
	r += (*(uint8_t*)(address+2))<<16;
	return r;
}

uint32_t read32(uint32_t address){
	uint32_t r = 0;
	r += *(uint8_t*)address;
	r += (*(uint8_t*)(address+1))<<8;
	r += (*(uint8_t*)(address+2))<<16;
	r += (*(uint8_t*)(address+3))<<24;
	return r;
}

void write_to_buf16(uint16_t word){
	flash_buf[flash_buf_length++] = word & 0xff;
	flash_buf[flash_buf_length++] = word>>8;
}


void write_to_buf32(uint32_t dword){
	flash_buf[flash_buf_length++] = dword & 0xff;
	flash_buf[flash_buf_length++] = (dword>>8) & 0xff;
	flash_buf[flash_buf_length++] = (dword>>16) & 0xff;
	flash_buf[flash_buf_length++] = (dword>>24);
}
