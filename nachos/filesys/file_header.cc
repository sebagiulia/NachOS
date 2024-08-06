/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>

/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.
/// * `maxDirectBlocks` is the number of direct sectors that de header must support (Default: NUM_DIRECT)
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize, unsigned maxDirectBlocks, int sector)
{
    ASSERT(freeMap != nullptr);
    ASSERT(maxDirectBlocks <= sizeof(RawFileHeader) - 2); ///> dataSectors size should be greater or equal.
                                                          ///> If it is equal there isn't space for 
                                                          ///> doubly-indirection sector. 

    if (fileSize > MAX_FILE_SIZE) {
        return false;
    }

    ///> Initialize class variables.
    hsector = sector;
    removed = false;
    processesReferencing = 0;
    
    //> Initialize raw
    raw.numBytes = fileSize;
    raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);
    for(unsigned i = 0; i < NUM_DIRECT + 1; i++)
        raw.dataSectors[i] = 0; 

    unsigned numDirectBlocks = raw.numSectors;
    unsigned numExtraHeaders = 0;    unsigned doublyHeader = 0;

    if(raw.numSectors > maxDirectBlocks) { ///> Doubly-indirection block needed
        DEBUG('h', "Doubly-indirection block needed for size %u\n", fileSize);
        numExtraHeaders = DivRoundUp(raw.numSectors - maxDirectBlocks,maxDirectBlocks+1); ///> Count number of header sectors 
                                                                                          ///> inside doubly indirection header.
        if(numExtraHeaders > maxDirectBlocks) { ///> Need more doubly-indirection headers.
            DEBUG('h', "One doubly-indirection not enough for size %u\n", fileSize);
            return false;
        }                                             
        numDirectBlocks = maxDirectBlocks;
        doublyHeader = 1;
    }

    
    if (freeMap->CountClear() < raw.numSectors + numExtraHeaders + doublyHeader) { ///> Count every sector including
        return false;  // Not enough space.                                        ///> those that function as metadata
                                                                                   ///> headers.
    }

    for (unsigned i = 0; i < numDirectBlocks; i++) {
        raw.dataSectors[i] = freeMap->Find(); ///> direct blocks.
    }

    ///> If it wasn't necessary a doubly-indirection pointer, return.
    if(numExtraHeaders == 0) return true; 

    unsigned doublyHeaderSector = freeMap->Find(); 

    ///> Create the doubly-indiretion header and every extra header inside 
    ///> until store every sector of the file. 
    FileHeader *sh = new FileHeader;
    sh->AllocateExtraHeaders(freeMap, numExtraHeaders, raw.numBytes - maxDirectBlocks * SECTOR_SIZE, doublyHeaderSector);
    sh->WriteBack(doublyHeaderSector);
    delete sh;
    raw.dataSectors[maxDirectBlocks] = doublyHeaderSector; ///> doubly-indirection block.
    return true;
}

void
FileHeader::AllocateExtraHeaders(Bitmap *freeMap, unsigned numExtraHeaders, unsigned restSize, int sect) {
    
    ///> Initialize every class variable.
    
    hsector = sect;
    removed = false;
    processesReferencing = 0;
    raw.numBytes = restSize;
    raw.numSectors = numExtraHeaders;
    for(unsigned i = 0; i < NUM_DIRECT + 1; i++)
        raw.dataSectors[i] = 0;
    
    unsigned restBytes = raw.numBytes;
    for(unsigned i = 0; i < numExtraHeaders; i++) {
        FileHeader *h = new FileHeader;
        unsigned sector = freeMap->Find();
        unsigned size = (NUM_DIRECT + 1) * SECTOR_SIZE; ///> This headers don't store a doubly-indirection
                                                        ///> header so they can store one more data sector.
        if(i == numExtraHeaders - 1) ///> Last sector.
            size = restBytes;
        else
            restBytes -= size;
        h->Allocate(freeMap, size, NUM_DIRECT + 1, sector);
        h->WriteBack(sector);
        delete h;
        raw.dataSectors[i] = sector;
    }
}


/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);
    DEBUG('h', "Deallocating file\n");
    for (unsigned i = 0; i < raw.numSectors; i++) {
        if(i == NUM_DIRECT) break;
        DEBUG('r', "Testing bit %d\n", raw.dataSectors[i]);
        ASSERT(freeMap->Test(raw.dataSectors[i]));  // ought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
    }
    if(raw.numSectors > NUM_DIRECT) { ///> If there is a double-indirection pointer,
                                      ///> deallocate every header inside it too.
        FileHeader *sh = new FileHeader;
        sh->FetchFrom(raw.dataSectors[NUM_DIRECT]);
        unsigned numHeaders = sh->GetRaw()->numSectors;
        for(unsigned i = 0; i < numHeaders; i++){
            FileHeader *h = new FileHeader;
            h->FetchFrom(sh->GetRaw()->dataSectors[i]);
            h->DeallocateDirect(freeMap);
            delete h;
        }
        sh->DeallocateDirect(freeMap);
        freeMap->Clear(raw.dataSectors[NUM_DIRECT]);
         
    }
}

