/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cullstabs.c	1.1	99/05/14 SMI"

/*
 * This file provides functions to perform processing of the stab
 * sections in the object files used to produce a shared object.
 * These functions are invoked by the link editor when it runs, via its
 * support interface (ld ... -S<support_so> ...).
 *
 * The stabs retained in the output object are of a significantly reduced
 * size (compared to the full set of debugging stabs produced by a -g
 * compile).  This is done by eliminating un-needed stab sections
 * (.stab.index and .stab.indexstr) and by selecting only stabs of the
 * desired kind (namely typedefs and typenames) from the retained
 * stab sections (.stab and .stabstr sections).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <stab.h>
#include <gelf.h>

#define	S_ERROR		(-1)

/* Following are flags used to indicate individual stab handling */
#define	STAB_UNDF	(1<<1)
#define	STAB_KEEP	(1<<0)
#define	STAB_CULL	0

/*
 * Buffer size for continued stab string
 * This may seem excessive but check out the 500+ line structure
 * in jni.h.  The complete stab for that structure is >16KB
 */
#define	MAX_STABSTRLEN	32000

static int debug_cull = 0;		/* Debugging output level */

/* Defined constants for use in section-name comparisons */
static const char *StabName = ".stab";
static const char *StabStrName = ".stabstr";
static const char *StabIndxName = ".stab.index";
static const char *StabIndxStrName = ".stab.indexstr";

#ifdef __i386
static const char *StabRelocName = ".rel.stab";
#endif /* __i386 */

/*
 * Input file name during processing
 */
static const char *in_fname;	/* current input file */

/*
 * Module external interface - linker support hooks
 */
void
ld_start(const char *out_name, const Elf32_Half etype, const char *caller);
void
ld_start64(const char *out_name, const Elf64_Half etype, const char *caller);

void
ld_file(const char *name, const Elf_Kind kind, int flags, Elf *elf);
void
ld_file64(const char *name, const Elf_Kind kind, int flags, Elf *elf);

void
ld_section(const char *scn_name, Elf32_Shdr *shdr, Elf32_Word scnndx,
    Elf_Data *s_data, Elf *elf);
void
ld_section64(const char *scn_name, Elf64_Shdr *shdr, Elf64_Word scnndx,
    Elf_Data *s_data, Elf *elf);

void
ld_atexit(int status);
void
ld_atexit64(int status);

/*
 * Module-local function prototypes (for any forward refs)
 */
static Elf_Data *
get_string_data(Elf *elf, GElf_Ehdr *ehdr, const char *strscn_name,
    GElf_Shdr *shdr);

static size_t
find_named_section(Elf *elf, GElf_Ehdr * ehdr, const char *search_name,
    const GElf_Word sh_type, const GElf_Xword sh_flags, Elf_Scn **ret_scn);

static void
ld_section_handler(const char *scn_name, GElf_Shdr *shdr, GElf_Word scnndx,
    Elf_Data *s_data, Elf *elf);

/*
 * stab_typename -
 *	Decode stab type and return as string (debugging output support)
 */
