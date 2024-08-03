/// Routines to manage an open Nachos file.  As in UNIX, a file must be open
/// before we can read or write to it.  Once we are all done, we can close it
/// (in Nachos, by deleting the `OpenFile` data structure).
///
/// Also as in UNIX, for convenience, we keep the file header in memory while
/// the file is open.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "open_file.hh"
#include "file_header.hh"
#include "threads/system.hh"

#include <string.h>
#include <stdio.h>


/// Open a Nachos file for reading and writing.  Bring the file header into
/// memory while the file is open.
///
/// * `sector` is the location on disk of the file header for this file.
OpenFile::OpenFile(int sector, FileHeader *fhdr)
{
    if(fhdr == nullptr) { ///> freeMapFile or directoryFile
        hdr = new FileHeader;
        hdr->FetchFrom(sector);
        sectorhdr = -1; ///> not necessary to know in this case

    } else { ///> file opened by some process
        hdr = fhdr;
        hdr->IncrementProcessesRefNumber();
        sectorhdr = sector;
    }
    seekPosition = 0;
}

/// Close a Nachos file, de-allocating any in-memory data structures.
OpenFile::~OpenFile()
{
    if(sectorhdr == -1) /// freeMapFile or directoryFile
        delete hdr;
    else{
        #ifdef FILESYS
        hdr->DecrementProcessesRefNumber();
        if(!openFileList->HasKey(sectorhdr)){
            return;
        }
        if(hdr->ProcessesReferencing() == 0) { /// this is the last reference to the file in memory.
            fileSystem->Remove(nullptr, hdr, sectorhdr);
        }
        #endif
    }
     ///else -> there are processes that still reference this file, so we do not remove data structure
    
    

}

/// Change the current location within the open file -- the point at which
/// the next `Read` or `Write` will start from.
///
/// * `position` is the location within the file for the next `Read`/`Write`.
void
OpenFile::Seek(unsigned position)
{
    seekPosition = position;
}

/// OpenFile::Read/Write
///
/// Read/write a portion of a file, starting from `seekPosition`.  Return the
/// number of bytes actually written or read, and as a side effect, increment
/// the current position within the file.
///
/// Implemented using the more primitive `ReadAt`/`WriteAt`.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.

int
OpenFile::Read(char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);
    int result = ReadAt(into, numBytes, seekPosition);
    seekPosition += result;
    return result;
}

int
OpenFile::Write(const char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    int result = WriteAt(into, numBytes, seekPosition);
    seekPosition += result;
    return result;
}

/// OpenFile::ReadAt/WriteAt
///
/// Read/write a portion of a file, starting at `position`.  Return the
/// number of bytes actually written or read, but has no side effects (except
/// that `Write` modifies the file, of course).
///
/// There is no guarantee the request starts or ends on an even disk sector
/// boundary; however the disk only knows how to read/write a whole disk
/// sector at a time.  Thus:
///
/// For ReadAt:
///     We read in all of the full or partial sectors that are part of the
///     request, but we only copy the part we are interested in.
/// For WriteAt:
///     We must first read in any sectors that will be partially written, so
///     that we do not overwrite the unmodified portion.  We then copy in the
///     data that will be modified, and write back all the full or partial
///     sectors that are part of the request.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.
/// * `position` is the offset within the file of the first byte to be
///   read/written.

int
OpenFile::ReadAt(char *into, unsigned numBytes, unsigned position)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);
    hdr->TakeLock();
    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    char *buf;

    if (position >= fileLength) {
        DEBUG('e', "position %d, fileLength %d \n", position, fileLength);
        return 0;  // Check request.
    }
    if (position + numBytes > fileLength) {
        numBytes = fileLength - position;
    }
    DEBUG('f', "Reading %u bytes at %u, from file of length %u.\n",
          numBytes, position, fileLength);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors = 1 + lastSector - firstSector;

    // Read in all the full and partial sectors that we need.
    buf = new char [numSectors * SECTOR_SIZE];
    memset(buf, 0, numSectors * SECTOR_SIZE);
    
    for (unsigned i = firstSector; i <= lastSector; i++) {
        int sector = hdr->ByteToSector(i * SECTOR_SIZE);
        if(sector == -1) {
            hdr->ReleaseLock();
            return -1;
        }
        synchDisk->ReadSector(sector,
                              &buf[(i - firstSector) * SECTOR_SIZE]);
    }

    // Copy the part we want.
    memcpy(into, &buf[position - firstSector * SECTOR_SIZE], numBytes);
    delete [] buf;
    hdr->ReleaseLock();
    return numBytes;
}

int
OpenFile::WriteAt(const char *from, unsigned numBytes, unsigned position)
{
    ASSERT(from != nullptr);
    ASSERT(numBytes > 0);
    hdr->TakeLock();
    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;
    
    DEBUG('f', "Writing %u bytes at %u, from file of length %u.\n",
          numBytes, position,fileLength);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector  = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors  = 1 + lastSector - firstSector;

    buf = new char [numSectors * SECTOR_SIZE];
    memset(buf, 0, numSectors * SECTOR_SIZE);

    ///> Only read until the last available sector at file
    unsigned lastAux = hdr->GetRaw()->numSectors < lastSector ? hdr->GetRaw()->numSectors : lastSector;
    firstAligned = position == firstSector * SECTOR_SIZE;
    lastAligned  = position + numBytes >= (lastAux + 1) * SECTOR_SIZE;

    // Read in first and last sector, if they are to be partially modified.
    if (!firstAligned) {
        ReadAt(buf, SECTOR_SIZE, firstSector * SECTOR_SIZE);
    }
    if (!lastAligned && (firstSector != lastAux || firstAligned)) {
        ReadAt(&buf[(lastAux - firstSector) * SECTOR_SIZE],
               SECTOR_SIZE, lastAux * SECTOR_SIZE);
    }

    // Copy in the bytes we want to change.
    memcpy(&buf[position - firstSector * SECTOR_SIZE], from, numBytes);

    // Write modified sectors back.
    for (unsigned i = firstSector; i <= lastSector; i++) {
        int sector = hdr->ByteToSector(i * SECTOR_SIZE);
        if(sector == -1) {
            hdr->ReleaseLock();
            return -1;
        }
        // Verifica e inicializa cualquier parte del buffer que no haya sido llenada
        synchDisk->WriteSector(sector,
                               &buf[(i - firstSector) * SECTOR_SIZE]);
    }
    delete [] buf;

    ///> Update the file header if new sectors were added.
    if(fileLength < position + numBytes) {
        hdr->IncrementNumBytes(position + numBytes - fileLength);
        if(sectorhdr != -1)
            hdr->WriteBack(sectorhdr);
    }
    hdr->ReleaseLock();
    return numBytes;
}

/// Return the number of bytes in the file.
unsigned
OpenFile::Length() const
{
    return hdr->FileLength();
}

FileHeader *
OpenFile::GetHeader() {
    return hdr;
}
