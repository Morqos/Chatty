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
#include <fcntl.h>
#include <sys/mman.h>

#include "icl_hash.h"
#include "connections.h"
#include "message.h"

int lockMutexUsers(hashtableUsers *ht, void* key)
{
    if(!ht || !key) return -1;

    unsigned int hash_val;
    hash_val = (* ht->hash_function)(key) % ht->maxUsers;
    pthread_mutex_lock(&ht->usersMutexes[hash_val % N_MUTEXES]);

    return 0;
}

int unlockMutexUsers(hashtableUsers *ht, void* key)
{
    if(!ht || !key) return -1;

    unsigned int hash_val;
    hash_val = (* ht->hash_function)(key) % ht->maxUsers;
    pthread_mutex_unlock(&ht->usersMutexes[hash_val % N_MUTEXES]);

    return 0;
}



message_data_t *listUsers(hashtableUsers *htPointer, char *nickUser){
    message_data_t *data = (message_data_t *) malloc(sizeof(message_data_t));
    if(data == NULL) return NULL;
    memset((char*)&(data->hdr), 0, sizeof(message_data_hdr_t));

    char *tmpBuffer = calloc(200*(MAX_NAME_LENGTH+1),sizeof(char));  

    user *curr, *bucket;
    int countOnlineUsers = 0, indexBuf = 0;

    for (int i=0; i<htPointer->maxUsers; i++) {
        bucket = htPointer->users[i];
        for (curr=bucket; curr!=NULL; curr=curr->next) {
            if(curr->online == 1 /* && strcmp(nickUser, curr->nickname) != 0 */){
                countOnlineUsers++;
                
                for(int j=0; j<strlen(curr->nickname); j++)
                    tmpBuffer[indexBuf+j] = curr->nickname[j];

                indexBuf += (MAX_NAME_LENGTH+1);
            }
        }
    }


    data->buf = (char *) calloc(countOnlineUsers*(MAX_NAME_LENGTH+1),sizeof(char));

    for(int i=0; i<countOnlineUsers*(MAX_NAME_LENGTH+1); i++)
        data->buf[i] = tmpBuffer[i];

    data->hdr.len = countOnlineUsers*(MAX_NAME_LENGTH+1);

    free(tmpBuffer);

    return data;
}


op_t responseOperation(user *userPointer, op_t opSuccess,op_t opFail){
    if(userPointer == NULL){
        return opFail;
    } else {
        return opSuccess;
    }
}


void freeData(message_data_t *data){
    if(data->buf != NULL) free(data->buf);
    if(data != NULL) free(data);
}


void createAndSendListUsers(hashtableUsers *htPointer, int fdUser,char *nickUser){
    message_data_t *data = listUsers(htPointer, nickUser);
    if(data ==  NULL){
        perror("createAndSendListUsers");
        freeData(data);
        return;
    }

    strncpy(data->hdr.receiver, nickUser, strlen(nickUser)+1);
    sendData(fdUser, data);
    freeData(data);
}

// lock()/unlock() called inside icl_hash_insert()
void handleRegister(hashtableUsers *htPointer, int fdUser,char *nickUser){
    if(htPointer == NULL){
        perror("NULL pointer");
        return;
    }

    user *newUser = icl_hash_insert(htPointer, nickUser, fdUser);

    op_t opReply = responseOperation(newUser, OP_OK,OP_NICK_ALREADY);

    if(sendReply(fdUser, opReply,nickUser) == -1){
        perror("sendReply");
        return;
    }
    
    if(opReply == OP_OK){
        statsRegister();
        createAndSendListUsers(htPointer, fdUser,nickUser);
    } else {
        statsError();
    }

}


void handleConnect(hashtableUsers *htPointer, int fdUser,char *nickUser){
    if(htPointer == NULL){
        perror("NULL pointer");
        return;
    }

    user *userConnected = icl_hash_find(htPointer, nickUser);
    
    op_t opReply = responseOperation(userConnected, OP_OK,OP_NICK_UNKNOWN);
    if(sendReply(fdUser, opReply,nickUser) == -1){
        perror("sendReply CONNECT");
        return;
    }

    // create usrlist
    if(opReply == OP_OK){
        lockMutexUsers(htPointer, nickUser);
        
        userConnected->fd = fdUser;
        int wasOnline = userConnected->online;
        if(userConnected->online == 0) userConnected->online = 1;   
        
        unlockMutexUsers(htPointer, nickUser);
        
        if(wasOnline == 0) statsConnect();

        createAndSendListUsers(htPointer, fdUser,nickUser);
    } else {
        statsError();
    }

}


