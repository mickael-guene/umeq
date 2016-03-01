/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <string.h>

#include <stdio.h>
int __builtin_strcmp(const char *s1, const char *s2)
{
    while(*s1 && (*s1==*s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1-*(unsigned char *)s2;
}

int __builtin_strncmp(const char *s1, const char *s2, size_t n)
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

void *memset(void *s, int c, size_t n)
{
	char *dst = (char *) s;
	while(n--)
		*dst++ = c;

	return s;
}
