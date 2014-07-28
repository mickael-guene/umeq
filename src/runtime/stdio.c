#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

FILE *stdin;
FILE *stdout;
FILE *stderr;

int putchar(int c)
{
	return write(2, &c, 1);
}

static void printchar(char **str, int c)
{
	//extern int putchar(int c);

	if (str) {
		**str = c;
		++(*str);
	}
	else (void)putchar(c);
}

#define PAD_RIGHT 1
#define PAD_ZERO 2

static int prints(char **out, const char *string, int width, int pad)
{
	register int pc = 0, padchar = ' ';

	if (width > 0) {
		register int len = 0;
		register const char *ptr;
		for (ptr = string; *ptr; ++ptr) ++len;
		if (len >= width) width = 0;
		else width -= len;
		if (pad & PAD_ZERO) padchar = '0';
	}
	if (!(pad & PAD_RIGHT)) {
		for ( ; width > 0; --width) {
			printchar (out, padchar);
			++pc;
		}
	}
	for ( ; *string ; ++string) {
		printchar (out, *string);
		++pc;
	}
	for ( ; width > 0; --width) {
		printchar (out, padchar);
		++pc;
	}

	return pc;
}

/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 32

static int printi(char **out, long i, int b, int sg, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	register char *s;
	register long t, neg = 0, pc = 0;
	register unsigned long u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN-1;
	*s = '\0';

	while (u) {
		t = u % b;
		if( t >= 10 )
			t += letbase - '0' - 10;
		*--s = t + '0';
		u /= b;
	}

	if (neg) {
		if( width && (pad & PAD_ZERO) ) {
			printchar (out, '-');
			++pc;
			--width;
		}
		else {
			*--s = '-';
		}
	}

	return pc + prints (out, s, width, pad);
}

static int print(char **out, const char *format, va_list args )
{
	register int width, pad;
	register int pc = 0;
	char scr[2];

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = pad = 0;
			if (*format == '\0') break;
			if (*format == '%') goto out;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			for ( ; *format >= '0' && *format <= '9'; ++format) {
				width *= 10;
				width += *format - '0';
			}

			if( *format == 'l' ) {
				++format;
				if( *format == 'd' ) {
					pc += printi (out, va_arg( args, long ), 10, 1, width, pad, 'a');
					continue;
				}
				if( *format == 'x' ) {
					pc += printi (out, va_arg( args, unsigned long ), 16, 0, width, pad, 'a');
					continue;
				}
				if( *format == 'X' ) {
					pc += printi (out, va_arg( args, unsigned long ), 16, 0, width, pad, 'A');
					continue;
				}
				if( *format == 'u' ) {
					pc += printi (out, va_arg( args, unsigned long ), 10, 0, width, pad, 'a');
					continue;
				}
			}

			if( *format == 's' ) {
				register char *s = (char *)(long) va_arg( args, long );
				pc += prints (out, s?s:"(null)", width, pad);
				continue;
			}
			if( *format == 'd' ) {
				pc += printi (out, va_arg( args, int ), 10, 1, width, pad, 'a');
				continue;
			}
			if( *format == 'x' ) {
				pc += printi (out, va_arg( args, unsigned int ), 16, 0, width, pad, 'a');
				continue;
			}
			if( *format == 'X' ) {
				pc += printi (out, va_arg( args, unsigned int ), 16, 0, width, pad, 'A');
				continue;
			}
			if( *format == 'u' ) {
				pc += printi (out, va_arg( args, unsigned int ), 10, 0, width, pad, 'a');
				continue;
			}
			if( *format == 'c' ) {
				/* char are converted to int then pushed on the stack */
				scr[0] = (char)va_arg( args, int );
				scr[1] = '\0';
				pc += prints (out, scr, width, pad);
				continue;
			}
		}
		else {
		out:
			printchar (out, *format);
			++pc;
		}
	}
	if (out) **out = '\0';
	va_end( args );
	return pc;
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
	return print( 0, format, args );
}

int printf(const char *format, ...)
{
        va_list args;

        va_start( args, format );
        return print( 0, format, args );
}

int __printf_chk(int flag, const char *format, ...)
{
        va_list args;

        va_start( args, format );
        return print( 0, format, args );
}

int fprintf(FILE *__restrict __stream, __const char *format, ...)
{
        va_list args;

        va_start( args, format );
        return print( 0, format, args );
}

int __fprintf_chk(FILE *__restrict __stream, int flag, __const char *format, ...)
{
        va_list args;

        va_start( args, format );
        return print( 0, format, args );
}

int sprintf(char *out, const char *format, ...)
{
        va_list args;

        va_start( args, format );
        return print( &out, format, args );
}

int __sprintf_chk(char * out, int flag, size_t strlen, const char * format, ...)
{
        va_list args;

        va_start( args, format );
        return print( &out, format, args );
}

int snprintf(char *__restrict __s, size_t __n, __const char *__restrict __format, ...)
{
    assert(0);
	return 0;
}

int fputc(int c, FILE *stream)
{
	return putchar(c);
}

int puts(const char *s)
{
    return printf("%s", s);
}

int fputs(const char *s, FILE *stream)
{
	return fprintf(stream, "%s", s);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int nb = size * nmemb;
    char *buf = (char *) ptr;

    while(nb--)
        fputc(*buf++, stream);

	return size * nmemb;
}
