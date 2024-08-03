/// Copyright (c) 2019-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"


void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount != 0);

    unsigned count;
    for(count = 0; count < byteCount; outBuffer++){
        int temp;
        count++;
        int pruebas = 0;
        while(machine->ReadMem(userAddress, 1, &temp) == 0 && pruebas < 4) pruebas++;
        ASSERT(pruebas < 4);
        userAddress++;
        *outBuffer = (unsigned char) temp;
        //DEBUG('v', "outBuffer=%c\n", *outBuffer);
    };
}

bool ReadStringFromUser(int userAddress, char *outString,
                        unsigned maxByteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        count++;
        int pruebas = 0;
        while(machine->ReadMem(userAddress, 1, &temp) == 0 && pruebas < 4) pruebas++;
        userAddress++;
        ASSERT(pruebas < 4);
        *outString = (unsigned char) temp;
    } while (*outString++ != '\0' && count < maxByteCount);

    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress,
                       unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(buffer != nullptr);
    ASSERT(byteCount != 0);
    unsigned count;
    for(count = 0; count < byteCount; buffer++){
        int temp = (int) *buffer;
        count++;
        int pruebas = 0;
        while(machine->WriteMem(userAddress, 1, temp) == 0 && pruebas < 4);
        ASSERT(pruebas < 4);
        userAddress++;
    };
}

void WriteStringToUser(const char *string, int userAddress)
{
    ASSERT(userAddress != 0);
    ASSERT(string != nullptr);
    do{
        int temp = (int) *string;
        int pruebas = 0;
        while(machine->WriteMem(userAddress, 1, temp) == 0 && pruebas < 4) pruebas++;
        ASSERT(pruebas < 4);
        userAddress++;
    }while(*string++ != '\0');
}