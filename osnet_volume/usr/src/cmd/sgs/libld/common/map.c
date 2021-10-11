/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)map.c	1.59	99/06/01 SMI"

/*
 * Map file parsing.
 */
#include	<fcntl.h>
#include	<string.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<errno.h>
#include	<limits.h>
#include	<dirent.h>
#include	"msg.h"
#include	"_libld.h"
#include	"_ld.h"
#include	"debug.h"

#ifdef _ELF64
#define	STRTOADDR	strtoull
#define	XWORD_MAX	ULLONG_MAX
#else	/* Elf32 */
#define	STRTOADDR	strtoul
#define	XWORD_MAX	UINT_MAX
#endif /* _ELF64 */

typedef enum {
	TK_STRING,	TK_COLON,	TK_SEMICOLON,	TK_EQUAL,
	TK_ATSIGN,	TK_DASH,	TK_LEFTBKT,	TK_RIGHTBKT,
	TK_PIPE,	TK_EOF
} Token;			/* Possible return values from gettoken. */


static char		*Mapspace;	/* Malloc space holding map file. */
static unsigned long	Line_num;	/* Current map file line number. */
static char		*Start_tok;	/* First character of current token. */
static char		*nextchr;	/* Next char in mapfile to examine. */

/*
 * Convert a string to lowercase.
 */
static void
lowercase(char *str)
{
	while (*str = tolower(*str))
		str++;
}


/*
 * Get a token from the mapfile.
 */
static Token
gettoken(const char *mapfile)
{
	static char	oldchr = '\0';	/* Char at end of current token. */
	char		*end;		/* End of the current token. */


	/* Cycle through the characters looking for tokens. */
	for (;;) {
		if (oldchr != '\0') {
			*nextchr = oldchr;
			oldchr = '\0';
		}
		if (!isascii(*nextchr) ||
		    (!isprint(*nextchr) && !isspace(*nextchr) &&
		    (*nextchr != '\0'))) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_ILLCHAR),
			    mapfile, EC_XWORD(Line_num),
			    *((unsigned char *) nextchr));
			/* LINTED */
			return ((Token)S_ERROR);
		}
		switch (*nextchr) {
		case '\0':	/* End of file. */
			return (TK_EOF);
		case ' ':	/* White space. */
		case '\t':
			nextchr++;
			break;
		case '\n':	/* White space too, but bump line number. */
			nextchr++;
			Line_num++;
			break;
		case '#':	/* Comment. */
			while (*nextchr != '\n' && *nextchr != '\0')
				nextchr++;
			break;
		case ':':
			nextchr++;
			return (TK_COLON);
		case ';':
			nextchr++;
			return (TK_SEMICOLON);
		case '=':
			nextchr++;
			return (TK_EQUAL);
		case '@':
			nextchr++;
			return (TK_ATSIGN);
		case '-':
			nextchr++;
			return (TK_DASH);
		case '|':
			nextchr++;
			return (TK_PIPE);
		case '{':
			nextchr++;
			return (TK_LEFTBKT);
		case '}':
			nextchr++;
			return (TK_RIGHTBKT);
		case '"':
			Start_tok = ++nextchr;
			if (((end = strpbrk(nextchr,
			    MSG_ORIG(MSG_MAP_TOK_1))) == NULL) ||
			    (*end != '"')) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_NOTERM),
				    mapfile, EC_XWORD(Line_num));
				/* LINTED */
				return ((Token)S_ERROR);
			}
			*end = '\0';
			nextchr = end + 1;
			return (TK_STRING);
		default:	/* string. */
			Start_tok = nextchr;		/* CSTYLED */
			end = strpbrk(nextchr, MSG_ORIG(MSG_MAP_TOK_2));
			if (end == NULL)
				nextchr = Start_tok + strlen(Start_tok);
			else {
				nextchr = end;
				oldchr = *nextchr;
				*nextchr = '\0';
			}
			return (TK_STRING);
		}
	}
}


/*
 * Process a mapfile segment declaration definition.
 *	segment_name	= segment_attribute;
 * 	segment_attribute : segment_type  segment_flags  virtual_addr
 *			    physical_addr  length alignment
 */
