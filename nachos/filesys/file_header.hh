/// Data structures for managing a disk file header.
///
/// A file header describes where on disk to find the data in a file, along
/// with other information about the file (for instance, its length, owner,
/// etc.)
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_FILEHEADER__HH
#define NACHOS_FILESYS_FILEHEADER__HH


#include "raw_file_header.hh"
#include "lib/bitmap.hh"
#include "threads/lock.hh"


/// The following class defines the Nachos "file header" (in UNIX terms, the
/// “i-node”), describing where on disk to find all of the data in the file.
/// The file header is organized as a simple table of pointers to data
/// blocks.
///
/// The file header data structure can be stored in memory or on disk.  When
/// it is on disk, it is stored in a single sector -- this means that we
/// assume the size of this data structure to be the same as one disk sector.
/// Without indirect addressing, this limits the maximum file length to just
/// under 4K bytes.
///
/// There is no constructor; rather the file header can be initialized
/// by allocating blocks for the file (if it is a new file), or by
/// reading it from disk.
class FileHeader {
public:

    /// Initialize a file header, including allocating space on disk for the
    /// file data.
    bool Allocate(Bitmap *bitMap, unsigned fileSize, unsigned numDirect = NUM_DIRECT);

    /// Initialize a doubly-indiretion header and its headers inside it.
    void AllocateExtraHeaders(Bitmap *bitMap, unsigned numHeaders, unsigned restSize);

    /// De-allocate this file's data blocks.
    void Deallocate(Bitmap *bitMap);

    ///> Deallocate files that only have direct data blocks.
    void DeallocateDirect(Bitmap *bitMap);

    /// Initialize file header from disk.
    void FetchFrom(unsigned sectorNumber);

    /// Write modifications to file header back to disk.
    void WriteBack(unsigned sectorNumber);

    /// Convert a byte offset into the file to the disk sector containing the
    /// byte.
    int ByteToSector(unsigned offset);

    /// Return the length of the file in bytes
    unsigned FileLength() const;

    /// Print the contents of the file.
    void Print(const char *title);

    /// Get the raw file header structure.
    ///
    /// NOTE: this should only be used by routines that operate on the file
    /// system at a low level.
    const RawFileHeader *GetRaw() const;

    /// Operations to the references number
    void IncrementProcessesRefNumber();
    void DecrementProcessesRefNumber();
    unsigned ProcessesReferencing();

    /// Increment number of bytes of header
    void IncrementNumBytes(unsigned numBytes);
    
    /// Increment number of sectors of header
    void IncrementNumSectors();
    
    /// Make space and add a new data sector  
    bool AddSector();
    
    /// Add data sector to the last position at header 
    void AppendDataSector(unsigned sector);
    
    /// Acquire lock
    void TakeLock();

    /// Release lock 
    void ReleaseLock();

    bool removed;
private:
    RawFileHeader raw;
    Lock *hdrLock; ///> Only one process can update [raw] at a time 
    unsigned processesReferencing; ///> Number of processes that reference
                                   ///> this file header at the moment. 
};


#endif
