
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)genalign.s	1.1	92/12/08 SMI"

#include <sys/pte.h>

#ifndef lint

#include "assym.h"

	!
	! this little hack generates a .note section where we tell
	! the booter what alignment we want
	!
	.section	".note"
	.align		4
	.word		.name_end - .name_begin
	.word		.desc_end - .desc_begin
	.word		ELF_NOTE_PAGESIZE_HINT
.name_begin:
	.asciz		ELF_NOTE_SOLARIS
.name_end:
	.align		4
	!
	! The pagesize is the descriptor.
	!
.desc_begin:
	.word		MMU_L2_SIZE
.desc_end:
	.align		4

#endif	/* lint */
