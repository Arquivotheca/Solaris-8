/*
 * Copyright (c) 1990-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)utls.c	1.13	97/11/21 SMI"

/*
 * miscellaneous routines for dis-specific symbol handling,
 * (the debuggers have their own routines that supply this functionality);
 * endian byte-swapping, I/O, and output conversion.
 */

#include	"dis.h"
#include	"extn.h"

#define	TOOL	"dis"

#define	FUNC_NM	0	/* for line_nums func, indicates the	*/
				/* line number entry value for the 	*/
				/* beginning of a function		*/
#define	BADADDR	-1L	/* used by the resynchronization	*/
				/* function to indicate that a restart	*/
				/* candidate has not been found	*/
/*
 *	This structure is used for storing data needed for the looking up
 *	of symbols for labels.  It consists of a pointer to the labels' name
 *	and it's location within the section, and a pointer to another
 *	structure of its own type (a forward linked list will be created).
 */

struct	LABELS {
	unsigned char	*label_name;
	GElf_Sxword	label_loc;
	struct	LABELS *next;
};

typedef struct LABELS elt1;
typedef elt1 *link1;
static	link1	fhead = NULL, ftail;

FUNCLIST	*currfunc;

static		GElf_Rel	*rel_data;
static		size_t		no_rel_entries;
static		size_t		no_sym_entries;
static		Elf_Data	*rel_sym_data;
static		GElf_Word	rel_sym_str_index;
static		char		*sym_name;

extern		char		*demangled_name();

/*
 *  Determine if the current section has a relocation section.
 *  If it does, then its relocation section will be searched
 *  during the symbolic disassembly phase.
 */
int
get_rel_section(GElf_Word sec_no)
{
	Elf_Scn		*scn,  *scn1;
	GElf_Shdr	g_shdr, g_shdr1;
	GElf_Shdr	*shdr = &g_shdr, *shdr1 = &g_shdr1;
	Elf_Data	*data, *data1;

	Rel_data = FAILURE;
	scn = 0;
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		if (gelf_getshdr(scn, shdr) != 0) {
			if (shdr->sh_type == SHT_REL &&
			    shdr->sh_info == sec_no) {
				/* get relocation data */
				data = 0;
				if ((data = elf_getdata(scn, data)) == 0 ||
				    data->d_size == 0)
					return (FAILURE);

				rel_data = (GElf_Rel *)data->d_buf;
				no_rel_entries = data->d_size / gelf_fsize(
					elf, ELF_T_REL, 1, EV_CURRENT);
				/* get its associated symbol table */
				if ((scn1 = elf_getscn(elf, shdr->sh_link))
				    != 0) {
					/*
					 * get index of symbol table's
					 * string table.
					 */
					if (gelf_getshdr(scn1, shdr1) != 0)
					    rel_sym_str_index = shdr1->sh_link;
					else
					    return (FAILURE);
					data1 = 0;
					if ((data1 = elf_getdata(scn1, data1))
					    == 0 || data1->d_size == 0)
						return (FAILURE);
					rel_sym_data = (Elf_Data *)data1;
					no_sym_entries =
					    data1->d_size / gelf_fsize(elf,
						ELF_T_SYM, 1, EV_CURRENT);
				} else
					return (FAILURE);

				Rel_data = SUCCESS;
				return (SUCCESS);
			}
		} else
			return (FAILURE);
	}
	return (FAILURE);
}

int spc;

void
search_rel_data(GElf_Addr in_offset, GElf_Sxword val, char **pos)
{

	GElf_Rel	*rel_data1;
	size_t		no_rel_entries1;
	size_t		sym;
	GElf_Sym	g_symbol, *symbol = &g_symbol;

	no_rel_entries1 = no_rel_entries;

	if (Rel_data) {
		rel_data1 = rel_data;
		while (no_rel_entries1--) {
			if (in_offset == rel_data1->r_offset) {
				sym = GELF_R_SYM(rel_data1->r_info);
				if ((sym <= no_sym_entries) && (sym >= 1)) {
					(void) gelf_getsym(rel_sym_data,
					    sym, &g_symbol);
					sym_name = (char *) elf_strptr(elf,
					    rel_sym_str_index, symbol->st_name);
					if (Cflag)
						sym_name =
						    demangled_name(sym_name);
					sprintf(*pos, "%s", sym_name);
					*pos += strlen(sym_name);
					return;
				} else {
					sprintf(*pos, oflag ? "0%llo" :
					    "0x%llx", val);
					*pos += 10;
					return;
				}
			}
			rel_data1++;
		}
		sprintf(*pos, oflag ? "0%llo":"0x%llx", val);
		*pos += 10;
		return;
	}
}