void
FileHeader::DeallocateDirect(Bitmap *freeMap) {
    ASSERT(freeMap != nullptr);
    for (unsigned i = 0; i < raw.numSectors; i++) {
        ASSERT(freeMap->Test(raw.dataSectors[i]));  // tought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
    }
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *) &raw);
    removed = false;
    processesReferencing = 0;
    hsector = sector;
}

/// Write the modified contents of the file header back to disk.
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    synchDisk->WriteSector(sector, (char *) &raw);
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
int
FileHeader::ByteToSector(unsigned offset)
{

    unsigned sectorNumber = offset / SECTOR_SIZE;
    bool sectorExists = true;

    if(sectorNumber + 1  > raw.numSectors) {
    ///> Not enough space, try to add a new sector
    ///> to the file
        sectorExists = AddSector();
    }
    if(!sectorExists) ///> No available new sectors
        return -1;
    
    if(sectorNumber >= NUM_DIRECT) { ///> Sector in doubly-indirection header
        sectorNumber -= (NUM_DIRECT - 1);
        unsigned offsetInDoublyIndHeader = DivRoundUp(sectorNumber,NUM_DIRECT+1) - 1; ///> Sector where the header needed is located 
        unsigned offsetInHeaderNeeded = sectorNumber - offsetInDoublyIndHeader*(NUM_DIRECT+1) - 1; ///> Sector in header needed 
                                                                                                ///> where the data sector is located
        
        ///> Here, we extract the diferent sectors along the path of headers until get the
        ///> sector needed.
        unsigned doublyHeaderSector = raw.dataSectors[NUM_DIRECT];
        TakeLock();
        FileHeader *doublyHeader = new FileHeader;
        doublyHeader->FetchFrom(doublyHeaderSector);
        
        unsigned sectorOfHeaderNeeded = doublyHeader->GetRaw()->dataSectors[offsetInDoublyIndHeader];
        
        doublyHeader->WriteBack(doublyHeaderSector);
        delete doublyHeader;

        FileHeader *headerNeeded = new FileHeader;
        headerNeeded->FetchFrom(sectorOfHeaderNeeded);
        
        unsigned dataSector = headerNeeded->GetRaw()->dataSectors[offsetInHeaderNeeded];
        
        headerNeeded->WriteBack(sectorOfHeaderNeeded);
        delete headerNeeded;
        ReleaseLock();
        
        return dataSector;
    } else {
        unsigned dataSector = sectorNumber;
        return raw.dataSectors[dataSector];
    }
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr) {
        printf("File header:\n");
    } else {
        printf("%s file header:\n", title);
    }

    printf("    size: %u bytes\n",
           raw.numBytes);

    printf("block indexes: ");
    for (unsigned i = 0; i < raw.numSectors; i++) {
        if(i == NUM_DIRECT) break;
        printf("%u ", raw.dataSectors[i]);
    }

    if(raw.numSectors > NUM_DIRECT) {
        FileHeader *sh = new FileHeader;
        TakeLock();
        sh->FetchFrom(raw.dataSectors[NUM_DIRECT]);
        unsigned numHeaders = sh->GetRaw()->numSectors;
        for(unsigned i = 0; i < numHeaders; i++){
            FileHeader *h = new FileHeader;
            h->FetchFrom(sh->GetRaw()->dataSectors[i]);
            for(unsigned j = 0; j < h->GetRaw()->numSectors; j++)
                printf("%u ", h->GetRaw()->dataSectors[j]);
            delete h;
        }
        ReleaseLock();
        delete sh;         
    }
    
    printf("\n");
    for (unsigned i = 0, k = 0; i < raw.numSectors; i++) {
        if(i == NUM_DIRECT) break;
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(raw.dataSectors[i], data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
            if (isprint(data[j])) {
                printf("%c", data[j]);
            } else {
                printf("\\%X", (unsigned char) data[j]);
            }
        }
        printf("\n");
    }

    if(raw.numSectors > NUM_DIRECT) {
        TakeLock();
        FileHeader *sh = new FileHeader;
        sh->FetchFrom(raw.dataSectors[NUM_DIRECT]);
        unsigned numHeaders = sh->GetRaw()->numSectors;
        for(unsigned i = 0; i < numHeaders; i++){
            FileHeader *h = new FileHeader;
            h->FetchFrom(sh->GetRaw()->dataSectors[i]);
            for(unsigned j = 0, k = 0; j < h->GetRaw()->numSectors; j++) {
              printf("    contents of block %u:\n", h->GetRaw()->dataSectors[j]);
              synchDisk->ReadSector(h->GetRaw()->dataSectors[j], data);
              for (unsigned m = 0; m < SECTOR_SIZE && k < h->GetRaw()->numBytes; m++, k++) {
                if (isprint(data[m]))
                  printf("%c", data[m]);
                else
                  printf("\\%X", (unsigned char) data[m]);
              }
              printf("\n");
            }
            delete h;
        }
        ReleaseLock();
        delete sh;
    }

    delete [] data;
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}


