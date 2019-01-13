/*
 * chatterbox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES		10
#define MAX_SLEEPING	3

#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#include <connections.h>
#include <message.h>

#define SOCKETNAME "chatterbox_sock_538908"

#define SYSCALL(r,c,e) \
    if((r = c) == -1){ perror(e); exit(errno); }

/**
 * @file  connection.c
 * @brief Functions for Client-Server Protocol
 *
 */

void appendPathFile(char *whereStorePath, char *path,char *file){
	strcpy(whereStorePath, path);
    strcat(whereStorePath, "/");
    strcat(whereStorePath, file);
	strcat(whereStorePath, "\0");
}


/**
 * @functions 		readAll() and writeAll()
 * @brief 			make sure to read/write the entire buffer
 * 
 * @param fd 		file descriptor where read/write
 * @param ptr		pointer to the buffer to read/write
 * @param size 		dimension of the buffer to read/write
 * 
 * return			1, 0 or -1 on error
 * 
*/

int readAll(int fd, void *ptr, int size)
{
	if(fd == -1) return -1;
	if(size == 0) return 0;

	int sum = 0, n = 0;
	while(sum < size)
	{
		if((n = read(fd, ptr+sum, size-sum)) == -1)
		{
			if(errno == EINTR) continue;
			else if(errno == ECONNRESET) return 0; // ignore the case where socket's connection is resetted
			else return -1;
		}
		if(n == 0) return 0;
		else sum += n;
	}

	return 1;
}


int writeAll(int fd, void *ptr, int size)
{
	if(ptr == NULL || fd == -1) return -1;
	if(size == 0) return 0;

	int sum = 0, n = 0;
	while(sum < size && ptr != NULL)
	{
		if((n = write(fd, ptr+sum, size-sum)) == -1){
			if(errno == EINTR) continue;
			else if(errno == EPIPE)	return 0; // ignore case where socket doesn't work correctly
			else return -1;
		}
		if(n == 0) return 0;
		else sum += n;
	}

	return 1;
}

// static inline int writen(long fd, void *buf, size_t size, char *msg)
// {
//     size_t left = size;
//     ssize_t r;
//     char *bufptr = (char*)buf;
//     while(left > 0 && buf != NULL)
//     {
//         if ((r = write((int)fd ,bufptr,left)) == -1)
//         {
//             if (errno == EINTR)
//                 continue;
//             else if (errno == EPIPE) // ignoro il caso in cui il socket non funziona correttamente
//                 return 0;
//             else
//                 ERRORE(msg)
//         }
//         if (r == 0) return 0;
//         left    -= r;
//         bufptr  += r;
//     }
//     return 1;
// }



/**
 * @function		openConnection
 * @brief 			Opens an AF_UNIX connection
 *
 * @param path 		Path of AF_UNIX socket 
 * @param ntimes 	Max attempts for retry
 * @param secs 		Wait time between two entries
 *
 * @return			fd connection, -1 on error
 *
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs){

    if( ntimes > MAX_RETRIES || secs > MAX_SLEEPING || (strlen(path) > UNIX_PATH_MAX) ) return -1;

    struct sockaddr_un sa;
	char pathOfSocket[128];

	strcpy(sa.sun_path, path);
	sa.sun_family = AF_UNIX;

	int fd_skt, i = 0;
	SYSCALL(fd_skt, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
    while ((connect(fd_skt, (struct sockaddr *)&sa, sizeof(sa)) == -1) && i < ntimes){
        printf("I'm waiting a connection\n");
        if( errno == ENOENT ){
            sleep(secs);
            i++;
        }
        else return -1;
    }

    return fd_skt;
}

// -------- server side ----- 
/**
 * @function	readHeader
 * @brief		Reads message's header
 *
 * @param fd	Connection's descriptor
 * @param hdr	Pointer to the header of the message
 *
 * @return		1 on success, -1 on error
 *
 */
int readHeader(long connfd, message_hdr_t *hdr){
   
	op_t temp;
	char sender[MAX_NAME_LENGTH+1];
	if(readAll(connfd,&temp,sizeof(op_t))<=0) return -1;
	if(readAll(connfd,sender,MAX_NAME_LENGTH+1)<=0) return -1;
	setHeader(hdr,temp,sender);
	return 1;
}





/**
 * @function	readData
 * @brief		Reads message's body
 *
 * @param fd	Connection's descriptor
 * @param data	Pointer to message's body
 *
 * @return		1 on success, -1 on error
 * 
 */
