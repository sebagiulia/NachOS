#include "coremap.hh"
#include "threads/system.hh"

Coremap::Coremap(unsigned nitems)
{
    ASSERT(nitems > 0);

    numItems       = nitems;
    bitmap         = new Bitmap(nitems);
    virtualPage    = new unsigned [nitems];
    proccessID     = new unsigned [nitems];
    #ifdef PRPOLICY_FIFO
    fifoPointer  = 0;
    #endif
}

Coremap::~Coremap()
{
    delete bitmap;
    delete [] virtualPage;
    delete [] proccessID;
}

void
Coremap::Mark(unsigned which, unsigned vPage)
{
    bitmap->Mark(which);
    virtualPage[which] = vPage;
    proccessID[which] = currentThread->sid;
}

void
Coremap::Clear(unsigned which)
{
    bitmap->Clear(which);
    virtualPage[which] = -1;
    proccessID[which] = -1;
}

int
Coremap::Find(unsigned vPage)
{
    int pp = bitmap->Find();
    if (pp == -1) return -1;
    else {
        virtualPage[pp] = vPage;
        proccessID[pp] = currentThread->sid;
        return pp;
    }
}

unsigned
Coremap::VirtualPage(unsigned which)
{
    ASSERT(bitmap->Test(which));
    return virtualPage[which];
}

unsigned
Coremap::ProccessID(unsigned which)
{
    ASSERT(bitmap->Test(which));
    return proccessID[which];
}

unsigned
Coremap::NumItems()
{
    return numItems;
}

#ifdef PRPOLICY_FIFO 
unsigned
Coremap::NextFIFOPointer()
{
    unsigned p = fifoPointer;
    fifoPointer = (fifoPointer + 1) % numItems;
    return p;
}
#endif

