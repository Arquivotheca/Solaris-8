/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)elf_stabs.c	1.1	99/05/14 SMI"

#include <string.h>
#include <stdlib.h>
#include <libelf.h>
#include <gelf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stab.h>
#include <errno.h>
#include <libintl.h>
#include "stabspf_impl.h"

/* Section names to look up. */
static const char *StabName = ".stab";
static const char *StabStrName = ".stabstr";

/*
 * The parser requires the ability to change chars in the stabstr
 * mainly for string portions.
 *
 * This will be stabstr in stab_copy() and will grow on demand.
 */
static namestr_t stabspace;

/*
 * In order to reduce the number of realloc() calls in stabcopy()
 * the initial value should be high.
 *
 * Profiling against OSNet shows that 4k is normally enough and in
 * some special cases (ie. libspl) there is a stab that is > 12k.
 * X11/Openwindows/CDE should prove to be interesting as well.
 * Therefore, we can start with 8k and grow by that same value.
 */
#define	STABSPACE_SZ 8192

/*
 * stabcopy() - makes a copy of the string from the stabstr section
 *	into stabspace and concatenates continued stab lines.
 */
static stabsret_t
stabcopy(struct stab stabs[], int max, int *sndx, const char *strings,
    char **stabstr)
{
	size_t new_strlen;
	size_t new_size = 1;
	const char *new_str;
	char *insert;
	int ndx = *sndx;

	stabspace.ms_len = 0;

loop:
	new_str = strings + stabs[ndx].n_strx;

	/* Short cut. */
	if (*new_str == '\0') {
		*stabstr = "";
		return (STAB_SUCCESS);
	}

	new_strlen = strlen(new_str);
	new_size += new_strlen;

	if (new_size > stabspace.ms_size) {
		char *str;
		size_t alloc_size;

		if (stabspace.ms_size == 0) {
			alloc_size = STABSPACE_SZ;
		} else {
			alloc_size = stabspace.ms_size * 2;
		}

		/* Double again until big enough. */
		while (alloc_size < new_size)
			alloc_size *= 2;

		str = realloc(stabspace.ms_str, alloc_size);
		if (str == NULL) {
			return (STAB_NOMEM);
		}
		stabspace.ms_str = str;
		stabspace.ms_size = alloc_size;
	}

	insert = stabspace.ms_str + stabspace.ms_len;
	(void) strcpy(insert, new_str);

	stabspace.ms_len += new_strlen;
	if (new_str[new_strlen - 1] == '\\') {
		--stabspace.ms_len;

		if (++ndx >= max) {
			return (STAB_FAIL);
		}
		/* append the continued stab */
		goto loop;
	}

	*sndx = ndx;
	*stabstr = stabspace.ms_str;

	return (STAB_SUCCESS);
}

static stabsret_t
walk_stabs(struct stab stabs[], int stab_max, const char *strings)
{
	int ndx;
	char *stabstr;
	uint_t stabstr_size = 0;
	stabsret_t ret;


	for (ndx = 0; ndx < stab_max; ndx++) {

		/*
		 * The incremental linker can mess with the string
		 * offset and therefore it must be adjusted.
		 */
		if (stabs[ndx].n_type == N_ILDPAD &&
		    stabs[ndx].n_value != 0) {
			strings += stabs[ndx].n_value;
			/* No need to send this line to add_stab(). */
			continue;
		}

		/*
		 * Begining of compilation units are marked by the
		 * N_UNDF stab.
		 * Stab string offsets are per compilation unit.
		 * We must increase the strings pointer every time we
		 * arrive at a new compilation unit.
		 */
		if (stabs[ndx].n_type == N_UNDF) {
			strings += stabstr_size;
			stabstr_size = stabs[ndx].n_value;
			/* Flush key_pairs because N_ENDM has been stripped. */
			ret = keypair_flush_table();
			if (ret != STAB_SUCCESS) {
				return (ret);
			}

		}

		/* Make a copy of the string. */
		ret = stabcopy(stabs, stab_max, &ndx, strings, &stabstr);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}

		/* Extract the information from the stab line. */
		ret = add_stab(stabs[ndx].n_type, stabstr);
		if (ret != STAB_SUCCESS && ret != STAB_NA) {
			(void) fprintf(stderr, gettext("FAIL: parsing:\n"
			    "\t.stabs[%u]\t= \"%s\", 0x%x\n"),
			    ndx, stabstr, stabs[ndx].n_type);
			return (ret);
		}
	}

	return (STAB_SUCCESS);
}

