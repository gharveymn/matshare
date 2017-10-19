#include "headers/getDataObjects.h"
#include "headers/ezq.h"


Queue* initQueue(void (* free_function)(void*))
{
	Queue* new_queue = malloc(sizeof(Queue));
	new_queue->abs_front = NULL;
	new_queue->front = NULL;
	new_queue->back = NULL;
	new_queue->length = 0;
	new_queue->total_length = 0;
	new_queue->free_function = free_function;
	pthread_mutex_init(&new_queue->lock, NULL);
	return new_queue;
}


void enqueue(Queue* queue, void* data)
{
	QueueNode* new_node = malloc(sizeof(QueueNode));
	new_node->data = data;
	pthread_mutex_lock(&queue->lock);
	if(queue->total_length == 0)
	{
		new_node->next = NULL;
		new_node->prev = NULL;
		queue->abs_front = new_node;
		queue->front = new_node;
		queue->back = queue->front;
	}
	else
	{
		new_node->prev = queue->back;
		new_node->next = queue->back->next;
		
		if(queue->back->next != NULL)
		{
			queue->back->next->prev = new_node;
		}
		queue->back->next = new_node;
		queue->back = new_node;
		if(queue->length == 0)
		{
			queue->front = new_node;
		}
	}
	queue->length++;
	queue->total_length++;
	pthread_mutex_unlock(&queue->lock);
}


void priorityEnqueue(Queue* queue, void* data)
{
	QueueNode* new_node = malloc(sizeof(QueueNode));
	pthread_mutex_lock(&queue->lock);
	new_node->data = data;
	if(queue->total_length == 0)
	{
		new_node->next = NULL;
		new_node->prev = NULL;
		queue->abs_front = new_node;
		queue->front = new_node;
		queue->back = queue->front;
	}
	else if(queue->length == 0)
	{
		//means the queue is reset, so just normal queue instead so we don't confuse the total length
		new_node->prev = queue->back;
		new_node->next = queue->back->next;
		if(queue->back->next != NULL)
		{
			queue->back->next->prev = new_node;
		}
		queue->back->next = new_node;
		queue->back = new_node;
		queue->front = new_node;
		
	}
	else
	{
		new_node->prev = queue->front->prev;
		new_node->next = queue->front;
		
		if(queue->front == queue->abs_front)
		{
			queue->abs_front = new_node;
		}
		else
		{
			queue->front->prev->next = new_node;
		}
		
		queue->front->prev = new_node;
		queue->front = new_node;
		
	}
	queue->length++;
	queue->total_length++;
	pthread_mutex_unlock(&queue->lock);
}


void resetQueue(Queue* queue)
{
	pthread_mutex_lock(&queue->lock);
	queue->front = queue->back;
	queue->length = 0;
	pthread_mutex_unlock(&queue->lock);
}

void restartQueue(Queue* queue)
{
	pthread_mutex_lock(&queue->lock);
	queue->front = queue->abs_front;
	queue->length = queue->total_length;
	pthread_mutex_unlock(&queue->lock);
}


void* dequeue(Queue* queue)
{
	pthread_mutex_lock(&queue->lock);
	if(queue->front != NULL)
	{
		void* to_return = queue->front->data;
		QueueNode* new_front = queue->front->next;
		if(new_front != NULL)
		{
			queue->front = new_front;
		}
		queue->length--;
		pthread_mutex_unlock(&queue->lock);
		return to_return;
	}
	else
	{
		pthread_mutex_unlock(&queue->lock);
		return NULL;
	}
}


void* peekQueue(Queue* queue, int queue_location)
{
	if(queue->front != NULL)
	{
		if(queue_location == QUEUE_FRONT)
		{
			return queue->front->data;
		}
		else if(queue_location == QUEUE_BACK)
		{
			return queue->back->data;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}


void flushQueue(Queue* queue)
{
	if(queue != NULL)
	{
		pthread_mutex_lock(&queue->lock);
		while(queue->abs_front != NULL)
		{
			QueueNode* next = queue->abs_front->next;
			if(queue->abs_front->data != NULL && queue->free_function != NULL)
			{
				queue->free_function(queue->abs_front->data);
			}
			free(queue->abs_front);
			queue->abs_front = next;
		}
		queue->abs_front = NULL;
		queue->front = NULL;
		queue->back = NULL;
		queue->length = 0;
		queue->total_length = 0;
		pthread_mutex_unlock(&queue->lock);
	}
}


void cleanQueue(Queue* queue)
{
	//move the absolute front to the same position as front and free up the queue objects along the way
	if(queue != NULL)
	{
		pthread_mutex_lock(&queue->lock);
		while(queue->abs_front != queue->front)
		{
			QueueNode* next = queue->abs_front->next;
			if(queue->abs_front->data != NULL)
			{
				queue->free_function(queue->abs_front->data);
			}
			queue->abs_front->prev = NULL;
			queue->abs_front->data = NULL;
			free(queue->abs_front);
			queue->abs_front = next;
			queue->total_length--;
		}
		pthread_mutex_unlock(&queue->lock);
	}
}


void freeQueue(Queue* queue)
{
	if(queue != NULL)
	{
		flushQueue(queue);
		free(queue);
	}
}