#include "syscall.h"
#include "lib.c"


int
main(void)
{
    putstr("HOLA!\n");
    int i = Create("a/Hola");  
    if(i != -1 ) putstr("Success: Archivo Hola creado por filesyst1.");
    else putstr("Error: No se pudo crear archivo Hola.");
    OpenFileId mundo = Open("a/Mundo");
    while(mundo == -1)
        mundo = Open("a/Mundo");
    putstr("filesyst1 escribiendo..");
    for(int i = 0; i < 100; i++)
        if(Write("a/Mundo", 5, mundo) == -1)
            putstr("Error: No se pudo escribir mundo");
    putstr("filesyst1 escribió en Mundo. ");
    Remove("a/Mundo");
    Exit(0);
    return 0;
}