	.file   "i386_data.s"

	.ident	"@(#)i386_data.s	1.3	95/12/07 SMI"

/ This file contains
/ the definition of the
/ global symbol errno
/ 
/ int errno;

	.globl	errno
	.comm	errno,4