static uintptr_t
map_equal(const char *mapfile, Sg_desc *sgp, Ofl_desc *ofl)
{
	Token	tok;			/* Current token. */
	Boolean	b_type  = FALSE;	/* True if seg types found. */
	Boolean	b_flags = FALSE;	/* True if seg flags found. */
	Boolean	b_len   = FALSE;	/* True if seg length found. */
	Boolean	b_round = FALSE;	/* True if seg rounding found. */
	Boolean	b_vaddr = FALSE;	/* True if seg virtual addr found. */
	Boolean	b_paddr = FALSE;	/* True if seg physical addr found. */
	Boolean	b_align = FALSE;	/* True if seg alignment found. */

	while ((tok = gettoken(mapfile)) != TK_SEMICOLON) {
		/* LINTED */
		if (tok == (Token)S_ERROR)
			return (S_ERROR);
		if (tok == TK_EOF) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_PREMEOF),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}
		if (tok != TK_STRING) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSEGATT),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}

		lowercase(Start_tok);

		/*
		 * Segment type.  Presently there can only be multiple
		 * PT_LOAD and PT_NOTE segments, other segment types are
		 * only defined in seg_desc[].
		 */
		if (strcmp(Start_tok, MSG_ORIG(MSG_MAP_LOAD)) == 0) {
			if (b_type) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_MOREONCE),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGTYP));
				return (S_ERROR);
			}
			if ((sgp->sg_flags & FLG_SG_TYPE) &&
			    (sgp->sg_phdr.p_type != PT_LOAD))
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_REDEFATT),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGTYP), sgp->sg_name);
			sgp->sg_phdr.p_type = PT_LOAD;
			sgp->sg_flags |= FLG_SG_TYPE;
			b_type = TRUE;
		} else if (strcmp(Start_tok, MSG_ORIG(MSG_MAP_NOTE)) == 0) {
			if (b_type) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_MOREONCE),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGTYP));
				return (S_ERROR);
			}
			if ((sgp->sg_flags & FLG_SG_TYPE) &&
			    (sgp->sg_phdr.p_type != PT_NOTE))
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_REDEFATT),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGTYP), sgp->sg_name);
			sgp->sg_phdr.p_type = PT_NOTE;
			sgp->sg_flags |= FLG_SG_TYPE;
			b_type = TRUE;
		}


		/* Segment Flags. */

		else if (*Start_tok == '?') {
			Word	tmp_flags = 0;
			char	*flag_tok = Start_tok + 1;

			if (b_flags) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_MOREONCE),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGFLAG));
				return (S_ERROR);
			}

			/*
			 * If ? has nothing following leave the flags cleared,
			 * otherwise or in any flags specified.
			 */
			if (*flag_tok) {
				while (*flag_tok) {
					switch (*flag_tok) {
					case 'r':
						tmp_flags |= PF_R;
						break;
					case 'w':
						tmp_flags |= PF_W;
						break;
					case 'x':
						tmp_flags |= PF_X;
						break;
					case 'e':
						sgp->sg_flags |= FLG_SG_EMPTY;
						break;
					case 'o':
						sgp->sg_flags |= FLG_SG_ORDER;
						ofl->ofl_flags |=
							FLG_OF_SEGORDER;
						break;
					case 'n':
						sgp->sg_flags |= FLG_SG_NOHDR;
						break;
					default:
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_UNKSEGFLG),
						    mapfile, EC_XWORD(Line_num),
						    *flag_tok);
						return (S_ERROR);
					}
					flag_tok++;
				}
			}
			if ((sgp->sg_flags & FLG_SG_FLAGS) &&
			    (sgp->sg_phdr.p_flags != tmp_flags))
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_REDEFATT),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGFLAG), sgp->sg_name);
			sgp->sg_flags |= FLG_SG_FLAGS;
			sgp->sg_phdr.p_flags = tmp_flags;
			b_flags = TRUE;
		}


		/* Segment address, length, alignment or rounding number. */

		else if (Start_tok[0] == 'l' || Start_tok[0] == 'v' ||
			Start_tok[0] == 'a' || Start_tok[0] == 'p' ||
			Start_tok[0] == 'r') {
			char		*end_tok;
			Lword		number;

			if ((number = (Lword)STRTOADDR(&Start_tok[1], &end_tok,
			    0))	>= XWORD_MAX) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGADDR),
				    mapfile, EC_XWORD(Line_num), Start_tok,
				    MSG_INTL(MSG_MAP_EXCLIMIT));
				return (S_ERROR);
			}

			if (end_tok != strchr(Start_tok, '\0')) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGADDR),
				    mapfile, EC_XWORD(Line_num), Start_tok,
				    MSG_INTL(MSG_MAP_NOBADFRM));
				return (S_ERROR);
			}

			switch (*Start_tok) {
			case 'l':
				if (b_len) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_MOREONCE),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGLEN));
					return (S_ERROR);
				}
				if ((sgp->sg_flags & FLG_SG_LENGTH) &&
				    (sgp->sg_length != number))
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_MAP_REDEFATT),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGLEN),
					    sgp->sg_name);
				/* LINTED */
				sgp->sg_length = (Xword)number;
				sgp->sg_flags |= FLG_SG_LENGTH;
				b_len = TRUE;
				break;
			case 'r':
				if (b_round) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_MOREONCE),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGROUND));
					return (S_ERROR);
				}
				if ((sgp->sg_flags & FLG_SG_ROUND) &&
				    (sgp->sg_round != number))
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_MAP_REDEFATT),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGROUND),
					    sgp->sg_name);
				/* LINTED */
				sgp->sg_round = (Xword)number;
				sgp->sg_flags |= FLG_SG_ROUND;
				b_round = TRUE;
				break;
			case 'v':
				if (b_vaddr) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_MOREONCE),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGVADDR));
					return (S_ERROR);
				}
				if ((sgp->sg_flags & FLG_SG_VADDR) &&
				    (sgp->sg_phdr.p_vaddr != number))
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_MAP_REDEFATT),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGVADDR),
					    sgp->sg_name);
				/* LINTED */
				sgp->sg_phdr.p_vaddr = (Addr)number;
				sgp->sg_flags |= FLG_SG_VADDR;
				b_vaddr = TRUE;
				break;
			case 'p':
				if (b_paddr) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_MOREONCE),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGPHYS));
					return (S_ERROR);
				}
				if ((sgp->sg_flags & FLG_SG_PADDR) &&
				    (sgp->sg_phdr.p_paddr != number))
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_MAP_REDEFATT),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGPHYS),
					    sgp->sg_name);
				/* LINTED */
				sgp->sg_phdr.p_paddr = (Addr)number;
				sgp->sg_flags |= FLG_SG_PADDR;
				b_paddr = TRUE;
				break;
			case 'a':
				if (b_align) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_MOREONCE),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGALIGN));
					return (S_ERROR);
				}
				if ((sgp->sg_flags & FLG_SG_ALIGN) &&
				    (sgp->sg_phdr.p_align != number))
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_MAP_REDEFATT),
					    mapfile, EC_XWORD(Line_num),
					    MSG_INTL(MSG_MAP_SEGALIGN),
					    sgp->sg_name);
				/* LINTED */
				sgp->sg_phdr.p_align = (Xword)number;
				sgp->sg_flags |= FLG_SG_ALIGN;
				b_align = TRUE;
				break;
			}
		} else {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_UNKSEGATT),
			    mapfile, EC_XWORD(Line_num), Start_tok);
			return (S_ERROR);
		}
	}

	/*
	 * Segment reservations are only allowable for executables. In addition
	 * they must have an associated address, size, no permisions,
	 * and are only meaningful for LOAD segments (the last failure
	 * we can correct, hence the warning condition).
	 */
	if (sgp->sg_flags & FLG_SG_EMPTY) {
		if (!(ofl->ofl_flags & FLG_OF_EXEC)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGEMPEXE),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}
		if (sgp->sg_phdr.p_flags != 0) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGEMNOPERM),
				mapfile, EC_XWORD(Line_num),
				EC_WORD(sgp->sg_phdr.p_flags));
			return (S_ERROR);
		}
		if ((sgp->sg_flags & (FLG_SG_LENGTH | FLG_SG_VADDR)) !=
		    (FLG_SG_LENGTH | FLG_SG_VADDR)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGEMPATT),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}
		if (sgp->sg_phdr.p_type != PT_LOAD) {
			eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_SEGEMPLOAD),
			    mapfile, EC_XWORD(Line_num));
			sgp->sg_phdr.p_type = PT_LOAD;
		}
	}

	/*
	 * All segment attributes have now been scanned.  Certain flags do not
	 * make sense if this is not a loadable segment, fix if necessary.
	 * Note, if the segment is of type PT_NULL it must be new, and any
	 * defaults will be applied back in map_parse().
	 * When clearing an attribute leave the flag set as an indicator for
	 * later entries re-specifying the same segment.
	 */
	if (sgp->sg_phdr.p_type != PT_NULL && sgp->sg_phdr.p_type != PT_LOAD) {
		if (sgp->sg_flags & FLG_SG_FLAGS)
			if (sgp->sg_phdr.p_flags != 0) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_NONLOAD),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGFLAG));
				sgp->sg_phdr.p_flags = 0;
			}
		if (sgp->sg_flags & FLG_SG_LENGTH)
			if (sgp->sg_length != 0) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_NONLOAD),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGLEN));
				sgp->sg_length = 0;
			}
		if (sgp->sg_flags & FLG_SG_ROUND)
			if (sgp->sg_round != 0) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_NONLOAD),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGROUND));
				sgp->sg_round = 0;
			}
		if (sgp->sg_flags & FLG_SG_VADDR)
			if (sgp->sg_phdr.p_vaddr != 0) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_NONLOAD),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGVADDR));
			}
		if (sgp->sg_flags & FLG_SG_PADDR)
			if (sgp->sg_phdr.p_paddr != 0) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_NONLOAD),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGPHYS));
				sgp->sg_phdr.p_paddr = 0;
			}
		if (sgp->sg_flags & FLG_SG_ALIGN)
			if (sgp->sg_phdr.p_align != 0) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_NONLOAD),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SEGALIGN));
				sgp->sg_phdr.p_align = 0;
			}
	}
	return (1);
}


/*
 * Process a mapfile mapping directives definition.
 * 	segment_name : section_attribute [ : file_name ]
 * 	segment_attribute : section_name section_type section_flags;
 */
