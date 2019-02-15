/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>


#include "stats.h"
#include "config.h"
#include "icl_hash.h"
#include "threadpool.h"
#include "connections.h"
#include "handle_requests.h"


// global variables
config *conf; // pointer to config struct
hashtableUsers *hashtablePointer; // pointer to hashtableUsers struct
threadPool *threadpoolPointer; // pointer to the threadpool
struct sockaddr_un sa; // socket address
char socketPathName[128]; // pathName of the socket
int fd_skt; // socket file descriptor
static struct sigaction sa1, sa2; // signal action pointers
struct statistics chattyStats = { 0,0,0,0,0,0,0 }; // server stats
pthread_mutex_t chattyStatsMutex;
fd_set set; // set of pselect()
int fd_hwm = 0; // maximux value for a fd in select

// macros
#define DESTROY_CONF \
		free(conf)

#define DESTROY_ALL \
        do{ \
            DESTROY_CONF; \
            poolDestroy(threadpoolPointer); \
            icl_hash_destroy(hashtablePointer, NULL, NULL); \
        } while(0)


#define DESTROY_SOCKET \
	    do{ close(fd_skt); \
            unlink(socketPathName); } while(0)

#define DESTROY_MUTEX_STATS \
        do{ \
            pthread_mutex_destroy(&chattyStatsMutex); \
        } while(0)


void destroyEverything(){
    DESTROY_SOCKET;
    DESTROY_MUTEX_STATS;
    DESTROY_ALL;
    exit(EXIT_SUCCESS);
}

user *findUserByFd(int fdSocket){
    user *curr, *bucket;

    for (int i=0; i<hashtablePointer->maxUsers; i++) {
        bucket = hashtablePointer->users[i];
        for (curr=bucket; curr!=NULL; curr=curr->next) {
            if(curr->fd == fdSocket) return curr;
        }
    }
    return NULL;
}


int checkTerminalDisconnected(int fd){
    if(fd < 0) return -1;

    int connectionOpened = 0;
    if(ioctl(fd,FIONREAD,&connectionOpened) < 0){ // checks if the terminal is disconnected
        perror("ioctl");
        return -1;
    }

    user *userToDisconnect = findUserByFd(fd);
    if(!connectionOpened){
        if(userToDisconnect != NULL){
            lockMutexUsers(hashtablePointer, userToDisconnect->nickname);
            userToDisconnect->online = 0;
            userToDisconnect->fd = -1;
            unlockMutexUsers(hashtablePointer, userToDisconnect->nickname);

            close(fd);
            statsDisconnect();
        }
    }

    return connectionOpened;
}

void addFdToQueue(int fd_client){
    pthread_mutex_lock(&threadpoolPointer->mutex);

    int headIndex = threadpoolPointer->queueDescriptor.head;
    int nFdInQueue = threadpoolPointer->queueDescriptor.count;

    if(nFdInQueue != threadpoolPointer->queueDescriptor.size){
        threadpoolPointer->queueDescriptor.queueFd[headIndex] = fd_client;
        headIndex = (headIndex + 1) % threadpoolPointer->queueDescriptor.size;
        threadpoolPointer->queueDescriptor.head = headIndex;
        threadpoolPointer->queueDescriptor.count++;
        pthread_cond_signal(&threadpoolPointer->cond);
    }

    pthread_mutex_unlock(&threadpoolPointer->mutex);
}

int threadsStartFunction(int socketFd){

    if(socketFd == -1) return 0;

    message_t msg;
    char nameUser[MAX_NAME_LENGTH+1];

    int correctRead = readMsg(socketFd, &msg);
    
    if(correctRead != -1){
        strcpy(nameUser, msg.hdr.sender);
        switch(msg.hdr.op) {
            case REGISTER_OP: handleRegister(hashtablePointer, socketFd,msg.hdr.sender); break;
            case CONNECT_OP: handleConnect(hashtablePointer, socketFd,msg.hdr.sender); break;
            case USRLIST_OP: handleUsrlist(hashtablePointer, socketFd,msg.hdr.sender); break;
            
            case POSTFILE_OP: handlePostFile(hashtablePointer, conf->DirName, socketFd,&msg); break;
            case GETFILE_OP: handleGetFile(hashtablePointer, conf->DirName, socketFd,&msg); break;

            case POSTTXT_OP: handlePostTxt(hashtablePointer, socketFd,&msg); break;
            case POSTTXTALL_OP: handlePostTxtAll(hashtablePointer, socketFd,&msg); break;
            case GETPREVMSGS_OP: handleGetPrevMsgs(hashtablePointer, socketFd,msg.hdr.sender); break;
            
            case UNREGISTER_OP: handleUnregister(hashtablePointer, socketFd,msg.hdr.sender); break;
            case DISCONNECT_OP: handleDisconnect(hashtablePointer, socketFd,msg.hdr.sender); break;
            default: break;
        }
        if(msg.data.buf != NULL) free(msg.data.buf);

        FD_SET(socketFd, &set); // add the fd-th client to set
        if(socketFd > fd_hwm)
            fd_hwm = socketFd;
    }
    else{
        checkTerminalDisconnected(socketFd);
    }

    return 0;
}


void createConfig(char *name){
    conf = malloc(sizeof(config));
    if(conf == NULL){
        perror("config allocation");
        exit(EXIT_FAILURE);
    }

    if(makeConfig(conf, name) == -1){
        perror("makeConfig");
        DESTROY_CONF;
        exit(EXIT_FAILURE);
    }
}

