/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)object.c	1.37	99/10/22 SMI"

/*
 * Object file dependent suport for ELF objects.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<unistd.h>
#include	<libelf.h>
#include	<string.h>
#include	<dlfcn.h>
#include	"libld.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"debug.h"
#include	"profile.h"

static Rt_map *		olmp = 0;

#define	SYM_NBKTS	211

/*
 * Process a relocatable object.  The static object link map pointer is used as
 * a flag to determine whether a concatenation is already in progress (ie. an
 * LD_PRELOAD may specify a list of objects).  The link map returned simply
 * specifies an `object' flag which the caller can interpret and thus call
 * elf_obj_fini() to complete the concatenation.
 */
static Rt_map *
elf_obj_init(Lm_list * lml, const char * name)
{
	Ofl_desc *	ofl;

	PRF_MCOUNT(108, elf_obj_init);

	/*
	 * Initialize an output file descriptor and the entrance criteria.
	 */
	if ((ofl = (Ofl_desc *)calloc(sizeof (Ofl_desc), 1)) == 0)
		return (0);
	ofl->ofl_e_machine = M_MACH;
	ofl->ofl_e_flags = 0;
	ofl->ofl_libver = EV_CURRENT;
	ofl->ofl_segalign = syspagsz;
	ofl->ofl_flags = (FLG_OF_DYNAMIC | FLG_OF_SHAROBJ | FLG_OF_STRIP |
		FLG_OF_MEMORY);
	ofl->ofl_flags1 = FLG_OF1_RELDYN | FLG_OF1_TEXTOFF;
	if ((ofl->ofl_symbkt =
	    (Sym_cache **)calloc(sizeof (Sym_cache), SYM_NBKTS)) == 0)
		return (0);
	ofl->ofl_symbktcnt = SYM_NBKTS;

	/*
	 * Obtain a generic set of entrance criteria (this is the first call to
	 * libld.so, which will effectively lazyload it).
	 */
	if (elf_rtld_load(lml_rtld.lm_head) == 0)
		return (0);

	if (ent_setup(ofl) == S_ERROR)
		return (0);

	/*
	 * Generate a link map place holder and use the `rt_priv' element to
	 * to maintain the output file descriptor.
	 */
	if ((olmp = (Rt_map *)calloc(sizeof (Rt_map), 1)) == 0)
		return (0);
	FLAGS(olmp) |= FLG_RT_OBJECT;
	olmp->rt_priv = (void *)ofl;

	/*
	 * Assign the output file name to be the initial object that got us
	 * here.  This name is being used for diagnostic purposes only as
	 * we don't actually generate an output file unless debugging is
	 * enabled.
	 */
	ofl->ofl_name = name;
	NAME(olmp) = (char *)name;
	LIST(olmp) = lml;

	if (lm_append(lml, olmp) == 0)
		return (0);
	return (olmp);
}


/*
 * Initial processing of a relocatable object.  If this is the first object
 * encountered we need to initialize some structures, then simply call the
 * link-edit functionality to provide the initial processing of the file (ie.
 * reads in sections and symbols, performs symbol resolution if more that one
 * object file have been specified, and assigns input sections to output
 * sections).
 */
Rt_map *
elf_obj_file(Lm_list * lml, const char * name)
{
	int err;

	PRF_MCOUNT(109, elf_obj_file);


	/*
	 * If this is the first relocatable object (LD_PRELOAD could provide a
	 * list of objects), initialize an input file descriptor and a link map.
	 * Note, because elf_obj_init() will reuse the global fmap structure we
	 * need to retain the original open file descriptor, both for the
	 * process_open() call and for the correct closure of the file on return
	 * to load_so().
	 */
	if (!olmp) {
		/*
		 * Load the link-editor library.
		 */
		int	fd = fmap->fm_fd;

		olmp = elf_obj_init(lml, name);
		fmap->fm_fd = fd;
		if (olmp == 0)
			return (0);
	}

	/*
	 * Proceed to process the input file.
	 */
	DBG_CALL(Dbg_util_nl());
	if (process_open(name, 0, fmap->fm_fd,
	    (Ofl_desc *)olmp->rt_priv, NULL, &err) == (Ifl_desc *)S_ERROR)
		return (0);

	return (olmp);
}
/*
 * Finish relocatable object processing.  Having already initially processed one
 * or more objects, complete the generation of a shared object image by calling
 * the appropriate link-edit functionality (refer to sgs/ld/common/main.c).
 */