static const char *
stab_typename(int stab_type)
{
	static char	Stab_Unknown[32];
	char		*type_name;

	switch (stab_type) {
	case N_ABS + N_EXT:
		type_name = "N_ABS+N_EXT";
		break;
	case N_ABS:
		type_name = "N_ABS";
		break;
	case N_ALIAS:
		type_name = "N_ALIAS";
		break;
	case N_BCOMM:
		type_name = "N_BCOMM";
		break;
	case N_BINCL:
		type_name = "N_BINCL";
		break;
	case N_BROWS:
		type_name = "N_BROWS";
		break;
	case N_BSS + N_EXT:
		type_name = "N_BSS+N_EXT";
		break;
	case N_BSS:
		type_name = "N_BSS";
		break;
	case N_CMDLINE:
		type_name = "N_CMDLINE";
		break;
	case N_COMM:
		type_name = "N_COMM";
		break;
	case N_CONSTRUCT:
		type_name = "N_CONSTRUCT";
		break;
	case N_DATA + N_EXT:
		type_name = "N_DATA+N_EXT";
		break;
	case N_DATA:
		type_name = "N_DATA";
		break;
	case N_DESTRUCT:
		type_name = "N_DESTRUCT";
		break;
	case N_ECOML:
		type_name = "N_ECOML";
		break;
	case N_ECOMM:
		type_name = "N_ECOMM";
		break;
	case N_EINCL:
		type_name = "N_EINCL";
		break;
	case N_ENDM:
		type_name = "N_ENDM";
		break;
	case N_ENTRY:
		type_name = "N_ENTRY";
		break;
	case N_EXCL:
		type_name = "N_EXCL";
		break;
	case N_FN:
		type_name = "N_FN";
		break;
	case N_FNAME:
		type_name = "N_FNAME";
		break;
	case N_FUN:
		type_name = "N_FUN";
		break;
	case N_GSYM:
		type_name = "N_GSYM";
		break;
	case N_ILDPAD:
		type_name = "N_ILDPAD";
		break;
	case N_LBRAC:
		type_name = "N_LBRAC";
		break;
	case N_LCSYM:
		type_name = "N_LCSYM";
		break;
	case N_LENG:
		type_name = "N_LENG";
		break;
	case N_LSYM:
		type_name = "N_LSYM";
		break;
	case N_ISYM:
		type_name = "N_ISYM";
		break;
	case N_ESYM:
		type_name = "N_ESYM";
		break;
	case N_MAIN:
		type_name = "N_MAIN";
		break;
	case N_OBJ:
		type_name = "N_OBJ";
		break;
	case N_OPT:
		type_name = "N_OPT";
		break;
	case N_PATCH:
		type_name = "N_PATCH";
		break;
	case N_PC:
		type_name = "N_PC";
		break;
	case N_PSYM:
		type_name = "N_PSYM";
		break;
	case N_RBRAC:
		type_name = "N_RBRAC";
		break;
	case N_ROSYM:
		type_name = "N_ROSYM";
		break;
	case N_RSYM:
		type_name = "N_RSYM";
		break;
	case N_SINCL:
		type_name = "N_SINCL";
		break;
	case N_SLINE:
		type_name = "N_SLINE";
		break;
	case N_SO:
		type_name = "N_SO";
		break;
	case N_SOL:
		type_name = "N_SOL";
		break;
	case N_SSYM:
		type_name = "N_SSYM";
		break;
	case N_STSYM:
		type_name = "N_STSYM";
		break;
	case N_TCOMM:
		type_name = "N_TCOMM";
		break;
	case N_TEXT + N_EXT:
		type_name = "N_TEXT+N_EXT";
		break;
	case N_TEXT:
		type_name = "N_TEXT";
		break;
	case N_UNDF + N_EXT:
		type_name = "N_UNDF+N_EXT";
		break;
	case N_UNDF:
		type_name = "N_UNDF";
		break;
	case N_USING:
		type_name = "N_USING";
		break;
	case N_WITH:
		type_name = "N_WITH";
		break;
	case N_XCOMM:
		type_name = "N_XCOMM";
		break;
	case N_XLINE:
		type_name = "N_XLINE";
		break;
	default:
		(void) sprintf(Stab_Unknown, "UNKNOWN(0x%2.2x)", stab_type);
		type_name = Stab_Unknown;
		break;
	}
	return (type_name);
}

/*
 * check_for_continued_stab -
 *	Check for and handle a stab that continues.
 * Synopsis:
 *	If we detect a contining stab, we will copy the stab and its string
 * (and each successive continuation pair) to the output.  We will also
 * accumulate the whole stabstring in an internal buffer which is returned
 * to the caller (this is so that the whole contiguous string can be handed
 * to a a parsing routine if desired).
 *
 * Side Effect:
 * Notice that the caller's stab and stabstring pointers are updated so
 * that they are each pointing to the last stab (and stabstring piece) of
 * a continuation set (i.e. the one that does not have a continue character
 * in its string).
 *
 *	stabP		pointer to the caller's current stab pointer
 *			(we update this if a continuation stab is seen)
 *	stabstrP 	pointer to the stab string for the current stab
 *	stabstr_buf	current stab strings buffer base address
 *	newstabP 	pointer to the caller's new (output) stab pointer
 *			(we update this if a continuation stab is seen)
 *	newstabstr_buf	new (output) stab strings buffer base address
 *
 *	<char * return>	return a pointer to the whole accumulated string
 *			if it was a continuation, otherwise return NULL.
 *
 * Note: Continuation string handling currently has a static size limit
 *	set above by MAX_STABSTRLEN.  It's large, but finite #:o)
 */
