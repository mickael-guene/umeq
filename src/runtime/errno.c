#include <errno.h>

//FIXME : need a thread version

static int errno_in;

int * __errno_location()
{
    return &errno_in;
}
