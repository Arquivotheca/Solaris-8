/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)entry.c	1.23	99/06/01 SMI"

#include	<stdio.h>
#include	<memory.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"


/*
 * The loader uses a `segment descriptor' list to describe the output
 * segments it can potentially create.   Additional segments may be added
 * using a map file.
 */
#ifdef _ELF64
/* Phdr packing changes under Elf64 */
static Sg_desc sg_desc[] = {
	{{PT_PHDR, PF_R + PF_X, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_PHDR), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_INTERP, PF_R, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_INTERP), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_LOAD, PF_R + PF_X, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_TEXT), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_LOAD, M_DATASEG_PERM, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_DATA), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_DYNAMIC, M_DATASEG_PERM, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_DYNAMIC), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_NOTE, 0, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_NOTE), 0, 0, {NULL, NULL}, {NULL, NULL},
		FLG_SG_TYPE, NULL},
	{{PT_SUNWBSS, 0, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_SUNW_BSS), 0, 0, {NULL, NULL}, {NULL, NULL},
		FLG_SG_TYPE, NULL},
#if defined(ELF_TARGET_IA64)
	{{PT_IA_64_UNWIND, PF_R + PF_X, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_IA64_UNWIND), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
#endif /* ELF_TARGET_IA64 */
	{{PT_NULL, 0, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_STR_EMPTY), 0, 0, {NULL, NULL}, {NULL, NULL},
		FLG_SG_TYPE, NULL}
};
#else  /* Elf32 */
static Sg_desc sg_desc[] = {
	{{PT_PHDR, 0, 0, 0, 0, 0, PF_R + PF_X, 0},
		MSG_ORIG(MSG_ENT_PHDR), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_INTERP, 0, 0, 0, 0, 0, PF_R, 0},
		MSG_ORIG(MSG_ENT_INTERP), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_LOAD, 0, 0, 0, 0, 0, PF_R + PF_X, 0},
		MSG_ORIG(MSG_ENT_TEXT), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_LOAD, 0, 0, 0, 0, 0, M_DATASEG_PERM, 0},
		MSG_ORIG(MSG_ENT_DATA), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_DYNAMIC, 0, 0, 0, 0, 0, M_DATASEG_PERM, 0},
		MSG_ORIG(MSG_ENT_DYNAMIC), 0, 0, {NULL, NULL}, {NULL, NULL},
		(FLG_SG_TYPE | FLG_SG_FLAGS), NULL},
	{{PT_NOTE, 0, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_NOTE), 0, 0, {NULL, NULL}, {NULL, NULL},
		FLG_SG_TYPE, NULL},
	{{PT_SUNWBSS, 0, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_ENT_SUNW_BSS), 0, 0, {NULL, NULL}, {NULL, NULL},
		FLG_SG_TYPE, NULL},
	{{PT_NULL, 0, 0, 0, 0, 0, 0, 0},
		MSG_ORIG(MSG_STR_EMPTY), 0, 0, {NULL, NULL}, {NULL, NULL},
		FLG_SG_TYPE, NULL}
};
#endif /* Elfxx */


/*
 * The input processing of the loader involves matching the sections of its
 * input files to an `entrance descriptor definition'.  The entrance criteria
 * is different for either a static or dynamic linkage, and may even be
 * modified further using a map file.  Each entrance criteria is associated
 * with a segment descriptor, thus a mapping of input sections to output
 * segments is maintained.
 */
static const Ent_desc	ent_desc[] = {
	{{NULL, NULL}, MSG_ORIG(MSG_SCN_SUNWBSS), NULL,
		SHF_ALLOC + SHF_WRITE, SHF_ALLOC + SHF_WRITE,
		(Sg_desc *)LD_SUNWBSS, 0, FALSE},
	{{NULL, NULL}, NULL, SHT_NOTE, 0, 0,
		(Sg_desc *)LD_NOTE, 0, FALSE},
	{{NULL, NULL}, NULL, NULL,
		SHF_ALLOC + SHF_WRITE, SHF_ALLOC,
		(Sg_desc *)LD_TEXT, 0, FALSE},
	{{NULL, NULL}, NULL, NULL,
		SHF_ALLOC + SHF_WRITE, SHF_ALLOC + SHF_WRITE,
		(Sg_desc *)LD_DATA, 0, FALSE},
	{{NULL, NULL}, NULL, 0, 0, 0,
		(Sg_desc *)LD_EXTRA, 0, FALSE}
};

/*
 * Initialize new entrance and segment descriptors and add them as lists to
 * the output file descriptor.
 */
uintptr_t
ent_setup(Ofl_desc * ofl)
{
	Ent_desc *	enp;
	Sg_desc *	sgp;
	size_t		size;

	/*
	 * Initialize the elf library.
	 */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ELF_LIBELF), EV_CURRENT);
		return (S_ERROR);
	}

	/*
	 * The datasegment permissions can differ depending on whether
	 * this object is built statically or dynamically.
	 */
	if (ofl->ofl_flags & FLG_OF_DYNAMIC) {
		sg_desc[LD_DATA].sg_phdr.p_flags = M_DATASEG_PERM;
		sg_desc[LD_SUNWBSS].sg_phdr.p_flags = M_DATASEG_PERM;
	} else {
		sg_desc[LD_DATA].sg_phdr.p_flags = M_DATASEG_PERM | PF_X;
	}

	/*
	 * Allocate and initialize writable copies of both the entrance and
	 * segment descriptors.
	 */
	if ((sgp = (Sg_desc *)libld_malloc(sizeof (sg_desc))) == 0)
		return (S_ERROR);
	(void) memcpy(sgp, sg_desc, sizeof (sg_desc));
	if ((enp = (Ent_desc *)libld_malloc(sizeof (ent_desc))) == 0)
		return (S_ERROR);
	(void) memcpy(enp, ent_desc, sizeof (ent_desc));

	/*
	 * Traverse the new entrance descriptor list converting the segment
	 * pointer entries to the absolute address within the new segment
	 * descriptor list.  Add each entrance descriptor to the output file
	 * list.
	 */
	for (size = 0; size < sizeof (ent_desc); size += sizeof (Ent_desc)) {
		enp->ec_segment = &sgp[(long)enp->ec_segment];
		if ((list_appendc(&ofl->ofl_ents, enp)) == 0)
			return (S_ERROR);
		enp++;
	}

	/*
	 * Traverse the new segment descriptor list adding each entry to the
	 * segment descriptor list.  For each loadable segment initialize
	 * a default alignment (ld(1) and ld.so.1 initialize this differently).
	 */
	for (size = 0; size < sizeof (sg_desc); size += sizeof (Sg_desc)) {

		Phdr *	phdr = &(sgp->sg_phdr);

		if ((list_appendc(&ofl->ofl_segs, sgp)) == 0)
			return (S_ERROR);
		if (phdr->p_type == PT_LOAD)
			phdr->p_align = ofl->ofl_segalign;

		sgp++;
	}
	return (1);
}