/*
 * locsympr(long val, int regno, char **pos)	locate local symbols
 *
 * val    offset from base register
 * regno  base register
 * pos    position in output string
 */
void
locsympr(GElf_Sxword val, int regno, char **pos)
{
#if defined(i386)
	extern	char	**regname;
#else	/* !i386 */
	extern	char	*regname[];
#endif	/* i386 */

	if (trace)
		printf("\noffset from base reg %d is %lld\n", regno, val);

	if (regno == -1) {
		sprintf(*pos, "%s", regname[val]);
		*pos += strlen(regname[val]);
	} else {
#if defined(i386)
		sprintf(*pos, oflag ? "0%llo+%s":"0x%llx+%s", val,
		    regname[regno]);
		*pos += strlen(regname[regno]);
#else	/* !i386 */
		sprintf(*pos, "%s", regname[val]);
		*pos += strlen(regname[val]);
		sprintf(*pos, oflag ? "(0%llo+%s)":"(0x%llx+%s)",
					val, regname[regno]);
		*pos += strlen(regname[regno])+10;
#endif	/* i386 */
	}
}

/*
 *	extsympr(GElf_Sxword val, char **pos)
 *
 *	find external symbols
 *
 *	arguments:
 *	val	address of current operand
 *	pos	position in output string
 */
void
extsympr(GElf_Sxword val, char **pos)
{
	Elf_Scn		*scn;
	GElf_Shdr	g_shdr, *shdr = &g_shdr;
	Elf_Data	*data;
	GElf_Sym	sym, *sym_data = &sym;
	size_t		no_sym_entries;
	char		*sym_name;
	int		sym_type, sym_counter;


	if ((scn = elf_getscn(elf, symtab)) != 0) {
	    if (gelf_getshdr(scn, shdr) != 0) {
		data = 0;
		sym_counter = 1;
		if ((data = elf_getdata(scn, data)) != 0 && data->d_size != 0) {
			no_sym_entries = data->d_size / gelf_fsize(elf,
			    ELF_T_SYM, 1, EV_CURRENT);

		    while (no_sym_entries--) {
			(void) gelf_getsym(data, sym_counter, &sym);
			sym_type = GELF_ST_TYPE(sym_data->st_info);
			sym_name = (char *) elf_strptr(elf, shdr->sh_link,
			    sym_data->st_name);
			if (Cflag)
				sym_name = demangled_name(sym_name);
#if 0
			printf("%s %d %d     ", sym_name, sym_data->st_value,
			    sym_data->st_size);
			if ((no_sym_entries % 3) == 0) printf("\n");
			if (0 && strcmp(sym_name, "main") == 0) {
				printf("now\n");
			}
#endif
			if (((sym_type == STT_OBJECT ||
			    sym_type == STT_NOTYPE) &&
			    (val == sym_data->st_value ||
			    (val >= sym_data->st_value &&
			    val < sym_data->st_value + sym_data->st_size))) ||
			    (sym_type == STT_FUNC &&
			    val == sym_data->st_value)) {
				sym_name = (char *) elf_strptr(elf,
				    shdr->sh_link, sym_data->st_name);
				if (Cflag)
					sym_name = demangled_name(sym_name);
				if (val - sym_data->st_value) {
					sprintf(*pos, "(%s+%lld)",
					    sym_name ? sym_name : "(null)",
					    val - sym_data->st_value);
					*pos += (sym_name ? strlen(sym_name) :
					    strlen("(null)")) + 10;
				} else {
					sprintf(*pos, "%s", sym_name ?
					    sym_name : "(null)");
					*pos += sym_name ? strlen(sym_name) :
					    strlen("(null)");
				}
				return;
			}
			sym_counter++;
		    }
		}
	    }
	}
	sprintf(*pos, oflag ? "0%llo":"0x%llx", val);
	*pos += 10;
}


