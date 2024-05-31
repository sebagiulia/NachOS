#include "coremap.hh"
#include "threads/system.hh"

Coremap::Coremap(unsigned nitems)
{
    ASSERT(nitems > 0);

    numItems       = nitems;
    bitmap         = new Bitmap(nitems);
    virtualPage    = new unsigned [nitems];
    proccessID     = new unsigned [nitems];
}

Coremap::~Coremap()
{
    delete bitmap;
    delete [] virtualPage;
    delete [] proccessID;
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
