#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_test_channel.hh"
#include "system.hh"
#include "channel.hh"

#define MCHANNEL 4
#define NCHANNEL 4
//#define BUFFER_LEN 3
bool doneChannel[8]; // Banderas para indicar el final de los hilos(solo valido para un productor y un consumidor)

Channel *channel;

static void prod_c(void *name)
{
    printf("Productor %s creado\n", (char *)name);

	for(int i = 1; i <= 5000; i++) {
    channel->Send(i);
  }
}

static void cons_c(void *name)
{
    printf("Consumidor %s creado\n", (char *)name);

	for(int j = 0; j < 5000; j++) {
    int message;
    channel->Receive(&message);
  }
    //done[1] = true;
}

void ThreadTestProdConsChannel() {
    channel = new Channel("channel");
    char **pnames = new char*[MCHANNEL];
    char **cnames = new char*[NCHANNEL];
    doneChannel[0] = doneChannel[1] = false;

    int i;
	for (i = 0; i < MCHANNEL; i++){
        pnames[i] = new char[10];
        sprintf(pnames[i], "Productor %d", i);
        Thread *p = new Thread(pnames[i]);
		p->Fork(prod_c, pnames[i]);
    }

    for (i = 0; i < NCHANNEL; i++){
        cnames[i] = new char[10];
        sprintf(cnames[i], "Consumidor %d", i);
        Thread *c = new Thread(cnames[i]);
		c->Fork(cons_c, cnames[i]);
    }

    while(!(doneChannel[0] && doneChannel[1])) {
        currentThread->Yield();
    };

    for (unsigned j = 0; j < MCHANNEL; j++) {
	    delete[] pnames[j];
    }
    delete []pnames;

    for (unsigned j = 0; j < NCHANNEL; j++) {
	    delete[] cnames[j];
    }
    delete []cnames;
    puts("Hilos finalizados");
}