static char *
check_for_continued_stab(struct stab **stabP, char **stabstrP,
    const char *stabstr_buf, struct stab **newstabP, char **newstabstrP,
    const char *newstabstr_buf)
{
	char		*stabstr = *stabstrP;
	char		*newstabstr;
	static char	Continued_stabstr [MAX_STABSTRLEN];
	int		Continued_stabstr_length;
	int		len;

	Continued_stabstr[0] = '\0';
	Continued_stabstr_length = 0;

	/*
	 * Process successive stabs as long as their corresponding string
	 * contains the continuation character.
	 */
	while (strchr(stabstr, '\\')) {

		/* debug */
		if (debug_cull >= 2)
			(void) printf("  continuation stab\n");

		len = strlen(stabstr) - 1;	/* to strip the trailing '\' */

		/*
		 * For now, we just check for buffer overrun
		 * vs. MAX_STABSTRLEN (a big number that's much larger
		 * than any reasonable continuation we've every seen).
		 * Later, could re-code to grow the string buffer as
		 * needed to deal with insanely gigantic ones.
		 */
		if ((Continued_stabstr_length + len) > MAX_STABSTRLEN) {
			(void) fputs(gettext("ldcullstabs: "
			    "Fatal error, stabstring continuation exceeds "
			    "buffer size\n"), stderr);
			exit(EXIT_FAILURE);
		}

		(void) strncat(Continued_stabstr, stabstr, len);
		Continued_stabstr_length += len;

		/*
		 * Copy stab and stabstr to output, then
		 * update stab pointers to point to next stab (and its string)
		 */

		**newstabP = **stabP;		/* copy the stab */

		/*
		 * Copy the stab string to the new stabstring buffer
		 * and update the new stab's string index to refer to it.
		 */
		newstabstr = *newstabstrP;
		(void) strcpy(newstabstr, stabstr);
		(*newstabP)->n_strx = newstabstr - newstabstr_buf;

		/*
		 * debug -
		 * Report input and output string table offsets
		 */
		if (debug_cull >= 2) {
			(void) printf("`%s' from %d (len %lu) -> %d\n",
			    stabstr, (*stabP)->n_strx,
			    (ulong_t)(strlen(stabstr) + 1),
			    (*newstabP)->n_strx);
		}

		/*
		 * Update pointers to input and output stab, and the output
		 * stabstrings buffer.
		 */
		(*stabP)++;
		(*newstabP)++;
		(*newstabstrP) += (len + 2);	/* == strlen(stabstr) + 1 */

		/*
		 * Get the string associated with the current (next) input
		 * stab in preparation for the next loop iteration.
		 */
		stabstr = (char *)(stabstr_buf) + (*stabP)->n_strx;
	}

	/*
	 * Before returning, append the string from the last stab
	 * in this continuation set (i.e. the currently referred to stab),
	 * to the Continuation string buffer.
	 */
	if (*Continued_stabstr) {
		len = strlen(stabstr);
		/* Check for buffer overrun */
		if ((Continued_stabstr_length + len) > MAX_STABSTRLEN) {
			(void) fputs(gettext("ldcullstabs: "
			    "Fatal error, stabstring continuation exceeds "
			    "buffer size\n"), stderr);
			exit(EXIT_FAILURE);
		}
		(void) strncat(Continued_stabstr, stabstr, len);
		Continued_stabstr_length += len;

		/* debug */
		if (debug_cull >= 2)
			(void) printf("  Continued stabs' string: `%s'"
			    " length (%d)\n",
			    Continued_stabstr, Continued_stabstr_length);

		/*
		 * If continuation occurred, update the caller's stabstr
		 * pointer, and return a pointer to the completely assembled
		 * continued stab string
		 */
		*stabstrP = stabstr;
		return (Continued_stabstr);
	}

	/* If continuation didn't occur, return a NULL string */
	return (NULL);
}

/*
 * parse_stab -
 *	Check the stab and keep it if it's one of the types we want
 */
static int
parse_stab(struct stab **stabP, char **stabstrP, const char *stabstr_buf,
    struct stab **newstabP, char **newstabstrP, const char *newstabstr_buf)
{
	char *stabstr = *stabstrP;

	/*
	 * debug -
	 *	Display stab type
	 */
	if (debug_cull >= 3) {
		(void) printf("offset %d %s\n", (*stabP)->n_strx,
		    stab_typename((*stabP)->n_type));
	}

