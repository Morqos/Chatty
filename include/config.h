/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file config.h
 * @brief File contenente alcune define con valori massimi utilizzabili
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define SOCKETNAME "chatterbox_sock_538908"
#define MAX_NAME_LENGTH 32 // max nickname characters

#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX 80 // max path characters
#endif

/* aggiungere altre define qui */

typedef struct{
    int MaxConnections;
    int MaxMsgSize;
    int MaxFileSize;
    int MaxHistMsgs;
    int ThreadsInPool;
    char DirName[128];
    char StatFileName[128];
    char UnixPath[128];
}config;

/* funzione che cerca ogni parametro dal file di configurazione e lo inserisce in conf */
int makeConfig(config *conf, char *file);

// to avoid warnings like "ISO C forbids an empty translation unit"
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
