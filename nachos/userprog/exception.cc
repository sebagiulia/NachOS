/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.
#include "args.hh"
#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "exception.hh"
#include <stdio.h>
#include "executable.hh"

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

static void PageFaultHandler(ExceptionType et){
  int virtualAddr = machine->ReadRegister(BAD_VADDR_REG);
  //DEBUG('w', "Page Fault Exception at addr %d\n", virtualAddr);
  int page = virtualAddr / PAGE_SIZE;
  ASSERT(currentThread->space->LoadTLB(unsigned(page)));
}

static void ReadOnlyHandler(ExceptionType et){
  DEBUG('e', "Read only exception\n");

  while(!currentThread->childList->IsEmpty()) {
    DEBUG('e', "Joining childs from thread %s\n", currentThread->GetName());
    (currentThread->childList->Pop())->Join();
  }

  currentThread->Finish(-1);
}


static void InitNewThread(void *args)
{
  #ifdef SWAP
    char *swapFileName = new char[7];
    sprintf(swapFileName, "SWAP.%u", currentThread->sid);
    fileSystem->Create(swapFileName, currentThread->space->NumPages()*PAGE_SIZE);
    delete [] swapFileName;
  #endif
  currentThread->space->InitRegisters();
	currentThread->space->RestoreState();
  if(args != nullptr){
      unsigned i = WriteArgs((char**)args);
      int sp = machine->ReadRegister(STACK_REG);
      machine->WriteRegister(4, i);
      machine->WriteRegister(5, sp);
      machine->WriteRegister(STACK_REG, sp-24);
      }
  else{
    machine->WriteRegister(4, 0);
  }
	machine->Run();
  ASSERT(false);
}

