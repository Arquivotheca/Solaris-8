/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/* The #ident directive confuses the DOS linker */
/*
#ident "@(#)types.h	1.2	97/03/10 SMI"
*/

/*
 * Definitions for the realmode driver interface.
 */

#ifndef _TYPES_H_
#define	_TYPES_H_

typedef unsigned char unchar;
typedef unsigned short ushort;
typedef unsigned short uint;
typedef unsigned long ulong;
typedef long paddr_t;

/*
 * Macros for far pointer manipulation:
 *	MK_FP:		make a far pointer from a segment and an offset
 *	FP_OFF:		extract the offset portion from a far pointer
 *	FP_SEG:		extract the segment portion from a far pointer
 *	FP_TO_LINEAR:	calculate a linear address from a far pointer
 */
#define	MK_FP(seg, off) (void __far *) \
	((((unsigned long)(seg)) << 16) | (unsigned long)(off))
#define	FP_OFF(fp)		(((unsigned long)(fp)) & 0xFFFF)
#define	FP_SEG(fp)		((((unsigned long)(fp)) >> 16) & 0xFFFF)
#define	FP_TO_LINEAR(p)		((FP_SEG(p) << 4) + FP_OFF(p))

#endif /* _TYPES_H_ */
