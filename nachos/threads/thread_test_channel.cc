#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_test_channel.hh"
#include "system.hh"
#include "channel.hh"

#define MC 3
#define NC 3

Channel *channel;

static void prod_c(void *name)
{
    printf("Productor %s creado\n", (char *)name);

	for(int i = 1; i <= 500; i++) {
    channel->Send(i);
  }
}

static void cons_c(void *name)
{
    printf("Consumidor %s creado\n", (char *)name);

	for(int j = 0; j < 500; j++) {
    int message;
    channel->Receive(&message);
  }
}

//Para debug de canales, correr con -d 'c'
void ThreadTestProdConsChannel() {
    channel = new Channel("channel");
    char **pnames = new char*[MC];
    char **cnames = new char*[NC];
    Thread **prods = new Thread*[MC];
    Thread **cons = new Thread*[NC];
    int i;
	for (i = 0; i < MC; i++){
        pnames[i] = new char[10];
        sprintf(pnames[i], "Productor %d", i);
        prods[i] = new Thread(pnames[i], true, i);
        prods[i]->Fork(prod_c, pnames[i]);
    }

    for (i = 0; i < NC; i++){
        cnames[i] = new char[10];
        sprintf(cnames[i], "Consumidor %d", i);
        cons[i] = new Thread(cnames[i], true, i);
		cons[i]->Fork(cons_c, cnames[i]);
    }
    for(i = 0; i < MC; i++){
        prods[i]->Join();
    }
    for(i=0; i < NC; i++){
        cons[i]->Join();
    }

    delete [] prods;
    delete [] cons;

    for (unsigned j = 0; j < MC; j++) {
	    delete[] pnames[j];
    }
    delete [] pnames;
    for (unsigned j = 0; j < NC; j++) {
	    delete[] cnames[j];
    }
    delete []cnames;
    puts("Hilos finalizados");
}