void handleUsrlist(hashtableUsers *htPointer, int fdUser,char *nickUser){
    if(htPointer == NULL){
        perror("NULL pointer");
        return;
    }

    op_t opReply = OP_OK;

    if(sendReply(fdUser, opReply,nickUser) == -1){
        perror("sendReply USRLIST");    
        return;
    }

    createAndSendListUsers(htPointer, fdUser,nickUser);
}


// There's no command in the client
void handleDisconnect(hashtableUsers *htPointer, int fdUser,char *nickUser){
    if(htPointer == NULL){
        perror("NULL pointer");
        return;
    }
    user *userDisconnected = icl_hash_find(htPointer, nickUser);

    op_t opReply = responseOperation(userDisconnected, OP_OK,OP_FAIL);

    if(sendReply(fdUser, opReply,nickUser) == -1){
        perror("sendReply DISCONNECT");
        return;
    }

    if(opReply == OP_OK){
        lockMutexUsers(htPointer, nickUser);
        userDisconnected->online = 0;
        userDisconnected->fd = -1;
        unlockMutexUsers(htPointer, nickUser);

        statsDisconnect();
    } else {
        statsError();
    }

}


// lock()/unlock() called inside icl_hash_delete()
void handleUnregister(hashtableUsers *htPointer, int fdUser,char *nickUser){
    if(htPointer == NULL){
        perror("NULL pointer");
        return;
    }

    int userUnregistered = icl_hash_delete(htPointer, nickUser, NULL,NULL);

    op_t opReply;

    if(userUnregistered == -1){
        opReply = OP_NICK_UNKNOWN;
        statsError();
    } else {
        statsUnregister(userUnregistered);
        opReply = OP_OK;
    }

    if(sendReply(fdUser, opReply,nickUser) == -1) perror("sendReply UNREGISTER");

}


int rightTxtMessage(char *txtMessage, int maxMsgSize){
    if(strlen(txtMessage) > maxMsgSize) return -1;
    else return 1;
}

void saveMessageInArray(message_t *whereToSave, message_t *whatToSave){

	setHeader(&whereToSave->hdr,whatToSave->hdr.op,whatToSave->hdr.sender);
    
    if(whereToSave->data.buf != NULL) free(whereToSave->data.buf);
    setData(&whereToSave->data,whatToSave->data.hdr.receiver,NULL,whatToSave->data.hdr.len);
    
    whereToSave->data.buf = malloc(whatToSave->data.hdr.len*sizeof(char));
    strcpy(whereToSave->data.buf, whatToSave->data.buf);
}


void handleOfflineReceiver(user *receiverMessage,hashtableUsers *htPointer, message_t *msg){
    if(htPointer == NULL || receiverMessage == NULL){
        perror("NULL pointers");
        return;
    }

    saveMessageInArray(receiverMessage->histMessages[receiverMessage->indexMessages], msg);

    receiverMessage->indexMessages += 1;
    receiverMessage->indexMessages = receiverMessage->indexMessages % (htPointer->maxHistMessages);

    if(receiverMessage->nRemoteMessages < htPointer->maxHistMessages)
        receiverMessage->nRemoteMessages++;
}


void handlePostTxt(hashtableUsers *htPointer, int fdUser,message_t *msg){
    if(htPointer == NULL || msg == NULL){
        perror("NULL pointers");
        return;
    }

    // check if msg.data.buf not too long
    if(rightTxtMessage(msg->data.buf, htPointer->maxMsgSize) == -1){
        sendReply(fdUser, OP_MSG_TOOLONG,msg->hdr.sender);
        statsError();
        return;
    }

    // check if msg.data.hdr.receiver exists
    user *receiverMessage= icl_hash_find(htPointer, msg->data.hdr.receiver);
    if(receiverMessage == NULL || strcmp(msg->data.hdr.receiver,msg->hdr.sender) == 0){
        sendReply(fdUser, OP_NICK_UNKNOWN,msg->hdr.sender);
        statsError();
        return;
    }

    lockMutexUsers(htPointer, msg->data.hdr.receiver);
    
    msg->hdr.op = TXT_MESSAGE;
    if(receiverMessage->online == 1){
        if(sendRequest(receiverMessage->fd, msg) == -1){
            perror("sendRequest POSTTXT");
            unlockMutexUsers(htPointer, msg->data.hdr.receiver);
            return;
        }
    }

    handleOfflineReceiver(receiverMessage,htPointer, msg);
    unlockMutexUsers(htPointer, msg->data.hdr.receiver);

    statsPostTxt(receiverMessage->online);

    // sending the ackok to the sender
    if(sendReply(fdUser, OP_OK,msg->hdr.sender) == -1){
        perror("sendReply POSTTXT OP_OK");
        return;
    }
}


