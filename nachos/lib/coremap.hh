#ifndef NACHOS_LIB_COREMAP__HH
#define NACHOS_LIB_COREMAP__HH

#include "bitmap.hh"

class Coremap {
public:

    /// Initialize a coremap with `nitems` items;
    Coremap(unsigned nitems);

    /// Uninitialize a coremap.
    ~Coremap();

    /// Clear the “nth” bit of the bitmap.
    void Clear(unsigned which);

    /// Return the index of a clear bit in the bitmap, and as a side effect,
    /// set the bit, the current process ID, and the corresponding virtual page.
    ///
    /// If no bits are clear, return -1.
    int Find(unsigned vPage);

private:

    unsigned numItems;

    Bitmap *bitmap;

    unsigned *virtualPage;

    unsigned *proccessID;

};

#endif
