#ifndef EZQ_H
#define EZQ_H

#include <pthread.h>

#define QUEUE_FRONT 0
#define QUEUE_BACK 1

typedef struct QueueNode_ QueueNode;
struct QueueNode_
{
	QueueNode* prev;
	QueueNode* next;
	void* data;
};

typedef struct
{
	QueueNode* abs_front; /* make it the queue's job to free all objects and data queued at the end */
	QueueNode* front;
	QueueNode* back;
	void (* free_function)(void*);
	int length;
	int total_length;
	pthread_mutex_t lock;
} Queue;


Queue* initQueue(void (* free_function)(void*));
void enqueue(Queue* queue, void* data);
void priorityEnqueue(Queue* queue, void* data);
void* dequeue(Queue* queue);
void* peekQueue(Queue* queue, int queue_location);
void flushQueue(Queue* queue);
void freeQueue(Queue* queue);
void resetQueue(Queue* queue);
void restartQueue(Queue* queue);
void cleanQueue(Queue* queue);

#endif //EZQ_H
