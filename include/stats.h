/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
#if !defined(MEMBOX_STATS_)
#define MEMBOX_STATS_

#include <stdio.h>
#include <time.h>
#include <pthread.h>

struct statistics {
    unsigned long nusers;                       // n. di utenti registrati
    unsigned long nonline;                      // n. di utenti connessi
    unsigned long ndelivered;                   // n. di messaggi testuali consegnati
    unsigned long nnotdelivered;                // n. di messaggi testuali non ancora consegnati
    unsigned long nfiledelivered;               // n. di file consegnati
    unsigned long nfilenotdelivered;            // n. di file non ancora consegnati
    unsigned long nerrors;                      // n. di messaggi di errore
};

/* aggiungere qui altre funzioni di utilita' per le statistiche */


static void modifyStats(int arrayValues[]){
    extern struct statistics chattyStats;
    extern pthread_mutex_t chattyStatsMutex;

    pthread_mutex_lock(&chattyStatsMutex);
    
    chattyStats.nusers += arrayValues[0]; 
    chattyStats.nonline += arrayValues[1];
    chattyStats.ndelivered += arrayValues[2];
    chattyStats.nnotdelivered += arrayValues[3];
    chattyStats.nfiledelivered += arrayValues[4];
    chattyStats.nfilenotdelivered += arrayValues[5];
    chattyStats.nerrors += arrayValues[6];

    pthread_mutex_unlock(&chattyStatsMutex);
}


void statsGetFile(){
    int arrayValues[7];

    arrayValues[0] = 0; // nusers
    arrayValues[1] = 0; // nonline
    arrayValues[2] = 0; // ndelivered
    arrayValues[3] = 0; // nnotdelivered
    arrayValues[4] = 1; // nfiledelivered
    arrayValues[5] = -1; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


// saved the file but not delivered yet 
void statsPostFile(){
    int arrayValues[7];

    arrayValues[0] = 0; // nusers
    arrayValues[1] = 0; // nonline
    arrayValues[2] = 0; // ndelivered
    arrayValues[3] = 0; // nnotdelivered
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 1; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


void statsGetMsg(){
    int arrayValues[7];

    arrayValues[0] = 0; // nusers
    arrayValues[1] = 0; // nonline
    arrayValues[2] = 1; // ndelivered
    arrayValues[3] = -1; // nnotdelivered
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 0; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


void statsPostTxt(int isOnline){
    int arrayValues[7];

    arrayValues[0] = 0; // nusers
    arrayValues[1] = 0; // nonline
    if(isOnline){
        arrayValues[2] = 1; // ndelivered
        arrayValues[3] = 0; // nnotdelivered
    }
    else{
        arrayValues[2] = 0; // ndelivered
        arrayValues[3] = 1; // nnotdelivered
    }
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 0; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


void statsUnregister(int wasOnline){
    int arrayValues[7];

    arrayValues[0] = -1; // nusers
    if(wasOnline) arrayValues[1] = -1; // nonline
    else arrayValues[1] = 0; // nonline
    arrayValues[2] = 0; // ndelivered
    arrayValues[3] = 0; // nnotdelivered
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 0; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


void statsDisconnect(){
    int arrayValues[7];

    arrayValues[0] = 0; // nusers
    arrayValues[1] = -1; // nonline
    arrayValues[2] = 0; // ndelivered
    arrayValues[3] = 0; // nnotdelivered
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 0; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


void statsError(){
    int arrayValues[7];

    arrayValues[0] = 0; // nusers
    arrayValues[1] = 0; // nonline
    arrayValues[2] = 0; // ndelivered
    arrayValues[3] = 0; // nnotdelivered
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 0; // nfilenotdelivered
    arrayValues[6] = 1; // nerrors

    modifyStats(arrayValues);
}


void statsConnect(){
    int arrayValues[7];

    arrayValues[0] = 0; // nusers
    arrayValues[1] = 1; // nonline
    arrayValues[2] = 0; // ndelivered
    arrayValues[3] = 0; // nnotdelivered
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 0; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


void statsRegister(){
    int arrayValues[7];

    arrayValues[0] = 1; // nusers
    arrayValues[1] = 1; // nonline
    arrayValues[2] = 0; // ndelivered
    arrayValues[3] = 0; // nnotdelivered
    arrayValues[4] = 0; // nfiledelivered
    arrayValues[5] = 0; // nfilenotdelivered
    arrayValues[6] = 0; // nerrors

    modifyStats(arrayValues);
}


/**
 * @function printStats
 * @brief Stampa le statistiche nel file passato come argomento
 *
 * @param fout descrittore del file aperto in append.
 *
 * @return 0 in caso di successo, -1 in caso di fallimento 
 */
static inline int printStats(FILE *fout) {
    extern struct statistics chattyStats;

    if (fprintf(fout, "%ld - %ld %ld %ld %ld %ld %ld %ld\n",
		(unsigned long)time(NULL),
		chattyStats.nusers, 
		chattyStats.nonline,
		chattyStats.ndelivered,
		chattyStats.nnotdelivered,
		chattyStats.nfiledelivered,
		chattyStats.nfilenotdelivered,
		chattyStats.nerrors
		) < 0) return -1;
    fflush(fout);
    return 0;
}

#endif /* MEMBOX_STATS_ */