static uintptr_t
map_colon(const char *mapfile, Ent_desc *enp)
{
	Token		tok;		/* Current token. */

	Boolean		b_name = FALSE;
	Boolean		b_type = FALSE;
	Boolean		b_attr = FALSE;
	Boolean		b_bang = FALSE;
	static	Xword	index = 0;


	while ((tok = gettoken(mapfile)) != TK_COLON && tok != TK_SEMICOLON) {
		/* LINTED */
		if (tok == (Token)S_ERROR)
			return (S_ERROR);
		if (tok == TK_EOF) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_PREMEOF),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}

		/* Segment type. */

		if (*Start_tok == '$') {
			if (b_type) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_MOREONCE),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SECTYP));
				return (S_ERROR);
			}
			b_type = TRUE;
			Start_tok++;
			lowercase(Start_tok);
			if (strcmp(Start_tok, MSG_ORIG(MSG_STR_PROGBITS)) == 0)
				enp->ec_type = SHT_PROGBITS;
			else if (strcmp(Start_tok,
			    MSG_ORIG(MSG_STR_SYMTAB)) == 0)
				enp->ec_type = SHT_SYMTAB;
			else if (strcmp(Start_tok,
			    MSG_ORIG(MSG_STR_DYNSYM)) == 0)
				enp->ec_type = SHT_DYNSYM;
			else if (strcmp(Start_tok,
			    MSG_ORIG(MSG_STR_STRTAB)) == 0)
				enp->ec_type = SHT_STRTAB;
			else if ((strcmp(Start_tok,
			    MSG_ORIG(MSG_STR_REL)) == 0) ||
			    (strcmp(Start_tok, MSG_ORIG(MSG_STR_RELA)) == 0))
				enp->ec_type = M_OBJREL_SHT_TYPE;
			else if (strcmp(Start_tok, MSG_ORIG(MSG_STR_HASH)) == 0)
				enp->ec_type = SHT_HASH;
			else if (strcmp(Start_tok, MSG_ORIG(MSG_STR_LIB)) == 0)
				enp->ec_type = SHT_SHLIB;
			else if (strcmp(Start_tok,
			    MSG_ORIG(MSG_STR_LD_DYNAMIC)) == 0)
				enp->ec_type = SHT_DYNAMIC;
			else if (strcmp(Start_tok, MSG_ORIG(MSG_STR_NOTE)) == 0)
				enp->ec_type = SHT_NOTE;
			else if (strcmp(Start_tok,
			    MSG_ORIG(MSG_STR_NOBITS)) == 0)
				enp->ec_type = SHT_NOBITS;
			else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_UNKSECTYP),
				    mapfile, EC_XWORD(Line_num), Start_tok);
				return (S_ERROR);
			}

		/*
		 * Segment flags.
		 * If a segment flag is specified then the appropriate bit is
		 * set in the ec_attrmask, the ec_attrbits fields determine
		 * whether the attrmask fields must be tested true or false
		 * ie.	for  ?A the attrmask is set and the attrbit is set,
		 *	for ?!A the attrmask is set and the attrbit is clear.
		 */
		} else if (*Start_tok == '?') {
			if (b_attr) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_MOREONCE),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SECFLAG));
				return (S_ERROR);
			}
			b_attr = TRUE;
			b_bang = FALSE;
			Start_tok++;
			lowercase(Start_tok);
			for (; *Start_tok != '\0'; Start_tok++)
				switch (*Start_tok) {
				case '!':
					if (b_bang) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_BADFLAG),
						    mapfile, EC_XWORD(Line_num),
						    Start_tok);
						return (S_ERROR);
					}
					b_bang = TRUE;
					break;
				case 'a':
					if (enp->ec_attrmask & SHF_ALLOC) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_BADFLAG),
						    mapfile, EC_XWORD(Line_num),
						    Start_tok);
						return (S_ERROR);
					}
					enp->ec_attrmask |= SHF_ALLOC;
					if (!b_bang)
						enp->ec_attrbits |= SHF_ALLOC;
					b_bang = FALSE;
					break;
				case 'w':
					if (enp->ec_attrmask & SHF_WRITE) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_BADFLAG),
						    mapfile, EC_XWORD(Line_num),
						    Start_tok);
						return (S_ERROR);
					}
					enp->ec_attrmask |= SHF_WRITE;
					if (!b_bang)
						enp->ec_attrbits |= SHF_WRITE;
					b_bang = FALSE;
					break;
				case 'x':
					if (enp->ec_attrmask & SHF_EXECINSTR) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_BADFLAG),
						    mapfile, EC_XWORD(Line_num),
						    Start_tok);
						return (S_ERROR);
					}
					enp->ec_attrmask |= SHF_EXECINSTR;
					if (!b_bang)
					    enp->ec_attrbits |= SHF_EXECINSTR;
					b_bang = FALSE;
					break;
				default:
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_BADFLAG),
					    mapfile, EC_XWORD(Line_num),
					    Start_tok);
					return (S_ERROR);
				}
		/*
		 * Section name.
		 */
		} else {
			if (b_name) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_MOREONCE),
				    mapfile, EC_XWORD(Line_num),
				    MSG_INTL(MSG_MAP_SECNAME));
				return (S_ERROR);
			}
			b_name = TRUE;
			if ((enp->ec_name =
			    (char *)libld_malloc(strlen(Start_tok) + 1)) == 0)
				return (S_ERROR);
			(void) strcpy((char *)enp->ec_name, Start_tok);
			/*
			 * get the index for text reordering
			 */
			/* LINTED */
			enp->ec_ndx = (Word)++index;
		}
	}
	if (tok == TK_COLON) {
		/*
		 * File names.
		 */
		while ((tok = gettoken(mapfile)) != TK_SEMICOLON) {
			char	*file;

			/* LINTED */
			if (tok == (Token)S_ERROR)
				return (S_ERROR);
			if (tok == TK_EOF) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_PREMEOF),
				    mapfile, EC_XWORD(Line_num));
				return (S_ERROR);
			}
			if (tok != TK_STRING) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_MALFORM),
				    mapfile, EC_XWORD(Line_num));
				return (S_ERROR);
			}
			if ((file =
			    (char *)libld_malloc(strlen(Start_tok) + 1)) == 0)
				return (S_ERROR);
			(void) strcpy(file, Start_tok);
			if (list_appendc(&(enp->ec_files), file) == 0)
				return (S_ERROR);
		}
	}
	return (1);
}

/*
 * Obtain a pseudo input file descriptor to assign to a mapfile.  This is
 * required any time a symbol is generated.  First traverse the input file
 * descriptors looking for a match.  As all mapfile processing occurs before
 * any real input file processing this list is going to be small and we don't
 * need to do any filename clash checking.
 */
static Ifl_desc *
map_ifl(const char *mapfile, Ofl_desc *ofl)
{
	Ifl_desc	*ifl;
	Listnode	*lnp;

	for (LIST_TRAVERSE(&ofl->ofl_objs, lnp, ifl))
		if (strcmp(ifl->ifl_name, mapfile) == 0)
			return (ifl);

	if ((ifl = (Ifl_desc *)libld_calloc(sizeof (Ifl_desc), 1)) == 0)
		return ((Ifl_desc *)S_ERROR);
	ifl->ifl_name = mapfile;
	if ((ifl->ifl_ehdr = (Ehdr *)libld_calloc(sizeof (Ehdr), 1)) == 0)
		return ((Ifl_desc *)S_ERROR);
	ifl->ifl_ehdr->e_type = ET_REL;

	if (list_appendc(&ofl->ofl_objs, ifl) == 0)
		return ((Ifl_desc *)S_ERROR);
	else
		return (ifl);
}

/*
 * Process a mapfile size symbol definition.
 * 	segment_name @ symbol_name;
 */
