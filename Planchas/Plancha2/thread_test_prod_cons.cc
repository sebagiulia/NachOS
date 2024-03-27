#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_test_prod_cons.hh"
#include "system.hh"
#include "synch_list.hh"

#define M 5
#define N 5

SynchList<int *> buffer;

static void prod_f(void *name)
{
    printf("Productor %s creado\n", (char *)name);

	while (1) {
        //usleep(50);
		sleep(random() % 3);

		int *p = new int;
		*p = random() % 100;
		printf("Productor %s: produje %p->%d\n", (char *)name, p, *p);
        buffer.Append(p);
        currentThread->Yield();
	}
}

static void cons_f(void *name)
{
    printf("Consumidor %s creado\n", (char *)name);

	while (1) {
        //usleep(50);
		sleep(random() % 3);

		int *p = buffer.Pop();
		printf("Consumidor %s: obtuve %p->%d\n", (char *)name, p, *p);
		delete p;
        currentThread->Yield();

	}
}

void ThreadTestProdCons() {
    char **pnames = new char*[M];
    char **cnames = new char*[N];

    int i;
	for (i = 0; i < M; i++){
        pnames[i] = new char[10];
        sprintf(pnames[i], "%d", i);
        Thread *p = new Thread(pnames[i]);
		p->Fork(prod_f, pnames[i]);
    }

    for (i = 0; i < N; i++){
        cnames[i] = new char[10];
        sprintf(cnames[i], "%d", i);
        Thread *c = new Thread(cnames[i]);
		c->Fork(cons_f, cnames[i]);
    }

    while(1) {
        currentThread->Yield();
    };
}