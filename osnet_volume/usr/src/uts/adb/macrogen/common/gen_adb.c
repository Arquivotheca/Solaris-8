/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)gen_adb.c	1.6	98/07/19 SMI"

#include	<stdio.h>
#include	<stdlib.h>
#include	<malloc.h>
#include	<memory.h>
#include	<string.h>

#include	"gen.h"

#define	TRUE		1
#define	FALSE		0

#define	NOT_ARRAY	1 	/* struct member is not an array */
#define	ARRAY_SEEN	0	/* we've seen this array */
#define	ARRAY_NOPRINT	1	/* we've seen it, but not printed it */
#define	ARRAY_PRINTED	2	/* we have printed this array */

#define	NO_TAB_SIZE	-1 	/* tab size not needed */
#define	TAB_SIZE_8	8 	/* # of characters printed for 8bit value */
#define	TAB_SIZE_16	8 	/* # of characters printed for 16bit value */
#define	TAB_SIZE_32	16 	/* # of characters printed for 32bit value */
#define	TAB_SIZE_64	16 	/* # of characters printed for 64bit value */

#define	ELEM_PER_LINE	3 	/* number of elements to print per line */

#define	INVALID		-1
#define	CHAR		 0
#define	ESCCHAR		 1
#define	UNSIGNED_OCTAL	 2
#define	SIGNED_OCTAL	 3
#define	UNSIGNED_DECIMAL 4
#define	SIGNED_DECIMAL	 5
#define	HEX		 6
#define	FLOAT		 7
#define	STRING		 8
#define	SYMBOLIC	 9
#define	DATE		10
#define	INSTRUCTION	11
#define	SUBSTRUCTURE	12
#define	INLINE		13

#define	ADB_CHAR	'c'
#define	ADB_ESCCHAR	'C'
#define	ADB_UOCTAL8	'b'
#define	ADB_UOCTAL16	'o'
#define	ADB_UOCTAL32	'O'
#define	ADB_UOCTAL64	'g'
#define	ADB_SOCTAL16	'q'
#define	ADB_SOCTAL32	'Q'
#define	ADB_SOCTAL64	'G'
#define	ADB_UDECIMAL16	'u'
#define	ADB_UDECIMAL32	'U'
#define	ADB_UDECIMAL64	'E'
#define	ADB_SDECIMAL16	'd'
#define	ADB_SDECIMAL32	'D'
#define	ADB_SDECIMAL64	'e'
#define	ADB_HEX8	'B'
#define	ADB_HEX16	'x'
#define	ADB_HEX32	'X'
#define	ADB_HEX64	'J'
#define	ADB_FLOAT32	'f'
#define	ADB_FLOAT64	'F'
#define	ADB_STRING	's'
#define	ADB_SYMBOLIC	'p'
#define	ADB_DATE32	'Y'
#define	ADB_DATE64	'y'
#define	ADB_INSTRUCTION	'i'
#define	ADB_SUBSTRUCT	'.'
#define	ADB_INLINEMODE	'#'

/* Inline request types (other than SIZEOF structure and OFFSET) */
#define	REQ_UNKNOWN		-1
#define	REQ_FMT_HEX		1
#define	REQ_FMT_DEC		2
#define	REQ_FMT_SDEC		3
#define	REQ_FMT_OCT		4
#define	REQ_FMT_SOCT		5
#define	REQ_SZ_PTR		6
#define	REQ_SZ_LONG		7
#define	REQ_SZ_INT		8

#define	MAXSTRLEN	256

struct macro_member
{
	char	*m_label;	/* label for this member if supplied */
	char	*m_submacro;	/* in case there's a sub-struct definition */
	unsigned char m_format;	/* format this mem wants to be printed in */
	int	m_array;	/* is this member an array or not */
	int	m_arrayprint; 	/* aid in printing out array entries */
	int	m_offset;	/* member offset - in bits */
	int	m_curroffset;	/* rel. mem offset - to next element */
	int	m_bitsize;	/* number of bits for this structure member */
	int	m_tabVar; 	/* variable tab - how much to tab to print */
	int	m_tabLabel; 	/* label tab - how much tab to print label */
};