Rt_map *
elf_obj_fini(Lm_list * lml, Rt_map * lmp)
{
	Ofl_desc *	ofl = (Ofl_desc *)lmp->rt_priv;
	Rt_map *	nlmp;
	Addr		etext;

	PRF_MCOUNT(110, elf_obj_fini);
	DBG_CALL(Dbg_util_nl());

	if (reloc_init(ofl) == S_ERROR)
		return (0);
	if (sym_validate(ofl) == S_ERROR)
		return (0);
	if (make_sections(ofl) == S_ERROR)
		return (0);
	if (create_outfile(ofl) == S_ERROR)
		return (0);
	if ((etext = update_outfile(ofl)) == (Addr)S_ERROR)
		return (0);
	if (reloc_process(ofl) == S_ERROR)
		return (0);

	/*
	 * At this point we have a memory image of the shared object.  The link
	 * editor would normally simply write this to the required output file.
	 * If we're debugging generate a standard temporary output file.
	 */
	DBG_CALL(Dbg_file_output(ofl));

	/*
	 * Generate a new link map representing the memory image created.
	 */
	if ((nlmp = LM_NEW_LM(lml->lm_head)(lml, ofl->ofl_name, ofl->ofl_name,
	    ofl->ofl_osdynamic->os_outdata->d_buf,
	    (unsigned long)ofl->ofl_ehdr, (unsigned long)ofl->ofl_ehdr + etext,
	    (unsigned long)ofl->ofl_size, 0, ofl->ofl_phdr,
	    ofl->ofl_ehdr->e_phnum, ofl->ofl_ehdr->e_phentsize,
	    ofl->ofl_osdynamic->os_outdata->d_buf,
	    (unsigned long)ofl->ofl_size)) == 0)
		return (0);

	/*
	 * Remove this link map from the end of the link map list and copy its
	 * contents into the link map originally created for this file (we copy
	 * the contents rather than manipulate the link map pointers as parts
	 * of the dlopen code have remembered the original link map address).
	 */
	NEXT((Rt_map *)PREV(nlmp)) = 0;
	lml->lm_tail = (Rt_map *)PREV(nlmp);
	lml->lm_obj--;
	lml->lm_init--;

	PREV(nlmp) = PREV(olmp);
	NEXT(nlmp) = NEXT(olmp);
	PERMIT(nlmp) = PERMIT(olmp);
	HANDLE(nlmp) = HANDLE(olmp);
	COUNT(nlmp) = COUNT(olmp);
	DONORS(nlmp) = DONORS(olmp);

	FLAGS(nlmp) |= ((FLAGS(olmp) & ~FLG_RT_OBJECT) | FLG_RT_IMGALLOC);
	FLAGS1(nlmp) |= FLAGS1(olmp);
	MODE(nlmp) |= MODE(olmp);

	free(ofl->ofl_symbkt);
	ofl_cleanup(ofl);
	free(olmp->rt_priv);
	(void) memcpy(olmp, nlmp, sizeof (Rt_map));
	free(nlmp);
	nlmp = olmp;
	olmp = 0;

	/*
	 * If we're being audited tell the audit library of the file we've just
	 * opened.
	 */
	if ((lml->lm_flags | FLAGS1(nlmp)) & FL1_MSK_AUDIT) {
		if (audit_objopen(lmp, lmp) == 0)
			return (0);
	}
	return (nlmp);
}
