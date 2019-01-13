/**
 * @file
 *
 * Header file for icl_hash routines.
 *
 */
/* $Id$ */
/* $UTK_Copyright: $ */

#ifndef icl_hash_h
#define icl_hash_h

#include <stdio.h>
#include <pthread.h>
#include "config.h"
#include "message.h"

#define N_MUTEXES 10 // mutexes for the users' hashtable
#define MAX_USERS_LISTS 200 // max server users' lists

typedef struct user_t {
    int fd;
    char nickname[MAX_NAME_LENGTH + 1];
    short online;
    int indexMessages;
    unsigned int nRemoteMessages;
    message_t **histMessages;
    struct user_t *next;
} user;

typedef struct users_t {
    int maxUsers;
    int maxHistMessages;
    int maxMsgSize;
    int maxFileFize;
    int nUsersSubscribed;
    user **users;
    pthread_mutex_t *usersMutexes;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
} hashtableUsers;

hashtableUsers *
icl_hash_create( int nbuckets, config *confStruct, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );

user
* icl_hash_find(hashtableUsers *, void* );

user
* icl_hash_insert(hashtableUsers *, void*, int);

int
icl_hash_destroy(hashtableUsers *, void (*)(void*), void (*)(void*)),
    icl_hash_dump(FILE *, hashtableUsers *);

int icl_hash_delete( hashtableUsers *ht, void* key, void (*free_key)(void*), void (*free_data)(void*) );


#define icl_hash_foreach(ht, tmpint, tmpent, kp, dp)    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++)        \
        for (tmpent=ht->buckets[tmpint];                                \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL); \
             tmpent=tmpent->next)


#endif /* icl_hash_h */