	switch ((*stabP)->n_type) {
	case N_UNDF:
		/*
		 * Seeing UNDF indicates beginning of another object.
		 * Return the size of this strtab piece, so the caller
		 * can update the base offset into the .stabstr data
		 */
		return (STAB_UNDF | STAB_KEEP);

	case N_OPT:
		/*
		 * N_OPT stab contains a string providing stabs version
		 */
		return (STAB_KEEP);

#ifdef KEEP_ENDM
	case N_ENDM:
		/*
		 * Keeping N_ENDM eases stabs parser's keypair table flushing
		 * (note: this is slight optimization only, we'll do it later)
		 */
		return (STAB_KEEP);
#endif

	case N_LSYM:
		if (strstr(stabstr, ":t") || strstr(stabstr, ":T")) {
			/*
			 * Do stab-continuation processing
			 */
			(void) check_for_continued_stab(stabP, stabstrP,
			    stabstr_buf, newstabP, newstabstrP,
			    newstabstr_buf);
#ifdef PARSE_FUNCS
			/*
			 * Later on we'll do function, typename or typedef
			 * parsing, and maintain a symbol table to determine
			 * which (unique) typenames and typedefs to keep.
			 * That's the reason that check_for_continued returns
			 * the whole accumulated string (so we can parse it)
			 *
			 * This future smarter culling will be a 2 pass method:
			 * Pass 1: read all the function defs to decide what
			 *	types are needed (plus the transitive type
			 *	dependency closure from those)
			 * Pass 2: Run through stabs again emitting only stabs
			 *	for those types and typenames used by the
			 *	functions defined.
			 */
			whole_stabstr = check_for_continued_stab(stabP,
			    stabstrP, stabstr_buf, newstabP, newstabstr_buf);
			parse_type(whole_stabstr);
#endif
			return (STAB_KEEP);
		}

		return (STAB_CULL);

	default:
		return (STAB_CULL);
	}
}

#ifdef __i386
/* Following is used to divert .stab relocations on i386 */
static int	DummyStab_offset;
#endif /* __i386 */

/*
 * select_stabs -
 *	Keep only the specified stab types in the output file.
 *
 *	Process the stab strings at the same time as the stab entries.
 *	Build up two new buffers - one for the stabs and one for the stab
 *	strings selected.
 */

