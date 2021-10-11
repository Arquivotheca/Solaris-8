/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_STRING_H
#define	_MDB_STRING_H

#pragma ident	"@(#)mdb_string.h	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <strings.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	NTOS_UPCASE	0x1	/* Use upper-case hexadecimal digits */
#define	NTOS_UNSIGNED	0x2	/* Value is meant to be unsigned */
#define	NTOS_SIGNPOS	0x4	/* Prefix positive values with sign '+' */
#define	NTOS_SHOWBASE	0x8	/* Show base under appropriate circumstances */

extern const char *numtostr(uintmax_t, int, uint_t);
extern uintmax_t strtonum(const char *, int);
extern int strisnum(const char *);
extern int strtoi(const char *);

extern char *strdup(const char *);
extern char *strndup(const char *, size_t);
extern void strfree(char *);

extern size_t stresc2chr(char *);
extern char *strchr2esc(const char *);

extern char *strsplit(char *, char);
extern char *strrsplit(char *, char);
extern const char *strnpbrk(const char *, const char *, size_t);

extern const char *strbasename(const char *);
extern char *strdirname(char *);

extern const char *strbadid(const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_STRING_H */