void handlePostTxtAll(hashtableUsers *htPointer, int fdUser,message_t *msg){
    if(htPointer == NULL || msg == NULL){
        perror("NULL pointer");
        return;
    }
    
    // check if msg.data.buf not too long
    if(rightTxtMessage(msg->data.buf, htPointer->maxMsgSize) == -1){
        statsError();
        sendReply(fdUser, OP_MSG_TOOLONG,msg->hdr.sender);
        return;
    }

    msg->hdr.op = TXT_MESSAGE;
    user *curr, *bucket;
    for (int i=0; i<htPointer->maxUsers; i++) {
        bucket = htPointer->users[i];
        for (curr=bucket; curr!=NULL; curr=curr->next) {
            if(strcmp(curr->nickname,msg->hdr.sender) != 0){
                lockMutexUsers(htPointer, curr->nickname);
                if(curr->online == 1){
                    if(sendRequest(curr->fd, msg) == -1){
                        perror("sendRequest POSTTXTALL");
                        unlockMutexUsers(htPointer, curr->nickname);
                        sendReply(fdUser, OP_FAIL,msg->hdr.sender);
                        return;
                    }
                }
                
                
                handleOfflineReceiver(curr,htPointer, msg);
                unlockMutexUsers(htPointer, curr->nickname);
                
                statsPostTxt(curr->online);
            }
        }
    }

    // sending the ackok to the sender
    if(sendReply(fdUser, OP_OK,msg->hdr.sender) == -1){
        perror("sendReply PostTxtAll OP_OK");
        return;
    }

}


void handleGetPrevMsgs(hashtableUsers *htPointer, int fdUser,char *nickUser){
    if(htPointer == NULL) return;

    user *receiverMessage= icl_hash_find(htPointer, nickUser);
    if(receiverMessage == NULL){
        statsError();
        if(sendReply(fdUser, OP_NICK_UNKNOWN,nickUser) == -1)
            perror("sendReply getPrevMsgs OP_NICK_UNKWOWN");
        return;
    }

    if(sendReply(fdUser, OP_OK,nickUser) == -1){
        perror("sendReply getPrevMsgs OP_OK");
        return;
    }

    message_data_t data;
    int nRemoteMessages = 0;
    lockMutexUsers(htPointer, nickUser);
    for(int i = 0; i < htPointer->maxHistMessages;i++) {
        if(receiverMessage->histMessages[i]->data.buf != NULL)
            nRemoteMessages++;
        else
            break;
    }
    
    setData(&data,nickUser,(char*)&nRemoteMessages,sizeof(int));
    
    if(sendData(fdUser, &data) == -1){
        perror("sending data getPrevMsgs");
        unlockMutexUsers(htPointer, nickUser);
        return;
    }

    int howManyMsgs = 0;
    for(int i=0; i<nRemoteMessages; i++) {
        if(sendRequest(receiverMessage->fd,receiverMessage->histMessages[i]) ==  -1){
            perror("sendRequest getPrevMsgs");
            break;
        }
        if(receiverMessage->histMessages[i]->hdr.op == TXT_MESSAGE) howManyMsgs++;
    }
    unlockMutexUsers(htPointer, nickUser);

    for(int i=0; i<howManyMsgs; i++){
        statsGetMsg();
    }
}


int copyFile(char *dirName, char *fileName){
    if(fileName == NULL || dirName == NULL) return -1;

    // check if file exists
    int fdFile = open(fileName, O_RDONLY);
    if (fdFile<0) {
        perror("opening file");
        return -1;
    }
    
    // create path to copy file
    char destinationPath[128];
    appendPathFile(destinationPath, dirName,fileName);

    unsigned char buffer[4096];
    int nRead, nWrite;
    
    int fdDestination = open(destinationPath, O_CREAT | O_WRONLY, 0777);
    if (fdDestination<0) {
        perror("opening destination file");
        close(fdFile);
        return -1;
    }

    // copy the content of the file
    while(1){

        if ((nRead = read(fdFile, buffer, 4096)) == -1) {
            perror("reading file");
            close(fdDestination);
            close(fdFile);
            return -1;
        }
        if(nRead == 0) break;

        if((nWrite = write(fdDestination, buffer, nRead)) == -1) {
            perror("writing file");
            close(fdDestination);
            close(fdFile);
            return -1;
        }
    }

    close(fdDestination);
    close(fdFile);

    return 0;
}

