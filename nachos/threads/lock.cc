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


/* Inversion de prioridades:

    Para el caso del problema de inversión de prioridades implementamos herencia de prioridades.
    Cuando un proceso P1 quiere tomar un lock ya tomado por otro P2, se evalúa cual tiene mayor
    prioridad, si la de P1 es mayor, se la "hereda" a P2, sino, se mantienen los valores actuales.
    Si ocurrió el primer caso, cuando el proceso P2 suelta el lock, su prioridad vuelve a su estado
    original.

    Inversión de prioridades en semáforos:

    No es posible solucionar el problema de inversión de prioridades en semáforos ya que no sabemos
    cual de todos los procesos es el encargado de levantar el contador del semáforo con V() y por
    ende no sabemos a cual aumentarle la prioridad para acelerar su ejecución. Esto no ocurre con
    locks ni con variables de condidicion ya que en estos últimos solo hay un proceso capaz de
    liberar el bloqueo, que es al cual le modificamos la prioridad.

*/

#include <stdio.h>
#include "lock.hh"
#include "lib/utility.hh"

class Thread;
#include "thread.hh"
#include "system.hh"
/// Dummy functions -- so we can compile our later assignments.

Lock::Lock(const char *debugName)
{
    name = debugName;
    owner = nullptr;
    sem = new Semaphore("sem_lock",1);
}

Lock::~Lock()
{
    delete sem;
}

const char *
Lock::GetName() const
{
    return name;
}

void
Lock::Acquire()
{
    ASSERT(!IsHeldByCurrentThread());

    if(owner && owner->GetPriority() < currentThread->GetPriority()){
        IntStatus oldLevel = interrupt->SetLevel(INT_OFF);
        scheduler->UpdateReadyMultiQueue(owner, currentThread->GetPriority());
        DEBUG('s', "Thread \"%s\" inherited priority of Thread\"%s\" \n", owner->GetName(), currentThread->GetName());
        interrupt->SetLevel(oldLevel);
    } 



    sem->P();
    DEBUG('s', "Thread \"%s\" acquired lock \"%s\" \n", currentThread->GetName(), name);
    owner = currentThread;
}

void
Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());

    if(owner->GetOriginalPriority() != owner->GetPriority()){
        IntStatus oldLevel = interrupt->SetLevel(INT_OFF);
        DEBUG('s', "Thread \"%s\" retaking its original priority \n", owner->GetName());
        scheduler->UpdateReadyMultiQueue(owner, owner->GetOriginalPriority());
        interrupt->SetLevel(oldLevel);
    }

    owner = nullptr;
    sem->V();
    DEBUG('s', "Thread \"%s\" released lock \"%s\" \n", currentThread->GetName(), name);
    }

bool
Lock::IsHeldByCurrentThread() const
{
    return currentThread == owner;
}
