/// Data structures to keep track of executing user programs (address
/// spaces).
///
/// For now, we do not keep any information about address spaces.  The user
/// level CPU state is saved and restored in the thread executing the user
/// program (see `thread.hh`).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_USERPROG_ADDRESSSPACE__HH
#define NACHOS_USERPROG_ADDRESSSPACE__HH


#include "filesys/file_system.hh"
#include "machine/translation_entry.hh"
#include "lib/table.hh"


const unsigned USER_STACK_SIZE = 1024;  ///< Increase this as necessary!


class AddressSpace {
public:

    /// Create an address space to run a user program.
    ///
    /// The address space is initialized from an already opened file.
    /// The program contained in the file is loaded into memory and
    /// everything is set up so that user instructions can start to be
    /// executed.
    ///
    /// Parameters:
    /// * `executable_file` is the open file that corresponds to the
    ///   program; it contains the object code to load into memory.
    AddressSpace(OpenFile *executable_file);

    /// De-allocate an address space.
    ~AddressSpace();

    /// Initialize user-level CPU registers, before jumping to user code.
    void InitRegisters();

    /// Save/restore address space-specific info on a context switch.

    void SaveState();
    void RestoreState();

    /// Return the number of pages of the address space
    unsigned NumPages();

    bool LoadTLB(unsigned page);

    bool ReadOnly(unsigned page);

    bool Dirty(unsigned page);

    void Invalidate(unsigned page);

    #ifdef SWAP
    bool *InSwap;
    #endif

    OpenFile *exe_file;

private:
    /// Select a random physical page number.
    int PickVictim();

    /// Assume linear page table translation for now!
    TranslationEntry *pageTable;
    int nextReplace;
    /// Number of pages in the virtual address space.
    unsigned numPages;
  //  int pf;
};


#endif
