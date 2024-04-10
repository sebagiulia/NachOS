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
bool done[2]; // Banderas para indicar el final de los hilos(solo valido para un productor y un consumidor)

int buffer[BUFFER_LEN]; 
int pos = 0;

Lock pos_lock("pos_lock"); // Mutex para acceder/modificar pos y al buffer

Condition non_empty_buffer_cond("non_empty_buffer_cond", &pos_lock);  // Condicion de no estar vacío el buffer 
Condition non_full_buffer_cond("non_full_buffer_cond", &pos_lock); // Condicion de no estar lleno el buffer


static void prod_f(void *name)
{
    printf("Productor %s creado\n", (char *)name);

	for(int i = 1; i <= 1000; i++) {
        usleep(50);
        pos_lock.Acquire();
        while(pos == BUFFER_LEN) {
            printf("Productor esperando (buffer lleno)\n"); 
            non_full_buffer_cond.Wait(); // Esperamos que se genere lugar en el buffer
        }
        buffer[pos] = i;
        printf("Productor produce: %d en %d\n", i, pos++);
        non_empty_buffer_cond.Signal();
        pos_lock.Release();
    }
    done[0] = true;
}

static void cons_f(void *name)
{
    printf("Consumidor %s creado\n", (char *)name);

	for(int j = 0; j < 1000; j++) {
        usleep(50);
        pos_lock.Acquire();
        while(pos == 0) {
            printf("Consumidor esperando (buffer vacio)\n");
            non_empty_buffer_cond.Wait(); // Esperamos que se ocupe un lugar en el buffer
        }
        int i = buffer[--pos];
        printf("Consumidor consume: %d en %d\n", i, pos);
        non_full_buffer_cond.Signal();
        pos_lock.Release();
    }
    done[1] = true;
}

void ThreadTestProdCons() {
    char **pnames = new char*[M];
    char **cnames = new char*[N];
    done[0] = done[1] = false;

    int i;
	for (i = 0; i < M; i++){
        pnames[i] = new char[10];
        sprintf(pnames[i], "Productor %d", i);
        Thread *p = new Thread(pnames[i]);
		p->Fork(prod_f, pnames[i]);
    }

    for (i = 0; i < N; i++){
        cnames[i] = new char[10];
        sprintf(cnames[i], "Consumidor %d", i);
        Thread *c = new Thread(cnames[i]);
		c->Fork(cons_f, cnames[i]);
    }

    while(!(done[0] && done[1])) {
        currentThread->Yield();
    };

    for (unsigned j = 0; j < M; j++) {
	    delete[] pnames[j];
    }
    delete []pnames;
    
    for (unsigned j = 0; j < N; j++) {
	    delete[] cnames[j];
    }
    delete []cnames;
    puts("Hilos finalizados");
}