static void
select_stabs(Elf *elf, GElf_Ehdr *ehdr,
    GElf_Shdr *stab_shdr, Elf_Data *stab_data)
{
	Elf_Data *stabstr_data;

	/* Buffers for old and new stabs */
	struct stab *stab_buf;
	struct stab *newstab_buf;

	/* Buffers for old and new stab strings */
	char	*stabstr_buf;
	char	*newstabstr_buf;

	/* Counters and iteraters */
	int n_stabs;
	int n_newstabs = 0;
	struct stab	*stab;
	struct stab	*newstab;
	struct stab	*last_stab;
	struct stab	*new_UNDFstab = 0;

	char *stabstr;
	char *newstabstr;
	uint_t	parse_flag;

	/*
	 * Algorithm: find the linked .stabstr section and its buffer
	 *
	 * Allocate buffers for the new stabs and the new strings
	 * (just use a size equal to the current ones even though the
	 * actual usage/length will be smaller when we're done culling).
	 *
	 * Loop through each stab in the input buffer (and its string)
	 * and copy it (and its string) to the output buffers if it
	 * meets the selection criteria.
	 *
	 * Finally, update buffer lengths etc. in *both* sections'
	 * Elf_Data structures.
	 */

	/*
	 * Locate the data in the string section corresponding to this
	 * .stab section (i.e. the data in its corresponding .stabtr)
	 */

	stabstr_data = get_string_data(elf, ehdr, StabStrName, stab_shdr);
	if (stabstr_data == (Elf_Data *)S_ERROR) {
		(void) printf("ldcullstabs Fatal error, Failed to get string"
		    " data for .stab section\n");
		exit(EXIT_FAILURE);
	}

	/* Base address of all stab strings */
	stabstr_buf = stabstr_data->d_buf;


	/*
	 * 2. Allocate a new string table *buffer* equal in size to .stabstr's
	 * (but not a string table section and data descriptor)
	 */

	newstabstr_buf = malloc(stabstr_data->d_size);	/* output strtab */
	if (newstabstr_buf == 0) {
		(void) fprintf(stderr, gettext("ldcullstabs: Fatal "
		    "error, file: %s malloc for new .stabstr buffer "
		    "failed\n"), in_fname);
		exit(EXIT_FAILURE);
	}

	/* debug */
	if (debug_cull >= 2)
		(void) printf("allocated new stabstr buffer at 0x%p "
		    "of size %lu\n",
		    (void *)newstabstr_buf, (ulong_t)(stabstr_data->d_size));

	/*
	 * 3. Determine the size of the .stab section and allocate a
	 *    new .stab *buffer* of equal size.
	 */
	n_stabs = (stab_shdr->sh_size / stab_shdr->sh_entsize);
	newstab_buf = calloc(n_stabs, sizeof (struct stab));
	if (newstab_buf == 0) {
		(void) fprintf(stderr, gettext("ldcullstabs: Fatal "
		    "error, file: %s calloc for new .stab buffer "
		    "failed\n"), in_fname);
		exit(EXIT_FAILURE);
	}

	/* debug */
	if (debug_cull >= 2)
		(void) printf("allocated new stab buffer at 0x%p of "
		    "size %lu (%lu)\n",
		    (void *)newstab_buf, (ulong_t)(stab_data->d_size),
		    (ulong_t)(n_stabs * sizeof (struct stab)));

	/*
	 * 4. Now copy/select stabs
	 *		Start w/stab pointers pointing to beginning of
	 * respective stab buffers.  Determine the last stab's location
	 * for the loop termination.  We use that method since stab
	 * continuation handling skips forward over the continuation stabs
	 * updating the stab and newstab pointers.
	 */

	stab_buf = (struct stab *)(stab_data->d_buf);
	stab = stab_buf;
	last_stab = (struct stab *)(stab_data->d_buf) + n_stabs;

	newstab = newstab_buf;
	newstabstr = newstabstr_buf;    /* point to [tail of] new strtab */
	/*
	 * Make sure the first string's index will be 1 (not 0)
	 * n_strx == 0 is a special flag to dumpstabs indicating
	 * that the stab has no string (i.e. it's a .stabn rather than .stabs).
	 */
	*newstabstr++ = '\0';		/* Store a NULL as first char */

	/* debug */
	if (debug_cull >= 1) {
		(void) printf("   ldcullstabs, File: (%s)\n", in_fname);
		(void) printf("    stabs in: %d\n", n_stabs);
	}

	while (stab <= last_stab) {

		stabstr = stabstr_buf + stab->n_strx;
		parse_flag = parse_stab(&stab, &stabstr, stabstr_buf,
		    &newstab, &newstabstr, newstabstr_buf);

		if (parse_flag & STAB_KEEP) {
			if (parse_flag & STAB_UNDF) {
#ifdef __i386
				/*
				 * (i386 .rel.stab override) -
				 * Insert a dummy stab before this one
				 * used later as the target for any stab
				 * relocations.
				 * Make it a BITFIELD type N_PATCH stab just
				 * so it's a quasi-legitimate looking stab.
				 *
				 * Remember we calloc'ed the storage, so
				 * newstab->n_strx = 0; (indicating a .stabn)
				 * and newstab->{n_other, n_value} = 0 too;
				 */
				newstab->n_type = N_PATCH;
				newstab->n_desc = P_BITFIELD;
				DummyStab_offset =
					(char *)&newstab->n_value -
					(char *)newstab_buf;
				/* debug */
				if (debug_cull >= 1)
				    (void) printf("    @stab[%ld], inserted "
					"N_PATCH Dummy, offset 0x%x\n",
					(ulong_t)(newstab - newstab_buf),
					DummyStab_offset);
				newstab++;
#endif  /* __i386 */
				new_UNDFstab = newstab;
			}

			/*
			 * Now copy this stab and its string out to
			 * the new stab and stab strings sections.
			 *
			 * Careful! (inbound side effect) - parse_stab()
			 * skips forward over one or more stabs if a
			 * continuation stab is seen.  In that case, stab and
			 * stabstr have been updated to point to the last stab
			 * (and string part) of the continuation set, with
			 * all but this last one having been copied to the
			 * output.  (newstabstr has also been updated
			 * properly).
			 * So just copy this last stab and stabstring out
			 */

			*newstab = *stab;
			(void) strcpy(newstabstr, stabstr);

			/*
			 * Next, calculate the offset of the new
			 * string in the new string table and update
			 * the new stab to refer to it.
			 *
			 * Then update the [tail] pointer to reflect
			 * the current end of the new string table.
			 */

			newstab->n_strx = (newstabstr - newstabstr_buf);
			newstabstr += (strlen(stabstr) + 1);

			/*
			 * Report input and output string table
			 * offsets (debugging)
			 */
			if (debug_cull >= 2) {
			    (void) printf("`%s' from %d (len %lu) -> %d\n",
				stabstr, stab->n_strx,
				(ulong_t)(strlen(stabstr) + 1),
				newstab->n_strx);
			}
			newstab++;	/* OK, finished with this new stab */
		}
		stab++;			/* move on to next input stab */
	}

	/*
	 * 5. Finally update the data descriptors and section headers
	 */

	/*
	 * First, remember to update to last "header" (UNDF) stab in the
	 * new stab buffer (output stab buffer) with the size of the [last
	 * piece of the] stab string table.
	 */
	new_UNDFstab->n_value = (newstabstr - newstabstr_buf);

	/*
	 * Update the data descriptor's pointers for the .stab section
	 * to refer to the new stab buffer (and size).
	 *
	 * Note: Calculate the number of newstabs based on the
	 * present newstab pointer vs. newstab_buf's base address
	 * We can't simply count newstabs in the loop above, since
	 * that wouldn't reflect the count of any continuation stabs
	 * processed in parse_stab().
	 */
	n_newstabs =  newstab - newstab_buf;
	stab_data->d_buf = newstab_buf;
	stab_data->d_size = n_newstabs * sizeof (struct stab);

	/* debug */
	if (debug_cull >= 1)
		(void) printf("    stabs out: (%s newstabs) %d\n",
		    in_fname, n_newstabs);

	/*
	 * Update the data descriptor's pointers for the .stabstr section
	 * to refer to the new stabstr buffer (and size).
	 */
	stabstr_data->d_buf = newstabstr_buf;
	stabstr_data->d_size =  (newstabstr - newstabstr_buf);
}

