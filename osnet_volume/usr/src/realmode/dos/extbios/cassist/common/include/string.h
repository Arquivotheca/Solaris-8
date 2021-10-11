#ifndef	_STRING_H
#define	_STRING_H
#ident	"@(#)string.h	1.3	95/04/20 SMI\n"

/*
 *  Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved
 *
 *  String functions for Solaris x86 realmode drivers
 *
 *    This file provide function prototypes for the ANSI-like string functions
 *    available to Solaris x86 realmode drivers.  Realmode drivers run with
 *    separate stack and data segments, so care must be taken when moving
 *    between local and global string buffers.  In order to reduce the risk
 *    of unintentional pointer garbling, these functions differ from their
 *    ANSI counterparts in the following ways:
 *
 *      1)  All buffer pointers are defined as "far" in the function proto-
 *          types.  This forces the compiler to push the appropriate segment
 *          register on the stack the caller passes a "near" pointer.
 *
 *      2)  ANSI functions that return pointers (e.g, strcpy) have been de-
 *          clared "void".  This is to prevent a driver writer from uncon-
 *          siously assigning a "far" pointer to a "near" pointer and losing 
 *          the segment specification in the process.  (Note that most pro-
 *          grammers ignore string functions' return values anyway, so this
 *          should have little effect).
 */

#include <dostypes.h>    /* Get "far" definition                             */

extern void memcpy(void far *, const void far *, unsigned);
extern int  memcmp(const void far *, const void far *, unsigned);
extern void memset(void far *, int, unsigned);

extern void strncpy(char far *, const char far *, unsigned);
extern int  strcmp(const char far *, const char far *);
extern void strcpy(char far *, const char far *);
extern char far *strchr(const char far *, int);
extern int  strlen(const char far *);

#define strcat(p,q) strcpy((p)+strlen(p), q)

// Now alias all these names ...
#define	_fmemcpy(d,s,n)  memcpy(d,s,n)
#define	_fmemcmp(d,s,n)  memcmp(d,s,n)
#define _fmemset(d,c,n)  memset(d,c,n)

#define _fstrlen(s)      strlen(s)
#define _fstrcmp(d,s)    strcmp(d,s)
#define _fstrcat(d,s)    strcat(d,s)
#define _fstrchr(d,c)    strchr(d,c)
#define _fstrcpy(d,s)    strcpy(d,s)
#define _fstrncpy(d,s,n) strncpy(d,s,n)

#endif
