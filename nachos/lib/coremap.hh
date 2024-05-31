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

    /// Return the index of a clear bit in the bitmap, and as a side effect, set the bit.
    ///
    /// If no bits are clear, return -1.
    int Find();

private:

    unsigned numItems;

    Bitmap *bitmap;

    unsigned *virtualPageNum;

    unsigned *proccessID;

};

#endif
