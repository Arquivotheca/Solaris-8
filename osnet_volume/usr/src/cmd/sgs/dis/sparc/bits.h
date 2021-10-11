/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)bits.h	1.3	97/09/07 SMI"

#include	"dis.h"

#define	FAILURE 0
#define	MAXERRS	1	 /* maximum # of errors allowed before	*/
				/* abandoning this disassembly as a	*/
				/* hopeless case			*/

#define		OPLEN	35 /* maximum length of a single operand */
				/* (will be used for printing)		*/

#define	TWO_8	256
#define	TWO_16	65536
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
