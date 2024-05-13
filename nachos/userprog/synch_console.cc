#include "synch_console.hh"

static void SynchConsole::ReadAvail(void *arg){
    readAvail->V();
}

static void SynchConsole::WriteDone(void *arg){
    writeDone->V();
}

SynchConsole::SynchConsole(const char *in, const char *out){
    readAvail = new Semaphore("read avail for synch console", 0);
    writeDone = new Semaphore("write done for synch console", 0);
    sConsole = new Console(in, out, ReadAvail, WriteDone, 0);
}


SynchConsole::~SynchConsole(){
    delete sConsole;
    delete readAvail;
    delete writeDone;
}
