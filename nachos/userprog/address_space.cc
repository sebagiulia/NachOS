/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"
#include "lib/coremap.hh"
#include <string.h>
#include <algorithm>
#include <time.h>
#include <cstdio>
extern Coremap *memoryPages;

/// First, set up the translation from program memory to physical memory.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    ASSERT(executable_file != nullptr);

    Executable exe (executable_file);

    ASSERT(exe.CheckMagic());     // -----Un programa puede romper el so si no es noff(arreglar) -Checkear en exec

    exe_file = executable_file;

    nextReplace = 0;

    // How big is address space?

    unsigned size = exe.GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;

      // ASSERT(numPages <= machine->GetNumPhysicalPages()); --> Está checkeado abajo
      // Check we are not trying to run anything too big -- at least until we
      // have virtual memory.

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.

    pageTable = new TranslationEntry[numPages];
    #ifndef DEMAND_LOADING
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        int physicalPage = memoryPages->Find(pageTable[i].virtualPage);
        //DEBUG('a', "asigno a pagina virtual %d pagina fisica %d\n", i, physicalPage);
        if(physicalPage == -1){
          DEBUG('a', "No space on memory to allocate the process.");
          ASSERT(false);
        }
        pageTable[i].physicalPage = physicalPage;
        pageTable[i].valid        = true;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
        char *mainMemory = machine->mainMemory;
        unsigned offset = physicalPage * PAGE_SIZE;
        memset(mainMemory + offset, 0, PAGE_SIZE);

    }

    char *mainMemory = machine->mainMemory;

    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();
    if (codeSize > 0) {
        uint32_t virtualAddr = exe.GetCodeAddr();
        DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
                virtualAddr, codeSize);
        uint32_t leido = 0;
        while(codeSize - leido> 0){
            uint32_t PageNumber = virtualAddr / PAGE_SIZE;
            uint32_t offset =  virtualAddr % PAGE_SIZE;
            uint32_t physicalAddr = (pageTable[PageNumber].physicalPage * PAGE_SIZE)+offset;
            uint32_t sizeToRead = std::min(codeSize-leido, PAGE_SIZE-offset);
            exe.ReadCodeBlock(&mainMemory[physicalAddr], sizeToRead, leido);
            leido+=sizeToRead;
            virtualAddr+=sizeToRead;
            if(sizeToRead == PAGE_SIZE){
              pageTable[PageNumber].readOnly = true;
            }
        }
    }
    if (initDataSize > 0) {
        uint32_t virtualAddr = exe.GetInitDataAddr();
        DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
              virtualAddr, initDataSize);
        uint32_t leido = 0;
        while(initDataSize - leido > 0){
            uint32_t PageNumber = virtualAddr / PAGE_SIZE;
            uint32_t offset =  virtualAddr % PAGE_SIZE;
            uint32_t physicalAddr = (pageTable[PageNumber].physicalPage * PAGE_SIZE)+offset;
            uint32_t sizeToRead = std::min(initDataSize-leido, PAGE_SIZE-offset);
            exe.ReadDataBlock(&mainMemory[physicalAddr], sizeToRead, leido);
            leido+=sizeToRead;
            virtualAddr+=sizeToRead;
            //DEBUG('a', "Leyendo %d bytes de datablock\n", sizeToRead);
        }
    }
  #else
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        pageTable[i].physicalPage = -1;
        pageTable[i].valid        = false;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
    }
  #endif

}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    for(unsigned i = 0; i < numPages; i++){
      if(pageTable[i].valid)
        memoryPages->Clear(pageTable[i].physicalPage);
    }
    delete [] pageTable;
    delete exe_file;
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, (numPages) * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          (numPages) * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{
    if((machine->GetMMU()->tlb) != nullptr){
      for (unsigned i = 0; i < TLB_SIZE; i++) {
        pageTable[machine->GetMMU()->tlb[i].virtualPage].use = machine->GetMMU()->tlb[i].use;
        pageTable[machine->GetMMU()->tlb[i].virtualPage].dirty = machine->GetMMU()->tlb[i].dirty;
      }
    }
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
    if((machine->GetMMU()->tlb) != nullptr){
      for (unsigned i = 0; i < TLB_SIZE; i++) {
        machine->GetMMU()->tlb[i].valid = false;
      }
    nextReplace = 0;
    }
    else{
      machine->GetMMU()->pageTable     = pageTable;
      machine->GetMMU()->pageTableSize = numPages;
    }
}

