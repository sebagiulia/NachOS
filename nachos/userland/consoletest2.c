#include "syscall.h"
#include "lib.c"

int main(void){
    SpaceId t1 = Exec("consoletest");

    SpaceId t2 = Exec("consoletest");

    //SpaceId t3 = Exec("consoletest");

    //SpaceId t4 = Exec("consoletest");

    //SpaceId t5 = Exec("consoletest");
    Join(t1);
    Join(t2);
    //Join(t3);
    //Join(t4);
    //Join(t5);
    Exit(0);
}