void handlePostFile(hashtableUsers *htPointer, char *dirName, int fdUser,message_t *msg){
    if(htPointer == NULL || msg == NULL){
        perror("null pointers");
        return;
    }

    // check if msg.data.hdr.receiver exists
    user *receiverMessage= icl_hash_find(htPointer, msg->data.hdr.receiver);
    if(receiverMessage == NULL || strcmp(msg->data.hdr.receiver,msg->hdr.sender) == 0){
        perror("receiverMessage Unknown");
        sendReply(fdUser, OP_NICK_UNKNOWN,msg->hdr.sender);
        statsError();
        return;
    }

    // data is used to check the dimension of the file
        // data BEGIN
    message_data_t *data = (message_data_t *) malloc(sizeof(message_data_t));
    if(data == NULL){
        perror("message_data_t allocation");
        sendReply(fdUser, OP_FAIL,msg->hdr.sender);
        return;
    }

    int fileDimension;
    if((fileDimension = readData(fdUser, data)) == -1){
        freeData(data);
        sendReply(fdUser, OP_FAIL,msg->hdr.sender);
        return;
    }

    if(data->hdr.len == 0){
        freeData(data);
        sendReply(fdUser, OP_FAIL,msg->hdr.sender);
        statsError();
        return;
    }

    // byte(file) > 1000 * max_nKB_fileSize
    if(data->hdr.len > htPointer->maxFileFize*1024){
        freeData(data);
        sendReply(fdUser, OP_MSG_TOOLONG,msg->hdr.sender);
        statsError();
        return;
    }
        // data END

    freeData(data);

    if(copyFile(dirName ,msg->data.buf) == -1){
        sendReply(fdUser, OP_FAIL,msg->hdr.sender);
        statsError();
        return;
    }
    
    int updateStats = 0;
    lockMutexUsers(htPointer, msg->data.hdr.receiver);
    msg->hdr.op = FILE_MESSAGE;
    if(receiverMessage->online == 1){
        if(sendRequest(receiverMessage->fd, msg) == -1){
            unlockMutexUsers(htPointer, msg->data.hdr.receiver);
            return;
        }
    }
    else updateStats = 1;
    
    handleOfflineReceiver(receiverMessage,htPointer, msg);
    unlockMutexUsers(htPointer, msg->data.hdr.receiver);

    if(updateStats == 1) statsPostFile();

    // sending the ackok to the sender
    if(sendReply(fdUser, OP_OK,msg->hdr.sender) == -1){
        perror("sendReply PostFile OP_OK");
    }

}


void handleGetFile(hashtableUsers *htPointer, char *dirName, int fdUser,message_t *msg){
    if(htPointer == NULL || msg == NULL){
        perror("null pointers");
        return;
    }

    // check if msg.data.hdr.receiver exists
    user *receiverMessage= icl_hash_find(htPointer, msg->hdr.sender);
    if(receiverMessage == NULL){
        perror("receiverMessage Unknown");
        sendReply(fdUser, OP_NICK_UNKNOWN,msg->hdr.sender);
        statsError();
        return;
    }

    message_t sendMsg;
    int fileDim;
    FILE *file;

    // Requested file doesn't exist
    if((file = fopen(msg->data.buf,"r")) == NULL) {
        perror("fopen GETFILE_OP");
        sendReply(fdUser, OP_NO_SUCH_FILE,msg->hdr.sender);
        statsError();
        return;
    }
    else {
        fseek(file, 0, SEEK_END); // Points at the end of the file
        fileDim = ftell(file);// Reads actual position
        char* buffer;
        
        // Allocates la buffer's dimension
        if(!(buffer = (char*) malloc(sizeof(char) * fileDim+1))) {
            perror("malloc");
            exit(errno);
        }
        fseek(file, 0, SEEK_SET);// Points at the beginning of the file
        fread(buffer, fileDim, 1, file);// Copy file's content in the buffer
        fclose(file);// Close the file
        setHeader(&sendMsg.hdr,OP_OK,msg->hdr.sender);
        setData(&sendMsg.data,msg->data.hdr.receiver,buffer,fileDim);
        sendRequest(fdUser,&sendMsg);

        statsGetFile();
        free(buffer);
    }
}