/*
 *	build_labels ()
 *
 *	Construct a forward linked structure containing all the label entries
 *	found in the  .debug section.  This is needed in looking up the
 *	labels.
 */
void
build_labels(unsigned char *name, GElf_Sxword location)
{
	if (fhead == NULL) {
		fhead = (link1) malloc(sizeof (elt1));
		ftail = fhead;
	} else {
		ftail->next = (link1) malloc(sizeof (elt1));
		ftail = ftail->next;
	}
	ftail->label_name = name;
	ftail->label_loc  = location;
	ftail->next = NULL;

	if (trace > 0)
		(void) printf("\nlabel_name %s and location %lld\n",
		    ftail->label_name, ftail->label_loc);
}


void
label_free(void)
{
	link1 fheadp;
	link1 temp;

	if (fhead == NULL)
		return;

	fheadp = fhead;
	while (fheadp) {
		temp = fheadp;
		fheadp = fheadp->next;
		(void) free(temp);
	}

	ftail = fhead = NULL;
}


/*
 *	compoff (lng, temp)
 *
 *	This routine will compute the location to which control is to be
 *	transferred.  'lng' is the number indicating the jump amount
 *	(already in proper form, meaning masked and negated if necessary)
 *	and 'temp' is a character array which already has the actual
 *	jump amount.  The result computed here will go at the end of 'temp'.
 *	(This is a great routine for people that don't like to compute in
 *	hex arithmetic.)
 */
void
compoff(GElf_Sxword lng, char *temp)
{
	lng += loc;
	if (oflag)
		(void) sprintf(temp, "%s <%llo>", temp, lng);
	else
		(void) sprintf(temp, "%s <%llx>", temp, lng);
}


/*
 * void lookbyte ()
 *
 * read a byte, mask it, then return the result in 'curbyte'.
 * The byte is not immediately placed into the string object[].
 * is incremented.
 */
void
lookbyte(void)
{
	unsigned char *p = (unsigned char *)&curbyte;

	*p = *p_data; ++p_data;
	loc++;
	curbyte = *p & 0377;
}


/*
 * void getbyte ()
 *
 * read a byte, mask it, then return the result in 'curbyte'.
 * The getting of all single bytes is done here.  The 'getbyte[s]'
 * routines are the only place where the global variable 'loc'
 * is incremented.
 */
void
getbyte(void)
{
	char temp[NCPS+1];
	unsigned char *p = (unsigned char *)&curbyte;

	*p = *p_data; ++p_data;
	loc++;
	curbyte = *p & 0377;
#if defined(i386)
	convert(curbyte, temp, NOLEADSHORT);
#else	/* !i386 */
	convert(curbyte, temp, NOLEAD);
#endif	/* i386 */
	(void) sprintf(object, "%s%s ", object, temp);
	if (trace > 1) {
		(void) printf("\nin getbyte object <%s>\n", object);
	}
}



/*
 *	void convert (num, temp, flag)
 *
 *	Convert the passed number to either hex or octal, depending on
 *	the oflag, leaving the result in the supplied string array.
 *	If  LEAD  is specified, preceed the number with '0' or '0x' to
 *	indicate the base (used for information going to the mnemonic
 *	printeut).  NOLEAD  will be used for all other printing (for
 *	printing the offset, object code, and the second byte in two
 *	byte immediates, displacements, etc.) and will assure that
 *	there are leading zeros.
 */
void
convert(unsigned num, char temp[], int flag)
{
	if (flag == NOLEAD)
		(oflag) ?	(void) sprintf(temp, "%06o", num):
				(void) sprintf(temp, "%04x", num);

	if (flag == LEAD)
		(oflag) ?	(void) sprintf(temp, "0%o", num):
				(void) sprintf(temp, "0x%x", num);
#if defined(i386)
	if (flag == NOLEADSHORT)
		(oflag) ?	(void) sprintf(temp, "%03o", num):
				(void) sprintf(temp, "%02x", num);
#endif	/* i386 */
}


/*
 *	dis_data ()
 *
 *	the routine to disassemble a data section,
 *	which consists of just dumping it with byte offsets
 */
