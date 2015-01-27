#include <string.h>

#include <stdio.h>
int __builtin_strcmp(const char *s1, const char *s2)
{
    while(*s1 && (*s1==*s2))
        s1++,s2++;
    return *(const unsigned char*)s1-*(const unsigned char*)s2;
}

/*int strncmp(const char *s1, const char *s2, size_t n)
{
	unsigned char c1 = '\0';
	unsigned char c2 = '\0';

	while (n > 0)
	{
		c1 = (unsigned char) *s1++;
		c2 = (unsigned char) *s2++;
		if (c1 == '\0' || c1 != c2)
		return c1 - c2;
		n--;
	}

	return c1 - c2;
}*/

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

void *memset(void *s, int c, size_t n)
{
	char *dst = (char *) s;
	while(n--)
		*dst++ = c;

	return s;
}
