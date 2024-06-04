#include "syscall.h"
#include "lib.c"
#define ARGC_ERROR    "Error: missing argument."
#define REMOVE_ERROR  "Error: could not remove file."

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        Write(ARGC_ERROR, sizeof(ARGC_ERROR) - 1, CONSOLE_OUTPUT);
        Exit(1);
    }

    int success = 1;
    for (unsigned i = 1; i < argc; i++) {
        if (Remove(argv[i]) < 0) {
            Write(REMOVE_ERROR, sizeof(REMOVE_ERROR) - 1, CONSOLE_OUTPUT);
            success = 0;
        }
    }
    return !success;
}