typedef struct macro_member MEMBER;

struct macro
{
	char	*m_name;	/* name of the macro */
	char	*m_prefix;	/* prefix of macro i.e., cpu_ */
	char	*m_filename;	/* filename to keep for this macro */
	int	size;  		/* size of structure */
	int	index;		/* # of fields in structure being generated */
	MEMBER	*member;	/* each individual member of the structure */
};

static	struct macro	MACRO;		/* the one and only MACRO definition */

/*		Local functions			*/

void gen_struct_begin(char *, char *, char *, int);
unsigned char gen_get_format(char *, int, int *);
int gen_element_size(int, unsigned char *);
void gen_add_struct_member(char *, int, int, int, char *);
void gen_macro(void);
void gen_free_memory();
void gen_clear_member(int);
short gen_valid_fmtstring(char *);
void do_inlinepp(int);
int get_offset(char *, char *);
int gen_tabs(int, int);
int next_tab(int, int);

/*		Defines called from framework		*/

#define	MEMFMT			MACRO.member[MACRO.index].m_format
#define	OFFSET			MACRO.member[MACRO.index].m_offset
#define	OFF(index)		(MACRO.member[(index)].m_offset)
#define	CURROFF(index)		(MACRO.member[(index)].m_curroffset)
#define	ARRAY(index)		MACRO.member[(index)].m_array
#define	ARRAYPR(index)		MACRO.member[(index)].m_arrayprint
#define	MEMBITS(index)		MACRO.member[(index)].m_bitsize
#define	MCRLABEL(index)		MACRO.member[(index)].m_label
#define	MCRFORMAT(index)	MACRO.member[(index)].m_format
#define	TABVAR(index)		MACRO.member[(index)].m_tabVar
#define	TABLABEL(index)		MACRO.member[(index)].m_tabLabel

/*	External Variables */
extern int	_sizeof_long, _sizeof_ptr, _sizeof_int;

/*
 * gen_struct_begin() gets called by the framework when we start a new struct.
 */
void gen_struct_begin(
	char	*name,
	char	*prefix,
	char	*filename,	/* filename to use for this macro */
	int	 size)
{
	MACRO.index = 0;

	/* check to see if a valid macro name was passed in */
	if (name)
	    MACRO.m_name = strdup(name);
	else {
	    printf("warning: no \"name\" provided for structure\n");
	    return;
	}

	if (filename)
	    MACRO.m_filename = strdup(filename);
	else
	    MACRO.m_filename = strdup(MACRO.m_name);

	/* check to see if a prefix was passed in or not */
	MACRO.m_prefix = NULL;
	if (prefix)
	    MACRO.m_prefix = strdup(prefix);

	MACRO.size = size;
	MACRO.member = NULL;

	if (strcmp(MACRO.m_name, MACRO.m_filename) == 0) {
	    printf("\nstruct: name=filename=%s  prefix=%s  size=%d\n",
		    MACRO.m_name, MACRO.m_prefix ? MACRO.m_prefix : "<none>",
		    MACRO.size);
	} else {
	    printf("\nstruct: name=%s  prefix=%s  filename=%s  size=%d\n",
		    MACRO.m_name, MACRO.m_prefix ? MACRO.m_prefix : "<none>",
		    MACRO.m_filename, MACRO.size);
	}
}

/*
 * gen_struct_member() gets called by the framework for each field of the
 * struct that the macro should display.
 *
 * this is a high level wrapper function since now we have to handle two
 * seperate cases - array and non-array.
 *
 * non-array case is trivial; array case takes figuring out how deep the
 * array is i.e., single, two or n-dimensional
 *
 */
