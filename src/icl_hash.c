/**
 * @file icl_hash.c
 *
 * Dependency free hash table implementation.
 *
 * This simple hash table implementation should be easy to drop into
 * any other peice of code, it does not depend on anything else :-)
 * 
 * @author Jakub Kurzak
 */
/* $Id: icl_hash.c 2838 2011-11-22 04:25:02Z mfaverge $ */
/* $UTK_Copyright: $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "icl_hash.h"

#include <limits.h>


#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))
/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberger's (PJW) generic hashing
 * algorithm based on Allen Holub's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
static unsigned int
hash_pjw(void* key)
{
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

static int string_compare(void* a, void* b) 
{
    return (strcmp( (char*)a, (char*)b ) == 0);
}


/**
 * Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 * @param[in] hash_function -- pointer to the hashing function to be used
 * @param[in] hash_key_compare -- pointer to the hash key comparison function to be used
 *
 * @returns pointer to new hash table.
 */

hashtableUsers *
icl_hash_create( int nbuckets, config *confPointer, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) )
{
    hashtableUsers *ht = NULL;
    int i;

    ht = (hashtableUsers*) malloc(sizeof(hashtableUsers));
    if(!ht) return NULL;

    ht->users = NULL;
    ht->users = (user**)malloc(nbuckets * sizeof(user*));
    if(!ht->users){
        free(ht);
        return NULL;
    }

    ht->usersMutexes = NULL;
    ht->usersMutexes = (pthread_mutex_t *) malloc(N_MUTEXES*sizeof(pthread_mutex_t));
    for(i=0;i<N_MUTEXES;i++){
        pthread_mutex_init(&(ht->usersMutexes[i]),NULL);
    }

    ht->maxHistMessages = confPointer->MaxHistMsgs;
    ht->maxMsgSize = confPointer->MaxMsgSize;
    ht->maxFileFize = confPointer->MaxFileSize;
    ht->maxUsers = nbuckets;
    for(i=0;i<ht->maxUsers;i++)
        ht->users[i] = NULL;

    ht->hash_function = hash_function ? hash_function : hash_pjw;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare;

    return ht;
}

/**
 * Search for an entry in a hash table.
 *
 * @param ht -- the hash table to be searched
 * @param key -- the key of the item to search for
 *
 * @returns pointer to the data corresponding to the key.
 *   If the key was not found, returns NULL.
 */

user *
icl_hash_find(hashtableUsers *ht, void* key)
{
    user* curr;
    unsigned int hash_val;
    if(!ht || !key) return NULL;

    hash_val = (* ht->hash_function)(key) % ht->maxUsers;

    for (curr=ht->users[hash_val]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->nickname, key) )
            return curr;

    return NULL;
}

/**
 * Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */

user *
icl_hash_insert(hashtableUsers *ht, void* key, int fileDescriptor)
{
    user *curr = NULL;
    unsigned int hash_val;

    if(!ht || !key) return NULL;

    hash_val = (* ht->hash_function)(key) % ht->maxUsers;
    pthread_mutex_lock(&ht->usersMutexes[hash_val % N_MUTEXES]);

    for (curr=ht->users[hash_val]; curr != NULL; curr=curr->next){
        /* key already exists */
        if ( ht->hash_key_compare(curr->nickname, key)){ 
            pthread_mutex_unlock(&ht->usersMutexes[hash_val % N_MUTEXES]);
            return(NULL);
        }
    }

    /* if key was not found */

    curr = (user*)malloc(sizeof(user));
    if(!curr){
        pthread_mutex_unlock(&ht->usersMutexes[hash_val % N_MUTEXES]);
        return NULL;
    }

    curr->histMessages = (message_t **) malloc(ht->maxHistMessages*sizeof(message_t*));
    for(int i=0; i<ht->maxHistMessages; i++){
        curr->histMessages[i] = NULL; // to make valgrind happy
        curr->histMessages[i] = (message_t *) malloc(sizeof(message_t));
        memset(curr->histMessages[i],0,sizeof(message_t));
        curr->histMessages[i]->data.buf = NULL; // to make valgrind happy
    }

    strncpy(curr->nickname,(char *)key, MAX_NAME_LENGTH);
    curr->indexMessages = 0;
    curr->nRemoteMessages = 0;
    curr->fd = fileDescriptor;
    curr->online = 1;
    curr->next = ht->users[hash_val]; /* add at start */

    ht->users[hash_val] = curr;
    ht->nUsersSubscribed++;

    pthread_mutex_unlock(&ht->usersMutexes[hash_val % N_MUTEXES]);
    return curr;
}

