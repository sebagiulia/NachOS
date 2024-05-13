#ifndef NACHOS_SYNCHCONSOLE__HH
#define NACHOS_SYNCHCONSOLE__HH

#include "machine/console.hh"
#include "threads/semaphore.hh"

class SynchConsole{
public: 
SynchConsole(const char *in = nullptr, const char *out = nullptr);

~SynchConsole();

static void ReadAvail(void *arg);

static void WriteDone(void *arg);

void PutChar(char ch);

char GetChar();



private: 
Console *sConsole;
static Semaphore *readAvail;
static Semaphore *writeDone;


};


#endif