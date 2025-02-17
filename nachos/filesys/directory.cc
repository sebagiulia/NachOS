/// Routines to manage a directory of file names.
///
/// The directory is a table of fixed length entries; each entry represents a
/// single file, and contains the file name, and the location of the file
/// header on disk.  The fixed size of each directory entry means that we
/// have the restriction of a fixed maximum size for file names.
///
/// The constructor initializes an empty directory of a certain size; we use
/// ReadFrom/WriteBack to fetch the contents of the directory from disk, and
/// to write back any modifications back to disk.
///
/// Also, this implementation has the restriction that the size of the
/// directory cannot expand.  In other words, once all the entries in the
/// directory are used, no more files can be created.  Fixing this is one of
/// the parts to the assignment.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "directory.hh"
#include "directory_entry.hh"
#include "file_header.hh"
#include "lib/utility.hh"
#include "threads/system.hh"

#include <stdio.h>
#include <string.h>
#include <cstring>
#include <cstdio>
/// Initialize a directory; initially, the directory is completely empty.  If
/// the disk is being formatted, an empty directory is all we need, but
/// otherwise, we need to call FetchFrom in order to initialize it from disk.
///
/// * `size` is the number of entries in the directory.
Directory::Directory(unsigned size, unsigned sect)
{
    ASSERT(size > 0);
    raw.table = new DirectoryEntry [size];
    raw.tableSize = size;
    for (unsigned i = 0; i < raw.tableSize; i++) {
        raw.table[i].inUse = false;
        raw.table[i].sector = 0;
        raw.table[i].sector = 0;
        memset(raw.table[i].name, 0, FILE_NAME_MAX_LEN + 1);
        raw.table[i].isDirectory = false;
    }
    extraEntry = nullptr;
    sector = sect;
}

/// De-allocate directory data structure.
Directory::~Directory()
{
    ReleaseLock();
    delete [] raw.table;
}

/// Read the contents of the directory from disk.
///
/// * `file` is file containing the directory contents.
void
Directory::FetchFrom(OpenFile *file)
{
    ASSERT(file != nullptr);
    TakeLock();
    file->ReadAt((char *) &raw.tableSize, sizeof(unsigned), 0);
    
    /// Maybe the raw.table is smaller than the disk version, we resize it.
    delete [] raw.table;
    raw.table = new DirectoryEntry [raw.tableSize];
    for (unsigned i = 0; i < raw.tableSize; i++) {
        raw.table[i].inUse = false;
        raw.table[i].sector = 0;
        raw.table[i].sector = 0;
        memset(raw.table[i].name, 0, FILE_NAME_MAX_LEN + 1);
        raw.table[i].isDirectory = false;
    }
    file->ReadAt((char *) raw.table,
                 raw.tableSize * sizeof (DirectoryEntry), sizeof(unsigned));
    //ReleaseLock();
}


/// Write any modifications to the directory back to disk.
///
/// * `file` is a file to contain the new directory contents.
void
Directory::WriteBack(OpenFile *file)
{
    ASSERT(file != nullptr);
    //TakeLock();
    unsigned tz = raw.tableSize;
    
    if(extraEntry != nullptr) raw.tableSize++; /// If there is another entry, update tableSize

    file->WriteAt((char *) &raw.tableSize, sizeof(unsigned), 0);
    
    file->WriteAt((char *) raw.table,
                  tz * sizeof (DirectoryEntry), sizeof(unsigned));
    
    if(extraEntry != nullptr) { ///> Write the extra entry at the last position in the table in disk
        file->WriteAt((char *)extraEntry, sizeof (DirectoryEntry),
                    tz * sizeof (DirectoryEntry) + sizeof(unsigned));
        delete extraEntry;
        extraEntry = nullptr;
        synchDisk->WriteSector(sector,
                                (char *) file->GetHeader()->GetRaw()); /// Write back the changes (numBytes) 
                                                                       /// to the directory header
    }
    ReleaseLock();
}

/// Look up file name in directory, and return its location in the table of
/// directory entries.  Return -1 if the name is not in the directory.
///
/// * `name` is the file name to look up.
int
Directory::FindIndex(const char *name, bool directory)
{
    ASSERT(name != nullptr);
    ASSERT(strlen(name) != 0);
    TakeLock();
    for (unsigned i = 0; i < raw.tableSize; i++) {
        if(raw.table[i].inUse){
            DEBUG('v', "aca hay %s con sector %d, iteracion %d. \n", raw.table[i].name, raw.table[i].sector, i);
        }
        if (raw.table[i].inUse
              && !strncmp(raw.table[i].name, name, FILE_NAME_MAX_LEN) && raw.table[i].isDirectory == directory ) {
            ReleaseLock();
            return i;
        }
    }
    ReleaseLock();
    return -1;  // name not in directory
}