/**
 * Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_delete(hashtableUsers *ht, void* key, void (*free_key)(void*), void (*free_data)(void*))
{
    user *curr, *prev;
    unsigned int hash_val;
    int wasOnline;

    if(!ht || !key) return -1;
    hash_val = (* ht->hash_function)(key) % ht->maxUsers;

    pthread_mutex_lock(&ht->usersMutexes[hash_val % N_MUTEXES]);

    prev = NULL;
    for (curr=ht->users[hash_val]; curr != NULL; )  {
        if ( ht->hash_key_compare(curr->nickname, key)) {
            if(curr->online == 1) wasOnline = 1;
            else wasOnline = 0;
            if (prev == NULL) {
                ht->users[hash_val] = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (*free_key && curr->nickname) (*free_key)(curr->nickname);
            //if (*free_data && curr->fd) (*free_data)(curr->fd);
            for(int i=0; i<ht->maxHistMessages; i++){
                if(curr->histMessages[i] != NULL){
                    if(curr->histMessages[i]->data.buf != NULL) free(curr->histMessages[i]->data.buf);
                    free(curr->histMessages[i]);
                }
            }
            free(curr->histMessages);
            ht->nUsersSubscribed--;
            free(curr);
            pthread_mutex_unlock(&ht->usersMutexes[hash_val % N_MUTEXES]);
            return wasOnline;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&ht->usersMutexes[hash_val % N_MUTEXES]);
    return -1;
}

/**
 * Free hash table structures (key and data are freed using functions).
 *
 * @param ht -- the hash table
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int
icl_hash_destroy(hashtableUsers *ht, void (*free_key)(void*), void (*free_data)(void*))
{
    user *bucket, *curr, *next;
    int i;

    if(!ht) return -1;

    for (i=0; i<ht->maxUsers; i++) {
        bucket = ht->users[i];
        for (curr=bucket; curr!=NULL; ) {
            next=curr->next;
            //if (*free_key && curr->nickname) (*free_key)(curr->nickname);
            //if (*free_data && curr->fd) (*free_data)(curr->fd);
            for(int i=0; i<ht->maxHistMessages; i++){
                if(curr->histMessages[i] != NULL){
                    if(curr->histMessages[i]->data.buf != NULL) free(curr->histMessages[i]->data.buf);
                    free(curr->histMessages[i]);
                }
            }
            free(curr->histMessages);
            
            free(curr);
            curr=next;
        }
    }
    for(i=0;i<N_MUTEXES;i++){
		    pthread_mutex_destroy(&(ht->usersMutexes[i]));
    }

    if(ht->usersMutexes) free(ht->usersMutexes);
    if(ht->users) free(ht->users);
    if(ht) free(ht);

    return 0;
}

/**
 * Dump the hash table's contents to the given file pointer.
 *
 * @param stream -- the file to which the hash table should be dumped
 * @param ht -- the hash table to be dumped
 *
 * @returns 0 on success, -1 on failure.
 */

int
icl_hash_dump(FILE* stream, hashtableUsers* ht)
{
    user *bucket, *curr;
    int i;

    if(!ht) return -1;

    for(i=0; i<ht->maxUsers; i++) {
        bucket = ht->users[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->nickname)
                fprintf(stream, "icl_hash_dump: %s: %p\n", (char *)curr->nickname, curr->fd);
            curr=curr->next;
        }
    }

    return 0;
}


