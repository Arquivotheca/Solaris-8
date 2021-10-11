/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MY_ALLOC_H
#define	_MY_ALLOC_H

#pragma ident	"@(#)debug_alloc.h	1.2	98/02/09 SMI"

#ifdef __cplusplus
extern "C" {
#endif

void *my_malloc(size_t, const char *, int);
void *my_realloc(void *, size_t, const char *, int);
void my_free(void *, const char *, int);
char *my_strdup(const char *, const char *, int);
void check_leaks(char *);

#define	AUTOFS_DUMP_DEBUG	1000000
#define	free(a)			my_free(a, __FILE__, __LINE__)
#define	malloc(a)		my_malloc(a, __FILE__, __LINE__)
#define	realloc(a, s)		my_realloc(a, s, __FILE__, __LINE__)
#define	strdup(a)		my_strdup(a, __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif	/* _MY_ALLOC_H */
