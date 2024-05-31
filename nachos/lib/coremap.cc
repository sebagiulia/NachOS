#include "coremap.hh"

Coremap::Coremap(unsigned nitems)
{
    ASSERT(nitems > 0);

    numItems       = nitems;
    bitmap         = new Bitmap(nitems);
    virtualPageNum = new unsigned [nitems];
    proccessID     = new unsigned [nitems];
}

Coremap::~Coremap()
{
    delete bitmap;
    delete [] virtualPageNum;
    delete [] proccessID;
}

void
Coremap::Clear(unsigned which)
{
    bitmap->Clear(which);
}

int
Coremap::Find()
{
    return bitmap->Find();
}
