/**
 * @file threadpool.h
 * @brief File contenente le struct per il pool e le sue dichiarazioni di funzioni
 * 
 */

#ifndef THREAD_H_
#define THREAD_H_

#include <pthread.h>

typedef struct _queueStruct {
	int size;		// max dimension
	int *queueFd;	// queue file descriptors
	int head;		// first free place index in the queue to insert
	int tail;		// index element to remove
	int count;		// number of actual elements in the queue 
} queueStruct;

typedef struct _threadDescriptor {
	pthread_t id;
	void * poolContainer;	// pointer to the pool container
} threadDescriptor;

// Thread pool
typedef struct _threadPool {
	int size;						// pool dimension
	threadDescriptor *threads;		// thread informations
	pthread_mutex_t mutex;			// mutex for thread sincronization
	pthread_cond_t cond;
	queueStruct queueDescriptor;	// queue file descriptors
	int (*startFunction)(int);		// threads execution functions
} threadPool;

threadPool *poolCreate(int, int, int(*)(int));
int poolWaitTerm(threadPool *);
void poolDestroy(threadPool *);

#endif /* THREAD_H_ */
