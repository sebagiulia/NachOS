/// Nachos initialization and cleanup routines.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "system.hh"
#ifdef USER_PROGRAM
#include "userprog/debugger.hh"
#include "userprog/exception.hh"
#include "lib/coremap.hh"
#endif

#include <stdlib.h>
#include <string.h>


/// This defines *all* of the global data structures used by Nachos.
///
/// These are all initialized and de-allocated by this file.

Thread *currentThread;        ///< The thread we are running now.
Thread *threadToBeDestroyed;  ///< The thread that just finished.
Scheduler *scheduler;         ///< The ready list.
Interrupt *interrupt;         ///< Interrupt status.
Statistics *stats;            ///< Performance metrics.
Timer *timer;                 ///< The hardware timer device, for invoking
                              ///< context switches.


#ifdef FILESYS_NEEDED
FileSystem *fileSystem;
#endif

#ifdef FILESYS
SynchDisk *synchDisk;
Lock **locksSector;
Lock *lockFS;
SynchList<FileHeader *> *openFileList;
#endif

#ifdef USER_PROGRAM  // Requires either *FILESYS* or *FILESYS_STUB*.
SynchConsole *synchConsole;
Machine *machine;  ///< User program memory and registers.
Coremap *memoryPages;
Table<Thread *> *processesTable;
#endif



// External definition, to allow us to take a pointer to this function.
extern void Cleanup();

/// Interrupt handler for the timer device.
///
/// The timer device is set up to interrupt the CPU periodically (once every
/// `TimerTicks`).  This routine is called each time there is a timer
/// interrupt, with interrupts disabled.
///
/// Note that instead of calling `Yield` directly (which would suspend the
/// interrupt handler, not the interrupted thread which is what we wanted to
/// context switch), we set a flag so that once the interrupt handler is
/// done, it will appear as if the interrupted thread called Yield at the
/// point it is was interrupted.
///
/// * `dummy` is because every interrupt handler takes one argument, whether
///   it needs it or not.
static void
TimerInterruptHandler(void *dummy)
{
    if (interrupt->GetStatus() != IDLE_MODE) {
        interrupt->YieldOnReturn();
    }
}

static bool
ParseDebugOpts(char *s, DebugOpts *out)
{
    ASSERT(s != nullptr);
    ASSERT(out != nullptr);

    char *save_p;
    for (;; s = nullptr) {
        char *token = strtok_r(s, ",", &save_p);
        if (token == nullptr) {
            break;
        }

        if (strcmp(token, "location") == 0
              || strcmp(token, "l") == 0) {
            out->location = true;
        } else if (strcmp(token, "function") == 0
                     || strcmp(token, "f") == 0) {
            out->function = true;
        } else if (strcmp(token, "sleep") == 0
                     || strcmp(token, "s") == 0) {
            out->sleep = true;
        } else if (strcmp(token, "interactive") == 0
                     || strcmp(token, "i") == 0) {
            out->interactive = true;
        } else {
            return false;  // Invalid option.
        }
    }

    return true;
}