void
dis_data(GElf_Shdr *shdr)
{
	static 	void	get2bytes();
#if defined(_LITTLE_ENDIAN)
	static	void	getswapb2();
#endif	/* _LITTLE_ENDIAN */

	short	count;
	GElf_Addr	last_addr;

	/*
	 * Blank out mneu so the printline routine won't print extraneous
	 * garbage.
	 */

	(void) sprintf(mneu, "");

	for (loc = aflag? shdr->sh_addr: 0, last_addr = loc + shdr->sh_size;
	    loc < last_addr; printline()) {
		/*
		 * if -da flag specified, actual address will be printed
		 * if -d flag specified, offset within section
		 * will be printed.
		 */

		(void) printf("\t");
		prt_offset();
#ifdef M32
		for (count = 0; (count < 8) && (loc < last_addr); count += 2)
#if defined(_LITTLE_ENDIAN)
			getswapb2();
#else	/* !_LITTLE_ENDIAN */
			get2bytes();
#endif	/* _LITTLE_ENDIAN */
#endif

#if defined(i386)
		for (count = 0; (count < 6) && (loc < last_addr); count += 2)
			get2bytes();
#endif	/* i386 */
	}
}


#if U3B || N3B || U3B15 || U3B5 || M32
/*
 *	dfpconv(fpword1, fpword2, fpdoub, fpshort)
 *
 *	This routine will convert the 2 longs (64 bit) "fpword1 fpword2" double
 *	precision floating point representation of a number into its decimal
 *	equivalent. The result will be stored in *fpdoub. The routine will
 *	return a value indicating what type of floating point number was
 *	converted.
 *	*NOTE*	The conversion routine will calculate a decimal value
 *		if the disassembler is to run native mode on the 3B.
 *		If the 3B disassembler is to be run on a DEC processor
 *		(pdp11 or vax) the routine will store the exponent in
 *		*fpshort. The mantissa will be stored in the form:
 *		"T.fraction" where "T" is the implied bit and the
 *		fraction is of radix 2. The mantissa will be stored
 *		in *fpdoub. This is due to the difference in range
 *		of floating point numbers between the 3B and DEC
 *		processors.
 */
int
dfpconv(GElf_Sxword fpword1, GElf_Sxword fpword2,
    double *fpdoub, short *fpshort)
{
	unsigned short exponent;
	short	leadbit, signbit;
	double	dtemp, mantissa;
	long	ltemp;
#if U3B || U3B5 || U3B15
	double dec2exp;
#endif

	exponent = (unsigned short)((fpword1>>20) & 0x7ffL);
	/* exponent is bits 1-11 of the double	*/

	ltemp = fpword1 & 0xfffffL;	/* first 20 bits of mantissa */
	mantissa = ((double)ltemp * TWO_32) + (double)fpword2;
	/* mantissa is bits 12-63 of the double	*/

	signbit = (short)((fpword1 >> 31) & 0x1L);
	/* sign bit (1-negative, 0-positive) is bit 0 of double	*/

	leadbit = 1;
	/* implied bit to the left of the decimal point	*/

	if (exponent == 2047)
		if (mantissa)
			return (NOTANUM);
		else
			return ((signbit) ? NEGINF : INFINITY);

	if (exponent == 0)
		if (mantissa)
		/*
		 * This is a denormalized number. The implied bit to
		 * the left of the decimal point is 0.
		 */
			leadbit = 0;
		else
			return ((signbit) ? NEGZERO : ZERO);

	/*
	 * Convert the 52 bit mantissa into "I.fraction" where
	 * "I" is the implied bit. The 52 bits are divided by
	 * 2 to the 52rd power to transform the mantissa into a
	 * fraction. Then the implied bit is added on.
	 */
	dtemp = (double)(leadbit + (mantissa/TWO_52));

#if U3B || U3B5 || U3B15
	/*
	 * Calculate 2 raised to the (exponent-BIAS) power and
	 * store it in a double.
	 */
	if (exponent < DBIAS)
		for (dec2exp = 1; exponent < DBIAS; ++exponent)
			dec2exp /= 2;
	else
		for (dec2exp = 1; exponent > DBIAS; --exponent)
			dec2exp *= 2;

	/*
	 * Multiply "I.fraction" by 2 raised to the (exponent-BIAS)
	 * power to obtain the decimal floating point number.
	 */
	*fpdoub = dtemp *dec2exp;

	if (signbit)
		*fpdoub = -(*fpdoub);
	return (FPNUM);
#else
	*fpshort = exponent - DBIAS;
	*fpdoub = ((signbit)? (-dtemp): dtemp);
	return (FPBIGNUM);
#endif /* U3B | U3B5 | U3B15 */
}
#endif /* U3B | N3B | U3B15 | U3B5 | M32 */


