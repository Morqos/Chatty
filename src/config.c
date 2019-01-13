#include "config.h"
#include <string.h> //for string ops
#include <stdlib.h>  //for free and atoi
#include <stdio.h> //for file, scan and get ops 
#include <assert.h> //for function assert


// Save all the configurations in conf, returns -1 on error
int makeConfig(config *conf, char *file){
    
    FILE *fp = fopen(file, "r");
    assert (fp != NULL);

    char line[128], str1[128], str2[128], str3[128];
    
    while (fgets(line, sizeof line, fp) != NULL){ /* Leggo ogni linea del file di configurazione */
        fscanf(fp, "%s %s %s", str1, str2, str3);
        if(strcmp(str1, "DirName") == 0) strcpy(conf->DirName,str3);
        else if(strcmp(str1, "StatFileName") == 0) strcpy(conf->StatFileName,str3);
        else if(strcmp(str1, "UnixPath") == 0) strcpy(conf->UnixPath,str3);
        else if(strcmp(str1, "MaxConnections") == 0) conf->MaxConnections = atoi(str3);
        else if(strcmp(str1, "MaxMsgSize") == 0) conf->MaxMsgSize = atoi(str3);
        else if(strcmp(str1, "MaxFileSize") == 0) conf->MaxFileSize = atoi(str3);
        else if(strcmp(str1, "MaxHistMsgs") == 0) conf->MaxHistMsgs = atoi(str3);
        else if(strcmp(str1, "ThreadsInPool") == 0) conf->ThreadsInPool = atoi(str3);
    }

    for(int i = 0; i < 8; i++){
        if(&conf[i] == NULL){
            free(conf);
            return -1;
        }
    }
    
    return 1;

}