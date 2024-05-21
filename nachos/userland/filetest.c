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
#include "lib.c"

int
main(int argc, char **argv)
{  
    char *buffer = "Hola Mundo\n";
    Create("hola.txt");
    OpenFileId fid = Open("hola.txt");
    Write(buffer, strlen(buffer), fid);
    Write(buffer, strlen(buffer), fid);
    Write(buffer, strlen(buffer), fid);
    Write(buffer, strlen(buffer), fid);
    Write(buffer, strlen(buffer), fid);
    Close(fid);
    Exit(0);
}