/*
 *	get2bytes()
 *
 *	This routine will get 2 bytes, print them in the object file
 *	and place the result in 'cur2bytes'.
 */
static void
get2bytes(void)
{
	unsigned	char 	*p = (unsigned char *)&cur2bytes;
	char	temp[NCPS+1];

	*p = *p_data; ++p_data; ++p;
	*p = *p_data; ++p_data;
	loc += 2;
	convert((cur2bytes & 0xffff), temp, NOLEAD);
	(void) sprintf(object, "%s%s ", object, temp);
	if (trace > 1)
		(void) printf("\nin get2bytes object<%s>\n", object);
}


#if defined(_LITTLE_ENDIAN)
/*
 *	static void getswapb2()
 *
 *	This routine is used only for little-endian implementations.
 *	It will get and swap 2 bytes, print them in the object file,
 *	and place the result in 'cur2bytes'.
 */
static void
getswapb2(void)
{
	char	temp[NCPS+1];
	unsigned char *p = (unsigned char *)&cur2bytes;

	*p = *p_data; ++p_data; ++p;
	*p = *p_data; ++p_data;
	loc += 2;

	/* swap the 2 bytes contained in 'cur2bytes' */
	cur2bytes = ((cur2bytes>>8) & (unsigned short)0x00ff) |
	    ((cur2bytes<<8) & (unsigned short)0xff00);
	convert((cur2bytes & 0xffff), temp, NOLEAD);
	(void) sprintf(object, "%s%s ", object, temp);

	if (trace > 1)
		(void) printf("\nin getswapb2 object<%s>\n", object);
}
#endif	/* _LITTLE_ENDIAN */


/*
 *	line_nums ()
 *
 *	This function prints out the names of functions being disassembled
 *	and break-pointable line numbers.  First it checks the address
 *	of the next function in the list of functions; if if matches
 *	the current location, it prints the name of that function.
 *
 *	It then examines the line number entries. If the address of the
 *	current line number equals that of the current location, the
 *	line number is printed.
 */
void
line_nums(void)
{

	while (next_function != NULL) {		/* not there yet */
		if (loc < (GElf_Sxword)next_function->faddr)
			break;

		if (loc > (GElf_Sxword)next_function->faddr) {
			/* this is an error condition */
			(void) fflush(stdout);
			(void) fprintf(stderr,
"\nWARNING: Possible strings in text or bad physical address before location ");
			if (oflag)
				(void) fprintf(stderr, "0%llo\n", loc);
			else
				(void) fprintf(stderr, "0x%llx\n", loc);
			loc = (GElf_Sxword)next_function->faddr;
		}

		(void) printf("%s()\n", next_function->funcnm);

		currfunc = next_function;
		next_function = next_function->nextfunc;
	}
#ifdef DEBUG_INFO
	if (line)
		print_line(loc, ptr_line_data, size_line);
#endif

	(void) printf("\t");
}


/*
 *	check_func ()
 *
 *	This function will check to see if we're very close to
 *	the next function.  If so, and the Intel opcode is 0,
 *	then we assume this is a pad of 0, and return 1.
 */
int
check_func()
{
	extern	FUNCLIST	*next_function;

	if (next_function != NULL) {		/* not there yet */
		if ((next_function->faddr == loc) ||
		    (next_function->faddr == loc + 1) ||
		    (next_function->faddr == loc + 2))
			/* found the label */
			return (1);
	}
	return (0);
}


/*
 *	at_func ()
 *
 *	This function will check to see if we're at the start
 *	of the next function.  If so, then we return 1.
 */
int
at_func()
{
	extern	FUNCLIST	*next_function;

	if (currfunc != NULL && currfunc->faddr == loc)
		return (1);
	return (0);
}


/*
 *	looklabel (addr)
 *
 *	This function will look in the symbol table to see if
 *	a label exists which may be printed.
 */