unsigned StartNewProcess(OpenFile *exec, char **args)
{
  Thread *newThread = new Thread("child", true);
  unsigned sid = processesTable->Add(newThread);
  newThread->sid = sid;
  currentThread->childList->Append(newThread);
	AddressSpace *space = new AddressSpace(exec);
  newThread->space = space;
  newThread->Fork(InitNewThread, args);
  return sid;
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);  // Return error code.
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);  // Return error code.
                break;
            }

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);
            if (!fileSystem->Create(filename, 0)){
                DEBUG('e', "Error: failed to create file `%s`.\n", filename);
                machine->WriteRegister(2, -1);  // Return error code.
            }
            else {
                DEBUG('e', "Created file `%s`.\n", filename);
                machine->WriteRegister(2, 0); //Return success code.
            }
            break;
        }

        case SC_OPEN: {
          int filenameAddr = machine->ReadRegister(4);
          if (filenameAddr == 0) {
              DEBUG('e', "Error: address to filename string is null.\n");
              machine->WriteRegister(2, -1);  // Return error code.
              break;
          }

          char filename[FILE_NAME_MAX_LEN + 1];
          if (!ReadStringFromUser(filenameAddr,
                                  filename, sizeof filename)) {
              DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                    FILE_NAME_MAX_LEN);
              machine->WriteRegister(2, -1);  // Return error code.
              break;
          }

          DEBUG('e', "`Open` requested for file `%s`.\n", filename);
          OpenFile* openFile = fileSystem->Open(filename);
          if (!openFile){
              DEBUG('e', "Error: failed to open file `%s`.\n", filename);
              machine->WriteRegister(2, -1);  // Return error code.
              break;
          }
          else {
              DEBUG('e', "Opened file `%s`.\n", filename);
          }

          int fid = currentThread->openFilesTable->Add(openFile);
          if (fid == -1) {
              DEBUG('u', "Error: failed to add open file `%s` to the open files table.\n", filename);
              delete openFile;
              DEBUG('e', "Closed file %u.\n", filename);
              machine->WriteRegister(2, -1);  // Return error code.
          }
          else {
              DEBUG('e', "Added file `%s` to the open files table.\n", filename);
              machine->WriteRegister(2, fid); //Return success code.
          }

          break;
        }

        case SC_WRITE: {
          int bufferAddr = machine->ReadRegister(4);
          int size = machine->ReadRegister(5);
          int fid = machine->ReadRegister(6);

          if (bufferAddr == 0) {
              DEBUG('e', "Error: address to buffer is null.\n");
              machine->WriteRegister(2, -1);  // Return error code.
              break;
          }

          if (size <= 0) {
            DEBUG('e', "Error: invalid size.\n");
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }

          if (fid < 0) {
            DEBUG('e', "Error: invalid file id.\n");
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }

          if (fid == CONSOLE_INPUT) {
            DEBUG('e', "Error: can`t write on console input.\n");
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }

          char buffer[size+1];
          ReadBufferFromUser(bufferAddr, buffer, size);
          buffer[size] = '\0';
          if (fid == CONSOLE_OUTPUT) {
            DEBUG('v', "Escribiendo en consola size %d buffer %s\n", size, buffer);
            for(int i = 0; i < size; i++){
               synchConsole->PutChar(buffer[i]);
            }
            DEBUG('e', "`Write` done on synch_console `%u`.\n", fid);
            machine->WriteRegister(2, 0); // Return success code.
            break;
          }


          if(!currentThread->openFilesTable->HasKey(fid)) {
            DEBUG('e', "Error: file id `%u` not found.\n", fid);
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }
          else {
            OpenFile *o = currentThread->openFilesTable->Get(fid);
            if (o == nullptr) {
              DEBUG('e', "Error: file not opened.\n");
              machine->WriteRegister(2, -1);  // Return error code.
              break;
            }
            int cant = o->Write(buffer,size);
            if (cant <= 0) {
              DEBUG('e', "Error: couldn`t write on file.\n");
              machine->WriteRegister(2, -1);  // Return error code.
              break;
            }
            DEBUG('e', "`Write` done on file `%u`.\n", fid);
            machine->WriteRegister(2, 0); //Return success code.
            break;
          }

        }

        case SC_READ: {
          int bufferAddr = machine->ReadRegister(4);
          int size = machine->ReadRegister(5);
          int fid = machine->ReadRegister(6);

          if (bufferAddr == 0) {
              DEBUG('e', "Error: address to buffer is null.\n");
              machine->WriteRegister(2, -1);  // Return error code.
              break;
          }

          if (size <= 0) {
            DEBUG('e', "Error: invalid size.\n");
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }

          if (fid < 0) {
            DEBUG('e', "Error: invalid file id.\n");
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }

          if (fid == CONSOLE_OUTPUT) {
            DEBUG('e', "Error: can`t read on console output.\n");
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }

          char buffer[size];

          if (fid == CONSOLE_INPUT) {
            int i = 0;
            for(; i<size; i++){
              buffer[i] = synchConsole->GetChar();
              if(buffer[i] == '\n') {
                i++;
                break;
              }
            }

            WriteBufferToUser(buffer, bufferAddr, i);
            machine->WriteRegister(2, i);
            break;
          }

          if(!currentThread->openFilesTable->HasKey(fid)) {
            DEBUG('e', "Error: file id `%u` not found.\n", fid);
            machine->WriteRegister(2, -1);  // Return error code.
            break;
          }
          else {
            OpenFile *o = currentThread->openFilesTable->Get(fid);
            if (o == nullptr) {
              DEBUG('e', "Error: file not opened.\n");
              machine->WriteRegister(2, -1);  // Return error code.
              break;
            }
            int cant = o->Read(buffer,size);
            if (cant <= 0) {
              DEBUG('e', "Error: couldn`t read on file.\n");
              machine->WriteRegister(2, cant);  // Return error code.
              break;
            }
            DEBUG('e', "`Read` done on file `%u`.\n", fid);
            WriteBufferToUser(buffer, bufferAddr, size);
            DEBUG('e',"buffer = %s.\n", buffer);
            machine->WriteRegister(2, cant); //Return success code.
            break;
          }
        }

        case SC_REMOVE: {
             int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);  // Return error code.
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);  // Return error code.
                break;
            }

           DEBUG('e', "`Remove` requested for file `%s`.\n", filename);
            if (!fileSystem->Remove(filename)){
                DEBUG('e', "Error: file `%s` not found.\n", filename);
                machine->WriteRegister(2, -1);  // Return error code.
            }
            else {
                DEBUG('e', "Removed file `%s`.\n", filename);
                machine->WriteRegister(2, 0); //Return success code.
            }
            break;

        }

        case SC_CLOSE: {
            int fid = machine->ReadRegister(4);

            DEBUG('e', "`Close` requested for id %i.\n", fid);
            if (fid < 0) {
              DEBUG('e', "Error: invalid file id.\n");
              machine->WriteRegister(2, -1);  // Return error code.
              break;
            }

            if(fid == 0 || fid == 1) {
                DEBUG('e', "Error: file id `%i` cannot be closed.\n", fid);
                machine->WriteRegister(2,-1);
                break;
            }
            if(!currentThread->openFilesTable->HasKey(fid)) {
                DEBUG('e', "Error: file id `%i` not found.\n", fid);
                machine->WriteRegister(2, -1);  // Return error code.
                break;
            }
            else {
              delete currentThread->openFilesTable->Get(fid);
              DEBUG('e', "Closed file id `%i`.\n", fid);
              currentThread->openFilesTable->Remove(fid);
              DEBUG('e', "Removed file id `%i` from the open files table.\n", fid);
              machine->WriteRegister(2, 0); // Return success code.
            }
            break;
        }

	      case SC_EXEC: {

            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);  // Return error code.
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);  // Return error code.
                break;
            }

            OpenFile *executable = fileSystem->Open(filename);
            if(executable == nullptr) {
              DEBUG('e', "Unable to execute file %s", filename);
              machine->WriteRegister(2,-1);
              break;
            }

            Executable exe (executable);

            if(!exe.CheckMagic()){
              DEBUG('e', "File %s is not noff\n", filename);
              machine->WriteRegister(2,-1);
              break;
            }


            unsigned spaceid = StartNewProcess(executable,nullptr);
            DEBUG('e', "Success: File %s executed.\n", filename);
            machine->WriteRegister(2, spaceid);
            break;
	}

      case SC_EXEC2: {
                int filenameAddr = machine->ReadRegister(4);
                int argvAddr = machine->ReadRegister(5);
                if (filenameAddr == 0) {
                    DEBUG('e', "Error: address to filename string is null.\n");
                    machine->WriteRegister(2, -1);  // Return error code.
                    break;
                }
                if (argvAddr == 0) {
                    DEBUG('e', "Error: address to argv is null.\n");
                    machine->WriteRegister(2, -1);  // Return error code.
                    break;
                }

                char filename[FILE_NAME_MAX_LEN + 1];
                if (!ReadStringFromUser(filenameAddr,
                                        filename, sizeof filename)) {
                    DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                          FILE_NAME_MAX_LEN);
                    machine->WriteRegister(2, -1);  // Return error code.
                    break;
                }

                OpenFile *executable = fileSystem->Open(filename);
                if(executable == nullptr) {
                  DEBUG('e', "Unable to execute file %s", filename);
                  machine->WriteRegister(2,-1);
                  break;
                }

                Executable exe (executable);

                if(!exe.CheckMagic()){
                  DEBUG('e', "File %s is not noff\n", filename);
                  machine->WriteRegister(2,-1);
                  break;
                }

                char **args = SaveArgs(argvAddr);

                unsigned spaceid = StartNewProcess(executable, args);
                DEBUG('e', "Success: File %s executed.\n", filename);
                machine->WriteRegister(2, spaceid);
                break;
      }

  case SC_JOIN: {
            SpaceId sid = machine->ReadRegister(4);

            if(sid < 0) {
              DEBUG('e', "Invalid Space Identifier %d\n",sid);
              machine->WriteRegister(2, -1);
              break;
            }

            if(processesTable->HasKey(sid)) {
              Thread *th = processesTable->Get(sid);
              currentThread->childList->Remove(th);
              DEBUG('e', "Thread %s Join to thread %s.\n", currentThread->GetName(), th->GetName());
              int exitstatus = 0;
              th->Join(&exitstatus);
              machine->WriteRegister(2, exitstatus);
            } else {
              DEBUG('e', "Invalid space id.\n");
              machine->WriteRegister(2, -1);
            }
            break;

  }

        case SC_EXIT: {
            int status = machine->ReadRegister(4);
            DEBUG('e', "`Exit` requested from thread `%s` with status %d.\n",currentThread->GetName(), status);

            while(!currentThread->childList->IsEmpty()) {
              DEBUG('e', "Removing childs from thread %s\n", currentThread->GetName());
              (currentThread->childList->Pop())->Join();
            }
            currentThread->Finish(status);
            break;
        }
        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}




/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &PageFaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &ReadOnlyHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
