	.file	"fpreal.s"

	.ident	"@(#)fpreal.s	1.1	92/04/17 SMI"

/
/	Floating Point Support Routines
/
/	This file defines the symbol __fltused for XENIX compatiblity.
/	The compiler generates an unresolved reference to __fltused
/	if floating point is used in the module.  This symbol is
/ 	resolved by this file. 

	.data
	.align	4
	.globl	__fltused
_dgdef_(__fltused):
	.long	0