void gen_struct_member(
    char	*label,			/* to display with value */
    int		offset_in_bits,		/* offset, in bits */
    dimension_t	*d,			/* number of elements in array */
    int		size_in_bits,		/* size, in bits */
    char	*format)		/* format */
{
	int	index;
	int	offset;

	/* if this is not a array, then we just add the structure member */
	if (!d) {
	    gen_add_struct_member(label, offset_in_bits,
					NOT_ARRAY, size_in_bits, format);
	} else {

	    dimension_t *ptr = d;
	    int dim_list = 1;

	    /* calculate depth of what we're generating */

	    /* do appropriate looping and fixup all macro names & offsets */
	    while (ptr->next != d) {
		dim_list++;
		ptr = ptr->next;
	    }

	    if (dim_list == 1) {
		gen_add_struct_member(label, offset_in_bits,
				d->num_elements, size_in_bits, format);
	    } else {
		int	index;
		int	curoffset;

		ptr = d;
		curoffset = offset_in_bits;
		for (index = 0; index < ptr->num_elements; index++) {
		    char	newlabel[MAXSTRLEN];

		    sprintf(newlabel, "%s[%d]", label, index);
		    if (index == 0) {
			gen_add_struct_member(newlabel, curoffset,
			    ptr->next->num_elements, size_in_bits, format);
		    } else {
			curoffset += (ptr->next->num_elements * size_in_bits);
			gen_add_struct_member(newlabel, curoffset,
			    ptr->next->num_elements, size_in_bits, format);
		    }
		}
	    }
	}
}

/*
 * gen_add_struct_member() gets called by gen_struct_member for each field
 * of the struct that the macro should display.
 */
void gen_add_struct_member(char *label, int offset_in_bits,
			    int num_elem, int size_in_bits, char *format)
{
	if (strcmp(format, "inline") == 0)
	    printf("\tlabel=<inline-adb-code> format=inline\n");
	else {
	    if (label)
		printf("\tlabel=%s ", renamed(label));
	    else
		printf("\tlabel=NULL ");
	    printf("offset=%x num_elem=%x size=%x format=%s\n",
			    offset_in_bits, num_elem, size_in_bits, format);
	}

	if (gen_valid_fmtstring(format) != INVALID) {
	    MACRO.member = (MEMBER *) realloc(MACRO.member,
				    sizeof (MEMBER) * (MACRO.index + 1));

	    /* clear out memory just allocated */
	    memset(&MACRO.member[MACRO.index], 0x0, sizeof (MEMBER));

	    if (strcmp(format, "inline") == 0)
		MEMFMT = (unsigned char) ADB_INLINEMODE;
	    else
		MEMFMT = (unsigned char) gen_get_format(format,
					    size_in_bits, &TABVAR(MACRO.index));
	    if (num_elem >= 1)
		ARRAY(MACRO.index) = num_elem;

	    OFFSET = offset_in_bits;
	    MACRO.member[MACRO.index].m_submacro = NULL;
	    if (MEMFMT == ADB_SUBSTRUCT) {
		/*
		 * this is so we get rid of the '.' that's passed in front of
		 * the substructure name
		 */
		/* copy the substructure name */
		MACRO.member[MACRO.index].m_submacro = strdup(&format[1]);
	    }

	    MEMBITS(MACRO.index) = size_in_bits;
	    MACRO.member[MACRO.index].m_label = strdup(label);
	    ARRAYPR(MACRO.index) = ARRAY_SEEN;
	    MACRO.index++;
	} else {
	    fprintf(stderr, "Error: Invalid format detected for ");
	    fprintf(stderr,
		"structure \"%s\" member \"%s\" [format \"%s\"]\n",
					MACRO.m_name, label, format);
	    return;
	}
}

/*
 * gen_valid_fmtstring() looks at the format string received for this
 * structure member and returns a token which signifies if this is a valid
 * format string or not.
 */
short
gen_valid_fmtstring(char *format)
{
	if (strcmp(format, "char") == 0)
	    return (CHAR);

	if (strcmp(format, "echar") == 0)
	    return (ESCCHAR);

	if (strcmp(format, "octal") == 0)
	    return (UNSIGNED_OCTAL);

	if (strcmp(format, "soctal") == 0)
	    return (SIGNED_OCTAL);

	if (strcmp(format, "decimal") == 0)
	    return (UNSIGNED_DECIMAL);

	if (strcmp(format, "sdecimal") == 0)
	    return (SIGNED_DECIMAL);

	if (strcmp(format, "hex") == 0)
	    return (HEX);

	if (strcmp(format, "float") == 0)
	    return (FLOAT);

	if (strcmp(format, "string") == 0)
	    return (STRING);

	if (strcmp(format, "symbolic") == 0)
	    return (SYMBOLIC);

	if (strcmp(format, "date") == 0)
	    return (DATE);

	if (strcmp(format, "ins") == 0)
	    return (INSTRUCTION);

	if (format[0] == '.')
	    return (SUBSTRUCTURE);

	if (strcmp(format, "inline") == 0)
	    return (INLINE);

	return (INVALID);
}

