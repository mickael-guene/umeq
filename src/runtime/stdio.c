#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

FILE *stdin;
FILE *stdout;
FILE *stderr;

#define PADDING_ZERO    1
#define PADDING_LEFT    2

static int putChar(char **out, int c)
{
    if (out) {
        **out = c;
        (*out)++;
        return 1;
    } else
        return write(2, &c, 1);
}

static int putString(char **out, char *s)
{
    if (out) {
        int len = strlen(s);
        int i;
        for(i = 0; i < len; i++) {
            **out = s[i];
            (*out)++;
        }
        return len;
    } else
        return write(2, s, strlen(s));
}

static int digit_to_char(int digit, int is_upper)
{
    if (digit < 10)
        return '0' + digit;
    if (is_upper)
        return 'A' + digit - 10;
    else
        return 'a' + digit - 10;
}

static int putInteger(char **out, long i, int base, int min_width, int padding, int is_upper, int is_sign)
{
    int res;
    char buf[128];
    unsigned long u = i;
    char *cursor = &buf[127];
    int print_sign = 0;

    /* clear padding with zero if padding left is required */
    if (padding & PADDING_LEFT)
        padding &= ~PADDING_ZERO;
    /* handle negative signed integer case */
    if (is_sign && i < 0) {
        print_sign = 1;
        u = -i;
    }
    /* write digit in buffer */
    *cursor--= '\0';
    do {
        int digit = u % base;
        *cursor--= digit_to_char(digit, is_upper);
        u = u / base;
    } while(u);
    /* handle sign digit*/
    if (print_sign)
        *cursor--= '-';
    /* compute number of characters write */
    cursor++;
    res = &buf[127] - cursor;
    /* handle padding right */
    if (!(padding & PADDING_LEFT)) {
        while(min_width > res) {
            putChar(out, padding & PADDING_ZERO?'0':' ');
            res++;
        }
    }
    /* write digit */
    putString(out, cursor);
    /* handle padding left */
    if (padding & PADDING_LEFT) {
        while(min_width > res) {
            putChar(out, ' ');
            res++;
        }
    }

    return res;
}

static int is_flag_character(int c)
{
    return c == '#' || c == '0' || c == '-' || c == ' ' || c == '+';
}

static int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int print(char **out, const char *format, va_list args)
{
    int res = 0;
    int is_long;
    int min_width;
    int padding;

    while(*format) {
        if (*format == '%') {
            format++;
            is_long = 0;
            min_width = 0;
            padding = 0;
            if (*format == '\0')
                break;
            if (*format == '%') {
                res += putChar(out, *format);
                format++;
                continue;
            }
            /* flag chararacters */
            while(is_flag_character(*format)) {
                switch(*format) {
                    case '0':
                        padding |= PADDING_ZERO;
                        format++;
                        break;
                    case '-':
                        padding |= PADDING_LEFT;
                        format++;
                        break;
                    default:
                        assert(0);
                }
            }
            /* field width */
            while(is_digit(*format)) {
                min_width = min_width * 10 + *format - '0';
                format++;
            }
            /* precision */
            if (*format == '.') {
                format++;
                if (is_digit(*format))
                    format++;
            }
            /* length modifier */
            switch(*format) {
                case 'l':
                    is_long = 1;
                    format++;
                    break;
            }

            /* conversion specifier */
            switch(*format) {
                case 's':
                    {
                        char *s = (char *)(long) va_arg(args, long);
                        format++;
                        if (s)
                            res += putString(out, s);
                        else
                            res += putString(out, "(null)");
                    }
                    break;
                case 'u':
                    {
                        unsigned long data;

                        if (is_long)
                            data = va_arg(args, unsigned long int);
                        else
                            data = va_arg(args, unsigned int);
                        format++;

                        res += putInteger(out, data, 10, min_width, padding, 0, 0);
                    }
                    break;
                case 'x':
                    {
                        unsigned long data;

                        if (is_long)
                            data = va_arg(args, unsigned long int);
                        else
                            data = va_arg(args, unsigned int);
                        format++;

                        res += putInteger(out, data, 16, min_width, padding, 0, 0);
                    }
                    break;
                case 'd':
                    {
                        long data;

                        if (is_long)
                            data = va_arg(args, long int);
                        else
                            data = va_arg(args, int);
                        format++;

                        res += putInteger(out, data, 10, min_width, padding, 0, 1);
                    }
                    break;
                default:
                    assert(0);
            }

        } else {
            res += putChar(out, *format);
            format++;
        }
    }
    if (out)
        **out = '\0';
    return res;
}

/* public api */
int printf(const char *format, ...)
{
        va_list args;

        va_start( args, format );
        return print(0, format, args );
}

int fprintf(FILE *__restrict __stream, __const char *format, ...)
{
        va_list args;

        va_start( args, format );
        return print(0, format, args );
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
    return print(0, format, args );
}

int sprintf(char *out, const char *format, ...)
{
    va_list args;

    va_start( args, format );
    return print(&out, format, args );
}
