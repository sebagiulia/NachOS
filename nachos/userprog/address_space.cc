/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

/***
 * Comparación de políticas: sort - matmult >> TLB 4 pages, MEMORY 32 pages
 * matmult:
 *       Política: FIFO
 *                 Fallos de memoria:  110
 *                 Accesos a disco:   105
 *                 Accesos a memoria: 747059
 * 
 *       Política: Algoritmo mejorado del reloj 
 *                 Fallos de memoria: 113
 *                 Accesos a disco: 79
 *                 Accesos a memoria: 747058
 *       
 *       Política: Algoritmo ideal
                   Fallos de memoria:  59
 *                 Accesos a disco:   31
 *                 Accesos a memoria: 747057
 * 
 * 
 * sort:
 *       Política: FIFO 
 *                 Fallos de memoria:  3182
 *                 Accesos a disco:   5536
 *                 Accesos a memoria: 22614330
 * 
 *       Política: Algoritmo mejorado del reloj 
 *                 Fallos de memoria:  2041     
 *                 Accesos a disco:   3413      
 *                 Accesos a memoria: 22614283
 *       
 *       Política: Algoritmo ideal
 *                 Fallos de memoria:  356
 *                 Accesos a disco:   596
 *                 Accesos a memoria: 22614301
*/


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"
#include "lib/coremap.hh"
#include "machine/statistics.hh"
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

    ASSERT(exe.CheckMagic());

    exe_file = executable_file;

    nextReplace = 0;
    //pf = 0;
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

    #ifdef SWAP
    InSwap = new bool[numPages];
    #endif

    #ifndef DEMAND_LOADING
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        int physicalPage = memoryPages->Find(pageTable[i].virtualPage);
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
        #ifdef SWAP
        InSwap[i] = false;
        #endif
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
    
    #ifdef SWAP
    delete [] InSwap;
    char * swap = new char[7];
    sprintf(swap, "SWAP.%u", currentThread->sid);
    fileSystem->Remove(swap);
    delete [] swap;

    #endif

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
    if((machine->GetMMU()->tlb) != nullptr && currentThread->space != nullptr){
      for (unsigned i = 0; i < TLB_SIZE; i++) {
        if(machine->GetMMU()->tlb[i].valid){
          pageTable[machine->GetMMU()->tlb[i].virtualPage].use = machine->GetMMU()->tlb[i].use;
          pageTable[machine->GetMMU()->tlb[i].virtualPage].dirty = machine->GetMMU()->tlb[i].dirty;
        }  
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
int fauls = 0;
bool
AddressSpace::LoadTLB(unsigned page)
{
    ASSERT(machine->GetMMU()->tlb != nullptr);
    if(page < 0 || page >= numPages) return false;

    if(!pageTable[page].valid){
      stats->memoryPageFaults++;
      DEBUG('e', "Page %d to be loaded in page table\n", page);

      //buscar pagina fisica
      unsigned physicalPage = memoryPages->Find(pageTable[page].virtualPage);

      if(physicalPage == (unsigned)-1){
        #ifdef SWAP
        //bandera w para debugear mas limpio
        DEBUG('w', "Tengo que swappear paginas, no hay espacio.\n");

        physicalPage = PickVictim();
        unsigned victimProccessId = memoryPages->ProccessID(physicalPage);
        unsigned victimVirtualPage = memoryPages->VirtualPage(physicalPage);

        processesTable->Get(victimProccessId)->space->Invalidate(victimVirtualPage);
        if(victimProccessId == currentThread->sid){
          DEBUG('w', "me quite una pagina a mi mismo, invalidando en TLB\n");
          //invalido en tlb y ademas sincronizo bit dirty, para poder usarlo mas abajo
          for(unsigned i = 0; i < TLB_SIZE; i++){
            if(machine->GetMMU()->tlb[i].physicalPage == physicalPage && machine->GetMMU()->tlb[i].valid) {
              unsigned virtualPage = machine->GetMMU()->tlb[i].virtualPage;
              pageTable[virtualPage].dirty = machine->GetMMU()->tlb[i].dirty;
              pageTable[virtualPage].use = machine->GetMMU()->tlb[i].use;
              machine->GetMMU()->tlb[i].valid = false;
            } 
          }
        }
        memoryPages->Mark(physicalPage, pageTable[page].virtualPage);
        bool mustSwap = true;
        AddressSpace *victimSpace = processesTable->Get(victimProccessId)->space;
        mustSwap = mustSwap && !victimSpace->ReadOnly(victimVirtualPage);
        mustSwap = mustSwap && (!victimSpace->InSwap[victimVirtualPage] || victimSpace->Dirty(victimVirtualPage));
        if(mustSwap){
          char *victimSwap = new char[7];
          sprintf(victimSwap, "SWAP.%u", victimProccessId);
          DEBUG('w', "mandando pagina %d a swap\n", physicalPage);
          OpenFile* openFile = fileSystem->Open(victimSwap);
          openFile->WriteAt(machine->mainMemory + physicalPage*PAGE_SIZE, PAGE_SIZE, victimVirtualPage*PAGE_SIZE);
          delete openFile;
          delete [] victimSwap;
          victimSpace->InSwap[victimVirtualPage] = true;
          stats->carryToSwap++;
        }
        else{
          DEBUG('w', "no me hizo falta mandar a swap");
        }
        #else
        DEBUG('a', "No space on memory to allocate the process.\n");
        ASSERT(false);
        #endif
      }
      bool inSwap = false;
      #ifdef SWAP
      inSwap = InSwap[page];
      #endif
      pageTable[page].physicalPage = physicalPage;
      pageTable[page].valid        = true;
      char *mainMemory = machine->mainMemory;
      memset(mainMemory + physicalPage*PAGE_SIZE, 0, PAGE_SIZE);
      if(inSwap){
        #ifdef SWAP
        DEBUG('w', "Trayendo pagina virtual %d de swap\n", page);

        char * swap = new char[7];
        sprintf(swap, "SWAP.%u", currentThread->sid);

        OpenFile* openFile = fileSystem->Open(swap);
        openFile->ReadAt(machine->mainMemory + physicalPage*PAGE_SIZE, PAGE_SIZE, pageTable[page].virtualPage * PAGE_SIZE);
        delete openFile;
        delete [] swap;
        pageTable[page].dirty = false;
        pageTable[page].use = false;

        stats->bringFromSwap++;
        #endif
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
    //DEBUG('w', "Reemplazando en tlb\n");
    if(machine->GetMMU()->tlb[nextReplace % TLB_SIZE].valid){
      pageTable[machine->GetMMU()->tlb[nextReplace % TLB_SIZE].virtualPage].use = machine->GetMMU()->tlb[nextReplace % TLB_SIZE].use;
      pageTable[machine->GetMMU()->tlb[nextReplace % TLB_SIZE].virtualPage].dirty = machine->GetMMU()->tlb[nextReplace % TLB_SIZE].dirty;
    }
    machine->GetMMU()->tlb[nextReplace % TLB_SIZE] = pageTable[page];
    nextReplace++;
    nextReplace%=TLB_SIZE;
    //DEBUG('w', "Termine de reemplazar %d %d\n", page, numPages);
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

  #ifdef PRPOLICY_CLOCK
    int snd = -1, trd = -1, fth = -1;
    bool dirty = false, use = false;
    unsigned init = memoryPages->NextFIFOPointer();
    
    //Recorre una vuelta entera a la memoryPages
    for(unsigned i = init, steps = 0; steps < memoryPages->NumItems(); i = memoryPages->NextFIFOPointer(), steps++) {
      dirty = pageTable[memoryPages->VirtualPage(i)].dirty;
      use = pageTable[memoryPages->VirtualPage(i)].use;
      // No fue usada usada recientemente ni está modificada (Primer opción - Se detiene)
      if(!dirty && !use)
        return i;
      // No fue usada recientemente pero si está modificada (Segunda opción)
      if(snd == -1 && dirty && !use) {
        snd = i;
      }
      // Fue usada recientemente pero no esta modificada (Tercera opción)
      else if(trd == -1 && !dirty && use) {
        trd = i;
      }
      // Fue usada recientemente y está modificada (Peor opcion)
      else if(fth == -1){
        fth = i;
      } 

      // Vamos desactivando las banderas de uso (Reloj)
      processesTable->Get(memoryPages->ProccessID(i))->space->pageTable[memoryPages->VirtualPage(i)].use = false;
    }

    if(snd != -1) {
      memoryPages->UpdateFIFOPointer(snd);
      return snd;
    } else if (trd != -1) {
      memoryPages->UpdateFIFOPointer(trd);
      return trd;
    } else {
      memoryPages->UpdateFIFOPointer(fth);
      return fth;
    }
  #endif


  #ifdef PRPOLICY_FIFO 
    return memoryPages->NextFIFOPointer(); 
  #endif
    //ASSERT(pf < 324);
    //unsigned reemplazo[] = {0, 0, 0, 0, 0, 1, 31, 29, 30, 29, 30, 29, 29, 27, 28, 27, 28, 27, 27, 25, 26, 25, 26, 25, 25, 23, 24, 23, 24, 23, 23, 21, 22, 21, 22, 21, 21, 19, 20, 19, 20, 19, 19, 17, 18, 17, 18, 17, 17, 15, 16, 15, 16, 15, 15, 27, 14, 27, 14, 27, 27, 12, 13, 12, 13, 13, 10, 11, 10, 11, 11, 8, 9, 8, 9, 9, 6, 7, 6, 7, 7, 4, 5, 4, 5, 5, 29, 3, 29, 3, 3, 30, 30, 25, 30, 30, 23, 28, 28, 28, 28, 21, 26, 26, 26, 26, 19, 24, 24, 24, 24, 17, 22, 22, 22, 22, 15, 20, 20, 20, 20, 27, 18, 18, 18, 18, 13, 16, 16, 16, 16, 11, 14, 14, 14, 14, 26, 12, 12, 12, 12, 10, 10, 10, 10, 8, 8, 8, 8, 6, 6, 6, 6, 4, 4, 4, 4, 29, 29, 29, 29, 25, 25, 25, 25, 24, 24, 24, 24, 22, 22, 22, 22, 20, 20, 20, 20, 18, 18, 18, 18, 16, 16, 16, 16, 14, 14, 14, 14, 12, 12, 12, 12, 10, 10, 10, 10, 8, 8, 8, 8, 6, 6, 6, 6, 4, 4, 4, 4, 14, 14, 14, 14, 29, 29, 29, 25, 25, 25, 24, 24, 24, 22, 22, 22, 20, 20, 20, 18, 18, 18, 16, 16, 16, 23, 23, 23, 21, 21, 21, 19, 19, 19, 17, 17, 17, 15, 15, 15, 27, 27, 27, 13, 13, 13, 11, 11, 11, 21, 21, 21, 26, 26, 9, 9, 7, 7, 5, 5, 3, 3, 30, 30, 28, 28, 19, 19, 17, 17, 15, 15, 27, 27, 13, 13, 11, 11, 21, 21, 26, 26, 9, 9, 7, 7, 27, 27, 5, 3, 30, 28, 19, 17, 15, 12, 10, 8, 6, 4, 14, 29, 25, 15, 1, 3, 4, 4, 5, 6, 7, 8, 7, 3, 0};
    int i = rand() % memoryPages->NumItems();
    //unsigned i =reemplazo[pf];
    //spf++;
    return i;
}


void AddressSpace::Invalidate(unsigned page){
  pageTable[page].valid = false;
}

bool AddressSpace::ReadOnly(unsigned page){
  return pageTable[page].readOnly;
}

bool AddressSpace::Dirty(unsigned page){
  return pageTable[page].dirty;
}