void
looklabel(GElf_Sxword addr)
{
	link1	label_ptr;
	/*
	 * if (fhead == NULL)	* the file may have debugging info by default,
	 * 	return;		* but it may not have label info because
	 *			* label info in only put in with "cc -g".
	 */

	label_ptr = fhead;
	while (label_ptr != NULL) {
		if (label_ptr->label_loc == addr)
			/* found the label so print it	*/
			(void) printf("%s:\n", label_ptr->label_name);
		label_ptr = label_ptr->next;
	}
}


/*
 *	printline ()
 *
 *	Print the disassembled line, consisting of the object code
 *	and the mnemonics.  The breakpointable line number, if any,
 *	has already been printed, and 'object' contains the offset
 *	within the section for the instruction.
 */
void
printline(void)
{
#if defined(I386)
	if (oflag > 0)
		(void) printf(sflag?"%-36s%s\n%20c[%s]\n":"%-36s%s\n",
		    object, mneu, ' ', symrep); /* to print octal */
	else
		(void) printf(sflag?"%-30s%s\n%20c[%s]\n":"%-30s%s\n",
		    object, mneu, ' ', symrep); /* to print hex */
#endif	/* i386 */

#if defined(SPARC)
	(oflag > 0) ?
		(void) printf("%-26s%s\n", object, mneu): /* to print octal */
		(void) printf("%-27s%s\n", object, mneu); /* to print hex */
#endif	/* SPARC */
}


/*
 *	prt_offset ()
 *
 *	Print the offset, right justified, followed by a ':'.
 */
void
prt_offset(void)
{

	if (oflag)
		(void) sprintf(object, "%6llo:  ", loc);
	else
		(void) sprintf(object, "%4llx:  ", loc);
}


/*
 *	resync ()
 *
 *	If a bad op code is encountered, the disassembler will attempt
 *	to resynchronize itself. The next line number entry and the
 *	next function symbol table entry will be found. The restart
 *	point will be the smaller of these two addresses and bytes
 *	of object code will be dumped (not disassembled) until the
 *	restart point is reached.
 */
int
resync(void)
{
	return (0);
}