int readData(long fd, message_data_t *data){

	char receiver[MAX_NAME_LENGTH+1];
	//char* buf=NULL;
	size_t sz;
	if(readAll(fd,receiver,MAX_NAME_LENGTH+1)<=0) return -1;
	if(readAll(fd,&sz,sizeof(size_t))<=0) return -1;
	if(sz>0){
	  data->buf = malloc((int)sz*sizeof(char));
	  memset(data->buf,0,(int)sz);
	  if(readAll(fd,data->buf,(int)sz)<=0) return -1;
	}
	setData(data,receiver,data->buf,sz);
  
	return sz;
}

/**
 * @function	readMsg    
 * @brief		Reads the entire message
 *
 * @param fd	Connection's descriptor
 * @param data	Pointer to the message
 *
 * @return		1 on success, -1 on error
 * 
 */
int readMsg(long fd, message_t *msg){
	op_t temp;
	size_t sz;
	char sender[MAX_NAME_LENGTH+1];
	char receiver[MAX_NAME_LENGTH+1];
	memset(sender,0,MAX_NAME_LENGTH+1);
	memset(receiver,0,MAX_NAME_LENGTH+1);
	msg->data.buf = NULL;

	msg->hdr.op = OP_FAIL; //initialize to make valgrind happy
	msg->hdr.sender[0] = '\0'; //initialize to make valgrind happy
	if(readAll(fd,&temp,sizeof(op_t))<=0) return -1;
	if(readAll(fd,sender,MAX_NAME_LENGTH+1)<=0) return -1;
	setHeader(&(msg->hdr),temp,sender);
  
	if(readAll(fd,receiver,MAX_NAME_LENGTH+1)<=0) return -1;
	if(readAll(fd,&sz,sizeof(size_t))<=0) return -1;
	if(sz>0)
	{
	  msg->data.buf = malloc((int)sz*sizeof(char*));
	  memset(msg->data.buf,0,(int)sz);
	  if(readAll(fd,msg->data.buf,(int)sz)<=0) return -1;
	}
	setData(&(msg->data),receiver,msg->data.buf,sz);
	return 1;
}

/* da completare da parte dello studente con altri metodi di interfaccia */

// ------- client side ------
/**
 * @function	sendRequest
 * @brief		Sends a request message to the Server
 *
 * @param fd	Connection's descriptor
 * @param msg	Pointer to the message
 *
 * @return		1 on success, -1 on error
 * 
 */
int sendRequest(long fd, message_t *msg){
	if(writeAll(fd,&(msg->hdr.op),sizeof(op_t))<=0) return -1;
	if(writeAll(fd,(char*)(*msg).hdr.sender,MAX_NAME_LENGTH+1)<=0) return -1;
	if(writeAll(fd,(char*)(*msg).data.hdr.receiver,MAX_NAME_LENGTH+1)<0) return -1;
  
	if(writeAll(fd,&((*msg).data.hdr.len),sizeof(size_t))<0) return -1;
	if((*msg).data.hdr.len>0)
	{
	   if(writeAll(fd,(char*)(*msg).data.buf,(*msg).data.hdr.len)<0) return -1;
	}
	return 1;
}

/**
 * @function	sendData
 * @brief		Sends the message's body to the Server
 *
 * @param fd	Connection's descriptor
 * @param msg	Pointer to the message to send
 *
 * @return		1 on success, -1 on error
 * 
 */
int sendData(long fd, message_data_t *msg){

	if(writeAll(fd,(char*)(*msg).hdr.receiver,MAX_NAME_LENGTH+1)<0) return -1;
	if(writeAll(fd,&((*msg).hdr.len),sizeof(size_t))<0) return -1;
	if(msg->hdr.len>0)
	{
		if(writeAll(fd,(*msg).buf,(*msg).hdr.len)<0) return -1;
	}
	return 1;
}


/* da completare da parte dello studente con eventuali altri metodi di interfaccia */

/**
 * @function	sendReply
 * @brief		Sends a response message to Client
 *
 * @param fd	Connection's descriptor
 * @param op	Response's code of operation
 * @param key	Object key
 * @param msg	Pointer to the message to send
 *
 * @return		1 on success, -1 on error
 */
int sendReply(long fd, op_t op, char *key) {
	if(writeAll(fd,&(op),sizeof(op_t))<=0) return -1;
	if(writeAll(fd,(char*)key,MAX_NAME_LENGTH+1)<=0) return -1;
	return 1;
}

#endif /* CONNECTIONS_H_ */