static uintptr_t
map_atsign(const char *mapfile, Sg_desc *sgp, Ofl_desc *ofl)
{
	Sym		*sym;		/* New symbol pointer */
	Sym_desc	*sdp;		/* New symbol node pointer */
	Ifl_desc	*ifl;		/* Dummy input file structure */
	Token		tok;		/* Current token. */

	if ((tok = gettoken(mapfile)) != TK_STRING) {
		/* LINTED */
		if (tok != (Token)S_ERROR)
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSYM_1),
			    mapfile, EC_XWORD(Line_num));
		return (S_ERROR);
	}

	if (sgp->sg_sizesym != NULL) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGSIZE), mapfile,
			EC_XWORD(Line_num), sgp->sg_name);
		return (S_ERROR);
	}

	/*
	 * Make sure we have a pseudo file descriptor to associate to the
	 * symbol.
	 */
	if ((ifl = map_ifl(mapfile, ofl)) == (Ifl_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Make sure the symbol doesn't already exist.  It is possible that the
	 * symbol has been scoped or versioned, in which case it does exist
	 * but we can freely update it here.
	 */
	if ((sdp = sym_find(Start_tok, SYM_NOHASH, ofl)) == NULL) {
		char	*name;
		Word hval;

		if ((name = (char *)libld_malloc(strlen(Start_tok) + 1)) == 0)
			return (S_ERROR);
		(void) strcpy(name, Start_tok);

		if ((sym = (Sym *)libld_calloc(sizeof (Sym), 1)) == 0)
			return (S_ERROR);
		sym->st_shndx = SHN_ABS;
		sym->st_size = 0;
		sym->st_info = ELF_ST_INFO(STB_GLOBAL, STT_OBJECT);

		DBG_CALL(Dbg_map_size_new(name));
		/* LINTED */
		hval = (Word)elf_hash(name);
		if ((sdp = sym_enter(name, sym, hval, ifl, ofl, 0)) ==
		    (Sym_desc *)S_ERROR)
			return (S_ERROR);
		sdp->sd_flags &= ~FLG_SY_CLEAN;
		sdp->sd_flags |= (FLG_SY_MAPREF | FLG_SY_GLOBREF);
		DBG_CALL(Dbg_map_symbol(&def_ehdr, sdp));
	} else {
		sym = sdp->sd_sym;

		if (sym->st_shndx == SHN_UNDEF) {
			sym->st_shndx = SHN_ABS;
			sym->st_size = 0;
			sym->st_info = ELF_ST_INFO(STB_GLOBAL, STT_OBJECT);
			DBG_CALL(Dbg_map_size_old(&def_ehdr, sdp));
		} else {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SYMDEF), mapfile,
			    EC_XWORD(Line_num), sdp->sd_name,
			    sdp->sd_file->ifl_name);
			return (S_ERROR);
		}
	}

	/*
	 * Assign the symbol to the segment and indicate the symbols use (this
	 * flag enables any version definitions to redefine the symbols scope).
	 */
	sgp->sg_sizesym = sdp;
	sdp->sd_flags |= FLG_SY_SEGSIZE;

	if (gettoken(mapfile) != TK_SEMICOLON) {
		/* LINTED */
		if (tok != (Token)S_ERROR)
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSCOL),
			    mapfile, EC_XWORD(Line_num));
		return (S_ERROR);
	} else
		return (1);
}


static uintptr_t
map_pipe(const char *mapfile, Sg_desc *sgp)
{
	char		*sec_name;	/* section name */
	Token		tok;		/* current token. */
	Sec_order	*sc_order;
	static Word	index = 0;	/* used to maintain a increasing */
					/* 	index for section ordering. */

	if ((tok = gettoken(mapfile)) != TK_STRING) {
		/* LINTED */
		if (tok != (Token)S_ERROR)
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSEC), mapfile,
			    EC_XWORD(Line_num));
		return (S_ERROR);
	}

	if ((sec_name = (char *)libld_malloc(strlen(Start_tok) + 1)) == 0)
		return (S_ERROR);
	(void) strcpy(sec_name, Start_tok);

	if ((sc_order = (Sec_order *)libld_malloc(sizeof (Sec_order))) == 0)
		return (S_ERROR);

	sc_order->sco_secname = sec_name;
	sc_order->sco_index = ++index;

	if (list_appendc(&(sgp->sg_secorder), sc_order) == 0)
		return (S_ERROR);

	DBG_CALL(Dbg_map_pipe(sgp, sec_name, index));

	if ((tok = gettoken(mapfile)) != TK_SEMICOLON) {
		/* LINTED */
		if (tok != (Token)S_ERROR)
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSCOL),
			    mapfile, EC_XWORD(Line_num));
		return (S_ERROR);
	}

	return (1);
} 


/*
 * Process a mapfile library specification definition.
 * 	shared_object_name - shared object definition
 *	shared object definition : [ shared object type [ = SONAME ]]
 *					[ versions ];
 */
static uintptr_t
map_dash(const char *mapfile, char *name, Ofl_desc *ofl)
{
	char		*version;
	Token		tok;
	Sdf_desc	*sdf;
	Sdv_desc	*sdv;
	enum {
	    MD_NONE = 0,
	    MD_SPECVERS,
	    MD_ADDVERS,
	    MD_NEEDED
	}		dolkey = MD_NONE;


	/*
	 * If a shared object definition for this file already exists use it,
	 * otherwise allocate a new descriptor.
	 */
	if ((sdf = sdf_find(name, &ofl->ofl_socntl)) == 0) {
		if ((sdf = sdf_add(name, &ofl->ofl_socntl)) ==
		    (Sdf_desc *)S_ERROR)
			return (S_ERROR);
		sdf->sdf_rfile = mapfile;
	}

	/*
	 * Get the shared object descriptor string.
	 */
	while ((tok = gettoken(mapfile)) != TK_SEMICOLON) {
		/* LINTED */
		if (tok == (Token)S_ERROR)
			return (S_ERROR);
		if (tok == TK_EOF) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_PREMEOF),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}
		if ((tok != TK_STRING) && (tok != TK_EQUAL)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSO),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}

		/*
		 * Determine if the library type is accompanied with a SONAME
		 * definition.
		 */
		if (tok == TK_EQUAL) {
			if ((tok = gettoken(mapfile)) != TK_STRING) {
				/* LINTED */
				if (tok == (Token)S_ERROR)
					return (S_ERROR);
				else {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_EXPSO), mapfile,
					    EC_XWORD(Line_num));
					return (S_ERROR);
				}
			}
			switch (dolkey) {
			case MD_NEEDED:
				if (sdf->sdf_flags & FLG_SDF_SONAME) {
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_MAP_MULSONAME),
					    mapfile, EC_XWORD(Line_num), name,
					    sdf->sdf_soname, Start_tok);
					dolkey = MD_NONE;
					continue;
				}
				if ((sdf->sdf_soname =
				    (char *)libld_malloc(strlen(Start_tok)
				    + 1)) == 0)
					return (S_ERROR);
				(void) strcpy((char *)sdf->sdf_soname,
					Start_tok);
				sdf->sdf_flags |= FLG_SDF_SONAME;
				break;
			case MD_SPECVERS:
			case MD_ADDVERS:
				if ((sdv = (Sdv_desc *)libld_calloc(
				    sizeof (Sdv_desc), 1)) == 0)
					return (S_ERROR);

				if (dolkey == MD_SPECVERS)
					sdf->sdf_flags |= FLG_SDF_SPECVER;
				else
					sdf->sdf_flags |= FLG_SDF_ADDVER;

				if ((sdf->sdf_flags & (FLG_SDF_SPECVER |
				    FLG_SDF_ADDVER)) == (FLG_SDF_SPECVER |
				    FLG_SDF_ADDVER)) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_INCOMFLG),
					    mapfile, EC_XWORD(Line_num),
					    sdf->sdf_name);
					return (S_ERROR);
				}
				if ((version = (char *)libld_malloc(
				    strlen(Start_tok) + 1)) == 0)
					return (S_ERROR);
				(void) strcpy(version, Start_tok);
				sdv->sdv_name = version;
				sdv->sdv_ref = mapfile;
				if (list_appendc(&sdf->sdf_verneed,
				    sdv) == 0)
					return (S_ERROR);
				break;
			case MD_NONE:
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_UNEXTOK),
				    mapfile, EC_XWORD(Line_num), '=');
				return (S_ERROR);
			}
			dolkey = MD_NONE;
			continue;
		}

		/*
		 * A shared object type has been specified.  This may also be
		 * accompanied by an SONAME redefinition (see above).
		 */
		if (*Start_tok == '$') {
			if (dolkey != MD_NONE) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_UNEXTOK),
				    mapfile, EC_XWORD(Line_num), '$');
				return (S_ERROR);
			}
			Start_tok++;
			lowercase(Start_tok);
			if (strcmp(Start_tok,
			    MSG_ORIG(MSG_MAP_NEED)) == 0)
				dolkey = MD_NEEDED;
			else if (strcmp(Start_tok,
			    MSG_ORIG(MSG_MAP_SPECVERS)) == 0)
				dolkey = MD_SPECVERS;
			else if (strcmp(Start_tok,
			    MSG_ORIG(MSG_MAP_ADDVERS)) == 0)
				dolkey = MD_ADDVERS;
			else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_UNKSOTYP),
				    mapfile, EC_XWORD(Line_num), Start_tok);
				return (S_ERROR);
			}
			continue;
		}

		/*
		 * shared object version requirement.
		 */
		if ((version = (char *)libld_malloc(strlen(Start_tok) +
		    1)) == 0)
			return (S_ERROR);
		(void) strcpy(version, Start_tok);
		if ((sdv = (Sdv_desc *)libld_calloc(sizeof (Sdv_desc), 1)) == 0)
			return (S_ERROR);
		sdv->sdv_name = version;
		sdv->sdv_ref = mapfile;
		sdf->sdf_flags |= FLG_SDF_SELECT;
		if (list_appendc(&sdf->sdf_vers, sdv) == 0)
			return (S_ERROR);
	}

	DBG_CALL(Dbg_map_dash(name, sdf));
	return (1);
}


