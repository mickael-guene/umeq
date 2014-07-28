#include <stdlib.h>
#include <stdio.h>

void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function)
{
    if (function)
        fprintf(stderr, "ASSERT : %s:%u:%s:%s\n", file, line, function, assertion);
    else
        fprintf(stderr, "ASSERT : %s:%u:<UNKNOWN>:%s\n", file, line, assertion);

    abort();
}
