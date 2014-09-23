#include <string.h>

#include <stdio.h>
int __builtin_strcmp(const char *s1, const char *s2)
{
    while(*s1 && (*s1==*s2))
        s1++,s2++;
    return *(const unsigned char*)s1-*(const unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	return __builtin_strcmp(s1, s2);
}

char *strcpy(char *dest, const char *src)
{
	char *res = dest;

	while(*src != '\0')
		*dest++ = *src++;

	*dest++ = *src++;

	return res;
}

/* FIXME :use n */
char *strncpy(char *dest, const char *src, size_t n)
{
	char *res = dest;

	while(*src != '\0')
		*dest++ = *src++;

	*dest++ = *src++;

	return res;
}

size_t strlen(const char *s)
{
	size_t res = 0;

	while(*s++ != '\0')
		res++;

	return res;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	char *pDest = (char *) dest;
	char *pSrc = (char *) src;

	while(n--)
		*pDest++ = *pSrc++;

	return dest;
}

void * __memcpy_chk(void * dest, const void * src, size_t len, size_t destlen)
{
    return memcpy(dest, src, len);
}

void *memset(void *s, int c, size_t n)
{
	char *dst = (char *) s;
	while(n--)
		*dst++ = c;

	return s;
}

void * __memset_chk(void * dest, int c, size_t len, size_t destlen)
{
	return memset(dest, c, len);
}