/// Initialize Nachos global data structures.
///
/// Interpret command line arguments in order to determine flags for the
/// initialization.
///
/// * `argc` is the number of command line arguments (including the name
///   of the command).  Example:
///       nachos -d +  ->  argc = 3
///
/// * `argv` is an array of strings, one for each command line argument.
///   Example:
///       nachos -d +  ->  argv = {"nachos", "-d", "+"}
void
Initialize(int argc, char **argv)
{
    ASSERT(argc == 0 || argv != nullptr);

    int argCount;
    const char *debugFlags = "";
    DebugOpts debugOpts;
    bool randomYield = false;

#ifdef USER_PROGRAM
    bool debugUserProg = false;  // Single step user program.
    int numPhysicalPages = DEFAULT_NUM_PHYS_PAGES;
#endif
#ifdef FILESYS_NEEDED
    bool format = false;  // Format disk.
#endif
#ifdef FILESYS
    char *cd = nullptr;
#endif

    for (argc--, argv++; argc > 0; argc -= argCount, argv += argCount) {
        argCount = 1;
        if (!strcmp(*argv, "-d")) {
            if (argc == 1) {
                debugFlags = "+";  // Turn on all debug flags.
            } else {
                debugFlags = *(argv + 1);
                argCount = 2;
            }
        } else if (!strcmp(*argv, "-do")) {
            ASSERT(argc > 1);
            char *s = *(argv + 1);
            ASSERT(ParseDebugOpts(s, &debugOpts));
            argCount = 2;
        } else if (!strcmp(*argv, "-rs")) {
            ASSERT(argc > 1);
            SystemDep::RandomInit(atoi(*(argv + 1)));
              // Initialize pseudo-random number generator.
            randomYield = true;
            argCount = 2;
        }
#ifdef USER_PROGRAM
        if (!strcmp(*argv, "-s")) {
            debugUserProg = true;
        }
        if (!strcmp(*argv, "-m")) {
            ASSERT(argc > 1);
            numPhysicalPages = atoi(*(argv + 1));
            argCount = 2;
        }
#endif
#ifdef FILESYS_NEEDED
        if (!strcmp(*argv, "-f")) {
            format = true;
        }
#endif
#ifdef FILESYS
        if (!strcmp(*argv, "-cd")) {
            ASSERT(argc > 1);
            cd = *(argv + 1);
            argCount = 2;
        }
#endif
    }
    debug.SetFlags(debugFlags);  // Initialize `DEBUG` messages.
    debug.SetOpts(debugOpts);    // Set debugging behavior.
    stats = new Statistics;      // Collect statistics.
    interrupt = new Interrupt;   // Start up interrupt handling.
    scheduler = new Scheduler;   // Initialize the ready queue.
    if (randomYield) {           // Start the timer (if needed).
        timer = new Timer(TimerInterruptHandler, 0, randomYield);
    }

    threadToBeDestroyed = nullptr;

    // We did not explicitly allocate the current thread we are running in.
    // But if it ever tries to give up the CPU, we better have a `Thread`
    // object to save its state.
    currentThread = new Thread("main");
    currentThread->SetStatus(RUNNING);

    interrupt->Enable();
    SystemDep::CallOnUserAbort(Cleanup);  // If user hits ctl-C...

#ifdef USER_PROGRAM
    Debugger *d = debugUserProg ? new Debugger : nullptr;
    machine = new Machine(d, numPhysicalPages);  // This must come first.
    synchConsole = new SynchConsole();
    memoryPages = new Coremap(numPhysicalPages);
    SetExceptionHandlers();
    processesTable = new Table<Thread *>();
#endif

#ifdef FILESYS
    synchDisk = new SynchDisk("DISK");
    lockFS = new Lock("File System lock");
    locksSector = new Lock* [NUM_SECTORS];
    for(unsigned int i = 0; i < NUM_SECTORS; i++) locksSector[i] = nullptr;
    openFileList = new SynchList<FileHeader *>();
#endif

#ifdef FILESYS_NEEDED
    fileSystem = new FileSystem(format);
#endif

#ifdef FILESYS
    if(cd != nullptr) fileSystem->ChangeDirectory(cd);
#endif
}

/// Nachos is halting.  De-allocate global data structures.
void
Cleanup()
{
    DEBUG('i', "Cleaning up...\n");

#ifdef USER_PROGRAM
    delete machine;
#endif

#ifdef FILESYS_NEEDED
    delete fileSystem;
#endif

#ifdef FILESYS
    for(unsigned int i = 0; i < NUM_SECTORS; i++){
        if (locksSector[i] != nullptr) delete locksSector[i];
    }
    delete [] locksSector;
    delete openFileList;
    delete lockFS;
    delete synchDisk;
#endif

    delete timer;
    delete scheduler;
    delete interrupt;

    delete stats;

    //The thread destructor checks that currentThread != this
    Thread *t = currentThread;
    currentThread = NULL;
    delete t;

    exit(0);
}
