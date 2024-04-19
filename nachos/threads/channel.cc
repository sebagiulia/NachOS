#include "channel.hh"
#include "system.hh"

Channel::Channel(const char *debugName)
{
    name = debugName;
    l = new Lock("lockChannel");
    cond_read = new Condition("cond_read", l);
    cond_write = new Condition("cond_write", l);
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