/*
 * find_named_section()
 *
 *	Find a section in elf that matches the supplied section name,
 *	type, and flags.
 *
 * Returns:
 *		section number if found
 *		 0 - if no matching section found
 *		-1 - if error
 *
 *	If shdr is a non-null pointer it will be set to the section header
 *	that was found.
 *
 */
static size_t
find_named_section(Elf *elf, GElf_Ehdr * ehdr, const char *search_name,
    const GElf_Word sh_type, const GElf_Xword sh_flags,
    Elf_Scn **ret_scn)
{
	Elf_Scn		*scn = NULL;
	GElf_Shdr 	gelf_shdr;	/* Elf32_Shdr or Elf64_Shdr */
	GElf_Shdr	*shdr = &gelf_shdr;
	Elf_Scn		*elf_shstr_scn;
	Elf_Data	*elf_shstr_data;
	char	*elf_shstrtab;

	/*
	 * Find the section headers string table - It contains the
	 * (string) names for all sections in this ELF object.
	 *
	 * Note: We don't bother to check for errors here, since
	 * every ELF file really must have this section.
	 */
	elf_shstr_scn = elf_getscn(elf, ehdr->e_shstrndx);
	elf_shstr_data = elf_getdata(elf_shstr_scn, NULL);
	elf_shstrtab = elf_shstr_data->d_buf;

	while ((scn = elf_nextscn(elf, scn)) != 0) {
		if (gelf_getshdr(scn, shdr) == NULL)
			return ((size_t)-1);
		if ((shdr->sh_type == sh_type) &&
		    (shdr->sh_flags == sh_flags) &&
		    (strcmp(elf_shstrtab + shdr->sh_name, search_name) == 0)) {

			size_t scn_ndx;

			/*
			 * We've got a match
			 */
			if ((scn_ndx = elf_ndxscn(scn)) == SHN_UNDEF)
				return ((size_t)-1);
			if (ret_scn)
				*ret_scn = scn;
			return (scn_ndx);
		} /* if */
	} /* while */

	/*
	 * No match found
	 */
	return (0);
} /* find_named_section() */


/*
 * get_string_data -
 *	Given a section, find the string table section corresponding
 *	to it.
 *
 *    elf -		Reference to the ELF object
 *    ehdr -		Pointer to the ELF object's ELF header
 *    shdr -		The section whose corresponding string table
 *			data we now want.
 *    strscn_name -	name of the string section (in case sh_link is NULL)
 */

