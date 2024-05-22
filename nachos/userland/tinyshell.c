#include "syscall.h"
#include "lib.c"


int
main(void)
{
    SpaceId    newProc;
    OpenFileId input  = CONSOLE_INPUT;
    OpenFileId output = CONSOLE_OUTPUT;
    char       prompt[2] = { '-', '-' };
    char       ch, buffer[60];
    int        i;
    char *line;
    int runInBackground = 0;

    for (;;) {
        Write(prompt, 2, output);
        i = 0;
        do {
            Read(&buffer[i], 1, input);
        } while (buffer[i++] != '\n');

        buffer[--i] = '\0';

        line = buffer;
        if(buffer[0] == '&') {
            line = &buffer[1];
            runInBackground = 1;
        }

        if (i > 0) {
            newProc = Exec(line);
            if(!runInBackground)
                Join(newProc);
            runInBackground = 0;
        }
    }

    return -1;
}
