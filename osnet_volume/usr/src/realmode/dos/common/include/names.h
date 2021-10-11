/*
 *  Copyright (c) 1995, by Sun Microsystems, Inc.
 *  All Rights Reserved.
 *
 *  names.h -- function prototypes for EISA name converters
 */

#ifndef	_NAMES_H
#define	_NAMES_H

#ident	"<@(#)names.h	1.1	95/06/16 SMI>"
#include <dostypes.h>

extern unsigned long CompressName(char far *);
extern void DecompressName(unsigned long, char far *);

#endif	/* _NAMES_H */
