#ifndef _FIFO_H_
#define _FIFO_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

enum FIFO_STATE
{
	FIFO_NOT_EXIST = 0,
	FIFO_READY,
};

typedef struct  
{
	uint8_t *ptr_mem;
	int 	item_size;
	void    *next;
} NODE_T;

typedef struct 
{
	NODE_T *list;
	uint8_t *item_array;
} SLOT_T;

typedef struct 
{
	int state;
	NODE_T *list_head;
	NODE_T *list_tail;
	pthread_mutex_t lock;
} FIFO_T;

int FifoCreateSlots(SLOT_T *ptr_slots, int item_count, int item_size);
int FifoDeleteSlots(SLOT_T *ptr_slots);
int FifoInit(FIFO_T *ptr_fifo);
int FifoFill(FIFO_T *ptr_fifo, SLOT_T *ptr_slot, int slot_cnt);
int FifoClear(FIFO_T *ptr_fifo);
int FifoPush(FIFO_T *ptr_fifo, NODE_T *ptr_item);
NODE_T *FifoPop(FIFO_T *ptr_fifo);


#endif