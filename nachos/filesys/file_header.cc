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
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize, unsigned maxDirectBlocks)
{
    ASSERT(freeMap != nullptr);
    ASSERT(maxDirectBlocks <= sizeof(RawFileHeader) - 2); ///> dataSectors size should be greater or equal.
                                                    ///> If it is equal there isn't space for 
                                                    ///> doubly-indirection sector. 

    if (fileSize > MAX_FILE_SIZE) {
        return false;
    }

    ///> Initialize class variables.
    removed = false;
    hdrLock = new Lock("File header lock");
    numberOpenFiles = 0;
    raw.numBytes = fileSize;
    raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);

    unsigned numDirectBlocks = raw.numSectors;
    unsigned numExtraHeaders = 0;
    unsigned doublyHeader = 0;

    if(raw.numSectors > maxDirectBlocks) { ///> Doubly-indirection block needed
        DEBUG('w', "Doubly-indirection block needed for size %u\n", fileSize);
        numExtraHeaders = DivRoundUp(raw.numSectors - maxDirectBlocks,maxDirectBlocks+1); ///> Count number of header sectors 
                                                                                          ///> inside doubly indirection header.
        if(numExtraHeaders > maxDirectBlocks) { ///> Need more doubly-indirection headers.
            DEBUG('w', "One doubly-indirection not enough for size %u\n", fileSize);
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
    sh->AllocateExtraHeaders(freeMap, numExtraHeaders, raw.numBytes - maxDirectBlocks * SECTOR_SIZE);
    sh->WriteBack(doublyHeaderSector);
    delete sh;
    raw.dataSectors[maxDirectBlocks] = doublyHeaderSector; ///> doubly-indirection block.
    return true;
}

void
FileHeader::AllocateExtraHeaders(Bitmap *freeMap, unsigned numExtraHeaders, unsigned restSize) {
    
    ///> Initialize every class variable.
    removed = false;
    hdrLock = new Lock("File header lock");
    numberOpenFiles = 0;
    raw.numBytes = restSize;
    raw.numSectors = numExtraHeaders;
    
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
        h->Allocate(freeMap, size, NUM_DIRECT + 1);
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
    DEBUG('w', "Deallocating file\n");
    for (unsigned i = 0; i < raw.numSectors; i++) {
        if(i == NUM_DIRECT) break;
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

/// Fetch contents of file header from disk, reinitalized [removed] and [hdrLock].
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *) &raw);
    removed = false;
    numberOpenFiles = 0;
    hdrLock = new Lock("File header lock");
}

/// Write the modified contents of the file header back to disk.
/// [hdrLock] is not necessary after that.
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    hdrLock->Acquire();
    synchDisk->WriteSector(sector, (char *) &raw);
    hdrLock->Release();
    delete hdrLock;
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
    unsigned sectorNumber = offset / SECTOR_SIZE;

    if(sectorNumber >= NUM_DIRECT) { ///> Sector in doubly-indirection header
        sectorNumber -= (NUM_DIRECT - 1);
        unsigned offsetInDoublyHeader = DivRoundUp(sectorNumber,NUM_DIRECT+1) - 1; ///> Sector where the header needed is located 
        unsigned offsetInHeaderNeeded = sectorNumber - offsetInDoublyHeader*(NUM_DIRECT+1) - 1; ///> Sector in header needed 
                                                                                                ///> where the data sector is located
        
        ///> Here, we extract the diferent sectors along the path of headers until get the
        ///> sector needed.
        unsigned doublyHeaderSector = raw.dataSectors[NUM_DIRECT];
        hdrLock->Acquire();
        FileHeader *doublyHeader = new FileHeader;
        doublyHeader->FetchFrom(doublyHeaderSector);
        
        unsigned sectorOfHeaderNeeded = doublyHeader->GetRaw()->dataSectors[offsetInDoublyHeader];
        
        doublyHeader->WriteBack(doublyHeaderSector);
        delete doublyHeader;

        FileHeader *headerNeeded = new FileHeader;
        headerNeeded->FetchFrom(sectorOfHeaderNeeded);
        
        unsigned dataSector = headerNeeded->GetRaw()->dataSectors[offsetInHeaderNeeded];
        
        headerNeeded->WriteBack(sectorOfHeaderNeeded);
        delete headerNeeded;
        hdrLock->Release();
        
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
        hdrLock->Acquire();
        sh->FetchFrom(raw.dataSectors[NUM_DIRECT]);
        unsigned numHeaders = sh->GetRaw()->numSectors;
        for(unsigned i = 0; i < numHeaders; i++){
            FileHeader *h = new FileHeader;
            h->FetchFrom(sh->GetRaw()->dataSectors[i]);
            for(unsigned j = 0; j < h->GetRaw()->numSectors; j++)
                printf("%u ", h->GetRaw()->dataSectors[j]);
            delete h;
        }
        hdrLock->Release();
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
        hdrLock->Acquire();
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
        hdrLock->Release();
        delete sh;
    }

    delete [] data;
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}

void
FileHeader::IncrementOpenFilesNumber() {        
    hdrLock->Acquire();
    numberOpenFiles++;
    hdrLock->Release();

}

void
FileHeader::DecrementOpenFilesNumber() {
    hdrLock->Acquire();
    numberOpenFiles--;
    hdrLock->Release();
}

unsigned
FileHeader::OpenFilesNumber() {
    return numberOpenFiles;
}
