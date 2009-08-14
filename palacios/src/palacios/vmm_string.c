/*
 * String library 
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* Modifications by Jack Lange <jarusl@cs.northwestern.edu> */



/*
 * NOTE:
 * These are slow and simple implementations of a subset of
 * the standard C library string functions.
 * We also have an implementation of snprintf().
 */


#include <palacios/vmm_types.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm.h>


#ifdef CONFIG_BUILT_IN_MEMSET
void * memset(void * s, int c, size_t n) {
    uchar_t * p = (uchar_t *) s;

    while (n > 0) {
	*p++ = (uchar_t) c;
	--n;
    }

    return s;
}
#endif

#ifdef CONFIG_BUILT_IN_MEMCPY
void * memcpy(void * dst, const void * src, size_t n) {
    uchar_t * d = (uchar_t *) dst;
    const uchar_t * s = (const uchar_t *)src;

    while (n > 0) {
	*d++ = *s++;
	--n;
    }

    return dst;
}
#endif


#ifdef CONFIG_BUILT_IN_MEMCMP
int memcmp(const void * s1_, const void * s2_, size_t n) {
    const char * s1 = s1_;
    const char * s2 = s2_;

    while (n > 0) {
	int cmp = (*s1 - *s2);
	
	if (cmp != 0) {
	    return cmp;
	}

	++s1;
	++s2;
    }

    return 0;
}
#endif


#ifdef CONFIG_BUILT_IN_STRLEN
size_t strlen(const char * s) {
    size_t len = 0;

    while (*s++ != '\0') {
	++len;
    }

    return len;
}
#endif



#ifdef CONFIG_BUILT_IN_STRNLEN
/*
 * This it a GNU extension.
 * It is like strlen(), but it will check at most maxlen
 * characters for the terminating nul character,
 * returning maxlen if it doesn't find a nul.
 * This is very useful for checking the length of untrusted
 * strings (e.g., from user space).
 */
size_t strnlen(const char * s, size_t maxlen) {
    size_t len = 0;

    while ((len < maxlen) && (*s++ != '\0')) {
	++len;
    }

    return len;
}
#endif


#ifdef CONFIG_BUILT_IN_STRCMP
int strcmp(const char * s1, const char * s2) {
    while (1) {
	int cmp = (*s1 - *s2);
	
	if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
	    return cmp;
	}
	
	++s1;
	++s2;
    }
}
#endif


#ifdef CONFIG_BUILT_IN_STRNCMP
int strncmp(const char * s1, const char * s2, size_t limit) {
    size_t i = 0;

    while (i < limit) {
	int cmp = (*s1 - *s2);

	if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
	    return cmp;
	}

	++s1;
	++s2;
	++i;
    }

    /* limit reached and equal */
    return 0;
}
#endif


#ifdef CONFIG_BUILT_IN_STRCAT
char * strcat(char * s1, const char * s2) {
    char * t1 = s1;

    while (*s1) { s1++; }
    while (*s2) { *s1++ = *s2++; }

    *s1 = '\0';

    return t1;
}
#endif


#ifdef CONFIG_BUILT_IN_STRNCAT
char * strncat(char * s1, const char * s2, size_t limit) {
    size_t i = 0;
    char * t1;

    t1 = s1;

    while (*s1) { s1++; }

    while (i < limit) {
	if (*s2 == '\0') {
	    break;
	}
	*s1++ = *s2++;		
    }
    *s1 = '\0';
    return t1;
}
#endif



#ifdef CONFIG_BUILT_IN_STRCPY
char * strcpy(char * dest, const char * src)
{
    char *ret = dest;

    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';

    return ret;
}
#endif


#ifdef CONFIG_BUILT_IN_STRNCPY
char * strncpy(char * dest, const char * src, size_t limit) {
    char * ret = dest;

    while ((*src != '\0') && (limit > 0)) {
	*dest++ = *src++;
	--limit;
    }

    if (limit > 0)
	*dest = '\0';

    return ret;
}
#endif



#ifdef  CONFIG_BUILT_IN_STRDUP
char * strdup(const char * s1) {
    char *ret;

    ret = V3_Malloc(strlen(s1) + 1);
    strcpy(ret, s1);

    return ret;
}
#endif




#ifdef CONFIG_BUILT_IN_ATOI
int atoi(const char * buf) {
    int ret = 0;

    while ((*buf >= '0') && (*buf <= '9')) {
	ret *= 10;
	ret += (*buf - '0');
	buf++;
    }

    return ret;
}
#endif


#ifdef CONFIG_BUILT_IN_STRCHR
char * strchr(const char * s, int c) {
    while (*s != '\0') {
	if (*s == c)
	    return (char *)s;
	++s;
    }
    return 0;
}
#endif


#ifdef CONFIG_BUILT_IN_STRRCHR
char * strrchr(const char * s, int c) {
    size_t len = strlen(s);
    const char * p = s + len;

    while (p > s) {
	--p;

	if (*p == c) {
	    return (char *)p;
	}
    }
    return 0;
}
#endif

#ifdef CONFIG_BUILT_IN_STRPBRK
char * strpbrk(const char * s, const char * accept) {
    size_t setLen = strlen(accept);

    while (*s != '\0') {
	size_t i;
	for (i = 0; i < setLen; ++i) {
	    if (*s == accept[i]) {
		return (char *)s;
	    }
	}
	++s;
    }

    return 0;
}
#endif

