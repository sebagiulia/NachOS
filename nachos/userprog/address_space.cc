/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"
#include "lib/bitmap.hh"
#include <string.h>
#include <algorithm>
extern Bitmap *memoryPages;

/// First, set up the translation from program memory to physical memory.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    ASSERT(executable_file != nullptr);

    Executable exe (executable_file);

    ASSERT(exe.CheckMagic());     // -----Un programa puede romper el so si no es noff(arreglar) -Checkear en exec

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
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        int physicalPage = memoryPages->Find();
    //    DEBUG('a', "asigno a pagina virtual %d fisica %d\n", i, physicalPage);
        if(physicalPage == -1){
          DEBUG('a', "No space on memory to allocate the process.");
          ASSERT(false);
        }
        pageTable[i].physicalPage = physicalPage;  
        pageTable[i].valid        = true;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
          // If the code segment was entirely on a separate page, we could
          // set its pages to be read-only.
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
            uint32_t sizeToRead = std::min(codeSize-leido, PAGE_SIZE);
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
            uint32_t sizeToRead = std::min(initDataSize-leido, PAGE_SIZE);
            exe.ReadDataBlock(&mainMemory[physicalAddr], initDataSize, leido);
            leido+=sizeToRead;
            virtualAddr+=sizeToRead;
            //DEBUG('a', "Leyendo %d bytes de datablock\n", sizeToRead); 
        } 
    }

}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    for(unsigned i = 0; i < numPages; i++){
      memoryPages->Clear(pageTable[i].physicalPage);
    }
    delete [] pageTable;
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
    machine->WriteRegister(STACK_REG, (pageTable[numPages-1].physicalPage + 1) * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          (pageTable[numPages-1].physicalPage + 1) * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
}
