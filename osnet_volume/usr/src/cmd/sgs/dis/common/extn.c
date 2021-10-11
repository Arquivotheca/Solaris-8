/*
 * Copyright (c) 1990-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)extn.c	1.6	97/09/07 SMI"

/*
 *	This file contains those global variables that are used in more
 *	than one source file. This file is meant to be an
 *	aid in keeping track of the variables used.  Note that in the
 *	other source files, the global variables used are declared as
 *	'static' to make sure they will not be referenced outside of the
 *	containing source file.
 */

#include	"dis.h"

Elf		*elf, *arf;
Elf_Arhdr	*mem_header;
Elf_Scn		*scn;
GElf_Shdr	g_shdr, *shdr = &g_shdr;
GElf_Shdr	g_scnhdr, *scnhdr = &g_scnhdr;
Elf_Cmd		cmd;
GElf_Ehdr	g_ehdr, *ehdr = &g_ehdr;
Elf_Data	*data;
unsigned char	*p_data;
unsigned char	*ptr_line_data;
size_t		size_line;
int		archive = 0;
int		debug = 0;
int		line  = 0;
int		symtab = 0;
int		Rel_sec = 0;
int		Rela_data = 0;
int		Rel_data = 0;
int		elfclass;
unsigned short	cur1byte;	/* for storing the results of 'get1byte()' */
unsigned short	cur2bytes;	/* for storing the results of 'get2bytes()' */
unsigned short	curbyte;

#if defined(_LITTLE_ENDIAN)
	unsigned long cur4bytes; /* for storing the results of 'get4bytes()' */
#endif	/* _LITTLE_ENDIAN */

char	bytebuf[4];

/*
 * aflag, oflag, trace, Lflag, sflag, and tflag are flags that are set
 * to 1 if specified by the user as options for the disassembly run.
 *
 * aflag     (set by the -da option) indicates that when disassembling
 *	     a section as data, the actual address of the data should be
 *	     printed rather than the offset within the data section.
 * oflag     indicates that output is to be in octal rather than hex.
 * trace     is for debugging of the disassembler itself.
 * Lflag     is for looking up labels.
 * fflag     is for disassembling named functions.
 *	     fflag is incremented for each named function on the command line.
 * ffunction contains information about each named function
 * sflag     is for symbolic disassembly (VAX, U3B, N3B and M32 only)
 * tflag     (set by the -t option) used later to determine if the .rodata
 *	     section was given as the section name to the -t option.
 * Sflag     is for forcing SPOP's to be disassembled as SPOP's (M32 only)
 * Rflag     specifies to Reverse 286/386 mode for translating boot (I386 only).
 * Cflag     call C++ demangle routine to demangle names before printing.
 */

int	oflag = 0;
int	trace = 0;
int	Lflag = 0;
int	Cflag = 0;
int	tflag = 0;
short	aflag = 0;
#if defined(i386)
int 	Rflag = 0;
#endif	/* i386 */
int	fflag = 0;
int	sflag = 0;
int	Sflag = 0;
int	Fflag = 0;

NFUNC   *ffunction;

GElf_Sxword	 loc = 0; /* byte location in section being disassembled */
char	object[NHEX];	/* array to store object code for output	 */
char	mneu[NLINE];	/* array to store mnemonic code for output	 */
char	symrep[NLINE];  /* array to store symbolic disassembly output	 */

char	*fname;		/* to save and pass name of file being processed */
char	*sname; 	/* to save and pass name of a section		 */

char	**namedsec;	/* contains names of sections to be disassembled */
int	*namedtype;	/* specifies whether the corresponding section	 */
			/* is to be disassembled as text or data	 */

int	nsecs = -1;		/* number of sections in the above arrays */

FUNCLIST	*next_function;	/* structure containing name and address  */
				/* of the next function in a text section */

SCNLIST		*sclist;	/* structure containing list of sections  */
				/* to be disassembled			  */
