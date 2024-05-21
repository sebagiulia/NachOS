#include "syscall.h"
#include "lib.c"
#define ARGC_ERROR    "Error: Wrong number of arguments."
#define OPEN_ERROR  "Error: could not open file."

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        Write(ARGC_ERROR, sizeof(ARGC_ERROR) - 1, CONSOLE_OUTPUT);
        Exit(1);
    }

    int success = 1;
    char buffer[201];
    OpenFileId fid = Open(argv[1]);
    if(fid == -1){
        Write(OPEN_ERROR, sizeof(ARGC_ERROR) - 1, CONSOLE_OUTPUT);
        Exit(1);
    }
    int read;
    while (read = Read(buffer, 200, fid) >0) {
            if(Write(buffer, strlen(buffer), CONSOLE_OUTPUT) == 1){
                putstr("error, couldn't write");
                success = 0;
                break;
            }
        }
    if(read == -1){
                putstr("error: cound't read");
                success = 0;
            }    
    return !success;
}