bool
FileHeader::AddSector() {
    if(raw.numSectors + 1 > MAX_FILE_SIZE) ///> No sector available 
        return false; 
    
    fileSystem->TakeLock();
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(fileSystem->GetFreeMapFile());
    unsigned clear = freeMap->CountClear();

    if(raw.numSectors < NUM_DIRECT && clear >= 1) { ///> Create new sector in a direct pointer of file header
        int newSector = freeMap->Find();
        DEBUG('f', "Adding sector %u to the file\n", newSector);
        AppendDataSector(newSector);
    } else if (raw.numSectors == NUM_DIRECT && clear >= 3) {
        /// Necessary news doubly-indirection sector, indirect sector and the data sector
        unsigned doubIndirectSector = freeMap->Find();

        FileHeader *doubIndSectHeader = new FileHeader;
        doubIndSectHeader->AllocateExtraHeaders(freeMap, 1, SECTOR_SIZE, doubIndirectSector);
        doubIndSectHeader->WriteBack(doubIndirectSector);
        AppendDataSector(doubIndirectSector);
        delete doubIndSectHeader;
    } else if (raw.numSectors > NUM_DIRECT) { ///> Doubly-indirection block already in header
        
        if((raw.numSectors - NUM_DIRECT) % (NUM_DIRECT + 1) == 0) { ///> Need a new indirect sector and data sector
            unsigned doubIndirectSector = raw.dataSectors[NUM_DIRECT];
            FileHeader *doubIndSectHeader = new FileHeader;
            doubIndSectHeader->FetchFrom(doubIndirectSector);

            if(doubIndSectHeader->GetRaw()->numSectors == NUM_DIRECT + 1 && clear < 2) { 
                ReleaseLock();
                fileSystem->ReleaseLock();
                delete doubIndSectHeader;
                delete freeMap;
                return false; ///> No available sectors
            }

            
            unsigned indHeaderSector = freeMap->Find(); ///> indirect header
            FileHeader *indHeader = new FileHeader;
            indHeader->Allocate(freeMap, SECTOR_SIZE, NUM_DIRECT, indHeaderSector); 
            indHeader->WriteBack(indHeaderSector);
            delete indHeader;

            doubIndSectHeader->AppendDataSector(indHeaderSector);
            doubIndSectHeader->IncrementNumSectors();
            doubIndSectHeader->WriteBack(doubIndirectSector);
            delete doubIndSectHeader;
        } else if(clear >= 1) { ///> New data sector in the last header of the doubly-indirection header
            unsigned doubIndirectSector = raw.dataSectors[NUM_DIRECT];
            FileHeader *doubIndSectHeader = new FileHeader;
            doubIndSectHeader->FetchFrom(doubIndirectSector);
            unsigned numSectors = doubIndSectHeader->GetRaw()->numSectors;
            unsigned indHeaderSect = doubIndSectHeader->GetRaw()->dataSectors[numSectors - 1];
            delete doubIndSectHeader;

            FileHeader *indHeader = new FileHeader;
            indHeader->FetchFrom(indHeaderSect);
            int newSector = freeMap->Find();
            indHeader->AppendDataSector(newSector);
            indHeader->IncrementNumBytes(SECTOR_SIZE);
            indHeader->IncrementNumSectors();
            indHeader->WriteBack(indHeaderSect);
            delete indHeader;
        }

    } else { ///> No available sectors
        ReleaseLock();
        fileSystem->ReleaseLock();
        delete freeMap;
        return false;
    }

    IncrementNumSectors();
    freeMap->WriteBack(fileSystem->GetFreeMapFile());
    fileSystem->ReleaseLock();
    return true; 
}

void
FileHeader::AppendDataSector(unsigned sector) { ///> Lock already taken
    raw.dataSectors[raw.numSectors] = sector;
}

void
FileHeader::IncrementProcessesRefNumber() { ///> Lock already taken   
    processesReferencing++;

}

void
FileHeader::DecrementProcessesRefNumber() {  ///> Lock already taken  
    processesReferencing--;
}

unsigned
FileHeader::ProcessesReferencing() {
    return processesReferencing;
}

void
FileHeader::IncrementNumBytes(unsigned numBytes) {
    raw.numBytes += numBytes;
}

void
FileHeader::IncrementNumSectors() {
    raw.numSectors ++;
}

void
FileHeader::TakeLock() {
    Lock *lock = fileSystem->GetLock(hsector);
    if(!lock->IsHeldByCurrentThread()) /// Prevent double release
        lock->Acquire();
}

void
FileHeader::ReleaseLock() {
    Lock *lock = fileSystem->GetLock(hsector);
    if(lock->IsHeldByCurrentThread()) /// Prevent double release
        lock->Release();
}