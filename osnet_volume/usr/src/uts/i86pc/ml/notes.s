#include <sys/elf_notes.h>

#pragma ident  "@(#)notes.s  1.5     98/01/29 SMI"

#if defined(lint)
#include <sys/types.h>
#else
#include <sys/mmu.h>

/
/ Tell the booter that we'd like to load unix on a large page
/ if the chip supports it.
/
	.section        .note
	.align          4
	.4byte           .name1_end - .name1_begin 
	.4byte           .desc1_end - .desc1_begin
	.4byte		ELF_NOTE_PAGESIZE_HINT
.name1_begin:
	.string         ELF_NOTE_SOLARIS
.name1_end:
	.align          4
.desc1_begin:
	.4byte		FOURMB_PAGESIZE
.desc1_end:
	.align		4
#endif
