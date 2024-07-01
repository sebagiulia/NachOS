#include "syscall.h"
#include "lib.c"


int
main(void)
{
    int j = Create("Mundo");   
    if(j != -1 ) putstr("Success: Archivo Mundo creado por filesyst2.");
    else putstr("Error: No se pudo crear archivo Mundo.");
    
    OpenFileId hola = Open("Hola");
    while(hola == -1)
        hola = Open("Hola");
    putstr("Success: Archivo Hola abierto por filesyst2");
    
    char buf[100], rc[5];
    buf[99] = 0;
    int r= -1;
    while(r < 1) 
        r = Read(buf, 100, hola);
    if(strlen(buf) == 99) {
        putstr("Success: Se leyo Hola correctamente:");
        putstr(buf);
    } else {
        putstr("Error: Hubo un error en la lectura del archivo");
        itoa(r, rc);
        putstr(buf);
        putstr(rc);
        return 0;
    }
    Remove("Hola");
    hola = Open("Hola");
    if(hola == -1) putstr("Success: Se elimino el archivo del directorio con exito");
    else putstr("Error: Se abrio un archivo que esta previamente removido");
    Exit(0);
    return 0;
}