static Elf_Data *
get_string_data(Elf *elf, GElf_Ehdr *ehdr, const char *strscn_name,
    GElf_Shdr *shdr)
{
	Elf_Scn	*str_scn;
	Elf_Data *str_data;

	/*
	 * The ELF index of the string table for a section is [normally]
	 * found through the shdr->sh_link value.
	 */
	if (shdr->sh_link != 0) {
		if ((str_scn = elf_getscn(elf, shdr->sh_link)) == NULL) {
			(void) printf("(ELF error) file %s: elf_getscn\n",
			    in_fname);
			return ((Elf_Data *)S_ERROR);
		}
	} else {
		/*
		 * Normally the sh_link field of a section header (e.g. the
		 * .stab section's header) should provide the index of the
		 * corresponding string table section (e.g. the .stabstr
		 * section's index).
		 * But if we get here, that's not set correctly:
		 * This can happen if the compiler has goofed when doing .stab
		 * and .stabstr., so we will try to find the stab strings
		 * section by name within the ELF object.
		 */
		size_t	strscn_ndx;

		strscn_ndx = find_named_section(elf, ehdr, strscn_name,
		    (GElf_Word)SHT_STRTAB,
		    shdr->sh_flags, &str_scn);
		if (strscn_ndx == 0) {
			(void) printf("(ELF Fatal error) file %s "
			    ".stab[.index] has no corresponding "
			    "string table\n",
			    in_fname);
			return ((Elf_Data *)S_ERROR);
		} else if (strscn_ndx == (size_t)-1) {
			(void) printf("(ELF Fatal error) file %s "
			    "error reading .stab[.index] string table\n",
			    in_fname);

			return ((Elf_Data *)S_ERROR);
		}
	}

	if ((str_data = elf_getdata(str_scn, NULL)) == NULL) {
		(void) printf("(ELF error) file %s elf_getdata\n", in_fname);
		return ((Elf_Data *)S_ERROR);
	}

	return (str_data);
}

/*
 * ld_section_handler -
 *	Do the work for ld_section[64] (see below), but with just one
 * common implementation (single piece of code) that does it using the
 * generic (GElf) data types.
 *
 * Args:
 *	name	- pointer to name of current section being processed.
 *	shdr	- pointer to Section Header of current in-file being
 *		  processed.
 *	s_data	- pointer to Section Data structure of current in-file
 *		  being processed.
 *	elf	- pointer to elf structure for current in-file being
 *		  processed
 */
