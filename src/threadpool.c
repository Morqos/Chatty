#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>


#define POOL_ERROR_HANDLER(msg, ret) \
			do{ perror(msg); return(ret); } while(0)

#define DESTROY_POOL_AND_EXIT(pool, msg, ret) \
		do{ \
			free(pool->queueDescriptor.queueFd); \
			free(pool); \
			POOL_ERROR_HANDLER(msg, ret); \
		} while(0);


// threads' first START FUNCTION
static void *initializeThreads(void *threadID){
	threadDescriptor *descriptor = (threadDescriptor *)threadID;
	threadPool *threadpoolPointer = (threadPool *)descriptor->poolContainer;
    int socketFd;

	while (1) {
		pthread_mutex_lock(&threadpoolPointer->mutex);

		while (threadpoolPointer->queueDescriptor.count == 0) {
			pthread_cond_wait(&threadpoolPointer->cond, &threadpoolPointer->mutex);
		}

		if(threadpoolPointer->queueDescriptor.count < 0){
			pthread_mutex_unlock(&threadpoolPointer->mutex);
			return NULL;
		} 

		socketFd = threadpoolPointer->queueDescriptor.queueFd[threadpoolPointer->queueDescriptor.tail];
		threadpoolPointer->queueDescriptor.queueFd[threadpoolPointer->queueDescriptor.tail] = -1;


		threadpoolPointer->queueDescriptor.tail = (threadpoolPointer->queueDescriptor.tail+1) % threadpoolPointer->queueDescriptor.size;
		threadpoolPointer->queueDescriptor.count--;
		
		pthread_mutex_unlock(&threadpoolPointer->mutex);
		
		/* Gestisce la connessione socketFd */
		threadpoolPointer->startFunction(socketFd);
	}

	return NULL;
}



/**
 * @function				poolCreate
 * @brief					Create a server pool.
 * 
 * @param	size			Number of threads
 * @param	queueSize		Queue dimension
 * @param	startFunction	Threads' execution function
 * 
 * @return	Pointer to threadPoll struct created, NULL on error
 * 
 */
threadPool *poolCreate(int size, int queueSize, int (*startFunction)(int)) {

	if (size <= 0 || queueSize <= 0) POOL_ERROR_HANDLER("invalid size or queueSize", NULL);

	threadPool *pool = (threadPool *)malloc(sizeof(threadPool));
	if(pool == NULL) POOL_ERROR_HANDLER("threadpool allocation", NULL);


	pool->startFunction = startFunction;
	pool->size = size;
	pool->queueDescriptor.size = queueSize;
	pool->queueDescriptor.tail = pool->queueDescriptor.head = pool->queueDescriptor.count = 0;
	pool->queueDescriptor.queueFd = (int *)malloc(sizeof(int) * queueSize);

	if (pool->queueDescriptor.queueFd == NULL){
        free(pool);
        POOL_ERROR_HANDLER("queue file descriptors allocation", NULL);
    }
	for(int i=0; i<queueSize; i++)
		pool->queueDescriptor.queueFd[i] = -1;

	pool->threads = (threadDescriptor *) malloc(pool->size*sizeof(threadDescriptor));
	if (pool->threads == NULL){
		DESTROY_POOL_AND_EXIT(pool, "threads allocation", NULL);
    }

	errno = pthread_mutex_init(&pool->mutex, NULL);
	if(errno) DESTROY_POOL_AND_EXIT(pool, "pthread_mutex_init", NULL);

	
	errno = pthread_cond_init(&pool->cond, NULL);
	if(errno) DESTROY_POOL_AND_EXIT(pool, "pthread_cond_init", NULL);


	pthread_attr_t threadAttribute;
	pthread_attr_init(&threadAttribute);
	// thread launch
	for (int i = 0; i < pool->size; i++) {
		pool->threads[i].poolContainer = pool;
		errno = pthread_create(&pool->threads[i].id, NULL, initializeThreads, (void *)&pool->threads[i]);
		if (errno) {
            pthread_mutex_destroy(&(pool->mutex));
			DESTROY_POOL_AND_EXIT(pool, "pthread_create", NULL);
		}
	}

	return pool;
}



/**
 * @function		poolDestroy
 * @brief			Uccide i thread e dealloca il pool.
 *
 * @param	pool	pool di thread
 * 
 */
void poolDestroy(threadPool *pool) {
	if (pool == NULL)
		return;

	pthread_mutex_lock(&pool->mutex);
	pthread_cond_broadcast(&pool->cond);
	pool->queueDescriptor.count = -9999;
	pthread_mutex_unlock(&pool->mutex);


	for (int i = 0 ; i < pool->size; i++) {
		pthread_join(pool->threads[i].id, NULL);
	}

	free(pool->threads);
	free(pool->queueDescriptor.queueFd);
    pthread_mutex_destroy(&(pool->mutex));
	free(pool);
	
}
