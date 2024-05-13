#ifndef NACHOS_SYNCHCONSOLE__HH
#define NACHOS_SYNCHCONSOLE__HH

#include "machine/console.hh"
#include "threads/semaphore.hh"

class SynchConsole{
public: 
SynchConsole(const char *in = nullptr, const char *out = nullptr);

~SynchConsole();

void ReadAvail();

void WriteDone();

void PutChar(char ch);

char GetChar();

private: 
Console *console;
Semaphore *readAvail;
Semaphore *writeDone;


};


#endif