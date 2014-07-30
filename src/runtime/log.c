#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

void debug(const char *format, ...) {
    va_list args;

    va_start(args, format);
    fprintf(stderr, "DEBUG: ");
    vfprintf(stderr, format, args);
    va_end(args) ;
}

void info(const char *format, ...) {
    va_list args;

    va_start(args, format);
    fprintf(stderr, "INFO: ");
    vfprintf(stderr, format, args);
    va_end(args) ;
}

void warning(const char *format, ...) {
    va_list args;

    va_start(args, format);
    fprintf(stderr, "WARNING: ");
    vfprintf(stderr, format, args);
    va_end(args) ;
}

void error(const char *format, ...) {
    va_list args;

    va_start(args, format);
    fprintf(stderr, "ERROR: ");
    vfprintf(stderr, format, args);
    va_end(args) ;
}

void fatal_internal(const char *format, ...) {
    va_list args;

    va_start(args, format);
    fprintf(stderr, "FATAL: ");
    vfprintf(stderr, format, args);
    va_end(args) ;
}
