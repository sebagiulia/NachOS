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
main(void)
{
    char rc[5];
    rc[4] = 0;
    SpaceId t1 = Exec("filesyst1");
    SpaceId t2 = Exec("filesyst2");
    OpenFileId hola = Open("Hola");
    while(hola == -1)
        hola = Open("Hola");

    OpenFileId mundo = Open("Mundo");
    while(mundo == -1)
        mundo = Open("Mundo");
    putstr("Success: Ambos archivos abiertos.");
    
    char *buf = "holaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholaholahol";
    int w = Write(buf, 100, hola);
    if(w == -1) putstr("Error: no se pudo escribir en Hola");

    char buf2[1000];
    Join(t1);
    Join(t2);
    int r = Read(buf2, 1000, mundo);
    if(r > 0) {
        putstr("Success: Archivo leido.");
        itoa(r, rc);
        putstr(rc);
        buf2[r] = 0;
        putstr(buf2);
        putstr("Se leyo correctamente de Mundo.");
    } else {
        putstr("Error: Error en la lectura");
    }
    Close(hola);
    Close(mundo);
    return 0;
}
