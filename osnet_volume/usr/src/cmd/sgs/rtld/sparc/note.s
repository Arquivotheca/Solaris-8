/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)note.s	1.6	97/05/01 SMI"

#ifndef	lint

#include	"sgs.h"

	.file	"note.s"

	.section	".note"
	.align	4
	.word	.L20 - .L10
	.word	0
	.word	0
.L10:
	.ascii		SGU_PKG
	.byte		" "
	.ascii		SGU_REL
	.byte		"\n"
	.ascii		"\tprofiling enabled"
#ifdef	PRF_RTLD
	.ascii		"  (with mcount)"
#endif
	.ascii		"\n"
	.ascii		"\tdebugging enabled\n"
.L20:

#endif
