#include "channel.hh"
#include "system.hh"
#include <stdio.h>
#include <string.h>
Channel::Channel(const char *debugName)
{
    name = debugName;

    nombre_lock = new char[9 + strlen(debugName)];
    sprintf(nombre_lock, "lock of %s", debugName);
    l = new Lock(nombre_lock);

    nombre_read = new char[14 + strlen(debugName)];
    sprintf(nombre_read, "cond_read of %s", debugName);
    cond_read = new Condition(nombre_read, l);

    nombre_write = new char[16 + strlen(debugName)];
    sprintf(nombre_write, "cond_write of %s", debugName);
    cond_write = new Condition(nombre_write, l);

    msj = -1;
    concurrido = 0;
    waiting = false;
}

const char *
Channel::GetName() const
{
    return name;
}

Channel::~Channel()
{
    delete cond_read;
    delete cond_write;
    delete l;
    delete [] nombre_read;
    delete [] nombre_lock;
    delete [] nombre_write;
}

void Channel::Receive(int *message)
{
  l->Acquire();
  concurrido++;
  cond_read->Signal();
  while(!waiting){
    cond_write->Wait();
  }
  *message = msj;
  DEBUG('c', "Thread \"%s\" receiving %d on channel \"%s\" \n", currentThread->GetName(), msj, name);
  concurrido--;
  waiting = false;
  cond_read->Signal();
  l->Release();
}

void Channel::Send(int message)
{
  l->Acquire();
  while(waiting || !concurrido){
    cond_read->Wait();
  }
  msj = message;
  DEBUG('c', "Thread \"%s\" sending %d on channel \"%s\" \n", currentThread->GetName(), msj, name);
  waiting = true;
  cond_write->Signal();
  l->Release();
}
