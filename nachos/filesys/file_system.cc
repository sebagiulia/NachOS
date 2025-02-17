/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "threads/system.hh"
#include "system.hh"
#include <stdio.h>
#include <string.h>

extern Lock **locksSector;


/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory(NUM_DIR_ENTRIES);
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        char buf[SECTOR_SIZE];
        memset(buf, 0, SECTOR_SIZE);
        for(unsigned i = 0; i < NUM_SECTORS; i++) {
            synchDisk->WriteSector(i, buf);
        }

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE, NUM_DIRECT, FREE_MAP_SECTOR));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE, NUM_DIRECT, DIRECTORY_SECTOR));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(directoryFile);
        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();
            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);
        
    }
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

void
FileSystem::ChangeDirectory(const char *name){
    ASSERT(name != nullptr);
    DEBUG('z', "cambio de directorio a %s\n", name);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(directoryFile);
    int sector = dir->Find(name, true);
    ASSERT(sector != -1);
    delete directoryFile;
    directoryFile = new OpenFile(sector);
    delete dir;
}


/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::Create(const char *name, unsigned initialSize, int dirsector)
{
    
    //create(hola/pepe/hola.txt,0) -> create(pepe/hola.txt,0,hola) -> create(hola.txt,0,hola/pepe)
    ASSERT(name != nullptr);
    ASSERT(initialSize < MAX_FILE_SIZE);

    DEBUG('v', "Creating file %s, size %u\n", name, initialSize);

    //TakeLock();
    OpenFile *d = nullptr;
    Directory *dir = nullptr;
    if(dirsector == -1){
        dir = new Directory(NUM_DIR_ENTRIES);
        dir->FetchFrom(directoryFile);
        d = directoryFile;
    }
    else{
        dir = new Directory(NUM_DIR_ENTRIES, dirsector);
        FileHeader *hdr = nullptr;
        Lock *l = fileSystem->GetLock(dirsector);
        if(!l->IsHeldByCurrentThread())
            l->Acquire();
        if(openFileList->HasKey(dirsector)) {
            hdr = openFileList->GetByKey(dirsector);
        } else {
            hdr = new FileHeader;
            hdr->FetchFrom(dirsector);
            openFileList->AppendKey(hdr, dirsector);
        }
        d = new OpenFile(dirsector, hdr);
        dir->FetchFrom(d);
        if(l->IsHeldByCurrentThread())
            l->Release();
    }
    
    bool success = true;
    if (dirsector == -1 && dir->Find(name) != -1) {
        DEBUG('f', "Can't create file %s, already in directory\n", name);
        success = false;  // File is already in directory.
    } else {
        char *str = new char[strlen(name)+1]; 
        char *path = new char[strlen(name)+1]; 
        strcpy(str, name);
        strcpy(path, name);
        for(int i = 0; i < (int)strlen(str); i++){
            if(str[i] == '/') {
                str[i] = '\0';
                break;
                }
        }
        bool directory = strlen(name) != strlen(str);
        int index = dir->FindIndex(str, directory);
        int sector;
        if(index == -1){
            DEBUG('v', "Creando archivo %s con str %s\n",name,str);
            Bitmap *freeMap = new Bitmap(NUM_SECTORS);
            freeMap->FetchFrom(freeMapFile);
            sector = freeMap->Find();
            // Find a sector to hold the file header.
            if (sector == -1) {
                DEBUG('f', "Can't allocate file header of %s\n", str);
                success = false;  // No free block for file header.
            } else if (!dir->Add(str, sector, directory)) {
                DEBUG('v', "Already in directory %s\n", str);
                success = false;  // No space in directory.
            } else {
                FileHeader *h = new FileHeader;
                success = h->Allocate(freeMap, directory ? 0 : initialSize, NUM_DIRECT, sector);
                // Fails if no space on disk for data.
                if (success) {
                    // Everything worked, flush all changes back to disk.

                    // If the file is a directory, initialize it and store at disk 
                    if(directory) {
                        freeMap->WriteBack(freeMapFile);
                        OpenFile *dfile = new OpenFile(sector, h);
                        Directory *direct = new Directory(1, sector);
                        direct->WriteBack(dfile);
                        delete dfile;
                        delete direct;
                        freeMap->FetchFrom(freeMapFile);
                    }

                    h->WriteBack(sector);
                    freeMap->WriteBack(freeMapFile);
                    dir->WriteBack(d);
                } else {
                    DEBUG('v', "No space for file %s\n", str);
                }
                delete h;
            }
            delete freeMap;
        }
        else{
            sector = (dir->GetRaw())->table[index].sector;
            //DEBUG('v', "Alocando %s en %d con success %d y directory %d\n", str, sector);
        }
        if(success && (directory != 0)){
           // DEBUG('v', "ENTRE\n");
            char *rest = &(path[strlen(str)+1]);
            DEBUG('v', "Alocando el resto %s en %d\n",rest,sector);
            success = Create(rest, initialSize, sector);
        }
        delete [] path;
        delete [] str;
    }
    //ReleaseLock();
    delete dir;
    if(dirsector != -1) delete d;
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    ASSERT(name != nullptr);

    //TakeLock();
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile  *openFile = nullptr;

    DEBUG('f', "Opening file %s by %s\n", name, currentThread->GetName());
    dir->FetchFrom(directoryFile);
    int sector = dir->Find(name);
    if (sector >= 0) {
        FileHeader *hdr = nullptr;
        ///> If the file was opened by other process, 
        ///> the file header is shared.
        #ifdef FILESYS
        Lock *l = fileSystem->GetLock(sector);
        if(!l->IsHeldByCurrentThread())
            l->Acquire();
        if(openFileList->HasKey(sector)) {
            hdr = openFileList->GetByKey(sector);
        } else {
            DEBUG('u',"no hay key para archivo %s con sector %d\n", name, sector);
            ///> If the file wasn´t opened, its file header
            ///> is added to a shared list and can be found there 
            ///> by its sector number at the disk .
            hdr = new FileHeader;
            hdr->FetchFrom(sector);
            openFileList->AppendKey(hdr, sector);
            ASSERT(openFileList->HasKey(sector));
        }
        #endif
        openFile = new OpenFile(sector, hdr);  // `name` was found in directory.
        if(l->IsHeldByCurrentThread())
            l->Release();
    } else {
        DEBUG('f', "File %s not found\n", name);
    }
    //ReleaseLock();
    delete dir;
    return openFile;  // Return null if not found.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *name, FileHeader *hdr, int hsector)
{
    //TakeLock();
    if(hdr == nullptr) { ///> Remove called from FileSystem  
        ASSERT(name != nullptr);
        Directory *dir = new Directory(NUM_DIR_ENTRIES);
        char *path = new char[strlen(name)+1];
        strcpy(path, name);
        int k;
        for(k = (int)strlen(path) - 1; k >= 0; k--){
            if(path[k] == '/'){
                path[k] = '\0';
                break;
            }
        }
        dir->FetchFrom(directoryFile);
        OpenFile *d = directoryFile;
        if(k >= 0){ //hay barras
            int headersector = dir->Find(path, true);
            if (headersector == -1) {
                ReleaseLock();
                delete dir;
                return false;  // file not found
            }
            Lock *l = GetLock(headersector);
            if(!l->IsHeldByCurrentThread())
                l->Acquire();
            FileHeader *hdr1;
            if(openFileList->HasKey(headersector)) {
                hdr1 = openFileList->GetByKey(headersector);
            } else {
                DEBUG('u',"no esta el sector %d para header de %s con path %s\n", headersector, name, path);
                hdr1 = new FileHeader;
                hdr1->FetchFrom(headersector);
                openFileList->AppendKey(hdr1, headersector);
                ASSERT(openFileList->HasKey(headersector));
            }
            d = new OpenFile(headersector, hdr1);
            dir->FetchFrom(d);
            if(l->IsHeldByCurrentThread())
                l->Release();
            path = &(path[strlen(path)+1]);
        }
        int sector = dir->Find(path);
        if (sector == -1) {
           ReleaseLock();
           delete dir;
           return false;  // file not found
        }

        FileHeader *fileH = nullptr;
        #ifdef FILESYS
            Lock *l = fileSystem->GetLock(sector);
            if(!l->IsHeldByCurrentThread())
                l->Acquire();
            if(openFileList->HasKey(sector)) { 
                ///> If the file is still opened by other process we do not remove it from disk yet
                ///> but we mark it with [removed] and remove its name from the directory.
                DEBUG('h', "Remove requested by %s but file %s still opened by "
                            "other processes, removing from directory.\n", currentThread->GetName(), name);
                fileH = openFileList->GetByKey(sector);
                fileH->removed = true;
                dir->Remove(path);
                dir->WriteBack(d);    // Flush to disk.   
            }
            if(l->IsHeldByCurrentThread())
                l->Release(); 
        #endif

        if(fileH == nullptr) { // File no opened, we remove from disk and directory
            DEBUG('h', "Removing file %s from disk.\n", name);
            
            fileH = new FileHeader;
            fileH->FetchFrom(sector);

            Bitmap *freeMap = new Bitmap(NUM_SECTORS);
            freeMap->FetchFrom(freeMapFile);

            dir->Remove(path);
            DEBUG('r',"borrando path %s\n", path);
            fileH->Deallocate(freeMap);  // Remove data blocks.
            freeMap->Clear(sector);      // Remove header block.

            dir->WriteBack(d);    // Flush to disk.
            freeMap->WriteBack(freeMapFile);  // Flush to disk.
            delete fileH;
            delete freeMap;
        }
        if(k >= 0) delete d;
        //delete path;
        delete dir;
    } else { ///> Remove called from ~OpenFile 
    #ifdef FILESYS
        if(hdr->removed == true) {
            DEBUG('h', "Removing file after last close.\n");
            /// If file was removed before, the file is removed from disk too.
            Bitmap *freeMap = new Bitmap(NUM_SECTORS);
            freeMap->FetchFrom(freeMapFile);

            hdr->Deallocate(freeMap);  // Remove data blocks.
            freeMap->Clear(hsector);      // Remove header block.

            freeMap->WriteBack(freeMapFile);  // Flush to disk.
            delete freeMap;
        } else {
            hdr->WriteBack(hsector);
        }
        DEBUG('h', "Queriendo sacar sector %d\n",hsector);
        ASSERT(openFileList->HasKey(hsector));
        openFileList->RemoveByKey(hsector);
        hdr->ReleaseLock();
        delete hdr;
    #endif
    }
    //ReleaseLock();
    return true;

}

/// List all the files in the file system directory.
void
FileSystem::List(char *name)
{
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    //TakeLock();
    dir->FetchFrom(directoryFile);
    if(name == nullptr){
        dir->List();
    }
    else{
        int sector = dir->Find(name,true);
        if(sector == -1){
            printf("Directory %s doesn't exist.\n", name);
        }
        else{
            FileHeader *hdr = nullptr;
            Lock *l = fileSystem->GetLock(sector);
            if(!l->IsHeldByCurrentThread())
                l->Acquire();
            if(openFileList->HasKey(sector)) {
                hdr = openFileList->GetByKey(sector);
        } else {
            hdr = new FileHeader;
            hdr->FetchFrom(sector);
            openFileList->AppendKey(hdr, sector);
        }
        OpenFile *d = new OpenFile(sector, hdr);
        dir->FetchFrom(d);
        if(l->IsHeldByCurrentThread())
            l->Release(); 
        dir->List();
        delete d;
        }
    }
    //ReleaseLock();
    delete dir;
}


static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value) {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check.")) {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    //TakeLock();
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    //ReleaseLock();
    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory(NUM_DIR_ENTRIES);

    //TakeLock();
    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");
    //ReleaseLock();

    delete freeMap;
    delete dir;
    delete dirH;
    delete bitH;
}

OpenFile *
FileSystem::GetFreeMapFile() {
    return freeMapFile;
}

OpenFile *
FileSystem::GetDirectoryFile() {
    return directoryFile;
}

void
FileSystem::TakeLock() {

    if(!lockFS->IsHeldByCurrentThread()) /// Prevent double acquire
        lockFS->Acquire();
}

void
FileSystem::ReleaseLock() { 
    if(lockFS->IsHeldByCurrentThread()) /// Prevent double release
        lockFS->Release();
}

Lock *
FileSystem::GetLock(int sector) {
    TakeLock();
    if(locksSector[sector] == nullptr){
        locksSector[sector] = new Lock("Directory lock");
    }
    ReleaseLock();
    return locksSector[sector];
}