#if !I386
#if !M32
int
resync(void)
{
	struct	lineno	*eptr = &line_ent;
	struct	syment	sentry;
	GElf_Sxword	paddr, linaddr, symaddr, dumpaddr, s_end;


	linaddr = BADADDR;
	symaddr = BADADDR;
	s_end = scnhdr.s_paddr + scnhdr.s_size;
	/*
	 * Find the next line number entry if the file has not been
	 * stripped of line number information. Each entry from the
	 * beginning will be read until one having a greater address
	 * than the present location is found.
	 */
	if (nosyms == TRUE) {
		(void) printf("\n\t**   FILE IS STRIPPED  **\t\n");
		(void) printf("\t**   SKIP TWO BYTES AND TRY AGAIN  **\n");
		return (SUCCESS);
	}
	if (stripped() != TRUE) {
		for (; lnno_cnt < scnhdr.s_nlnno; lnno_cnt++) {
			if (eptr->l_lnno == 0) {
				if (symb == NULL)
					break;
				(void) ldtbread(symb, eptr->l_addr.l_symndx,
				    &sentry);
				linaddr = sentry.n_value;
				if (loc <= linaddr)
					goto symfind;
			} else if (eptr->l_addr.l_paddr >= loc) {
				linaddr = eptr->l_addr.l_paddr;
				goto symfind;
			}
			(void) FREAD(eptr, LINESZ, 1, l_ptr);
		}
		/*
		 * Cannot restart based on line number information.
		 * Try to use symbol table information to find restart
		 * point.
		 */
		if (symb == NULL)
			linaddr = BADADDR;
		else
			/*
			 * If no further line numbers have been found,
			 * the address of the section end will be a
			 * candidate for a restart point.
			 */
			linaddr = s_end;
	}
symfind:
	/* Find next function symbol table entry.	*/
	for (; next_function; currfunc = next_function,
	    next_function = next_function->nextfunc)
		if (next_function->faddr >= loc) {
			symaddr = next_function->faddr;
			break;
		}
#if AR32W || defined(_LITTLE_ENDIAN)
	if (next_function == NULL)
		symaddr = scnhdr.s_paddr + scnhdr.s_size;
#else
	/* scan the symbol table	*/
	if ((filhdr.f_nsyms != 0L) && (symb != NULL) &&
	    (ldtbseek(symb) != FAILURE)) {
		/*
		 * set firstfn equal to the symbol table index
		 * of the first function symbol table entry.
		 * The search for the next function symbol
		 * table entry will always begin at firstfn.
		 */
		septr = &stbentry;
		if (firstfn == BADADDR) {
			(void) ldtbread(symb, 0L, septr);
			for (sindex = 1+septr->n_numaux;
			    (sindex < filhdr.f_nsyms) &&
			    ((septr->n_scnum < 1) ||
			    (!ISFCN(septr->n_type) ||
			    (septr->n_value < scnhdr.s_paddr) ||
			    (septr->n_value > s_end))); sindex++) {
				(void) ldtbread(symb, sindex, septr);
				sindex += septr->n_numaux;
			}
			firstfn = sindex -1 - septr->n_numaux;
		}
		(void) ldtbread(symb, (long)firstfn, septr);
		sindex = firstfn + 1 + septr->n_numaux;
		for (; (sindex < filhdr.f_nsyms) && ((septr->n_scnum < 1) ||
		    (septr->n_numaux == 0) || (!ISFCN(septr->n_type)) ||
		    (septr->n_value < scnhdr.s_paddr) ||
		    (septr->n_value > s_end) || (septr->n_value <= loc));
		    sindex++) {
			(void) ldtbread(symb, sindex, septr);
			sindex += septr->n_numaux;
		}
		/*
		* If the next fuction symbol table entry has been
		* found, it is a candidate for a restart point.
		* If the entire symbol table has been read, and a
		* possible restart point has not been found, the end
		* of the section is a candidate for a restart point.
		*/
		if ((sindex - septr->n_numaux) == filhdr.f_nsyms)
			symaddr = s_end;
		else
			symaddr = septr->n_value;
	}
	/* end of the section is end of the function */
#endif

	if (symaddr == BADADDR) {
		/*
		 * Restart point was not found by searching the
		 * symbol table.
		 */
		if (linaddr == BADADDR) {
			/*
			 * Restart point was not found by looking
			 * at line number information.
			 */
			{
				(void) printf(
			"\n\t**\tCANNOT FIND NEXT LINE NUMBER\t\t\t**\n");
				(void) printf(
		"\t**  CANNOT FIND NEXT FUNCTION SYMBOL TABLE ENTRY\t**\n");
				(void) printf(
		"\t**  FOLLOWING DISASSEMBLY MAY BE OUT OF SYNC\t\t**\n");
			}
			return (FAILURE);
		} else {
			/*
			 * The next line number entry will be the
			 * restart point.
			 */
			paddr = linaddr;
			{
				(void) printf(
			"\n\t** OBJECT CODE WILL BE DUMPED UNTIL\t**\n");
				(void) printf(
			"\t** THE NEXT BREAKPOINTABLE LINE NUMBER\t**\n");
			}
		}
	} else if (linaddr == BADADDR) {
		/*
		 * The next function symbol table entry or the end
		 * of the section will be the restart point.
		 */
		paddr = symaddr;
		{
			(void) printf(
			    "\n\t**   OBJECT CODE WILL BE DUMPED UNTIL\t**\n");
			(void) printf(
			    "\t**  THE BEGINNING OF THE NEXT FUNCTION\t**\n");
			(void) printf(
			    "\t**    OR UNTIL THE END OF THE SECTION\t**\n");
		}
	} else {
		/*
		 * The smaller address of the next line number
		 * entry and the next function symbol table entry
		 * will be the restart point.
		 */
		paddr = (linaddr < symaddr) ? linaddr : symaddr;
		{
			(void) printf(
			    "\n\t**    OBJECT CODE WILL BE DUMPED UNTIL\t**\n");
			(void) printf(
			    "\t**           RESTART POINT IS REACHED\t\t**\n");
		}
	}
	dumpaddr = paddr;

	for (; loc < dumpaddr; ) {
		/* Dump bytes until the restart point is reached. */
		(void) printf("\t");
		prt_offset();
#if IAPX || iAPX286 || I386
		getbyte();
		if (loc < dumpaddr)
			getbyte();
#endif
#ifdef MC68
		get2bytes();
#endif
#ifdef N3B
		get2bytes();
#endif
#if M32 || VAX
		get1byte();
#endif
		(void) sprintf(mneu, "");
		printline();
	}	/* end of dump of unused object code	*/

		(void) printf("\n\t** DISASSEMBLER RESYNCHRONIZED **\n");
	return (SUCCESS);

}
#endif /* I386 */
#endif /* M32 */