/*
 * gen_get_format() looks at input format string and determines a close ADB
 * format string.that corresponds to it
 */
unsigned char
gen_get_format(format, size, tabsize)
char	*format;
int	size, *tabsize;
{
	short	fmt_type;

	if ((fmt_type = gen_valid_fmtstring(format)) == INVALID)
	    return (INVALID);

	switch (size) {
	    case 8:
		*tabsize = TAB_SIZE_8;
		break;
	    case 16:
		*tabsize = TAB_SIZE_16;
		break;
	    case 32:
		*tabsize = TAB_SIZE_32;
		break;
	    case 64:
		*tabsize = TAB_SIZE_64;
		break;
	    default:
		*tabsize = TAB_SIZE_32;
		break;
	}

	switch (fmt_type) {
	    case CHAR:
		*tabsize = TAB_SIZE_8;
		return (ADB_CHAR);

	    case ESCCHAR:
		*tabsize = TAB_SIZE_8;
		return (ADB_ESCCHAR);

	    case UNSIGNED_OCTAL:
		switch (size) {
		    case 8: return (ADB_UOCTAL8);
		    case 16: return (ADB_UOCTAL16);
		    case 32: return (ADB_UOCTAL32);
		    case 64: return (ADB_UOCTAL64);
		    default: break;
		}
		return (ADB_HEX32);

	    case SIGNED_OCTAL:
		switch (size) {
		    case 8: return (ADB_UOCTAL8); /* no s_octal for 1byte */
		    case 16: return (ADB_SOCTAL16);
		    case 32: return (ADB_SOCTAL32);
		    case 64: return (ADB_SOCTAL64);
		    default: break;
		}
		return (ADB_HEX32);

	    case UNSIGNED_DECIMAL:
		switch (size) {
		    case 8: return (ADB_UOCTAL8); /* no u_decimal for 1byte */
		    case 16: return (ADB_UDECIMAL16);
		    case 32: return (ADB_UDECIMAL32);
		    case 64: return (ADB_UDECIMAL64);
		    default: break;
		}
		return (ADB_HEX32);

	    case SIGNED_DECIMAL:
		switch (size) {
		    case 8: return (ADB_UOCTAL8); /* no s_decimal for 1byte */
		    case 16: return (ADB_SDECIMAL16);
		    case 32: return (ADB_SDECIMAL32);
		    case 64: return (ADB_SDECIMAL64);
		    default: break;
		}
		return (ADB_HEX32);

	    case HEX:
		switch (size) {
		    case 8: return (ADB_HEX8);
		    case 16: return (ADB_HEX16);
		    case 32: return (ADB_HEX32);
		    case 64: return (ADB_HEX64);
		    default: break;
		}
		return (ADB_HEX32);

	    case FLOAT:
		switch (size) {
		    case 32: return (ADB_FLOAT32);
		    case 64: return (ADB_FLOAT64);
		    default: break;
		}
		return (ADB_HEX32);

	    case STRING:
		*tabsize = TAB_SIZE_8;
		return (ADB_STRING);

	    case SYMBOLIC:
		switch (size) {
		    case 32: /* fall through */
		    case 64:
			return (ADB_SYMBOLIC);
		    default: break;
		}
		return (ADB_HEX32);

	    case DATE:
		switch (size) {
		    case 32: return (ADB_DATE32);
		    case 64: return (ADB_DATE64);
		    default: break;
		}
		return (ADB_HEX32);

	    case INSTRUCTION:
		switch (size) {
		    case 32: return (ADB_INSTRUCTION);
		    default: break;
		}
		return (ADB_HEX32);

	    case SUBSTRUCTURE:
		*tabsize = 0;
		return (ADB_SUBSTRUCT);

	    default :
		*tabsize = TAB_SIZE_32;
		return (ADB_HEX32);
	}
}

