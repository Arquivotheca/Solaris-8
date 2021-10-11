/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)stdio.h	1.1	97/03/14 SMI"
 */

#ifndef	_STDIO_H
#define	_STDIO_H

/*
 *  Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved
 *
 *  Standard I/O for realmode drivers
 *
 *    This file provides function prototypes for the minimial "stdio" library
 *    available to Solaris x86 realmode drivers.  The only I/O streams sup-
 *    ported by this library are the stdin, stdout, and stderr, all of which
 *    are automatically open on the system console (fopen/fclose are NOT
 *    provided).
 *
 *    Standard I/O functions provided by the library consist of fputc and
 *    the printf variants.  This file provides prototype definitions for
 *    these functions but nothing else.
 */

#include <dostypes.h>    /* Get "va_list" definition */

#define	EOF (-1)
typedef int FILE;	/* FILE structures consist of a single word .. */
extern  FILE _iob;	/* .. and we only have one of them! */

#define	stdin	(&_iob)  /* All known I/O streams overlay the default FILE  */
#define	stdout	(&_iob)  /* .. struct, which is open on the console.	    */
#define	stderr	(&_iob)

extern  void	    putchar(char);
#define	putc(c, f)  putchar(c)
#define	fputc(c, f) putchar(c)

extern	int	printf(const char *, ...);
extern	int	fprintf(FILE *, const char *, ...);
extern	int	vfprintf(FILE *, const char *, va_list);
extern	int	sprintf(char *, const char *, ...);
#endif