/*
 * Process a symbol definition list
 * 	version name {
 *		local:
 *		    symbol { = (FUNC|OBJECT) (PASSTHRU|PARENT|EXTERN)
 *				($ABS|$COMMON) A V };
 *			*;
 *		global:
 *		symbolic:
 *		    symbol { = (FUNC|OBJECT) (PASSTHRU|PARENT|EXTERN)
 *				($ABS|$COMMON) A V };
 *	} [references];
 */
#define	SYMNO		50		/* Symbol block allocation amount */
#define	FLG_SCOPE_LOCL	0		/* local scope flag */
#define	FLG_SCOPE_GLOB	1		/* global scope flag */
#define	FLG_SCOPE_SYMB	2		/* symbolic scope flag */
#define	FLG_SCOPE_ELIM	3		/* eliminate symbol from symtabs */

static uintptr_t
map_version(const char *mapfile, char *name, Ofl_desc *ofl)
{
	Token		tok;
	Sym		*sym;
	int		scope = FLG_SCOPE_GLOB;
	Ver_desc	*vdp;
	Word		hash;
	Ifl_desc	*ifl;

	/*
	 * If we're generating segments within the image then any symbol
	 * reductions will be processed (ie. applied to relocations and symbol
	 * table entries).  Otherwise (when creating a relocatable object) any
	 * versioning information is simply recorded for use in a later
	 * (segment generating) link-edit.
	 */
	if (ofl->ofl_flags & FLG_OF_RELOBJ)
		ofl->ofl_flags |= FLG_OF_VERDEF;

	/*
	 * If this is a new mapfile reference generate an input file descriptor
	 * to represent it.  Otherwise this must simply be a new version within
	 * the mapfile we've previously been processing, in this case continue
	 * to use the original input file descriptor.
	 */
	if ((ifl = map_ifl(mapfile, ofl)) == (Ifl_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * If no version descriptors have yet been set up, initialize a base
	 * version to represent the output file itself.  This `base' version
	 * catches any internally generated symbols (_end, _etext, etc.) and
	 * serves to initialize the output version descriptor count.
	 */
	if (ofl->ofl_vercnt == 0) {
		if (vers_base(ofl) == (Ver_desc *)S_ERROR)
			return (S_ERROR);
	}

	/*
	 * If this definition has an associated version name then generate a
	 * new version descriptor and an associated version symbol index table.
	 */
	if (name) {
		ofl->ofl_flags |= FLG_OF_VERDEF;

		/*
		 * Traverse the present version descriptor list to see if there
		 * is already one of the same name, otherwise create a new one.
		 */
		/* LINTED */
		hash = (Word)elf_hash(name);
		if ((vdp = vers_find(name, hash, &ofl->ofl_verdesc)) == 0) {
			if ((vdp = vers_desc(name, hash,
			    &ofl->ofl_verdesc)) == (Ver_desc *)S_ERROR)
				return (S_ERROR);
		}

		/*
		 * Initialize any new version with an index, the file from which
		 * it was first referenced, and a WEAK flag (indicates that
		 * there are no symbols assigned to it yet).
		 */
		if (vdp->vd_ndx == 0) {
			/* LINTED */
			vdp->vd_ndx = (Half)++ofl->ofl_vercnt;
			vdp->vd_file = ifl;
			vdp->vd_flags = VER_FLG_WEAK;
		}
	} else {
		/*
		 * If a version definition hasn't been specified assign any
		 * symbols to the base version.
		 */
		vdp = (Ver_desc *)ofl->ofl_verdesc.head->data;
	}

	/*
	 * Scan the mapfile entry picking out scoping and symbol definitions.
	 */
	while ((tok = gettoken(mapfile)) != TK_RIGHTBKT) {
		Sym_desc * 	sdp;
		Half		shndx = SHN_UNDEF;
		unsigned char	type = STT_NOTYPE;
		Addr		value = 0;
		Addr		size = 0;
		char		*_name;
		unsigned long	sym_flags;

		sym_flags = 0;

		if ((tok != TK_STRING) && (tok != TK_COLON)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSYM_2),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}

		if ((_name = (char *)libld_malloc(strlen(Start_tok) + 1)) == 0)
			return (S_ERROR);
		(void) strcpy(_name, Start_tok);

		if ((tok != TK_COLON) &&
		    /* LINTED */
		    (tok = gettoken(mapfile)) == (Token)S_ERROR)
			return (S_ERROR);

		/*
		 * Turn off the WEAK flag to indicate that definitions are
		 * associated with this version.  It would probably be more
		 * accurate to only remove this flag with the specification of
		 * global symbols, however setting it here allows enough slop
		 * to compensate for the various user inputs we've seen so far.
		 * Only if a closed version is specified (i.e., "SUNW_1.x {};")
		 * will a user get a weak version (which is how we document the
		 * creation of weak versions).
		 */
		vdp->vd_flags &= ~VER_FLG_WEAK;

		switch (tok) {
		case TK_COLON:
			/*
			 * Establish a new scope.  All symbols added by this
			 * mapfile are actually global entries. They will be
			 * reduced to locals during sym_update().
			 */
			if (strcmp(MSG_ORIG(MSG_STR_LOCAL), _name) == 0)
				scope = FLG_SCOPE_LOCL;
			else if (strcmp(MSG_ORIG(MSG_MAP_GLOBAL), _name) == 0)
				scope = FLG_SCOPE_GLOB;
			else if (strcmp(MSG_ORIG(MSG_STR_SYMBOLIC), _name) == 0)
				scope = FLG_SCOPE_SYMB;
			else if (strcmp(MSG_ORIG(MSG_STR_ELIMINATE), _name)
			    == 0)
				scope = FLG_SCOPE_ELIM;
			else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_UNKSYMSCO),
				    mapfile, EC_XWORD(Line_num), _name);
				return (S_ERROR);
			}
			continue;

		case TK_EQUAL:
			/*
			 * A full blown symbol definition follows.
			 * Determine the symbol type and any virtual address or
			 * alignment specified and then fall through to process
			 * the entire symbols information.
			 */
			while ((tok = gettoken(mapfile)) != TK_SEMICOLON) {

				/*
				 * Determine any Value or Size attributes.
				 */
				lowercase(Start_tok);
				if (Start_tok[0] == 'v' ||
				    Start_tok[0] == 's') {
					char		*end_tok;
					Lword		number;

					if ((number = (Lword)STRTOADDR(
					    &Start_tok[1], &end_tok, 0)) ==
					    XWORD_MAX) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_SEGADDR),
						    mapfile, EC_XWORD(Line_num),
						    Start_tok,
						    MSG_INTL(MSG_MAP_EXCLIMIT));
						return (S_ERROR);
					}

					if (end_tok !=
					    strchr(Start_tok, '\0')) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_SEGADDR),
						    mapfile, EC_XWORD(Line_num),
						    Start_tok,
						    MSG_INTL(MSG_MAP_NOBADFRM));
						return (S_ERROR);
					}

					switch (*Start_tok) {
					case 'v':
					    if (value) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_MOREONCE),
						    mapfile, EC_XWORD(Line_num),
						    MSG_INTL(MSG_MAP_SYMVAL));
						return (S_ERROR);
					    }
					    /* LINTED */
					    value = (Addr)number;
					    break;
					case 's':
					    if (size) {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_MAP_MOREONCE),
						    mapfile, EC_XWORD(Line_num),
						    MSG_INTL(MSG_MAP_SYMSIZE));
						return (S_ERROR);
					    }
					    /* LINTED */
					    size = (Addr)number;
					    break;
					}

				} else if (strcmp(Start_tok,
				    MSG_ORIG(MSG_MAP_FUNCTION)) == 0) {
					shndx = SHN_ABS;
					type = STT_FUNC;
				} else if (strcmp(Start_tok,
				    MSG_ORIG(MSG_MAP_DATA)) == 0) {
					shndx = SHN_ABS;
					type = STT_OBJECT;
				} else if (strcmp(Start_tok,
				    MSG_ORIG(MSG_MAP_COMMON)) == 0) {
					shndx = SHN_COMMON;
					type = STT_OBJECT;
				} else if (strcmp(Start_tok,
				    MSG_ORIG(MSG_MAP_PASSTHRU)) == 0) {
					sym_flags |= FLG_SY_PASSTHRU;
				} else if (strcmp(Start_tok,
				    MSG_ORIG(MSG_MAP_PARENT)) == 0) {
					sym_flags |= FLG_SY_PARENT;
				} else if (strcmp(Start_tok,
				    MSG_ORIG(MSG_MAP_EXTERN)) == 0) {
					sym_flags |= FLG_SY_EXTERN;
				} else {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_UNKSYMDEF),
					    mapfile, EC_XWORD(Line_num),
					    Start_tok);
					return (S_ERROR);
				}
			}
			/* FALLTHROUGH */

		case TK_SEMICOLON:
			/*
			 * The special auto-reduction directive `*' can be
			 * specified in local scope and indicates that all
			 * symbols processed that are not explicitly defined to
			 * be global are to be reduced to local scope in the
			 * output image.  This also applies that a version
			 * definition is created as the user has effectively
			 * defined an interface.
			 */
			if (*_name == '*') {
				if (scope == FLG_SCOPE_LOCL)
					ofl->ofl_flags |=
					    (FLG_OF_AUTOLCL | FLG_OF_VERDEF);
				else if (scope == FLG_SCOPE_ELIM) {
					ofl->ofl_flags |= FLG_OF_VERDEF;
					ofl->ofl_flags1 |= FLG_OF1_AUTOELM;
				}
				continue;
			}

			/*
			 * Add the new symbol.  It should be noted that all
			 * symbols added by the mapfile start out with global
			 * scope, thus they will fall through the normal symbol
			 * resolution process.  Symbols defined as locals will
			 * be reduced in scope after all input file processing.
			 */
			/* LINTED */
			hash = (Word)elf_hash(_name);
			DBG_CALL(Dbg_map_version(name, _name, scope));
			if ((sdp = sym_find(_name, hash, ofl)) == NULL) {
				if ((sym =
				    (Sym *)libld_calloc(sizeof (Sym), 1)) == 0)
					return (S_ERROR);

				/*
				 * For passthru, parent and extern symbols make
				 * the symbol reference undefined and indicate a
				 * symbol info section is required.
				 */
				if (sym_flags & FLG_SY_PASSTHRU) {
					sym->st_shndx = SHN_UNDEF;
					ofl->ofl_dtflags |= DF_1_DIRECT;
				} else if (sym_flags &
				    (FLG_SY_PARENT | FLG_SY_EXTERN)) {
					sym->st_shndx = SHN_UNDEF;
					ofl->ofl_ossyminfo = (Os_desc *)1;
				} else
					sym->st_shndx = shndx;

				sym->st_value = value;
				sym->st_size = size;
				sym->st_info = ELF_ST_INFO(STB_GLOBAL, type);

				if ((sdp = sym_enter(_name, sym, hash, ifl,
				    ofl, 0)) == (Sym_desc *)S_ERROR)
					return (S_ERROR);

				sdp->sd_flags &= ~FLG_SY_CLEAN;

				if (shndx == SHN_UNDEF)
					sdp->sd_flags |=
					    (FLG_SY_MAPREF | FLG_SY_GLOBREF);
			} else {
				sym = sdp->sd_sym;

				/*
				 * The only symbol we can overwrite is one
				 * created as a segment size (see map_atsign()),
				 * and this can only be scoped or versioned.
				 */
				if (!((sdp->sd_flags & FLG_SY_SEGSIZE) &&
				    (shndx == SHN_UNDEF) &&
				    (type == STT_NOTYPE) &&
				    (value == 0) && (size == 0))) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_SYMDEF), mapfile,
					    EC_XWORD(Line_num), _name,
					    sdp->sd_file->ifl_name);
					return (S_ERROR);
				}
			}


			/*
			 * Indicate the new symbols scope.
			 */
			if (scope == FLG_SCOPE_LOCL)
				sdp->sd_flags |= FLG_SY_LOCAL;
			else if (scope == FLG_SCOPE_ELIM)
				sdp->sd_flags |= FLG_SY_LOCAL | FLG_SY_ELIM;
			else {
				sdp->sd_flags |= sym_flags;

				sdp->sd_flags |= FLG_SY_GLOBAL;
				if (scope == FLG_SCOPE_SYMB) {
					sdp->sd_flags |= FLG_SY_SYMBOLIC;
					ofl->ofl_flags |= FLG_OF_BINDSYMB;
				}

				/*
				 * Record the present version index for later
				 * potential versioning.
				 */
				sdp->sd_aux->sa_verndx = vdp->vd_ndx;
				vdp->vd_flags |= FLG_VER_REFER;
			}

			/*
			 * If we've encountered a symbol definition simulate
			 * that an input file has been processed - this allows
			 * things like filters to be created purely from a
			 * mapfile.
			 */
			if (type != STT_NOTYPE)
				files = TRUE;
			DBG_CALL(Dbg_map_symbol(&def_ehdr, sdp));
			break;

		default:
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSCOL),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}
	}

	/*
	 * Determine if any version references are provided after the close
	 * bracket.
	 */
	while ((tok = gettoken(mapfile)) != TK_SEMICOLON) {
		Ver_desc	*_vdp;
		char		*_name;

		if (tok != TK_STRING) {
			/* LINTED */
			if (tok != (Token)S_ERROR)
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPVERS),
				    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}

		name = Start_tok;
		if (vdp->vd_ndx == VER_NDX_GLOBAL) {
			eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_UNEXDEP),
			    mapfile, EC_XWORD(Line_num), name);
			continue;
		}

		/*
		 * Generate a new version descriptor if it doesn't already
		 * exist.
		 */
		/* LINTED */
		hash = (Word)elf_hash(name);
		if ((_vdp = vers_find(name, hash, &ofl->ofl_verdesc)) == 0) {
			if ((_name = (char *)libld_malloc(strlen(name) +
			    1)) == 0)
				return (S_ERROR);
			(void) strcpy(_name, name);

			if ((_vdp = vers_desc(_name, hash,
			    &ofl->ofl_verdesc)) == (Ver_desc *)S_ERROR)
				return (S_ERROR);
		}

		/*
		 * Add the new version descriptor to the parent version
		 * descriptors reference list.  Indicate the version descriptors
		 * first reference (used for error disgnostics if undefined
		 * version dependencies remain).
		 */
		if (vers_find(name, hash, &vdp->vd_deps) == 0)
			if (list_appendc(&vdp->vd_deps, _vdp) == 0)
				return (S_ERROR);

		if (_vdp->vd_ref == 0)
			_vdp->vd_ref = vdp;
	}
	return (1);
}