/*
 * gen_struct_end() gets called by the framework whenever we end a struct.
 * We finish the line we were working on and wrap up the macro.
 */
void
gen_struct_end()
{
	gen_macro();
	gen_free_memory();
}

/*
 * gen_free_memory - memory cleanup routine.
 * make sure we don't exit without handing back all that we've taken
 */

void
gen_free_memory()
{
	int index;

	for (index = 0; index < MACRO.index; index++) {
	    if (MACRO.member[index].m_submacro != NULL)
		free(MACRO.member[index].m_submacro);
	    if (MACRO.member[index].m_label != NULL)
		free(MACRO.member[index].m_label);
	}

	free((MEMBER *) MACRO.member);

	if (MACRO.m_name)
	    free(MACRO.m_name);

	if (MACRO.m_prefix)
	    free(MACRO.m_prefix);

	MACRO.index = 0;
}

/*
 * gen_fixup_offsets - this function calculates the correct offset for this
 * member of the structure (based on the next element of the structure)
 */
int
gen_fixup_offsets()
{
	int	index;
	int	offset = 0;

	for (index = 0; index < MACRO.index - 1; index++) {
		/* Can't do much about offsets for inline code, skip it */
		if (MCRFORMAT(index) == ADB_INLINEMODE ||
				    MCRFORMAT(index+1) == ADB_INLINEMODE) {
		    continue;
		}
		CURROFF(index) = (OFF(index + 1) - (OFF(index) +
					(ARRAY(index) * MEMBITS(index))))/8;
	}

	if (MCRFORMAT(index) != ADB_INLINEMODE) {
		/*
		 * take care of the case when we're the last element in
		 * the array we have to make sure that the entire array
		 * member is traversed, so the offset for the last element
		 * is
		 *	sizeof(struct in bits) - current offset
		 */
		index = MACRO.size;
		index *= 8; /* convert the bytes to bits */
		CURROFF(MACRO.index - 1) = (index - (OFF(MACRO.index - 1) +
		    (ARRAY(MACRO.index - 1) * MEMBITS(MACRO.index - 1))))/8;
	}
}

void
gen_offset(FILE *macroFP, int index)
{
	char sign = '+';

	if (CURROFF(index) < 0) {
	    sign = '-';
	    CURROFF(index) = - CURROFF(index);
	}

	fprintf(macroFP, "%d%c", CURROFF(index), sign);
}

/* given a number, this function will return the next higher multiple of 8 */
int
closest(int offset)
{
	int	div, mod;

	div = offset / 8;
	mod = offset % 8;

	if (mod > 0)
	    mod = 1;

	div = div + mod;

	return (div * 8);
}

FILE *
gen_macrofile_create()
{
	return (fopen(MACRO.m_filename, "w+"));
}

void
gen_print_newline(FILE *macroFP, int index, int printed)
{
	if (printed == TRUE) {
	    if (MCRFORMAT(index) == ADB_INLINEMODE)
		fprintf(macroFP, "n");
	    else {
		if (ARRAY(index) <= 1 || MCRFORMAT(index) == ADB_SUBSTRUCT) {
		    /* need this for line break purposes in ADB macros */
		    fprintf(macroFP, "n");
		}
	    }
	}
}