/// Look up file name in directory, and return the disk sector number where
/// the file's header is stored.  Return -1 if the name is not in the
/// directory.
///
/// * `name` is the file name to look up.
int
Directory::Find(const char *name, bool directory) // hola/pepe/hola.txt --> hola.Find(/pepe/hola.txt);
{   
    ASSERT(name != nullptr);
    ASSERT(strlen(name) != 0);
    char *str = new char[strlen(name) +1]; 
    char *path = new char[strlen(name)+1]; 
    strcpy(str, name);
    strcpy(path, name);
    for(int i = 0; i < (int)strlen(str); i++){
        if(str[i] == '/') {
            str[i] = '\0';
            break;
            }
    }
    int i;
    DEBUG('v', "Buscando %s largo %d\n", str, strlen(str));
    if(strlen(str) == strlen(name)){
        i = FindIndex(name, directory);
        if(i != -1) i = raw.table[i].sector;
    }
    else{
        DEBUG('v', "Buscando directorio %s\n", str);
        i = FindIndex(str, true);
        if(i == -1){
            DEBUG('v', "No encontre el directorio\n");
            delete [] str;
            delete [] path;
            return -1;
        }
        int sect = raw.table[i].sector;
        Lock *l = fileSystem->GetLock(sect);
        if(!l->IsHeldByCurrentThread())
            l->Acquire(); 
        FileHeader *hdr;
        if(openFileList->HasKey(sect)) {
            hdr = openFileList->GetByKey(sect);
        } else {
            hdr = new FileHeader;
            hdr->FetchFrom(sect);
            openFileList->AppendKey(hdr, sect);
        }
        OpenFile *dir = new OpenFile(sect, hdr);
        Directory *d = new Directory(1);
        d->FetchFrom(dir);
        if(l->IsHeldByCurrentThread())
            l->Release(); 
        char *rest = &(path[strlen(str)+1]);
        i = d->Find(rest,directory);
        delete d;
        delete dir;
    }
    delete [] str;
    delete [] path;
    return i;
}

/// Add a file into the directory.  Return true if successful; return false
/// if the file name is already in the directory, or if the directory is
/// completely full, and has no more space for additional file names.
///
/// * `name` is the name of the file being added.
/// * `newSector` is the disk sector containing the added file's header.
bool
Directory::Add(const char *name, int newSector, bool directory)
{
    ASSERT(name != nullptr);
    TakeLock();
    if (FindIndex(name) != -1) {
        return false;
    }
    //hola/pepe/hola.txt raiz.Add(hola,true) hola.Add(pepe, true) pepe.Add(hola.txt,false)
    for (unsigned i = 0; i < raw.tableSize; i++) {
        if (!raw.table[i].inUse) {
            raw.table[i].inUse = true;
            raw.table[i].isDirectory = directory;
            strncpy(raw.table[i].name, name, FILE_NAME_MAX_LEN);
            raw.table[i].sector = newSector;
            ReleaseLock();
            return true;
        }
    }

    ///> If there isn´t no more space, we store it in the temporaly variable
    ///> until the write back instruccion.
    DEBUG('b', "Expanding directory for file %s\n", name);
    extraEntry = new DirectoryEntry;
    extraEntry->inUse = true;
    extraEntry->sector = newSector;
    extraEntry->isDirectory = directory;
    strncpy(extraEntry->name, name, FILE_NAME_MAX_LEN);
    ReleaseLock();
    return true;
}

/// Remove a file name from the directory.   Return true if successful;
/// return false if the file is not in the directory.
///
/// * `name` is the file name to be removed.
bool
Directory::Remove(const char *name)
{
    ASSERT(name != nullptr);

    TakeLock();
    int i = FindIndex(name);
    if (i == -1) {
        ReleaseLock();
        return false;  // name not in directory
    }
    raw.table[i].inUse = false;
    ReleaseLock();
    return true;
}

/// List all the file names in the directory.
void
Directory::List() const
{
    for (unsigned i = 0; i < raw.tableSize; i++) {
        if (raw.table[i].inUse) {
            printf("%s\n", raw.table[i].name);
        }
    }
}

/// List all the file names in the directory, their `FileHeader` locations,
/// and the contents of each file.  For debugging.
void
Directory::Print() const
{
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (unsigned i = 0; i < raw.tableSize; i++) {
        if (raw.table[i].inUse) {
            printf("\nDirectory entry:\n"
                   "    name: %s\n"
                   "    sector: %u\n",
                   raw.table[i].name, raw.table[i].sector);
            hdr->FetchFrom(raw.table[i].sector);
            hdr->Print(nullptr);
        }
    }
    printf("\n");
    delete hdr;
}

const RawDirectory *
Directory::GetRaw() const
{
    return &raw;
}

void 
Directory::TakeLock(){
    Lock *lock = fileSystem->GetLock(sector);
    if(!lock->IsHeldByCurrentThread()) /// Prevent double acquire
        lock->Acquire();
}

void 
Directory::ReleaseLock(){
    Lock *lock = fileSystem->GetLock(sector);
    if(lock->IsHeldByCurrentThread()) /// Prevent double release
        lock->Release();
}