/*
 * Sort the segment list by increasing virtual address.
 */
uintptr_t
sort_seg_list(Ofl_desc *ofl)
{
	List 		seg1, seg2;
	Listnode	*lnp1, *lnp2, *lnp3;
	Sg_desc		*sgp1, *sgp2;

	seg1.head = seg1.tail = seg2.head = seg2.tail = NULL;

	/*
	 * Add the .phdr and .interp segments to our list. These segments must
	 * occur before any PT_LOAD segments (refer exec/elf/elf.c).
	 */
	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp1)) {
		Word	type = sgp1->sg_phdr.p_type;

		if ((type == PT_PHDR) || (type == PT_INTERP))
			if (list_appendc(&seg1, sgp1) == 0)
				return (S_ERROR);
	}

	/*
	 * Add the loadable segments to another list in sorted order.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp1)) {
		DBG_CALL(Dbg_map_sort_orig(sgp1));
		if (sgp1->sg_phdr.p_type != PT_LOAD)
			continue;
		if (!(sgp1->sg_flags & FLG_SG_VADDR) ||
		    (sgp1->sg_flags & FLG_SG_EMPTY)) {
			if (list_appendc(&seg2, sgp1) == 0)
				return (S_ERROR);
		} else {
			if (seg2.head == NULL) {
				if (list_appendc(&seg2, sgp1) == 0)
					return (S_ERROR);
				continue;
			}
			lnp3 = NULL;
			for (LIST_TRAVERSE(&seg2, lnp2, sgp2)) {
				if (!(sgp2->sg_flags & FLG_SG_VADDR) ||
				    (sgp2->sg_flags & FLG_SG_EMPTY)) {
					if (lnp3 == NULL) {
						if (list_prependc(&seg2,
						    sgp1) == 0)
							return (S_ERROR);
					} else {
						if (list_insertc(&seg2,
						    sgp1, lnp3) == 0)
							return (S_ERROR);
					}
					lnp3 = NULL;
					break;
				}
				if (sgp1->sg_phdr.p_vaddr <
				    sgp2->sg_phdr.p_vaddr) {
					if (lnp3 == NULL) {
						if (list_prependc(&seg2,
						    sgp1) == 0)
							return (S_ERROR);
					} else {
						if (list_insertc(&seg2,
						    sgp1, lnp3) == 0)
							return (S_ERROR);
					}
					lnp3 = NULL;
					break;
				} else if (sgp1->sg_phdr.p_vaddr >
				    sgp2->sg_phdr.p_vaddr) {
					lnp3 = lnp2;
				} else {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_SEGSAME),
					    sgp1->sg_name, sgp2->sg_name);
					return (S_ERROR);
				}
			}
			if (lnp3 != NULL)
				if (list_appendc(&seg2, sgp1) == 0)
					return (S_ERROR);
		}
	}

	/*
	 * add the sorted loadable segments to our list
	 */
	for (LIST_TRAVERSE(&seg2, lnp1, sgp1)) {
		if (list_appendc(&seg1, sgp1) == 0)
			return (S_ERROR);
	}

	/*
	 * add all other segments to our list
	 */
	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp1)) {
		Word	type = sgp1->sg_phdr.p_type;

		if ((type != PT_PHDR) && (type != PT_INTERP) &&
		    (type != PT_LOAD))
			if (list_appendc(&seg1, sgp1) == 0)
				return (S_ERROR);
	}
	ofl->ofl_segs.head = ofl->ofl_segs.tail = NULL;

	/*
	 * Now rebuild the original list and process all of the
	 * segment/section ordering information if present.
	 */
	for (LIST_TRAVERSE(&seg1, lnp1, sgp1)) {
		DBG_CALL(Dbg_map_sort_fini(sgp1));
		if (list_appendc(&ofl->ofl_segs, sgp1) == 0)
			return (S_ERROR);
	}
	return (1);
}