/* gen_macro - generate the actual macro ... */
void
gen_macro()
{
	int 	index = 0;
	FILE 	*macroFP = NULL;

	if (MACRO.index == 0)
	    return;

	macroFP = gen_macrofile_create();
	gen_fixup_offsets();

	if (MCRFORMAT(index) != ADB_INLINEMODE)
	    fprintf(macroFP, "./");

	while (index < MACRO.index) {
	    int inindex = ELEM_PER_LINE;
	    int svindex = index;
	    int printed = FALSE;

	    if (MCRFORMAT(index) == ADB_INLINEMODE) {
		do_inlinepp(index);
		fprintf(macroFP, "%s", MCRLABEL(index));
		printed = TRUE;
		index++;
	    } else {
		/*
		 * if we are printing out a sub-structure, then no formatting
		 * magic is needed
		 */
		if (MCRFORMAT(index) == ADB_SUBSTRUCT) {
		    fprintf(macroFP, "\"%s\"\n.$<<%s\n",
			    MCRLABEL(index), MACRO.member[index].m_submacro);
		    printed = TRUE;
		    index++;
		} else {
		    /* this is where da' labels are printed */
		    while ((index < MACRO.index) && (inindex > 0)) {

			if (MCRFORMAT(index) == ADB_INLINEMODE)
			    break;

			if (MCRFORMAT(index) == ADB_SUBSTRUCT)
			    break;

			if ((ARRAY(index) > 1) &&
					(ARRAYPR(index) == ARRAY_SEEN)) {
			    ARRAYPR(index) = ARRAY_NOPRINT;
			    break;
			}

			gen_tabs(index, inindex);

			/*
			 * If the user has explicitly supplied a "" as the
			 * label, we treat that as a request not to print
			 * anything, instead of printing nothing ("").
			 *
			 * We may need to update the tab position, though.
			 *
			 */
			if (strlen(MCRLABEL(index))) {
			    if (TABLABEL(index) > 0)
				fprintf(macroFP, "\"%s\"%dt",
					    MCRLABEL(index), TABLABEL(index));
			    else
				fprintf(macroFP, "\"%s\"", MCRLABEL(index));
			    printed = TRUE;
			} else if (TABLABEL(index) > 0) {
			    fprintf(macroFP, "%dt", TABLABEL(index));
			    printed = TRUE;
			}

			if ((ARRAY(index) > 1) &&
					(ARRAYPR(index) == ARRAY_NOPRINT)) {
			    ARRAYPR(index) = ARRAY_PRINTED;
			    break;
			}
			index++;
			inindex--;
		    }
		    index = svindex; /* reset the counters */
		    inindex = ELEM_PER_LINE; /* reset the counters */
		    gen_print_newline(macroFP, index, printed);
		    printed = FALSE;

		    /* this is where the variables are printed */
		    while ((index < MACRO.index) && (inindex > 0)) {

			if (MCRFORMAT(index) == ADB_INLINEMODE)
			    break;

			if (MCRFORMAT(index) == ADB_SUBSTRUCT)
			    break;

			/* Move to the first member before printing it */
			if ((index == 0) && (OFF(index) != 0))
			    fprintf(macroFP, "%d+", OFF(index)/8);

			if (ARRAY(index) > 1) {
			    if (ARRAYPR(index) == ARRAY_NOPRINT) {
				break;
			    }

			    if (ARRAYPR(index) == ARRAY_PRINTED) {
				fprintf(macroFP, "%d%c",
					ARRAY(index), MCRFORMAT(index));
				if (CURROFF(index) != 0)
				    gen_offset(macroFP, index);
				printed = TRUE;
				index++;
				break;
			    } else {
				if (((index+1) == MACRO.index) &&
							ARRAY(index) > 1) {
				    fprintf(macroFP, "%d%c",
					    ARRAY(index), MCRFORMAT(index));
				    if (CURROFF(index) != 0)
					gen_offset(macroFP, index);
				    printed = TRUE;
				}
			    }
			} else {
			    if (TABVAR(index) > 0) {
				fprintf(macroFP, "%c%dt",
					MCRFORMAT(index), TABVAR(index));
			    } else
				fprintf(macroFP, "%c", MCRFORMAT(index));

			    /* position dot to next member if not inline */
			    if (index < (MACRO.index -1) &&
				    MCRFORMAT(index+1) != ADB_INLINEMODE) {
				if (CURROFF(index) != 0)
				    gen_offset(macroFP, index);
			    }

			    printed = TRUE;
			}
			index++; /* point to the next macro structure entry */
			inindex--;
		    }

		    if (printed == TRUE)
			fprintf(macroFP, "\n");
		}
	    }

	    if (MCRFORMAT(index) == ADB_INLINEMODE)
		continue;

	    if (index < MACRO.index) {
		if (printed == TRUE) {
		    fprintf(macroFP, "+/");
		}
		fflush(macroFP);
	    }
	}

	/*
	 * we wouldn't be here unless we were able to open the file in
	 * the first place
	 */
	fflush(macroFP);
	fclose(macroFP);
}

