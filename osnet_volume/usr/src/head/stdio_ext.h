/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Extensions to the stdio package
 */

#ifndef _STDIO_EXT_H
#define	_STDIO_EXT_H

#pragma ident	"@(#)stdio_ext.h	1.2	99/06/10 SMI"

#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Even though the contents of the stdio FILE structure have always been
 * private to the stdio implementation, over the years some programs have
 * needed to get information about a stdio stream that was not accessible
 * through a supported interface. These programs have resorted to accessing
 * fields of the FILE structure directly, rendering them possibly non-portable
 * to new implementations of stdio, or more likely, preventing enhancements
 * to stdio because those programs will break.
 *
 * In the 64-bit world, the FILE structure is opaque. The routines here
 * are provided as a way to get the information that used to be retrieved
 * directly from the FILE structure. They are based on the needs of
 * existing programs (such as 'mh' and 'emacs'), so they may be extended
 * as other programs are ported. Though they may still be non-portable to
 * other operating systems, they will work from each Solaris release to
 * the next. More portable interfaces are being developed.
 */

#define	FSETLOCKING_QUERY	0
#define	FSETLOCKING_INTERNAL	1
#define	FSETLOCKING_BYCALLER	2

extern size_t __fbufsize(FILE *stream);
extern int __freading(FILE *stream);
extern int __fwriting(FILE *stream);
extern int __freadable(FILE *stream);
extern int __fwritable(FILE *stream);
extern int __flbf(FILE *stream);
extern void __fpurge(FILE *stream);
extern size_t __fpending(FILE *stream);
extern void _flushlbf(void);
extern int __fsetlocking(FILE *stream, int type);

#ifdef	__cplusplus
}
#endif

#endif	/* _STDIO_EXT_H */