/* ARGSUSED2 */
static void
ld_section_handler(const char *scn_name, GElf_Shdr *shdr, GElf_Word scnndx,
    Elf_Data *s_data, Elf *elf)
{
	GElf_Ehdr	gelf_ehdr;

	if (gelf_getehdr(elf, &gelf_ehdr) == 0) {
		(void) fprintf(stderr, gettext("ldcullstabs: Fatal error, "
		    "file: %s: gelf_getehdr() failed: %s\n"),
		    in_fname, elf_errmsg(0));
		exit(EXIT_FAILURE);
	}

	/*
	 * Only operate on relocatable objects (.o files).
	 * We assume the link editor is producing a shared object
	 */
	if (gelf_ehdr.e_type != ET_REL) {
		return;
	}

	/*
	 * Minor optimization for speed.  If section name
	 * does not begin with "stab" we don't call strcmp().
	 */
	if ((scn_name[1] == 's') &&
	    (scn_name[2] == 't') &&
	    (scn_name[3] == 'a') &&
	    (scn_name[4] == 'b')) {

		if (strcmp(scn_name, StabName) == 0) {
			/*
			 * Process .stab section
			 *
			 * Note: Set initial offset into .stabstr
			 * string table to zero (a little hacky
			 * doing it here, but ...)
			 */
			select_stabs(elf, &gelf_ehdr, shdr, s_data);

		} else if ((strcmp(scn_name, StabIndxName) == 0) ||
		    (strcmp(scn_name, StabIndxStrName) == 0)) {
			/*
			 * Truncate the .stab.index and .stab.indexstr
			 * sections if seen.
			 */
			s_data->d_size = 0;
			s_data->d_buf = 0;
		}
	}
#ifdef __i386
	/*
	 * Also eliminate any relocations on stabs (the .rel.stab
	 * section found in i386 .o's)
	 * Note: same minor speed optimization as above.
	 * Look for "rel" prefix in section name string before strcmp()
	 */
	if ((scn_name[1] == 'r') &&
	    (scn_name[2] == 'e') &&
	    (scn_name[3] == 'l') &&
	    (strcmp(scn_name, StabRelocName) == 0)) {

		Elf32_Rel	*newrel_buf;
		Elf32_Rel	*rel;
		int		n_rels;

		/* debug */
		if (debug_cull >= 1)
			(void) printf("   ldcullstabs, seeing: (%s)\n",
			    scn_name);

		/*
		 * Regrettably we can't modify the section header
		 * (to indicate zero size) since that's mapped in
		 * by libelf as a read-only page.  So we'll allocate
		 * a new data buffer of the same size but fill it
		 * with NULL relocations (calloc zeroes the buffer).
		 */

		newrel_buf = malloc((size_t)shdr->sh_size);
		if (newrel_buf == 0) {
			(void) fprintf(stderr, gettext("ldcullstabs: Fatal "
			    "error, file: %s: malloc for .rel.stab "
			    "failed\n"), in_fname);
			exit(EXIT_FAILURE);
		}
		/*
		 * Copy relocations to new buffer, then modify
		 * all relocations to point to the same stab - a
		 * dummy stab we created earlier just as a target for
		 * them all.
		 */
		memmove(newrel_buf, s_data->d_buf, s_data->d_size);
		n_rels = shdr->sh_size / shdr->sh_entsize;
		/* debugging */
		if (debug_cull >= 1)
		    (void) printf("    .rel.stab has %d relocations\n", n_rels);
		rel = newrel_buf;
		while (n_rels--) {
			/* debugging */
			if (debug_cull >= 2) {
			    (void) printf("  rel(%ld) offset 0x%lx -> 0x%x\n",
				(long)(rel - newrel_buf), rel->r_offset,
				DummyStab_offset);
			}
			rel->r_offset = DummyStab_offset;
			rel++;
		}
		s_data->d_buf = newrel_buf;
		if (debug_cull >= 1)
			(void) printf("    relocations nullified: (%s)\n",
			    scn_name);
	}
#endif /* __i386 */
}

/* ARGSUSED1 */
void
ld_file(const char *name, const Elf_Kind kind, int flags, Elf *elf)
{
	in_fname = name;
}
/* ARGSUSED1 */
void
ld_file64(const char *name, const Elf_Kind kind, int flags, Elf *elf)
{
	in_fname = name;
}


/*
 * ld_section -
 *	Convert from use of the two-compiles method (e.g. Ehdr)
 * to use of Generic ELF (gelf) interfaces (e.g. GElf_Ehdr)
 */
/* ARGSUSED2 */
void
ld_section(const char *scn_name, Elf32_Shdr *shdr, Elf32_Word scnndx,
    Elf_Data *s_data, Elf *elf)
{
	GElf_Shdr	gelf_shdr;
	GElf_Word	gelf_scn_ndx;

	gelf_scn_ndx = scnndx;

	gelf_shdr.sh_name = shdr->sh_name;		/* section name */
	gelf_shdr.sh_type = shdr->sh_type;		/* SHT_... */
	gelf_shdr.sh_flags = shdr->sh_flags;		/* SHF_... */
	gelf_shdr.sh_addr = shdr->sh_addr;		/* virtual address */
	gelf_shdr.sh_offset = shdr->sh_offset;		/* file offset */
	gelf_shdr.sh_size = shdr->sh_size;		/* section size */
	gelf_shdr.sh_link = shdr->sh_link;		/* misc info */
	gelf_shdr.sh_info = shdr->sh_info;		/* misc info */
	gelf_shdr.sh_addralign = shdr->sh_addralign;	/* memory alignment */
	/* entry size of table */
	gelf_shdr.sh_entsize = shdr->sh_entsize;

	ld_section_handler(scn_name, &gelf_shdr, gelf_scn_ndx, s_data, elf);
}

/* ARGSUSED2 */
void
ld_section64(const char *scn_name, Elf64_Shdr *shdr, Elf64_Word scnndx,
    Elf_Data *s_data, Elf *elf)
{
	ld_section_handler(scn_name, (GElf_Shdr *)shdr, (GElf_Word) scnndx,
	    s_data, elf);
}