/* Replace special requests in inline adb code with values */
void
do_inlinepp(ndx)
int	ndx;
{
	int	i, off, req_type;
	char	*label = MCRLABEL(ndx), *new_label;
	char	sizeofstr[MAXSTRLEN], offsetstr[MAXSTRLEN];
	char	gen_req_str[MAXSTRLEN];
	char	*p, *q, *tmp_p;
	char	*mem_pos;

	if (!label)
	    return;

	/* SIZEOF requests */
	sprintf(sizeofstr, "0t%d", MACRO.size);

	/* Preprocessed label in a new label string */
	new_label = (char *) malloc((strlen(label)+1) * 2);
	if (!new_label) {
	    fprintf(stderr, "warning: no mem for inline pp\n");
	    return;
	}

	p = label;
	q = new_label;
	while (TRUE) {
	    while (*p && (*p != '{'))
		*q++ = *p++;

	    if (*p == NULL) {
		*q++ = 0;
		new_label = realloc(new_label, q - new_label);	/* trim tail */
		free(label);
		MCRLABEL(ndx) = new_label;		/* set up pp'd label */
		return;
	    }

	    if (strncmp(p+1, "SIZEOF}", 7) == 0) {
		/* copy the macro size in place of the request */
		for (i = 0; sizeofstr[i]; i++)
		    *q++ = sizeofstr[i];

		/* update src ptr to point after the request */
		p += 8;
		continue;
	    }

	    if (strncmp(p+1, "OFFSET", 6) == 0) {
		/* skip white space */
		mem_pos = p + 7;
		while (*mem_pos == ' ' || *mem_pos == '\t')
		    mem_pos++;

		/* expect a comma here */
		if (*mem_pos != ',') {
		    fprintf(stderr, "warning: missing ',' in inline req.\n");
		    return;
		} else
		    mem_pos++;

		/* skip white space and position to start of member string */
		while (*mem_pos == ' ' || *mem_pos == '\t')
		    mem_pos++;

		/* check if member name is empty */
		if (*mem_pos == '}') {
		    fprintf(stderr, "warning: missing member in inline req\n");
		    return;
		}

		/* otherwise, check if request is terminated properly */
		if ((tmp_p = strchr(mem_pos, '}')) == NULL) {
		    fprintf(stderr, "warning: missing '}' in inline req.\n");
		    return;
		} else
		    *tmp_p = 0;

		/* find offset of member */
		if ((off = get_offset(MACRO.m_name, mem_pos)) < 0) {
		    fprintf(stderr,
			"warning: unknown mem %s in inline\n", mem_pos);
		    return;
		}

		if (off == 0)
		    sprintf(offsetstr, "0");
		else
		    sprintf(offsetstr, "0t%d", off);

		/* replace request with offset and update src ptr */
		for (i = 0; offsetstr[i]; i++)
		    *q++ = offsetstr[i];
		p = tmp_p + 1;
		continue;
	    }


	    /* Other requests */
	    switch (req_type = get_req_type(p)) {
		case REQ_SZ_PTR:
		    sprintf(gen_req_str, "0t%d", _sizeof_ptr);
		    for (i = 0; gen_req_str[i]; i++)
			*q++ = gen_req_str[i];
		    break;

		case REQ_SZ_LONG:
		    sprintf(gen_req_str, "0t%d", _sizeof_long);
		    for (i = 0; gen_req_str[i]; i++)
			*q++ = gen_req_str[i];
		    break;

		case REQ_SZ_INT:
		    sprintf(gen_req_str, "0t%d", _sizeof_int);
		    for (i = 0; gen_req_str[i]; i++)
			*q++ = gen_req_str[i];
		    break;

		case REQ_FMT_HEX :
		    if (_sizeof_ptr == 4)
			*q++ = ADB_HEX32;
		    else if (_sizeof_ptr == 8)
			*q++ = ADB_HEX64;
		    break;

		case REQ_FMT_DEC :
		    if (_sizeof_long == 4)
			*q++ = ADB_UDECIMAL32;
		    else if (_sizeof_long == 8)
			*q++ = ADB_UDECIMAL64;
		    break;

		case REQ_FMT_SDEC :
		    if (_sizeof_long == 4)
			*q++ = ADB_SDECIMAL32;
		    else if (_sizeof_long == 8)
			*q++ = ADB_SDECIMAL64;
		    break;

		case REQ_FMT_OCT :
		    if (_sizeof_long == 4)
			*q++ = ADB_UOCTAL32;
		    else if (_sizeof_long == 8)
			*q++ = ADB_UOCTAL64;
		    break;

		case REQ_FMT_SOCT :
		    if (_sizeof_long == 4)
			*q++ = ADB_SOCTAL32;
		    else if (_sizeof_long == 8)
			*q++ = ADB_SOCTAL64;
		    break;

		default:
		    fprintf(stderr,
			"warning: unrecognized inline keyword in %s\n",
							    MACRO.m_name);
		    *q++ = *p++;
	    }

	    /* update src pointer */
	    if (req_type != REQ_UNKNOWN)
		p = strchr(p+1, '}') + 1;
	}
}