#if N3B || U3B || M32 || U3B5 || U3B15
/*
 *	sfpconv(fprep, fpdoub)
 *
 *	This routine will convert the long "fprep" single precision
 *	floating point representation of a number into its decimal
 *	equivalent. The result will be stored in *fpdoub. The routine
 *	will return a value indicating what type of floating point
 *	number was converted.
 */
int
sfpconv(long fprep, double *fpdoub)
{
	unsigned short exponent;
	short	leadbit, signbit;
	long	mantissa;
	double	dtemp, dec2exp;

	exponent = (unsigned short)((fprep>>23) & 0xffL);
	/* exponent is bits 1-8 of the long	*/

	mantissa = fprep & 0x7fffffL;
	/* mantissa is bits 9-31 of the long	*/

	signbit = (fprep>>31) & 0x1L;
	/* sign bit (1-negative, 0-positive) is bit 0 of long	*/

	leadbit = 1;
	/* implied bit to the left of the decimal point	*/

	if (exponent == 255)
		if (mantissa)
			return (NOTANUM);
		else
			return ((signbit) ? NEGINF : INFINITY);

	if (exponent == 0)
		if (mantissa)
		/*
		 * This is a denormalized number. The implied bit to
		 * the left of the decimal point is 0.
		 */
			leadbit = 0;
		else
			return ((signbit) ? NEGZERO : ZERO);

	/*
	 * Convert the 23 bit mantissa into "I.fraction" where
	 * "I" is the implied bit. The 23 bits are divided by
	 * 2 to the 23rd power to transform the mantissa into a
	 * fraction. Then the implied bit is added on.
	 */
	dtemp = (double)(leadbit + (double)mantissa/TWO_23);

	/*
	 * Calculate 2 raised to the (exponent-BIAS) power and
	 * store it in a double.
	 */
	if (exponent < BIAS)
		for (dec2exp = 1; exponent < BIAS; ++exponent)
			dec2exp /= 2;
	else
		for (dec2exp = 1; exponent > BIAS; --exponent)
			dec2exp *= 2;

	/*
	 * Multiply "I.fraction" by 2 raised to the (exponent-BIAS)
	 * power to obtain the decimal floating point number.
	 */
	*fpdoub = dtemp *dec2exp;

	if (signbit)
		*fpdoub = -(*fpdoub);
	return (FPNUM);
}
#endif	/* N3B | U3B | M32 | U3B5 | U3B15 */


/*
 *	fatal()
 *
 *	print an error message and quit
 */
void
fatal(char *message)
{
	if (archive)
		(void) fprintf(stderr, "\n%s: %s[%s]: %s\n", TOOL, fname,
		    mem_header->ar_name, message);
	else
		(void) fprintf(stderr, "\n%s: %s: %s\n", TOOL, fname,  message);

	exit(1);
	/*NOTREACHED*/
}


#if !defined(I386)
/* bext() sign extends the first byte of byt and puts the result in lval */

long
bext(int byt)
{
	long	lval; /* signed long value */

	lval = byt;
	if (byt & 0x0080)
		lval = lval | 0xffffff00;
	else
		lval = lval & 0x000000ff;
	return (lval);
}


/*
 * hext() sign extends the first half word of half and puts the result in lval
 */
long
hext(int half)
{
	long	lval; /* signed long value */

	lval = half;
	if (half & 0x8000)
		lval = lval | 0xffff0000;
	else
		lval = lval & 0x0000ffff;
	return (lval);
}
#endif	/* !defined(i386) */


/* demangle C++ names */
static char *format = "%s\t[%s]";
char *formatted;

char *
demangled_name(char *s)
{
	int ret_demangle;
	char *buf;

	buf = (char *)sgs_demangle(s);

	if (strcmp(s, buf) == 0)
		return (s);

	if (formatted != NULL)
		free(formatted);

	formatted = (char *)malloc(strlen(buf) + 1 + 1 + strlen(s) + 1 + 1);
	if (formatted == NULL)
		return (s);

	(void) sprintf(formatted, format, buf, s);
	return (formatted);
}
