/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_simple.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>

#ifdef SEMAPHORE_TEST
#include "semaphore.hh"
Semaphore sem("sem", 3);
#endif


/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.

bool threads[4] = {false, false, false, false};
void
SimpleThread(void *name_)
{
    for (unsigned num = 0; num < 10; num++) {
	#ifdef SEMAPHORE_TEST
    	sem.P();
        DEBUG('s', "Thread `%s` called P()\n", currentThread->GetName());
	#endif

        printf("*** Thread `%s` is running: iteration %u\n", currentThread->GetName(), num);

    	#ifdef SEMAPHORE_TEST	
    	sem.V();
        DEBUG('s', "Thread `%s` called V()\n", currentThread->GetName());
    	#endif
        
	currentThread->Yield();
    }

    if(!strcmp(currentThread->GetName(), "2nd")) threads[0] = true;
    else if(!strcmp(currentThread->GetName(), "3rd")) threads[1] = true;
    else if(!strcmp(currentThread->GetName(), "4th")) threads[2] = true;
    else if(!strcmp(currentThread->GetName(), "5th")) threads[3] = true;
    printf("!!! Thread `%s` has finished SimpleThread\n", currentThread->GetName());
}

void
ThreadTestSimple()
{
    Thread *newThread2 = new Thread("2nd", true, 6);
    Thread *newThread3 = new Thread("3rd", true, 7);
    Thread *newThread4 = new Thread("4th", true, 8);
    Thread *newThread5 = new Thread("5th", true, 9);
    newThread2->Fork(SimpleThread, NULL);
    newThread3->Fork(SimpleThread, NULL);
    newThread4->Fork(SimpleThread, NULL);
    newThread5->Fork(SimpleThread, NULL);

    //the "main" thread also executes the same function
    SimpleThread(NULL);

   //Wait for the threads to finish if needed
    newThread2->Join();
    newThread3->Join();
    newThread4->Join();
    newThread5->Join();
    
    printf("Test finished\n");
}