int
get_req_type(char *cur)
{
	/* format requests */
	if (strncmp(cur, "{HEX}", 5) == 0)
		return (REQ_FMT_HEX);
	if (strncmp(cur, "{DEC}", 5) == 0)
		return (REQ_FMT_DEC);
	if (strncmp(cur, "{SDEC}", 6) == 0)
		return (REQ_FMT_SDEC);
	if (strncmp(cur, "{OCT}", 5) == 0)
		return (REQ_FMT_OCT);
	if (strncmp(cur, "{SOCT}", 6) == 0)
		return (REQ_FMT_SOCT);

	/* sizeof requests */
	if (strncmp(cur, "{PTRSIZE}", 9) == 0)
		return (REQ_SZ_PTR);
	if (strncmp(cur, "{LONGSIZE}", 10) == 0)
		return (REQ_SZ_LONG);
	if (strncmp(cur, "{INTSIZE}", 9) == 0)
		return (REQ_SZ_INT);

	return (REQ_UNKNOWN);
}


/*
 * This routine assumes that the value of TABVAR() is set by gen_get_format()
 * based on the size of the variable. Must make sure this routine is called
 * only once for every member to be printed, since it updates
 * TABVAR()/TABLABEL() values based on current TABVAR().
 */
int
gen_tabs(index, inindex)
int	index;
int	inindex;
{
	static int	cur_offset = 0;
	int		diff, ret, ntab;
	unsigned int	var_len, label_len;

	/* Get the length of the label and its data item */
	label_len = strlen(MCRLABEL(index));
	var_len = TABVAR(index);

	/*
	 * If this is the last element in this line, no tabs after
	 * label/var printing is required
	 */
	if (inindex == 1) {
	    TABLABEL(index) = TABVAR(index) = 0;
	    return (0);
	} else if (inindex == ELEM_PER_LINE)
	    cur_offset = 0;

	/* label to be printed is smaller than the item size */
	if ((diff = var_len - label_len) > 0) {
	    TABLABEL(index) = var_len;
		/*
		 * If, after tab'ing, the label print ends at
		 * cur_offset+var_len, there's no need to tab var.
		 * Otherwise, we need to tab var too.
		 */
	    if ((ntab = next_tab(cur_offset + label_len, TABLABEL(index))) >
						    (cur_offset + var_len))  {
		TABVAR(index) = var_len;
	    } else
		TABVAR(index) = 0;
	} else {
	    diff = -diff;
	    TABLABEL(index) = TAB_SIZE_8;
	    TABVAR(index) = closest(diff + 1);
	    ntab = next_tab(cur_offset + label_len, TABLABEL(index));
	}

	/* Update cur_offset for next member */
	cur_offset = ntab;

	return (0);
}

/*
 * Given an offset and a tab size, this will return the nearest tab from
 * the offset. If the offset is already tab'd properly, this will return
 * the next tab position.
 */
int
next_tab(offset, tab_size)
int	offset;
int	tab_size;
{
	int	div;

	div = (offset / tab_size) + 1;

	return (div * tab_size);
}
