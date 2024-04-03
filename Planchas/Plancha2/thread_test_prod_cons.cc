#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_test_prod_cons.hh"
#include "system.hh"
#include "lock.hh"
#include "condition.hh"

#define M 1
#define N 1
#define BUFFER_LEN 3

int buffer[BUFFER_LEN];
int pos = 0;

Lock cons_lock("cons_lock"); // Mutex para controlar que los consumidores no reciban si el buffer esta vacío
Condition non_empty_buffer_cond("non_empty_buffer_cond", &cons_lock);  // Condicion de no estar vacío el buffer 

Lock prod_lock("prod_lock"); // Mutex para controlar que los productores no envien si el buffer esta lleno
Condition non_full_buffer_cond("non_full_buffer_cond", &prod_lock); // Condicion de no estar lleno el buffer

Lock pos_lock("pos_lock"); // Mutex para acceder/modificar pos y al buffer

static void prod_f(void *name)
{
    printf("Productor %s creado\n", (char *)name);

	for(int i = 1; i <= 1000; i++) {
        usleep(50);
        prod_lock.Acquire();
        while(pos == BUFFER_LEN) {
            printf("Productor esperando (buffer lleno)\n"); 
            non_full_buffer_cond.Wait(); // Esperamos que se genere lugar en el buffer
        }

        pos_lock.Acquire();
        buffer[pos] = i;
        printf("Productor produce: %d en %d\n", i, pos++);
        pos_lock.Release();
        non_empty_buffer_cond.Signal();

	}
}

static void cons_f(void *name)
{
    printf("Consumidor %s creado\n", (char *)name);

	while (1) {
        usleep(50);
        cons_lock.Acquire();
        while(pos == 0) {
            printf("Consumidor esperando (buffer vacio)\n");
            non_empty_buffer_cond.Wait(); // Esperamos que se ocupe un lugar en el buffer
        }
        
        pos_lock.Acquire();
        int i = buffer[--pos];
        printf("Consumidor consume: %d en %d\n", i, pos);
        pos_lock.Release();
        non_full_buffer_cond.Signal();

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