/* load_stabs() - Given an Elf handle find the .stab and .stabstr sections. */
static stabsret_t
load_stabs(Elf *elf)
{
	GElf_Ehdr	ehdr;
	Elf_Scn		*scn = NULL;
	Elf_Scn		*stabscn = NULL;
	Elf_Scn		*stabstr_scn = NULL;
	int		ndx = 1;	/* Elf section nos. start at 1 */
	char		*section_name;
	GElf_Shdr	stab_shdr;
	Elf_Data	*stab_data;
	int		n_stabs;
	Elf_Data	*stabstr_data;
	GElf_Shdr	shdr;
	int		stab_ndx;
	int		stabstr_ndx;
	int		found = 0;

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		return (STAB_FAIL);
	}

	/* Sequentially search for the sections we are interested in. */
	while (found < 2) {
		if ((scn = elf_nextscn(elf, scn)) == NULL ||
		    gelf_getshdr(scn, &shdr) == NULL) {
			return (STAB_FAIL);
		}

		/* Get the section name. */
		section_name = elf_strptr(elf, ehdr.e_shstrndx, shdr.sh_name);

		/* See if it matches the interesting section names. */
		if (strcmp(section_name, StabName) == 0) {
			stab_ndx = ndx;
			++found;

			/*
			 * It is possible that the .stabstr section has
			 * been "linked" in.
			 */
			if (shdr.sh_link != 0) {
				/* No need to search further. */
				stabstr_ndx = shdr.sh_link;
				++found;
			}
		} else if (strcmp(section_name, StabStrName) == 0) {
			stabstr_ndx = ndx;
			++found;
		}
		ndx++;
	}

	/*
	 * Get to the data for the .stab and the .stabstr sections
	 * Determine the size of the .stab section (number of stabs)
	 */
	stabscn = elf_getscn(elf, stab_ndx);
	stabstr_scn = elf_getscn(elf, stabstr_ndx);

	if (gelf_getshdr(stabscn, &stab_shdr) == NULL		||
	    (stab_data = elf_getdata(stabscn, NULL)) == NULL	||
	    (stabstr_data = elf_getdata(stabstr_scn, NULL)) == NULL) {
		return (STAB_FAIL);
	}

	if (stab_shdr.sh_entsize != sizeof (struct stab)) {
		(void) fputs("Stabs information is incompatible!\n",
		    stderr);
		return (STAB_FAIL);
	}

	n_stabs = stab_shdr.sh_size / stab_shdr.sh_entsize;

	return (walk_stabs(stab_data->d_buf, n_stabs, stabstr_data->d_buf));
}

/*
 * spf_load_stabs() - open a file and start ripping apart its sections.
 */
stabsret_t
spf_load_stabs(const char *elf_fname)
{
	int fd;
	Elf *elf;
	stabsret_t ret = STAB_FAIL;

	/*
	 * Initialize the Elf library, must be called before elf_begin()
	 * can be called.
	 */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fputs(gettext("FAIL: libelf is out of date\n"), stderr);
		return (STAB_FAIL);
	}

	/* Open Elf file. */
	fd = open(elf_fname, O_RDONLY);
	if (fd == -1) {
		(void) fprintf(stderr, gettext("FAIL: opening file: %s: %s\n"),
		    elf_fname, strerror(errno));
		return (STAB_FAIL);
	}

	/* Start picking at elf. */
	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (elf == NULL) {
		(void) fprintf(stderr, gettext("FAIL: cannot get an elf "
		    "descriptor for: %s\n"), elf_fname);
		(void) close(fd);
		return (STAB_FAIL);
	}

	if (elf_kind(elf) != ELF_K_ELF) {
		(void) fprintf(stderr, gettext("FAIL: not an ELF file: %s\n"),
		    elf_fname);
		(void) elf_end(elf);
		(void) close(fd);
		return (STAB_FAIL);
	}

	/* This does the real work. */
	ret = load_stabs(elf);
	if (ret != STAB_SUCCESS) {
		(void) fprintf(stderr, gettext("FAIL: parsing file: %s\n"),
		    elf_fname);
	}

	(void) elf_end(elf);
	(void) close(fd);

	return (ret);
}
