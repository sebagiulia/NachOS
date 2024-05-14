#include "synch_console.hh"

/// Console interrupt handlers.  Need this to be a C routine, because C++ cannot
/// handle pointers to member functions.
static void
SynchConsoleReadAvail(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *sc = (SynchConsole *) arg;
    sc->ReadAvail();
}

static void
SynchConsoleWriteDone(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *sc = (SynchConsole *) arg;
    sc->WriteDone();
}

void SynchConsole::ReadAvail(){
    readAvail->V();
}

void SynchConsole::WriteDone(){
    writeDone->V();
}

SynchConsole::SynchConsole(const char *in, const char *out){
    readAvail = new Semaphore("read avail for synch console", 0);
    writeDone = new Semaphore("write done for synch console", 0);
    console = new Console(in, out, SynchConsoleReadAvail, SynchConsoleWriteDone, this);
}


SynchConsole::~SynchConsole(){
    delete console;
    delete readAvail;
    delete writeDone;
}

char SynchConsole::GetChar(){
    readAvail->P();
    return console->GetChar();
}

void SynchConsole::PutChar(char c){
    console->PutChar(c);
    writeDone->P();
}