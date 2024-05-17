
#include "syscall.h"

unsigned strlen(const char *str ){
    unsigned i;
    for(i=0; str[i] != '\0'; i++);
    return i;
}


//Returns the converted integer value if the execution is successful. 
//If the passed string is not convertible to an integer, it returns zero.
int atoi(const char *str){
    unsigned n = strlen(str);
    int res = 0;
    for(unsigned i=0; i<n; i++){
        int conv = str[i] - '0';
        if(conv < 0 || conv > 9) return 0;
        res*=10; res+= conv;
    }
    return res;
}

void reverse(char *str){
    unsigned n = strlen(str);
    for(unsigned i=0; i<n/2; i++){
        char aux = str[i];
        str[i] = str[n-i-1];
        str[n-i-1] = aux;
    }
}
//Convierte n a string, y lo guarda en str.
void itoa(int n, char *str){
    int i = 0;
    for(;n!=0;i++){
        str[i] = n%10 + '0';
        n/=10;
    }
    str[i] = '\0';
    reverse(str);
}
//puts, escribe str en la consola seguido de un \n.
void putstr(const char *str){
    unsigned n = strlen(str);
    Write(str, n, 1);
    Write("\n",1,1);
}
