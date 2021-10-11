/*
 *	Copyright (c) 1994 Sun Microsystems, Inc.
 */
#pragma ident	"@(#)note.s	1.4	97/05/23 SMI"

#ifndef	lint

#include	"sgs.h"

	.file	"note.s"

	.section	.note
	.align	4
	.long	.L20 - .L10
	.long	0
	.long	0
.L10:
	.string		SGU_PKG
	.byte		32 / " "
	.string		SGU_REL
	.byte		10 / "\n"
	.string		"\tprofiling enabled"
#ifdef	PRF_RTLD
	.string		"  (with mcount)"
#endif
	.string		"\n"
	.string		"\tdebugging enabled\n"
.L20:

#endif