bool
AddressSpace::LoadTLB(unsigned page)
{
    ASSERT(machine->GetMMU()->tlb != nullptr);
    if(page < 0 || page > numPages) return false;

    if(!pageTable[page].valid){
      DEBUG('e', "Page %d to be loaded in page table\n", page);

      bool inSwap = false;

      //buscar pagina fisica
      int physicalPage = memoryPages->Find(pageTable[page].virtualPage);

      if(physicalPage == -1){
        #ifdef SWAP
        DEBUG('a', "Swapping page.");

        physicalPage = PickVictim();
        unsigned victimProccessId = memoryPages->ProccessID(physicalPage);
        unsigned victimVirtualPage = memoryPages->VirtualPage(physicalPage);
        // ¿aca faltaria poner en falso el bit valid de la entrada victimVirtualPage de la pageTable de victimProccessId?
        // capaz eso lo deberia cambiar el proceso victimProccessId cuando retoma el procesador chequeando el coremap
        // ademas tambien habria que chequear el bit dirty de la victimVirtualPage, ya que si es false no hace falta llevarla a swap
        memoryPages->Mark(physicalPage, pageTable[page].virtualPage);

        char *victimSwap = new char[7];
        sprintf(victimSwap, "SWAP.%u", victimProccessId);

        //primero deberiamos chequear que la pagina no este ya en swap
        OpenFile* openFile = fileSystem->Open(victimSwap);
        openFile->WriteAt(machine->mainMemory + physicalPage*PAGE_SIZE, PAGE_SIZE, victimVirtualPage);
        delete openFile;

        delete victimSwap;

        if(pageTable[page].physicalPage != (unsigned)-1) inSwap = true;

        ASSERT(false); //esto no iria mas
        #else
        DEBUG('a', "No space on memory to allocate the process.");
        ASSERT(false);
        #endif
      }

      pageTable[page].physicalPage = physicalPage;
      pageTable[page].valid        = true;
      char *mainMemory = machine->mainMemory;
      memset(mainMemory + physicalPage*PAGE_SIZE, 0, PAGE_SIZE);

      if(inSwap){ // chequear que la pagina este en swap
        //cargar pagina de swap
        DEBUG('a', "Swapping page.");

        char * swap = new char[7];
        sprintf(swap, "SWAP.%u", currentThread->sid);

        OpenFile* openFile = fileSystem->Open(swap);
        openFile->ReadAt(machine->mainMemory + physicalPage*PAGE_SIZE, PAGE_SIZE, pageTable[page].virtualPage);
        delete openFile;

        delete swap;
      }else{
        //cargar la pagina del ejecutable
        Executable exe (exe_file);
        uint32_t codeSize = exe.GetCodeSize();
        uint32_t initDataSize = exe.GetInitDataSize();
        uint32_t physicalAddr = (physicalPage * PAGE_SIZE);
        if(page * PAGE_SIZE < codeSize){ //cargar segmento de codigo
          uint32_t sizeToRead = std::min(codeSize-(page*PAGE_SIZE), PAGE_SIZE);
          exe.ReadCodeBlock(&mainMemory[physicalAddr], sizeToRead, page*PAGE_SIZE);
          if(sizeToRead == PAGE_SIZE){
            pageTable[page].readOnly = true;
          }
          else if(initDataSize > 0){ //tengo que seguir llenando con initdata
            uint32_t newSizeToRead = PAGE_SIZE - sizeToRead;
            newSizeToRead = std::min(newSizeToRead, codeSize+initDataSize-(page*PAGE_SIZE));
            exe.ReadDataBlock(&mainMemory[physicalAddr+sizeToRead], newSizeToRead, 0);
          }
        }
        else if(page * PAGE_SIZE < initDataSize+codeSize){ //cargar initdata
          uint32_t sizeToRead = std::min(codeSize+initDataSize-(page*PAGE_SIZE), PAGE_SIZE);
          exe.ReadDataBlock(&mainMemory[physicalAddr], sizeToRead, page*PAGE_SIZE - codeSize);
        }
        //sino ya esta lleno de ceros
      }
    }

    pageTable[machine->GetMMU()->tlb[nextReplace % TLB_SIZE].virtualPage].use = machine->GetMMU()->tlb[nextReplace % TLB_SIZE].use;
    pageTable[machine->GetMMU()->tlb[nextReplace % TLB_SIZE].virtualPage].dirty = machine->GetMMU()->tlb[nextReplace % TLB_SIZE].dirty;
    machine->GetMMU()->tlb[nextReplace % TLB_SIZE] = pageTable[page];
    nextReplace++;
    nextReplace%=TLB_SIZE;
    return true;
}

unsigned
AddressSpace::NumPages()
{
    return numPages;
}

int
AddressSpace::PickVictim()
{
    srand(time(NULL));
    int i = rand() % memoryPages->NumItems();
    return i;
}