void createHashtable(){
    if((hashtablePointer = icl_hash_create(MAX_USERS_LISTS, conf, NULL, NULL)) == NULL){
        perror("createHashtable");
        DESTROY_CONF;
        exit(EXIT_FAILURE);
    }
}

void createThreadpool(){
    if((threadpoolPointer = poolCreate(conf->ThreadsInPool, 3000, threadsStartFunction)) == NULL){
        perror("createThreadpool");
        DESTROY_CONF;
        icl_hash_destroy(hashtablePointer, NULL, NULL);
        exit(EXIT_FAILURE);
    }
}

void cleanup() {
    (void)unlink(socketPathName);
}

int createSocket(){

    mkdir(conf->UnixPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir(conf->DirName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    appendPathFile(socketPathName, conf->UnixPath, SOCKETNAME);

    cleanup();    
    
    if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) return -1;    
    strncpy(sa.sun_path, socketPathName, strlen(socketPathName)+1);
    sa.sun_family = AF_UNIX;

    return 0;
}

int notInQueueFd(int fd){
    pthread_mutex_lock(&threadpoolPointer->mutex);

    int returnValue = 1;

    for(int i=0; i<threadpoolPointer->queueDescriptor.size; i++){
        if(threadpoolPointer->queueDescriptor.queueFd[i] == fd){
            returnValue = 0;
            break;
        }
    }
    
    pthread_mutex_unlock(&threadpoolPointer->mutex);

    return returnValue;
}


int launchServer(){
    // variables used for running the function
    int fd, fd_client, notUsed;
    fd_set read_set;
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 5;

    // creation, bind and listen
    if((notUsed = bind(fd_skt, (struct sockaddr *)&sa, sizeof(sa))) == -1) return -1;
    if((notUsed = listen(fd_skt, conf->MaxConnections)) == -1) return -1;

    // starting the server
    FD_ZERO(&set);

    FD_SET(fd_skt, &set);
    if(fd_skt > fd_hwm) fd_hwm = fd_skt;

    while(1){
        read_set = set;
        pselect(fd_hwm + 1, &read_set, NULL, NULL, &timeout, &sa1.sa_mask);

        for (fd = 0; fd <= fd_hwm; fd++){
            if(FD_ISSET(fd, &read_set)){
                if(fd == fd_skt) {
                    fd_client = accept(fd_skt, NULL, 0);

                    FD_SET(fd_client, &set); // add the fd-th client to set
                    if(fd_client > fd_hwm)
                        fd_hwm = fd_client;
                }
                else {
                    FD_CLR(fd, &set); //remove fd from set
                    if(fd == fd_hwm){
                        for(int i = (fd_hwm-1); i>=0; --i){
                           if (FD_ISSET(i, &set)){
                               fd_hwm = i;
                               break;
                           }
                        }
                    }

                    addFdToQueue(fd); // adding fd to the queue
                }
            }
        }
    }
}

void createSocketLaunchServer(){
    if(createSocket() == -1){
        perror("createSocket");
        DESTROY_ALL;
        DESTROY_MUTEX_STATS;
        unlink(socketPathName);
        exit(EXIT_FAILURE);
    }

    if(launchServer() == -1){
        perror("launchServer");
        DESTROY_ALL;
        DESTROY_MUTEX_STATS;
        DESTROY_SOCKET;
        exit(EXIT_FAILURE);
    }
}

void printStatistics(){
    FILE *fileStats = fopen(conf->StatFileName, "a");
    if (fileStats == NULL) {
        perror("fopen file statistics");
        fclose(fileStats);
        return;
    }

    if(printStats(fileStats) == -1)
        perror("printStats");
    
    fclose(fileStats);
}

void createMutexStats(){
    if (pthread_mutex_init(&chattyStatsMutex, NULL) != 0){
        perror("pthread_mutex_init chattyStatsMutex");
		DESTROY_ALL;
        DESTROY_SOCKET;
        exit(EXIT_FAILURE);
    }
}

void createSignals(){

    createMutexStats();

    signal(SIGPIPE, SIG_IGN);

	memset(&sa1, 0, sizeof(struct sigaction));
	memset(&sa2, 0, sizeof(struct sigaction));
	sa1.sa_handler = printStatistics;
    sa2.sa_handler = destroyEverything;
    sigemptyset(&sa2.sa_mask);
	sigaddset(&sa2.sa_mask, SIGINT);
	sigaddset(&sa2.sa_mask, SIGTERM);
	sigaddset(&sa2.sa_mask, SIGQUIT);

    sigemptyset(&sa1.sa_mask);
	sigaddset(&sa1.sa_mask, SIGUSR1);

	if (
		sigaction(SIGUSR1, &sa1, NULL) != 0 ||
		sigaction(SIGINT, &sa2, NULL) != 0 ||
		sigaction(SIGTERM, &sa2, NULL) != 0 ||
		sigaction(SIGQUIT, &sa2, NULL) != 0
	) {
		perror("sigaction");
		DESTROY_ALL;
        DESTROY_SOCKET;
        exit(EXIT_FAILURE);
	}

}



static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}



int main(int argc, char *argv[]) {

    // launched with ./server -f confFile
    if(argc != 3 || strcmp(argv[1], "-f")){
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    createConfig(argv[2]);

    createHashtable();

    createThreadpool();

    createSignals();

    createSocketLaunchServer();

    return 0;
}
