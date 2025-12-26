#include "fifo.h"

int FifoCreateSlots(SLOT_T *ptr_slots, int item_count, int item_size)
{
	if(!ptr_slots){
		printf("[%s], ptr_slots = NULL\n", __func__);
		return -1;
	}
	
	ptr_slots->list = (NODE_T *)malloc(item_count * sizeof(NODE_T));
	if(!ptr_slots->list){
		printf("[%s], ptr_slots->list = NULL\n", __func__);
		return -1;
	}
	
	ptr_slots->item_array = malloc(item_count * item_size * sizeof(uint8_t));
	if(!ptr_slots->item_array){
		printf("[%s], ptr_slots->item_array = NULL\n", __func__);
		free(ptr_slots->list);
		return -1;
	}
	
	// init free item list
	for(int i = 0; i < item_count; i++)
	{
		ptr_slots->list[i].next = 0;
		ptr_slots->list[i].item_size = item_size;
		ptr_slots->list[i].ptr_mem = &ptr_slots->item_array[i * item_size];
	}
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int FifoDeleteSlots(SLOT_T *ptr_slots)
{
	if(!ptr_slots){
		printf("[%s], ptr_slots = NULL\n", __func__);
		return -1;
	}
	
	free(ptr_slots->item_array);
	free(ptr_slots->list);
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int FifoInit(FIFO_T *ptr_fifo)
{
	if(!ptr_fifo){
		printf("[%s], ptr_fifo = NULL\n", __func__);
		return -1;
	}
	
	if(ptr_fifo->state == FIFO_READY){
		printf("[%s], fifo already initialized\n", __func__);
		return -1;
	}
	
	ptr_fifo->state = FIFO_NOT_EXIST;
	
	if(pthread_mutex_init(&ptr_fifo->lock, NULL) == -1){
		printf("[%s], free_list_lock = -1\n", __func__);
		return -1;
	}
		
	ptr_fifo->list_head = 0;
	ptr_fifo->list_tail = 0;
	ptr_fifo->state = FIFO_READY;
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int FifoFill(FIFO_T *ptr_fifo, SLOT_T *ptr_slot, int slot_cnt)
{
	int ret = 0;
	
	for(int i = 0; i < slot_cnt; i++)
	{
		ret = FifoPush(ptr_fifo, &ptr_slot->list[i]);
		if(ret != 0)
			break;
	}
	
	return ret;
}

/*---------------------------------------------------------------------------------------------*/

int FifoClear(FIFO_T *ptr_fifo)
{
	if(ptr_fifo->state == FIFO_READY){
		printf("[%s], fifo already initialized\n", __func__);
		return -1;
	}
	
	if(ptr_fifo->state != FIFO_READY){
		printf("[%s], fifo not exist\n", __func__);
		return -1;
	}
	
	pthread_mutex_lock(&ptr_fifo->lock);
	
	ptr_fifo->list_head = 0;
	ptr_fifo->list_tail = 0;
	
	pthread_mutex_unlock(&ptr_fifo->lock);
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

int FifoPush(FIFO_T *ptr_fifo, NODE_T *ptr_item)
{
	
	if(!ptr_fifo){
		printf("[%s], ptr_fifo = NULL\n", __func__);
		return -1;
	}
	
	if(!ptr_item){
		printf("[%s], ptr_item = NULL\n", __func__);
		return -1;
	}
	
	if(ptr_fifo->state != FIFO_READY){
		printf("[%s], fifo not exist\n", __func__);
		return -1;
	}
	
	pthread_mutex_lock(&ptr_fifo->lock);
	
	ptr_item->next = 0;
	
	if(ptr_fifo->list_tail)
		ptr_fifo->list_tail->next = ptr_item;
	
	ptr_fifo->list_tail = ptr_item;
	
	if(!ptr_fifo->list_head)
		ptr_fifo->list_head = ptr_fifo->list_tail;
	
	pthread_mutex_unlock(&ptr_fifo->lock);
	
	return 0;
}

/*---------------------------------------------------------------------------------------------*/

NODE_T *FifoPop(FIFO_T *ptr_fifo)
{
	NODE_T *item;
	
	if(!ptr_fifo){
		printf("[%s], ptr_fifo = NULL\n", __func__);
		return 0;
	}
	
	if(ptr_fifo->state != FIFO_READY){
		printf("[%s], fifo not exist\n", __func__);
		return 0;
	}
	
	if(!ptr_fifo->list_head)
		return 0;
	
	item = ptr_fifo->list_head;
	
	pthread_mutex_lock(&ptr_fifo->lock);
	
	if(ptr_fifo->list_head->next)
		ptr_fifo->list_head = ptr_fifo->list_head->next;
	else {
		ptr_fifo->list_head = 0;
		ptr_fifo->list_tail = 0;
	}
	
	pthread_mutex_unlock(&ptr_fifo->lock);
	
	return item;
}
