#include "syscall.h"
#include "lib.c"
#define ARGC_ERROR    "Error: Wrong number of arguments."
#define OPEN_ERROR  "Error: could not open file."

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        putstr(ARGC_ERROR);
        Exit(1);
    }

    int success = 1;
    Create(argv[2]);
    OpenFileId fid_des = Open(argv[2]);
    if(fid_des == -1){
        putstr(OPEN_ERROR);
        Exit(1);
    }
    char buffer[201];
    OpenFileId fid_or = Open(argv[1]);
    if(fid_or == -1){
        putstr(OPEN_ERROR);
        Exit(1);
    }
    int read;
    while ((read = Read(buffer, 200, fid_or)) >0) {
            if(Write(buffer, read, fid_des) == -1){
                putstr("Error: couldn't write");
                success = 0;
                break;
            }
        }
    if(read == -1){
                putstr("Error: cound't read");
                success = 0;
            }
    Close(fid_or);
    Close(fid_des);
    return !success;
}