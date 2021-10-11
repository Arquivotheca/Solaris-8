/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)extn.h	1.1	97/09/07 SMI"

/*
 * external declarations common to all source files
 *
 */
extern  int	tflag;
extern  int	Cflag;
extern	int	oflag;
extern	int	sflag;
extern  int	Sflag;
extern	int	trace;
extern	int	fflag;
extern  int	Fflag;
extern	int	Lflag;
extern	short	aflag;
extern  NFUNC   *ffunction;
extern	char	**namedsec;
extern	int	*namedtype;
extern	int	nsecs;
extern int	trace;

#if defined(i386)
extern int Rflag; /* Reverse 286/386 mode */
#endif	/* i386 */

extern Elf		*elf, *arf;
extern Elf_Arhdr	*mem_header;
extern Elf_Scn		*scn;
extern GElf_Shdr	g_shdr, *shdr;
extern GElf_Shdr	g_scnhdr, *scnhdr;
extern Elf_Cmd		cmd;
extern GElf_Ehdr	g_ehdr, *ehdr;
extern Elf_Data		*data;
extern unsigned char 	*p_data;
extern unsigned char	*ptr_line_data;
extern size_t		size_line;
extern int		archive;
extern int		debug;
extern int		line;
extern int		symtab;
extern int		Rel_sec;
extern int		Rel_data;
extern int		Rela_data;
extern int		elfclass;
extern unsigned short	cur1byte;
extern unsigned short	cur2bytes;
extern unsigned short	curbyte;
extern char		*fname;
extern	int		Rel_sec;
extern	char		*sname, *fname;
extern	FUNCLIST	*next_function, *currfunc;
extern	SCNLIST		*sclist;
#if defined(_LITTLE_ENDIAN)
extern unsigned long	cur4bytes;
#endif /* _LITTLE_ENDIAN */
extern char		bytebuf[];
extern GElf_Sxword	loc;

extern char		object[];
extern char		mneu[];
extern char		symrep[];
extern int		nosyms;
extern unsigned long	prevword;
extern int		sparcver;
