/// Simple program to test whether running a user program works.
///
/// Just do a “syscall” that shuts down the OS.
///
/// NOTE: for some reason, user programs with global data structures
/// sometimes have not worked in the Nachos environment.  So be careful out
/// there!  One option is to allocate data structures as automatics within a
/// procedure, but if you do this, you have to be careful to allocate a big
/// enough stack to hold the automatics!


#include "syscall.h"


int
main(void)
{
    //Remove("test.txt");
    Create("test.txt");
    OpenFileId o = Open("test.txt");
    Write("Hello world\n\0",13,o);
    Close(o);
    o = Open("test.txt");
    char buffer[50];
    //o = 1;
    Read(buffer,13,o);
    Close(o);
    Remove("test.txt");
    //int r = Read(buffer, 50, 0);
    Write(buffer, 13, 1);
    return 0;
}