/*
 * Parse the mapfile.
 */
uintptr_t
map_parse(const char *mapfile, Ofl_desc *ofl)
{
	struct stat	stat_buf;	/* stat of mapfile */
	int		mapfile_fd;	/* descriptor for mapfile */
	Listnode	*lnp1;		/* node pointer */
	Listnode	*lnp2;		/* node pointer */
	Sg_desc		*sgp1;		/* seg descriptor being manipulated */
	Sg_desc		*sgp2;		/* temp segment descriptor pointer */
	Ent_desc	*enp;		/* Segment entrance criteria. */
	Token		tok;		/* current token. */
	Listnode	*e_next = NULL;
					/* next place for entrance criterion */
	Boolean		new_segment;	/* If true, defines new segment. */
	char		*name;
	int		err;

	DBG_CALL(Dbg_map_parse(mapfile));

	/*
	 * Determine if we're dealing with a file or a directory.
	 */
	if (stat(mapfile, &stat_buf) == -1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_STAT), mapfile,
		    strerror(err));
		return (S_ERROR);
	}
	if (stat_buf.st_mode & S_IFDIR) {
		DIR		*dirp;
		struct dirent	*denp;

		/*
		 * Open the directory and interpret each visible file as a
		 * mapfile.
		 */
		if ((dirp = opendir(mapfile)) == 0)
			return (1);

		while ((denp = readdir(dirp)) != NULL) {
			char	path[PATH_MAX];

			/*
			 * Ignore any hidden filenames.  Construct the full
			 * pathname to the new mapfile.
			 */
			if (*denp->d_name == '.')
				continue;
			(void) sprintf(path, MSG_ORIG(MSG_STR_PATH), mapfile,
			    denp->d_name);
			if (map_parse(path, ofl) == S_ERROR)
				return (S_ERROR);
		}
		(void) closedir(dirp);
		return (1);
	} else if (!(stat_buf.st_mode & S_IFREG)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_NOTREG), mapfile);
		return (S_ERROR);
	}

	/*
	 * We read the entire mapfile into memory.
	 */
	if ((Mapspace = (char *)libld_malloc(stat_buf.st_size + 1)) == 0)
		return (S_ERROR);
	if ((mapfile_fd = open(mapfile, O_RDONLY)) == -1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), mapfile,
		    strerror(err));
		return (S_ERROR);
	}

	if (read(mapfile_fd, Mapspace, stat_buf.st_size) != stat_buf.st_size) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_READ), mapfile,
		    strerror(err));
		return (S_ERROR);
	}
	Mapspace[stat_buf.st_size] = '\0';
	nextchr = Mapspace;

	/*
	 * Set up any global variables, the line number counter and file name.
	 */
	Line_num = 1;

	/*
	 * We now parse the mapfile until the gettoken routine returns EOF.
	 */
	while ((tok = gettoken(mapfile)) != TK_EOF) {
		int	ndx = -1;

		/*
		 * Don't know which segment yet.
		 */
		sgp1 = NULL;

		/*
		 * At this point we are at the beginning of a line, and the
		 * variable `Start_tok' points to the first string on the line.
		 * All mapfile entries start with some string token except it
		 * is possible for a scoping definition to start with `{'.
		 */
		if (tok == TK_LEFTBKT) {
			if (map_version(mapfile, (char *)0, ofl) == S_ERROR)
				return (S_ERROR);
			continue;
		}
		if (tok != TK_STRING) {
			/* LINTED */
			if (tok != (Token)S_ERROR)
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPSEGNAM),
				    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}

		/*
		 * Save the initial token.
		 */
		if ((name = (char *)libld_malloc(strlen(Start_tok) + 1)) == 0)
			return (S_ERROR);
		(void) strcpy(name, Start_tok);

		/*
		 * Now check the second character on the line.  The special `-'
		 * and `{' characters do not involve any segment manipulation so
		 * we handle them first.
		 */
		if ((tok = gettoken(mapfile)) == TK_DASH) {
			if (map_dash(mapfile, name, ofl) == S_ERROR)
				return (S_ERROR);
			continue;
		}
		if (tok == TK_LEFTBKT) {
			if (map_version(mapfile, name, ofl) == S_ERROR)
				return (S_ERROR);
			continue;
		}

		/*
		 * If we're here we need to interpret the first string as a
		 * segment name.  Find the segment named in the token.
		 */
		for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp2)) {
			ndx++;
			if (strcmp(sgp2->sg_name, name) == 0) {
				sgp1 = sgp2;
				new_segment = FALSE;
				break;
			}
		}

		/*
		 * If the second token is a '|' then we had better
		 * of found a segment.  It is illegal to perform
		 * section within segment ordering before the segment
		 * has been declared.
		 */
		if (tok == TK_PIPE) {
			if (sgp1 == NULL) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SECINSEG),
				    mapfile, EC_XWORD(Line_num), name);
				return (S_ERROR);
			} else {
				if (map_pipe(mapfile, sgp1) == S_ERROR)
					return (S_ERROR);
				continue;
			}
		}

		/*
		 * If segment is still NULL then it does not exist.  Create a
		 * new segment, and leave its values as 0 so that map_equal()
		 * can detect changing attributes.
		 */
		if (sgp1 == NULL) {
			if ((sgp1 = (Sg_desc *)libld_calloc(sizeof (Sg_desc),
			    1)) == 0)
				return (S_ERROR);
			sgp1->sg_phdr.p_type = PT_NULL;
			sgp1->sg_name = name;
			new_segment = TRUE;
			ndx = -1;
		}

		if ((strcmp(sgp1->sg_name, MSG_ORIG(MSG_STR_INTERP))  == 0) ||
		    (strcmp(sgp1->sg_name, MSG_ORIG(MSG_STR_LD_DYNAMIC)) ==
		    0)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGRESV),
			    mapfile, EC_XWORD(Line_num));
			return (S_ERROR);
		}

		/*
		 * Now check the second token from the input line.
		 */
		if (tok == TK_EQUAL) {
			if (map_equal(mapfile, sgp1, ofl) == S_ERROR)
				return (S_ERROR);
			ofl->ofl_flags |= FLG_OF_SEGSORT;
			DBG_CALL(Dbg_map_equal(new_segment));
		} else if (tok == TK_COLON) {
			/*
			 * If this is an existing segment reservation, sections
			 * can't be assigned to it.
			 */
			if ((new_segment == FALSE) &&
			    (sgp1->sg_flags & FLG_SG_EMPTY)) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_SEGEMPSEC),
				    mapfile, EC_XWORD(Line_num));
				return (S_ERROR);
			}

			/*
			 * We are looking at a new entrance criteria line.
			 * Note that entrance criteria are added in the order
			 * they are found in the map file, but are placed
			 * before any default criteria.
			 */
			if ((enp =
			    (Ent_desc *)libld_calloc(sizeof (Ent_desc),
				1)) == 0)
				return (S_ERROR);
			enp->ec_segment = sgp1;
			if (e_next == NULL) {
				if ((e_next = list_prependc(&ofl->ofl_ents,
				    enp)) == 0)
					return (S_ERROR);
			} else {
				if ((e_next = list_insertc(&ofl->ofl_ents,
				    enp, e_next)) == 0)
					return (S_ERROR);
			}

			if (map_colon(mapfile, enp) == S_ERROR)
				return (S_ERROR);
			ofl->ofl_flags |= FLG_OF_SEGSORT;
			DBG_CALL(Dbg_map_ent(new_segment, enp, ofl));
		} else if (tok == TK_ATSIGN) {
			if (map_atsign(mapfile, sgp1, ofl) == S_ERROR)
				return (S_ERROR);
			DBG_CALL(Dbg_map_atsign(new_segment));
		} else {
			/* LINTED */
			if (tok != (Token)S_ERROR) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_EXPEQU),
				    mapfile, EC_XWORD(Line_num));
				return (S_ERROR);
			}
		}

		/*
		 * Having completed parsing an entry in the map file determine
		 * if the segment to which it applies is new.
		 */
		if (new_segment) {
			int	src_type, dst_type;

			/*
			 * If specific fields have not been supplied via
			 * map_equal(), make sure defaults are supplied.
			 */
			if (sgp1->sg_phdr.p_type == PT_NULL) {
				sgp1->sg_phdr.p_type = PT_LOAD;
				sgp1->sg_flags |= FLG_SG_TYPE;
			}
			if ((sgp1->sg_phdr.p_type == PT_LOAD) &&
				(!(sgp1->sg_flags & FLG_SG_FLAGS))) {
				sgp1->sg_phdr.p_flags = PF_R + PF_W + PF_X;
				sgp1->sg_flags |= FLG_SG_FLAGS;
			}

			/*
			 * Determine where the new segment should be inserted
			 * in the seg_desc[] list.  Presently the user can
			 * only add a LOAD or NOTE segment.  Note that these
			 * segments must be added after any PT_PHDR and
			 * PT_INTERP (refer Generic ABI, Page 5-4).
			 */
			switch (sgp1->sg_phdr.p_type) {
			case PT_LOAD:
				if (sgp1->sg_flags & FLG_SG_EMPTY)
					src_type = 3;
				else
					src_type = 2;
				break;
			case PT_NOTE:
				src_type = 6;
				break;
			default:
				eprintf(ERR_FATAL, MSG_INTL(MSG_MAP_UNKSEGTYP),
				    mapfile, EC_XWORD(Line_num),
				    EC_WORD(sgp1->sg_phdr.p_type));
				return (S_ERROR);
			}
			lnp2 = NULL;
			for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp2)) {
				ndx++;
				switch (sgp2->sg_phdr.p_type) {
				case PT_PHDR:
					dst_type = 0;
					break;
				case PT_INTERP:
					dst_type = 1;
					break;
				case PT_LOAD:
					dst_type = 2;
					break;
				case PT_DYNAMIC:
					dst_type = 4;
					break;
				case PT_SHLIB:
					dst_type = 5;
					break;
				case PT_NOTE:
					dst_type = 6;
					break;
				case PT_NULL:
					dst_type = 7;
					break;
				default:
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_MAP_UNKSEGTYP),
					    mapfile, EC_XWORD(Line_num),
					    EC_WORD(sgp2->sg_phdr.p_type));
					return (S_ERROR);
				}
				if (src_type <= dst_type) {
					if (lnp2 == NULL) {
						if (list_prependc(
						    &ofl->ofl_segs, sgp1) == 0)
							return (S_ERROR);
					} else {
						if (list_insertc(&ofl->ofl_segs,
						    sgp1, lnp2) == 0)
							return (S_ERROR);
					}
					break;
				}
				lnp2 = lnp1;
			}
		}
		DBG_CALL(Dbg_map_seg(ofl->ofl_e_machine, ndx, sgp1));
	}
	return (1);
}
