/// Data structures for the thread dispatcher and scheduler.
///
/// Primarily, the list of threads that are ready to run.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_THREADS_SCHEDULER__HH
#define NACHOS_THREADS_SCHEDULER__HH


#include "thread.hh"
#include "lib/list.hh"


/// The following class defines the scheduler/dispatcher abstraction --
/// the data structures and operations needed to keep track of which
/// thread is running, and which threads are ready but not running.
class Scheduler {
public:

    /// Initialize multiqueue of ready threads.
    Scheduler(int priorities = 10);

    /// De-allocate ready multiqueue.
    ~Scheduler();

    /// Thread can be dispatched.
    void ReadyToRun(Thread *thread);

    /// Dequeue first thread on the ready queue in the multiqueue, if any, and return thread.
    Thread *FindNextToRun();

    /// Cause `nextThread` to start running.
    void Run(Thread *nextThread);

    // Print contents of ready multiqueue.
    void Print();

    // Update the 'thread' priority and the multiqueue.
    void UpdateReadyMultiQueue(Thread *thread, int newPriority);

private:

    // MultiQueue of threads that are ready to run, but not running.
    List<Thread*>** readyMultiQueue;
    int numberOfPriorities;
};


#endif
