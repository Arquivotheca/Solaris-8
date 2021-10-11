/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)genalign.s	1.8	97/09/22 SMI"

#if defined(lint)

/*LINTED: empty translation unit*/

#else

#include "assym.h"
#include <sys/machparam.h>

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
	.word		MMU_PAGESIZE4M
.desc_end:
	.align		4

#endif /* !lint */
