/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "@(#)fp.h	1.2	93/09/02 SMI"

/* Useful asm routines and data types for grungy 87 hacking */

#define EXCPMASK (FP_X_INV|FP_X_DNML|FP_X_DZ|FP_X_OFL|FP_X_UFL|FP_X_IMP)

typedef short _envbuf87[7]; /* buffer for f{ld,st}env instruction       */
#define _swoff   1          /* status word index in _envbuf87           */
#define _swboff  2          /* status word BYTE offset in _envbuf87     */
