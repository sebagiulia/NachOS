/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "condition.hh"
#include "thread.hh"
#include "system.hh"
#include <stdio.h>
#include <string.h>
/// Dummy functions -- so we can compile our later assignments.
///
Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name = debugName;
    l = conditionLock;
    nombre_sem = new char[8 + strlen(debugName)];
    sprintf(nombre_sem, "sem of %s", debugName);
    s = new Semaphore(nombre_sem, 0);
    count = 0;
}

Condition::~Condition()
{
    delete s;
    delete nombre_sem;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    ASSERT(l->IsHeldByCurrentThread());
    DEBUG('s', "Thread \"%s\" calling Wait on condition \"%s\" \n", currentThread->GetName(), name);
    count++;
    l->Release();
    s->P();
    l->Acquire();
}

void
Condition::Signal()
{
    ASSERT(l->IsHeldByCurrentThread());
    DEBUG('s', "Thread \"%s\" calling Signal on condition \"%s\" \n", currentThread->GetName(), name);
    if(count != 0) {
        s->V();
        count--;
    }
}

void
Condition::Broadcast()
{
    ASSERT(l->IsHeldByCurrentThread());
    DEBUG('s', "Thread \"%s\" calling Broadcast on condition \"%s\" \n", currentThread->GetName(), name);
    for(; count > 0; count--)
        s->V();      
}
