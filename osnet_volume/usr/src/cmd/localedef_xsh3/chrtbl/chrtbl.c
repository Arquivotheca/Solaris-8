/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)chrtbl.c	1.18	99/05/04 SMI"

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module was created for NLS on Sep. 02 '87		*/	/*NLS*/

/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/
/*								*/
/* 	wchrtbl - generate character class definition table	*/
/*			for supplementary code set		*/
/*								*/
/*	end for NLS	*/					
#include <stdio.h>
#include <ctype.h>
#include <varargs.h>
#include <string.h>
#include <signal.h>

/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/
#include <stdlib.h>
#include <wctype.h>
/*	end for NLS	*/

/*	Definitions	*/

#define HEX    1
#define OCTAL  2
#define RANGE  1
#define UL_CONV 2
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/
#define	CSLEN	10
/* #define SIZE	2 * 257 */
#define SIZE	2 * 257 + CSLEN 	/* SIZE must be multiple of 4	*/
/*	end for NLS	*/
#define	START_CSWIDTH	(2 * 257)
#define	START_NUMERIC	(2 * 257) + 7
#define	ISUPPER		1
#define ISLOWER		2
#define ISDIGIT		4 
#define ISSPACE		8	
#define ISPUNCT		16
#define ISCNTRL		32
#define ISBLANK		64
#define ISXDIGIT	128
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
#define UL		0xff
#define	ISWCHAR1	0x00000100	/* phonogram (international use) */
#define	ISWCHAR2	0x00000200	/* ideogram (international use) */
#define	ISWCHAR3	0x00000400	/* English (international use) */
#define	ISWCHAR4	0x00000800	/* number (international use) */
#define	ISWCHAR5	0x00001000	/* special (international use) */
#define	ISWCHAR6	0x00002000	/* reserved (international use) */
#define	ISWCHAR7	0x00004000	/* reserved (international use) */
#define	ISWCHAR8	0x00008000	/* reserved (international use) */
#define	ISWCHAR9	0x00010000
#define	ISWCHAR10	0x00020000
#define	ISWCHAR11	0x00040000
#define	ISWCHAR12	0x00080000
#define	ISWCHAR13	0x00100000
#define	ISWCHAR14	0x00200000
#define	ISWCHAR15	0x00400000
#define	ISWCHAR16	0x00800000
#define	ISWCHAR17	0x01000000
#define	ISWCHAR18	0x02000000
#define	ISWCHAR19	0x04000000
#define	ISWCHAR20	0x08000000
#define	ISWCHAR21	0x10000000
#define	ISWCHAR22	0x20000000
#define	ISWCHAR23	0x40000000
#define	ISWCHAR24	0x80000000
/*	end for NLS	*/						/*NLS*/
#define	LC_CTYPE	10
#define	CSWIDTH		11
#define	LC_NUMERIC	13
#define DECIMAL_POINT	14
#define THOUSANDS_SEP	15
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
#define LC_CTYPE1	21
#define LC_CTYPE2	22
#define LC_CTYPE3	23
#define	WSIZE		0xffff * 4
#define NUM_BANKS	20
/*	end for NLS	*/						/*NLS*/

extern	int	unlink();
extern	int	atoi();

/*   Internal functions  */

static	void	error();
static	void	init();
static	void	process();
static	void	create1();
static	void	create2();
static	void	create3();
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
static	void	pre_process();
static	void	create0w();
static	void	create1w();
static	void	create2w();
static	void	setmem();
/*	end for NLS	*/
static 	void	createw_empty();
static  void	parse();
static	int	empty_line();
static	void	check_chrclass();
static	void	clean();
static	void	comment1();
static	void	comment2();
static	void	comment3();
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
static	void	comment4();
static	void	comment5();
/*	end for NLS	*/
static 	int	cswidth();
static 	int	setnumeric();
static	int	check_digit();
static	unsigned  char	*clean_line();

/*	Static variables	*/

struct  classname	{
	char	*name;
	int	num;
	char	*repres;
}  cln[]  =  {
	"isupper",	ISUPPER,	"_U",
	"islower",	ISLOWER,	"_L",
	"isdigit",	ISDIGIT,	"_N",
	"isspace",	ISSPACE,	"_S",
	"ispunct",	ISPUNCT,	"_P",
	"iscntrl",	ISCNTRL,	"_C",
	"isblank",	ISBLANK,	"_B",
	"isxdigit",	ISXDIGIT,	"_X",
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
	"iswchar1",	ISWCHAR1,	"_E1",
	"isphonogram",	ISWCHAR1,	"_E1",
	"iswchar2",	ISWCHAR2,	"_E2",
	"isideogram",	ISWCHAR2,	"_E2",
	"iswchar3",	ISWCHAR3,	"_E3",
	"isenglish",	ISWCHAR3,	"_E3",
	"iswchar4",	ISWCHAR4,	"_E4",
	"isnumber",	ISWCHAR4,	"_E4",
	"iswchar5",	ISWCHAR5,	"_E5",
	"isspecial",	ISWCHAR5,	"_E5",
	"iswchar6",	ISWCHAR6,	"_E6",
	"iswchar7",	ISWCHAR7,	"_E7",
	"iswchar8",	ISWCHAR8,	"_E8",
	"iswchar9",	ISWCHAR9,	"_E9",
	"iswchar10",	ISWCHAR10,	"_E10",
	"iswchar11",	ISWCHAR11,	"_E11",
	"iswchar12",	ISWCHAR12,	"_E12",
	"iswchar13",	ISWCHAR13,	"_E13",
	"iswchar14",	ISWCHAR14,	"_E14",
	"iswchar15",	ISWCHAR15,	"_E15",
	"iswchar16",	ISWCHAR16,	"_E16",
	"iswchar17",	ISWCHAR17,	"_E17",
	"iswchar18",	ISWCHAR18,	"_E18",
	"iswchar19",	ISWCHAR19,	"_E19",
	"iswchar20",	ISWCHAR20,	"_E20",
	"iswchar21",	ISWCHAR21,	"_E21",
	"iswchar22",	ISWCHAR22,	"_E22",
	"iswchar23",	ISWCHAR23,	"_E23",
	"iswchar24",	(int)ISWCHAR24,	"_E24",
/*	end for NLS	*/						/*NLS*/
	"ul",		UL,		NULL,
	"LC_CTYPE",	LC_CTYPE,	NULL,
	"cswidth",	CSWIDTH,	NULL,
	"LC_NUMERIC",	LC_NUMERIC,	NULL,
	"decimal_point",DECIMAL_POINT,	NULL,
	"thousands_sep",THOUSANDS_SEP,	NULL,
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
	"LC_CTYPE1",	LC_CTYPE1,	NULL,				/*NLS*/
	"LC_CTYPE2",	LC_CTYPE2,	NULL,				/*NLS*/
	"LC_CTYPE3",	LC_CTYPE3,	NULL,				/*NLS*/
/*	end for NLS	*/						/*NLS*/
	NULL,		NULL,		NULL
};

int	readstd;			/* Process the standard input */
unsigned char	linebuf[256];		/* Current line in input file */
unsigned char	 *p = linebuf;
int	chrclass = 0;			/* set if LC_CTYPE is specified */
int	lc_numeric;			/* set if LC_NUMERIC is specified */
int	lc_ctype;
char	chrclass_name[20];		/* save current chrclass name */
int	chrclass_num;			/* save current chrclass number */
int	ul_conv = 0;			/* set when left angle bracket
					 * is encountered. 
					 * cleared when right angle bracket
					 * is encountered */
int	cont = 0;			/* set if the description continues
					 * on another line */
int	action = 0;			/*  action = RANGE when the range
					 * character '-' is ncountered.
					 *  action = UL_CONV when it creates
					 * the conversion tables.  */
int	in_range = 0;			/* the first number is found 
					 * make sure that the lower limit
					 * is set  */
int	ctype[SIZE];			/* character class and character
					 * conversion table */
int	range = 0;			/* set when the range character '-'
					 * follows the string */
int	width;				/* set when cswidth is specified */
int	numeric;			/* set when numeric is specified */
char	tablename1[24];			/* save name of the first data file */
char	tablename2[24];			/* save name of the second date file */
char	*cmdname;			/* Save command name */
char	input[24];			/* Save input file name */
char	tokens[] = ",:\0";
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
int	codeset=0;			/* code set number */
int	codeset1=0;			/* set when charclass1 found */
int	codeset2=0;			/* set when charclass2 found */
int	codeset3=0;			/* set when charclass3 found */
unsigned *wctyp[3];			/* tmp table for wctype */
unsigned int	*wconv[3];			/* tmp table for conversion */
struct	_wctype	*wcptr[3];		/* pointer to ctype table */
struct	_wctype	wctbl[3];		/* table for wctype */
unsigned char *cindex[3];		/* code index	*/
unsigned *type[3];			/* code type	*/
unsigned int	*code[3];			/* conversion code	*/
int	cnt_index[3];			/* number of index	*/
int	cnt_type[3];			/* number of type	*/
int	cnt_code[3];			/* number conversion code */
int	num_banks[3] = {0,0,0};		/* number of banks used */
/*	end for NLS	*/						/*NLS*/

/* Error  messages */
/* vprintf() is used to print the error messages */

char	*msg[] = {
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
/*    0    */ "Usage: wchrtbl [ - | file ] ",
/*	end for NLS	*/						/*NLS*/
/*    1    */ "the name of the output file for \"LC_CTYPE\" is not specified",
/*    2    */ "incorrect character class name \"%s\"",
/*    3    */ "--- %s --- left angle bracket  \"<\" is missing",
/*    4    */ "--- %s --- right angle bracket  \">\" is missing",
/*    5    */ "--- %s --- wrong input specification \"%s\"",
/*    6    */ "--- %s --- number out of range \"%s\"",
/*    7    */ "--- %s --- nonblank character after (\"\\\") on the same line",
/*    8    */ "--- %s --- wrong upper limit \"%s\"",
/*    9    */ "--- %s --- wrong character \"%c\"",
/*   10    */ "--- %s --- number expected",
/*   11    */ "--- %s --- too many range \"-\" characters",
/*   12    */ "--- %s --- wrong specification, %s",
/*   13    */ "malloc error",
/*   14	   */ "--- %s --- wrong specification, %s",
/*   15    */ "the name of the output file for \"LC_NUMERIC\" is not specified",
/*   16    */ "numeric editing information \"numeric\" must be specified",
/*   17    */ "character classification and conversion information must be specified",
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
/*   18    */ "the same output file name was used in \"LC_CTYPE\" and \"LC_NUMERIC\"",
/*   19    */ "character class \"LC_CTYPE%d\" duplicated",
/*   20    */ "character class table \"LC_CTYPE%d\" exhausted",
/*	end for NLS	*/
/*   21    */ "illegal 3byte EUC code specified",
/*   22    */ "INTERNAL ERROR in GetMinMaxwctyp.",
/*   23    */ "INTERNAL ERROR in GetOriAddr-illegal address(%x).",
/*   24    */ "INTERNAL ERROR in GetNewAddr-illegal address(%x)."
};

static char cswidth_info[] = "\n\nCSWIDTH FORMAT: n1[[:s1][,n2[:s2][,n3[:s3]]]]\n\tn1 byte width (SUP1)\n\tn2 byte width (SUP2)\n\tn3 byte width (SUP3)\n\ts1 screen width (SUP1)\n\ts2 screen width (SUP2)\n\ts3 screen width (SUP3)";

static char numeric_info[] = "\n\nNUMERIC FORMAT: d1[d2]\n\td1 decimal delimiter\n\td2 thousand delimiter";


#define _ADDR(__x)  (__x & 0x7f)|((__x & 0x7f00) >> 1)|((__x & 0x7f0000) >> 2)
#define ON	1
#define OFF	0

main(argc, argv)
int	argc;
char	**argv;
{
	p = linebuf;
	if (cmdname = strrchr(*argv, '/'))
		++cmdname;
	else
		cmdname = *argv;
	if ( (argc != 1)  && (argc != 2) )
		error(cmdname, msg[0]);
	if ( argc == 1 || strcmp(argv[1], "-") == 0 )
		{
		readstd++;
		(void)strcpy(input, "standard input");
		}
	else
		(void)strcpy(input, argv[1]);
	if (signal(SIGINT, SIG_IGN) == SIG_DFL)
		(void)signal(SIGINT, clean);
	if (!readstd && freopen(argv[1], "r", stdin) == NULL)
		perror(argv[1]), exit(1);
	init();
	init_add_conv();
	process();
	if (!lc_ctype && chrclass)
		error(input, msg[17]);
	if (lc_ctype) {
		if (!chrclass) 
			error(input, msg[1]);
		else {
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
			if (codeset1 || codeset2 || codeset3)
				create0w();
			create1();
			if (codeset1 || codeset2 || codeset3)
				create1w();
			else
				createw_empty();
			create2();
			if (codeset1 || codeset2 || codeset3)
				create2w();
/*	end for NLS	*/
		}
	}
	if (lc_numeric && !numeric)
		error(input, msg[16]);
	if (numeric && !lc_numeric)
		error(input, msg[15]);
	if (strcmp(tablename1, tablename2) == NULL)	
		error(input, msg[18]);
	if (lc_numeric && numeric)
		create3();
	exit(0);
}


/* Initialize the ctype array */

static	void
init()
{
	register i;
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
	register j;
/*	end for NLS	*/
	for(i=0; i<256; i++)
		(ctype + 1)[257 + i] = i;
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
	ctype[START_CSWIDTH] = 1;
	ctype[START_CSWIDTH + 1] = 0;
	ctype[START_CSWIDTH + 2] = 0;
	ctype[START_CSWIDTH + 3] = 1;
	ctype[START_CSWIDTH + 4] = 0;
	ctype[START_CSWIDTH + 5] = 0;
	ctype[START_CSWIDTH + 6] = 1;
	for(i=0; i<3; i++){
		wcptr[i] = 0;
		cnt_index[i] = 0;
		cnt_type[i] = 0;
		cnt_code[i] = 0;
	}
}

/*	end for NLS	*/

/* Read line from the input file and check for correct
 * character class name */

static	void
process()
{

	unsigned char	*token();
	register  struct  classname  *cnp;
	register unsigned char *c;
/*	start for NLS by NUIX Pacific on Nov. 17		*/	/*NLS*/
	int	i;
/*	end for NLS	*/
	for (;;) {
		if (fgets((char *)linebuf, sizeof linebuf, stdin) == NULL ) {
			if (ferror(stdin)) {
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
				perror("wchrtbl (stdin)");
/*	end for NLS	*/
				exit(1);	
				}
				break;
	        }
		p = linebuf;
		/*  comment line   */
		if ( *p == '#' ) 
			continue; 
		/*  empty line  */
		if ( empty_line() )  
			continue; 
		if ( ! cont ) 
			{
			c = token();
			for (cnp = cln; cnp->name != NULL; cnp++) 
				if(strcmp(cnp->name, (char *)c) == NULL) 
					break; 
			}	
		switch(cnp->num) {
		default:
		case NULL:
			error(input, msg[2], c);
		case ISUPPER:
		case ISLOWER:
		case ISDIGIT:
		case ISXDIGIT:
		case ISSPACE:
		case ISPUNCT:
		case ISCNTRL:
		case ISBLANK:
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
		case ISWCHAR1:
		case ISWCHAR2:
		case ISWCHAR3:
		case ISWCHAR4:
		case ISWCHAR5:
		case ISWCHAR6:
		case ISWCHAR7:
		case ISWCHAR8:
		case ISWCHAR9:
		case ISWCHAR10:
		case ISWCHAR11:
		case ISWCHAR12:
		case ISWCHAR13:
		case ISWCHAR14:
		case ISWCHAR15:
		case ISWCHAR16:
		case ISWCHAR17:
		case ISWCHAR18:
		case ISWCHAR19:
		case ISWCHAR20:
		case ISWCHAR21:
		case ISWCHAR22:
		case ISWCHAR23:
		case ISWCHAR24:
/*	end for NLS	*/
		case UL:
				lc_ctype++;
				(void)strcpy(chrclass_name, cnp->name);
				chrclass_num = cnp->num;
				parse(cnp->num);
				break;
		case LC_CTYPE:
				chrclass++;
				if ( (c = token()) == NULL )
					error(input, msg[1]);
				(void)strcpy(tablename1, "\0");
				(void)strcpy(tablename1, (char *)c);
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
				if (freopen("wctype.c", "w", stdout) == NULL)
					perror("wctype.c"), exit(1);
/*	end for NLS	*/
				break;
		case LC_NUMERIC:
				lc_numeric++;
				if ( (c = token()) == NULL )
					error(input, msg[15]);
				(void)strcpy(tablename2, "\0");
				(void)strcpy(tablename2, (char *)c);
				break;
		case CSWIDTH:
				width++;
				(void)strcpy(chrclass_name, cnp->name);
				if (! cswidth() )
					error(input, msg[12], chrclass_name, cswidth_info);
				break;
		case DECIMAL_POINT:
		case THOUSANDS_SEP:
				numeric++;
				(void)strcpy(chrclass_name, cnp->name);
				if (! setnumeric(cnp->num) )
					error(input, msg[14], chrclass_name, numeric_info);
				break;
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
		case LC_CTYPE1:
				if (codeset1)
					error(input,msg[19],codeset);
				codeset=1;
				codeset1=1;
				(void)setmem(1);
				break;
		case LC_CTYPE2:
				if (codeset2)
					error(input,msg[19],codeset);
				codeset=2;
				codeset2=1;
				(void)setmem(2);
				break;
		case LC_CTYPE3:
				if (codeset3)
					error(input,msg[19],codeset);
				codeset=3;
				codeset3=1;
				(void)setmem(3);
				break;
/*	end for NLS	*/
		}
	} /* for loop */
}

static	int
empty_line()
{
	register unsigned char *cp;
	cp = p;
	for (;;) {
		if ( (*cp == ' ') || (*cp == '\t') ) {
				cp++;
				continue; }
		if ( (*cp == '\n') || (*cp == '\0') )
				return(1); 
		else
				return(0);
	}
}

/* 
 * token() performs the parsing of the input line. It is sensitive
 * to space characters and returns a character pointer to the
 * function it was called from. The character string itself
 * is terminated by the NULL character.
 */ 

unsigned char *
token()
{
	register  unsigned char  *cp;
	for (;;) {
	check_chrclass(p);
	switch(*p) {
		case '\0':
		case '\n':
			in_range = 0;
			cont = 0;
			return(NULL);
		case ' ':
		case '\t':
			p++;
			continue;
		case '>':
			if (action == UL)
				error(input, msg[10], chrclass_name);
			ul_conv = 0;
			p++;
			continue;
		case '-':
			if (action == RANGE)
				error(input, msg[11], chrclass_name);
			action = RANGE;
			p++;
			continue;
		case '<':
			if (ul_conv)
				error(input, msg[4], chrclass_name);
			ul_conv++;
			p++;
			continue;
		default:
			cp = p;
			while(*p!=' ' && *p!='\t' && *p!='\n' && *p!='>' && *p!='-')  
				p++;   
			check_chrclass(p);
			if (*p == '>')
				ul_conv = 0;
			if (*p == '-')
				range++;
			*p++ = '\0';
			return(cp);

		}
	}
}


/* conv_num() is the function which converts a hexadecimal or octal
 * string to its equivalent integer represantation. Error checking
 * is performed in the following cases:
 *	1. The input is not in the form 0x<hex-number> or 0<octal-mumber>
 *	2. The input is not a hex or octal number.
 *	3. The number is out of range.
 * In all error cases a descriptive message is printed with the character
 * class and the hex or octal string.
 * The library routine sscanf() is used to perform the conversion.
 */


Conv_num(s)
unsigned char	*s;
{
	unsigned char	*cp;
	int	i, j;
	int	num;
	cp = s;
	if ( *cp != '0' ) 
		error(input, msg[5], chrclass_name, s);
	if ( *++cp == 'x' )
		num = HEX;
	else
		num = OCTAL;
	switch (num) {
	case	HEX:
			cp++;
			for (j=0; cp[j] != '\0'; j++) 
				if ((cp[j] < '0' || cp[j] > '9') && (cp[j] < 'a' || cp[j] > 'f'))
					break;
				
				break;
	case   OCTAL:
			for (j=0; cp[j] != '\0'; j++)
				if (cp[j] < '0' || cp[j] > '7')
					break;
			break;
	default:
			error(input, msg[5], chrclass_name, s);
	}
	if ( num == HEX )  { 
		if (cp[j] != '\0' || sscanf((char *)s, "0x%x", &i) != 1)  
			error(input, msg[5], chrclass_name, s);
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
/*		if ( i > 0xff ) 	*/
		if ((codeset == 0 && i > 0xff) ||
	    (codeset != 0 && i > 0xffffff) || 
	    /* make sure high bit is on for both 1,2 and 3 bytee case */
	    (codeset != 0 && i < 0xff && !((i & 0x8080) == 0x0080)) ||
	    (codeset != 0 && i > 0xff && i < 0xffff && !((i & 0x8080) == 0x8080)) ||
	    (codeset != 0 && i > 0xffff && i < 0xffffff && !((i & 0x808080) == 0x808080))) 
/*	end for NLS	*/
			error(input, msg[6], chrclass_name, s);
		else
			return(Convert(i));
	}
	if (cp[j] != '\0' || sscanf((char *)s, "0%o", &i) != 1) 
		error(input, msg[5], chrclass_name, s);
/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
/*	if ( i > 0377 ) 		*/
	if ((codeset == 0 && i > 0xff) ||
	    /* make sure high bit is on for both 1,2, and 3 byte case */
	    (codeset != 0 && i < 0xff && !((i & 0x8080) == 0x0080)) ||
	    (codeset != 0 && i > 0xff && !((i & 0x8080) == 0x8080)) ||
	    (codeset != 0 && i > 0xffff && i < 0xffffff && !((i & 0x808080) == 0x808080)))
/*	end for NLS	*/
		error(input, msg[6], chrclass_name, s);
	else
		return(Convert(i));
/*NOTREACHED*/
}

/* parse() gets the next token and based on the character class
 * assigns a value to corresponding table entry.
 * It also handles ranges of consecutive numbers and initializes
 * the uper-to-lower and lower-to-upper conversion tables.
 */

static	void
parse(type)
int type;
{
	unsigned char	*c;
	int	ch1 = 0;
	int	ch2;
	int 	lower= 0;
	int	upper;
#ifdef DEBUG
fprintf (stderr, "PARSE codeset = %x, type = %x\n", codeset, type);
#endif
	while ( (c = token()) != NULL) {
		if ( *c == '\\' ) {
			if ( ! empty_line()  || strlen((char *)c) != 1) 
				error(input, msg[7], chrclass_name);
			cont = 1;
			break;
			}
		switch(action) {
		case	RANGE:
			upper = Conv_num(c);
			if ( (!in_range) || (in_range && range) ) 
				error(input, msg[8], chrclass_name, c);
			if (codeset == 0)				/*NLS*/
				((ctype + 1)[upper]) |= type;
			else						/*NLS*/
				(wctyp[codeset-1][_ADDR(upper)]) |= type;/*NLS*/
			if ( upper <= lower ) 
				error(input, msg[8], chrclass_name, c);
	
			while ( ++lower <= upper ) 
				if (codeset == 0)			/*NLS*/
					((ctype + 1)[lower]) |= type;
				else{					/*NLS*/
				 	wctyp[codeset-1][_ADDR(lower)] |= type;/*NLS*/
#ifdef DEBUG
	fprintf (stderr, "	wctyp[%x][%x] = %x\n", codeset-1, _ADDR(lower),
				 	wctyp[codeset-1][_ADDR(lower)]);
#endif
				}
			action = 0;
			range = 0;
			in_range = 0;
			break;
		case	UL_CONV:
			ch2 = Conv_num(c);
			if (codeset == 0){				/*NLS*/
				(ctype + 1)[ch1 + 257] = ch2;
				(ctype + 1)[ch2 + 257] = ch1;
			}						/*NLS*/
			else{						/*NLS*/
				wconv[codeset - 1][_ADDR(ch1)] = ch2;	/*NLS*/
				wconv[codeset - 1][_ADDR(ch2)] = ch1;	/*NLS*/
			}						/*NLS*/
			action = 0;
			break;   
		default:
			lower = ch1 = Conv_num(c);
			in_range ++;
			if (type == UL) 
				if (ul_conv)
					{
					action = UL_CONV;
					break;
					}
				else
					error(input, msg[3], chrclass_name);
			else
				if (range)
					{
					action = RANGE;
					range = 0;
					}
				else
					;
			
			if (codeset == 0)				/*NLS*/
				((ctype + 1)[lower]) |= type;
			else {						/*NLS*/
				wctyp[codeset - 1][_ADDR(lower)] |= type;/*NLS*/
#ifdef DEBUG
	fprintf (stderr, "	wctyp[%x][%x] = %x\n", codeset-1, _ADDR(lower),
				 	wctyp[codeset-1][_ADDR(lower)]);
#endif
			}
			break;
		}
	}
	if (action)
		error(input, msg[10], chrclass_name);
}

/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
/* create0w() produce wctype structure in memory */

static	void
create0w()
{
	register i,j,k,l;
	int	s,mask;
	int min, max;
	unsigned char *index_addr;
	unsigned *type_addr;
	unsigned int  *code_addr;
	unsigned sv_type;
	int	cnt_index_save[3]; 


#ifdef DEBUG
	dumpconv();
#endif
	if (codeset1)
		wcptr[0] = &wctbl[0];
	if (codeset2)
		wcptr[1] = &wctbl[1];
	if (codeset3)
		wcptr[2] = &wctbl[2];
	index_addr = (unsigned char *)(sizeof (struct _wctype) * 3);
	for (s = 0; s < 3; s++){
		if (wcptr[s] == 0) continue;
		/*
		 * Search for minimum.
		 */
		GetMinMaxwctyp (s, &min, &max);
		i = min;
		if (i != -1){
			wctbl[s].tmin = i;
			sv_type = wctyp[s][GetNewAddr(i, ON)];
			cnt_type[s] = 1;
			wctbl[s].tmax = max;
#ifdef DEBUG
fprintf (stderr, "\nCREAT0w, CODE = %d, min(tmin) = 0x%x(%x) max(tmax) = 0x%x(%x)\n",
		s, min,wctbl[s].tmin, max, wctbl[s].tmax);
#endif
			cnt_index[s] = wctbl[s].tmax - wctbl[s].tmin + 1;

			/* save real number of cnt_index for */
			/* clearing patch below */
			cnt_index_save[s] = cnt_index[s];

			if ((cnt_index[s] % 8) != 0)
				cnt_index[s] = ((cnt_index[s] / 8) + 1) * 8;
			if ((cindex[s] = (unsigned char *)(malloc((unsigned)cnt_index[s]))) == NULL ||
		    	    (type[s] = (unsigned *)(malloc((unsigned)cnt_index[s] * 4))) == NULL) {	
#ifdef DEBUG
				perror ("MALLOC #1");
#endif
				error(cmdname,msg[13]);
			}
			type[s][0] = sv_type;

			/* clear values for patched index */
			for (i=1, k = wctbl[s].tmax; i<=(cnt_index[s]-cnt_index_save[s]); i++)
				wctyp[s][GetNewAddr(k+i, ON)] = 0;
			for (i = 0, j = 0, k = wctbl[s].tmin; i < cnt_index[s]; i++,k++){
				for (l = j; l >= 0; l--) {
					int val;
					val = GetNewAddr(k, OFF);
					if (val == -1) {
						l = 1;
						break;
					}
					else
						if (type[s][l] == wctyp[s][val])
							break;
				}
				if (l >= 0) {
					cindex[s][i] = l;
				}
				else{
					int val;
					val = GetNewAddr (k, OFF);
					if (val == -1)
						type[s][++j] = 0;
					else
						type[s][++j] = wctyp[s][val];
					cindex[s][i] = j;
					cnt_type[s]++;
					if (j > 256) {
#ifdef DEBUG
						fprintf (stderr, "CNT_CTYPE OVERFLOW\n");
#endif
						error(input, msg[20],++s);
					}
				}
			}
			if ((cnt_type[s] % 8) != 0)
				cnt_type[s] = ((cnt_type[s] / 8) + 1) * 8;
		}
		GetMinMaxWconv (s, &min, &max);
		i = min;
		if (i != -1){
			wctbl[s].cmin = i;
			wctbl[s].cmax = max;
			cnt_code[s] = wctbl[s].cmax - wctbl[s].cmin + 1;
			if ((cnt_code[s] % 8) != 0)
				cnt_code[s] = ((cnt_code[s] / 8) + 1) * 8;
		    	if ((code[s] = (unsigned int *)(malloc((unsigned)cnt_code[s] * 4))) == NULL) {
#ifdef DEBUG
				perror ("MALLOC #2");
#endif
				error(cmdname,msg[13]);
			}
			if (s == 0)
				mask = 0x8080;
			else if (s == 1)
				mask = 0x0080;
			else
				mask = 0x8000;
			for (i = 0, j = wctbl[s].cmin; i < cnt_code[s]; i++)
				code[s][i] = ((((j + i) & 0x3f8080) << 2) |
					      (((j + i) & 0x3f80) << 1) |
					       ((j + i) & 0x7f)) | mask;
			for (i = 0,j = wctbl[s].cmin; i < cnt_code[s]; i++,j++){
				int val;
				val = GetNewAddr (j, OFF);
				if (val != -1)
					if (wconv[s][GetNewAddr(j, OFF)] != 0xffffff)
						code[s][i] = wconv[s][GetNewAddr(j, OFF)];
			}
		}
		if (cnt_index[s] != 0)
			wctbl[s].index = index_addr;
		type_addr = (unsigned *)(index_addr + cnt_index[s]);
		if (cnt_type[s] != 0)
			wctbl[s].type = type_addr;
		code_addr = (unsigned int *)(type_addr + cnt_type[s]);
		if (cnt_code[s] != 0)
			wctbl[s].code = (wchar_t *)code_addr;
		index_addr = (unsigned char *)(code_addr + cnt_code[s]);
	}
}

/*	end for NLS	*/


/* create1() produces a C source file based on the definitions
 * read from the input file. For example for the current ASCII
 * character classification where LC_CTYPE=ascii it produces a C source
 * file named wctype.c.
 */


static	void
create1()
{
	struct  field {
		unsigned char  ch[20];
	} out[8];
	unsigned char	*hextostr();
	unsigned char	outbuf[256];
	int	cond = 0;
	int	flag=0;
	int i, j, index1, index2;
	int	line_cnt = 0;
	register struct classname *cnp;
	int 	num;
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
	int	k;
/*	end for NLS	*/


	comment1();
	(void)sprintf((char *)outbuf,"unsigned char\t_ctype_[] =  { 0,");
	(void)printf("%s\n",outbuf);
	
	index1 = 0;
	index2 = 7;
	while (flag <= 1) {
		for (i=0; i<=7; i++)
			(void)strcpy((char *)out[i].ch, "\0");
		for(i=index1; i<=index2; i++) {
			if ( ! ((ctype + 1)[i]) )  {
				(void)strcpy((char *)out[i - index1].ch, "0");
				continue; }
			num = (ctype + 1)[i];
			if (flag) {      
				(void)strcpy((char *)out[i - index1].ch, "0x");  
				(void)strcat((char *)out[i - index1].ch, (char *)hextostr(num));
				continue; }
			while (num)  {
				for(cnp=cln;cnp->num != UL;cnp++) {
					if(!(num & cnp->num))  
						continue; 
					if ( (strlen((char *)out[i - index1].ch))  == NULL)  
						(void)strcat((char *)out[i - index1].ch,cnp->repres);
					else  {
						(void)strcat((char *)out[i - index1].ch,"|");
						(void)strcat((char *)out[i - index1].ch,cnp->repres); }  
				num = num & ~cnp->num;  
					if (!num) 
						break; 
				}  /* end inner for */
			}  /* end while */
		} /* end outer for loop */
		(void)sprintf((char *)outbuf,"\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,",
		out[0].ch,out[1].ch,out[2].ch,out[3].ch,out[4].ch,out[5].ch,
		out[6].ch,out[7].ch);
		if ( ++line_cnt == 32 ) {
			line_cnt = 0;
			flag++; 
			cond = flag; }
		switch(cond) {
		case	1:
			(void)printf("%s\n", outbuf);
			comment2();
			(void)printf("\t0,\n");
			index1++;
			index2++;
			cond = 0;
			break;
		case	2:
			(void)printf("%s\n", outbuf);
			(void)printf("\n\t/* multiple byte character width information */\n\n");
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
				k = 6;
/*			for(j=0; j<6; j++) 			*/
			for(j=0; j<k; j++){
				if ((j%8 == 0) && (j !=0))
					(void)printf("\n");
/*	end for NLS	*/
				(void)printf("\t%d,", ctype[START_CSWIDTH + j]);
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
			}
			(void)printf("\t%d\n", ctype[START_CSWIDTH + k]);
/*	end for NLS	*/
			(void)printf("};\n");
			
			break;
		default:
			printf("%s\n", outbuf);
			break;
		}
		index1 += 8;
		index2 += 8;
	}  /* end while loop */
	if (width)
		comment3();
	/* print the numeric array here. */
	(void)printf("\n\nunsigned char\t_numeric[SZ_NUMERIC] = \n");
	(void)printf("{\n");
	(void)printf("\t%d,\t%d,\n", ctype[START_NUMERIC], ctype[START_NUMERIC +1]);
	(void)printf("};\n");
}


/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/

/* create1w() produces a C source program for supplementary code sets */


static	void
create1w()
{
	struct  field {
		unsigned char  ch[100];
	} out[8];
	unsigned char	*hextostr();
	unsigned char	outbuf[256];
	unsigned char	*cp;
	int	cond = 0;
	int	flag=0;
	int i, j, index1, index2;
	int	line_cnt = 0;
	register struct classname *cnp;
	int 	num;
	int	s;


	comment4();
	(void)sprintf((char *)outbuf,"struct _wctype _wcptr[3] = {\n");
	(void)printf("%s",outbuf);
	for (s = 0; s < 3; s++){
		(void)printf("\t{");
		if (wctbl[s].tmin == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].tmin));
		if (wctbl[s].tmin == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].tmax));
		if (wctbl[s].index == 0)
			(void)printf("0,\t");
		else
			(void)printf("(unsigned char *)0x%s,\t",hextostr(wctbl[s].index));
		if (wctbl[s].type == 0)
			(void)printf("0,\t");
		else
			(void)printf("(unsigned *)0x%s,\t",hextostr(wctbl[s].type));
		if (wctbl[s].cmin == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].cmin));
		if (wctbl[s].cmax == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].cmax));
		if (wctbl[s].code == 0)
			(void)printf("0");
		else
			(void)printf("(unsigned int *)0x%s",hextostr(wctbl[s].code));
		if (s < 2)
			(void)printf("},\n");
		else
			(void)printf("}\n");
	}
	(void)printf("};\n");
	

	comment5();
   for (s = 0; s < 3; s++){
	index1 = 0;
	index2 = 7;
	flag = 0;
	while (flag <= 2) {
		if (line_cnt == 0){
			if (flag == 0 && cnt_index[s])
				(void)printf("unsigned char index%d[] = {\n",s+1);
			else if ((flag == 0 || flag == 1) && cnt_type[s]){
				(void)printf("unsigned type%d[] = {\n",s+1);
				flag = 1;
			}
			else if (cnt_code[s]){
				(void)printf("unsigned int code%d[] = {\n",s+1);
				flag = 2;
			}
			else
				break;
		}
		for (i=0; i<=7; i++)
			(void)strcpy((char *)out[i].ch, "\0");
		for(i=index1; i<=index2; i++) {
			if ((flag == 0 && !cindex[s][i]) ||
			    (flag == 1 && !type[s][i]) ||
			    (flag == 2 && !code[s][i])){ 
				(void)strcpy((char *)out[i - index1].ch, "0");
				continue; }
			if (flag == 0)
				num = cindex[s][i];
			else if (flag == 1)
				num = type[s][i];
			else
				num = code[s][i];
			if (flag == 0 || flag ==2) {      
				(void)strcpy((char *)out[i - index1].ch, "0x");  
				(void)strcat((char *)out[i - index1].ch, (char *)hextostr(num));
				continue; }
			while (num)  {
				for(cnp=cln;cnp->num != UL;cnp++) {
					if(!(num & cnp->num))  
						continue; 
					if ( (strlen((char *)out[i - index1].ch))  == NULL)  
						(void)strcat((char *)out[i - index1].ch,cnp->repres);
					else  {
						(void)strcat((char *)out[i - index1].ch,"|");
						(void)strcat((char *)out[i - index1].ch,cnp->repres); }  
				num = num & ~cnp->num;  
					if (!num) 
						break; 
				}  /* end inner for */
			}  /* end while */
		} /* end outer for loop */
		(void)sprintf((char *)outbuf,"\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,",
		out[0].ch,out[1].ch,out[2].ch,out[3].ch,out[4].ch,out[5].ch,
		out[6].ch,out[7].ch);
		line_cnt++;
		if ((flag == 0 && line_cnt == cnt_index[s]/8) ||
		    (flag == 1 && line_cnt == cnt_type[s]/8) ||
		    (flag == 2 && line_cnt == cnt_code[s]/8)){
			line_cnt = 0;
			flag++; 
			cond = flag; }
		switch(cond) {
		case	1:
		case	2:
		case	3:
			cp = outbuf + strlen((char *)outbuf);
			*--cp = ' ';
			*++cp = '\0';
			(void)printf("%s\n", outbuf);
			(void)printf("};\n");
			index1 = 0;
			index2 = 7;
			cond = 0;
			break;
		default:
			(void)printf("%s\n", outbuf);
			index1 += 8;
			index2 += 8;
			break;
		}
	}  /* end while loop */
   }
}
/*	end for NLS	*/

/* create2() & create2w() produces a data file containing the ctype array
 * elements. The name of the file is the same as the value
 * of the environment variable LC_CTYPE.
 */


static	void
create2()
{
	register   i=0;
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
	int	j;
/*	end for NLS	*/
	if (freopen(tablename1, "w", stdout) == NULL){
		perror(tablename1), exit(1);
		exit(1);
	}
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
	if (codeset1 || codeset2 || codeset3)
		j = SIZE;
	else
		j = SIZE - CSLEN + 7;
/*	for (i=0; i< SIZE - 2; i++)				*/
	for (i=0; i< j; i++)
/*	end for NLS	*/
		(void)printf("%c", ctype[i]);
}

/*	start for NLS by UNIX Pacific on Sep. 02 '87		*/	/*NLS*/
static	void
create2w()
{
	int	s;
	if (fwrite(wctbl,(sizeof (struct _wctype)) * 3,1,stdout) == NULL)
		perror(tablename1), exit(1);
	for (s = 0; s < 3; s++){
		if (cnt_index[s] != 0)
			if (fwrite(&cindex[s][0],cnt_index[s], 1, stdout) == NULL)
				perror(tablename1), exit(1);
		if (cnt_type[s] != 0)
			if (fwrite(&type[s][0],4,cnt_type[s], stdout) == NULL)
				perror(tablename1), exit(1);
		if (cnt_code[s] != 0)
			if (fwrite(&code[s][0],(sizeof((unsigned int) 0)),cnt_code[s], stdout) == NULL)
				perror(tablename1), exit(1);
	}
}
/*	end for NLS	*/
	
static void
create3()
{
	int	length;
	char	*numeric_name;

	if (freopen(tablename2, "w", stdout) == NULL) {
		perror(tablename2);
		exit(1);
	}
	(void)printf("%c%c", ctype[START_NUMERIC], ctype[START_NUMERIC + 1]);
}

/* Convert a hexadecimal number to a string */

unsigned char *
hextostr(num)
int	num;
{
	unsigned char	*idx;
	static unsigned char	buf[64];
	idx = buf + sizeof(buf);
	*--idx = '\0';
	do {
		*--idx = "0123456789abcdef"[num % 16];
		num /= 16;
	  } while (num);
	return(idx);
}

static void
comment1()
{
/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
		(void)printf("#include <ctype.h>\n");
		(void)printf("#include <widec.h>\n");
		(void)printf("#include <wctype.h>\n\n\n");
/*	end for NLS	*/
	(void)printf("\t/*\n");
	(void)printf("\t ************************************************\n");
	(void)printf("\t *		%s  CHARACTER  SET                \n", tablename1);
	(void)printf("\t ************************************************\n");
	(void)printf("\t */\n\n");
	(void)printf("\t/* The first 257 characters are used to determine\n");
	(void)printf("\t * the character class */\n\n");
}

static	void
comment2()
{
	(void)printf("\n\n\t/* The next 257 characters are used for \n");
	(void)printf("\t * upper-to-lower and lower-to-upper conversion */\n\n");
}

static void
comment3()
{
	(void)printf("\n\n\t/*  CSWIDTH INFORMATION                           */\n");
	(void)printf("\t/*_____________________________________________   */\n");
	(void)printf("\t/*                    byte width <> screen width  */\n");
	(void)printf("\t/* SUP1	  		     %d    |     %d         */\n",
		ctype[START_CSWIDTH], ctype[START_CSWIDTH + 3]);
	(void)printf("\t/* SUP2			     %d    |     %d         */\n",
		ctype[START_CSWIDTH + 1], ctype[START_CSWIDTH + 4]);
	(void)printf("\t/* SUP3			     %d    |     %d         */\n",
		ctype[START_CSWIDTH + 2], ctype[START_CSWIDTH + 5]);
	(void)printf("\n\t/* MAXIMUM CHARACTER WIDTH        %d               */\n",
		ctype[START_CSWIDTH + 6]);
}

/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
static	void
comment4()
{
	(void)printf("\n\n\t/* The next entries point to wctype tables */\n");
	(void)printf("\t/*          and have upper and lower limit */\n\n");
}

static	void
comment5()
{
	(void)printf("\n\n\t/* The folowing table is used to determine\n");
	(void)printf("\t * the character class for supplementary code sets */\n\n");
}
/*	end for NLS	*/

/*VARARGS*/
static	void
error(va_alist)
va_dcl
{
	va_list	args;
	char	*fmt;
	char	*file;
	va_start(args);
	file = va_arg(args, char *);
	(void)fprintf(stderr, "ERROR in %s: ", file);
	fmt = va_arg(args, char *);
	(void)vfprintf(stderr, fmt, args);
	(void)fprintf(stderr, "\n");
	va_end(args);
	clean();
}

static	void
check_chrclass(cp)
unsigned char	*cp;
{
	if (chrclass_num != UL)
		if (*cp == '<' || *cp == '>')
			error(input, msg[9], chrclass_name, *cp);
		else
			;
	else
		if (*cp == '-')
			error(input, msg[9], chrclass_name, *cp);
		else
			;
}

static	void
clean()
{
	(void)signal(SIGINT, SIG_IGN);
	(void)unlink("wctype.c");
	(void)unlink(tablename1);
	(void)unlink(tablename2);
	exit(1);
}

/*
 *
 * 	n1[:[s1][,n2:[s2][,n3:[s3]]]]
 *      
 *	n1	byte width (supplementary code set 1)
 *	n2	byte width (supplementary code set 2)
 *	n3	byte width (supplementary code set 3)
 *	s1	screen width (supplementary code set 1)
 *	s2	screen width (supplementary code set 2)
 *	s3	screen width (supplementary code set 3)
 *
 */
static int
cswidth()
{
        char *byte_width[3];
	char *screen_width[3];
	char *buf;
	int length;
	unsigned int len;
	int suppl_set = 0;
	unsigned char *cp;

	if (*(p+strlen((char *)p)-1) != '\n') /* terminating newline? */
		return(0);
	p = clean_line(p);
	if (!(length = strlen((char *)p))) /* anything left */
		return(0);
	if (! isdigit((char)*p) || ! isdigit((char)*(p+length-1)) )
		return(0);
        if ((buf = malloc((unsigned)length + 1)) == NULL)
		{perror ("malloc, 2");error(cmdname, msg[13]);}
	(void)strcpy(buf, (char *)p);
	cp = p;
	while (suppl_set < 3) {
		if ( !(byte_width[suppl_set] = strtok((char *)cp, tokens)) || ! check_digit(byte_width[suppl_set]) ){
			return(0);
		}
		ctype[START_CSWIDTH  + suppl_set] = atoi(byte_width[suppl_set]);
		if ( p + length == (unsigned char *)(byte_width[suppl_set] + 1) )
			break;
		len = (unsigned char *)(byte_width[suppl_set] + 1) - p;
		if (*(buf + len) == ',') {
			cp = (unsigned char *)(byte_width[suppl_set] + 2);
			suppl_set++;
			continue;
		}
		tokens[0] = ',';
		if ( !(screen_width[suppl_set] = strtok((char *)0, tokens)) || ! check_digit(screen_width[suppl_set]) )  {
			return(0);
		}
		ctype[START_CSWIDTH + suppl_set + 3] = atoi(screen_width[suppl_set]);
		if ( p + length == (unsigned char *)(screen_width[suppl_set] + 1) )
			break;
		cp = (unsigned char *)(screen_width[suppl_set] + 2);
		tokens[0] = ':';
		suppl_set++;
	}
	suppl_set = 0;
	while (suppl_set < 3) {
		if ( (ctype[START_CSWIDTH + suppl_set]) && !(ctype[START_CSWIDTH + suppl_set + 3]) )
			ctype[START_CSWIDTH + suppl_set + 3] = ctype[START_CSWIDTH + suppl_set];
		suppl_set++;
	}
	ctype[START_CSWIDTH + 6] = ctype[START_CSWIDTH + 1] > ctype[START_CSWIDTH + 2] ? ctype[START_CSWIDTH + 1] : ctype[START_CSWIDTH + 2];
	if (ctype[START_CSWIDTH + 6] > 1)
		++ctype[START_CSWIDTH + 6];
/*	start for NLS by UNIX Pacific on Nov. 26 '87		*/	/*NLS*/
	if (ctype[START_CSWIDTH] > ctype[START_CSWIDTH + 6])
		ctype[START_CSWIDTH + 6] = ctype[START_CSWIDTH];
/*	end for NLS	*/
return(1);
}

static unsigned char  *
clean_line(s)
unsigned char *s;
{
	unsigned char  *ns;

	*(s + strlen((char *)s) -1) = (char) 0; /* delete newline */
	if (!strlen((char *)s))
		return(s);
	ns = s + strlen((char *)s) - 1; /* s->start; ns->end */
	while ((ns != s) && (isspace((char)*ns))) {
		*ns = (char)0;	/* delete terminating spaces */
		--ns;
		}
	while (*ns)             /* delete beginning white spaces */
		if (isspace((char)*s))
			++s;
		else
			break;
	return(s);
}

static int
check_digit(s)
char *s;
{
	if (strlen(s) != 1 || ! isdigit(*s))
		return(0);
	else
		return(1);
}

static int
setnumeric(num_category)
int	num_category;
{
	int	len;
	unsigned char	q, r;

	p = clean_line(p);
	if ((len = strlen((char *)p)) == 0)
		return (1);
	if ((p[0]=='\''||p[0]=='"')&&(q=p[0])==p[len-1]){
		/* Quoted character. */
		++p; --len;
		if (p[0]=='\\'){ /* Escape sequence */
			int	i;

			++p;
			if ((sscanf((char *)p, "%*[xX]%2x", &i)==1)||
			    (sscanf((char *)p, "%3o", &i)==1)){
				r=(unsigned char)i;
			}else{
				return (0);
			}
		}else{ /* Regular character - must be one byte long */
			if (len!=2){
				return (0);
			}else{
				r=p[0];
			}
		}
	}else{ /* Free-standing char */
		if (len > 1)
			return (0);
		else
			r=p[0];
	}
	ctype[START_NUMERIC + num_category - DECIMAL_POINT] = (char)r;
	return (1);
}

/*	start for NLS by UNIX Pacific on Nov. 17 '87		*/	/*NLS*/
static	void
setmem(codeset)
	int	codeset;
{
	register i;
	if ((wctyp[codeset-1] = (unsigned *)(malloc(WSIZE * NUM_BANKS))) == NULL ||
	    (wconv[codeset-1] = (unsigned int *)(malloc(WSIZE * NUM_BANKS))) == NULL) 
		error(cmdname,msg[13]);
	for (i=0; i<(WSIZE/4)*NUM_BANKS; i++){
		wctyp[codeset-1][i] = 0;
		wconv[codeset-1][i] = 0xffffff;
	}
	num_banks[codeset-1] = NUM_BANKS;
}
/*	end for NLS	*/


static void
createw_empty()
{
	
	(void)printf("\n\nstruct _wctype _wcptr[3] = {\n");
	(void)printf("\t{0,	0,	0,	0,	0,	0,	0},\n");
	(void)printf("\t{0,	0,	0,	0,	0,	0,	0},\n");
	(void)printf("\t{0,	0,	0,	0,	0,	0,	0}\n");
	(void)printf("};\n\n");

}

#define LOW14		0x03fff

struct addr_conv {
	signed char	flg;
	unsigned char 	old;
}; 

struct addr_conv *addr_conv;

init_add_conv()
{
	int i;
	addr_conv = (struct addr_conv *) 
		malloc((sizeof (struct addr_conv))*NUM_BANKS);
	if (addr_conv == NULL) {
		error(cmdname,msg[13]);
	}
	for (i = 0; i < NUM_BANKS; i++) {
		addr_conv[i].flg = 0;
		addr_conv[i].old = 0;
	}
	addr_conv[i-1].flg = -1;
}

/*
 * Convert
 */
Convert (num)
	unsigned int num;
{
	unsigned char top8;
	struct addr_conv *p = addr_conv;
	int i = 0;
	/*
	 * If it is alread 16 bits, return it.
	 */
	if ((num & 0xff0000) == 0)
		return (num);

	top8 = (num & 0xff0000) >> 16;

	while (p->flg != -1) {
		if (p->old == top8)	/* found, assigned already */
			break;
		if (p->flg == 0) {
			p->old = top8;
			p->flg = 1;
			break;
		}
		p++;
		i++;
	}
	if (p->flg == -1) {
		/*
		 * Illegal 3 byte EUC code spacified.
		 */
		 error (input, msg[21]);
	}


	return (((i+1) << 16+2) | (num & 0xffff));
}
#undef DEBUG

/*
 * Get min and max
 */
GetMinMaxwctyp(s, ret_min, ret_max)
	int s;
	int *ret_min;
	int *ret_max;
{
	int min[NUM_BANKS];
	int max[NUM_BANKS];
	int Min = -1;
	int Max = -1;
	int i, j;

	for (i = 0; i < NUM_BANKS; i++)
		min[i] = max[i] = -1;

	/*
	 * Get minimum and maximum.
	 */
	for (j = 0; j < num_banks[s]; j++) {
		for (i = 0; i < 0xffff && wctyp[s][i + j*0xffff] == 0; i++);
		if (i != 0xffff) {
			min[j] = GetRealAddr (s, i + j*0xffff);
			for (i = 0xffff-1; i >= 0 && wctyp[s][i+j*0xffff] == 0; i--);
			max[j] = GetRealAddr (s, i + j*0xffff);
		}
	}

	/*
	 * Decide minimum bank
	 */
	for (i = 0; i < num_banks[s]; i++) {
		if (min[i] != -1) {
			if (Min == -1) 
				Min = min[i];
			else {
				if (Min > min[i])
					Min = min[i];
			}
		}
		if (max[i] != -1) {
			if (Max == -1)
				Max = max[i];
			else {
				if (max[i] > Max)
					Max = max[i];
			}
		}
	}
#ifdef DEBUG
fprintf (stderr, "	MAX = %x, MIN = %x\n", Max, Min);
	for (i = 0; i < NUM_BANKS; i++) {
		fprintf (stderr, "	min[%x] = %x, max[%x] = %x\n", 
				i, min[i], i, max[i]);
	}
#endif
	*ret_min = Min;
	*ret_max = Max;
}


GetMinMaxWconv(s, ret_min, ret_max)
	int s;
	int *ret_min;
	int *ret_max;
{
	int min[NUM_BANKS];
	int max[NUM_BANKS];
	int Min = -1;
	int Max = -1;
	int i, j;

	for (i = 0; i < num_banks[s]; i++)
		min[i] = max[i] = -1;

	/*
	 * Get minimum and maximum.
	 */
	for (j = 0; j < num_banks[s]; j++) {
		for (i = 0; i < 0xffff && wconv[s][i + j*0xffff] == 0xffffff; i++);
		if (i != 0xffff) {
			min[j] = GetRealAddr (s, i + j*0xffff);
			for (i = 0xffff-1; i >= 0 && wconv[s][i+j*0xffff] == 0xffffff; i--);
			max[j] = GetRealAddr (s, i + j*0xffff);
		}
	}

	/*
	 * Decide minimum bank
	 */
	for (i = 0; i < num_banks[s]; i++) {
		if (min[i] != -1) {
			if (Min == -1) 
				Min = min[i];
			else {
				if (Min > min[i])
					Min = min[i];
			}
		}
		if (max[i] != -1) {
			if (Max == -1)
				Max = max[i];
			else {
				if (max[i] > Max)
					Max = max[i];
			}
		}
	}
	*ret_min = Min;
	*ret_max = Max;
}

/*
 * GetOriAddr
 *	Given the address in wctyp, it returns the real address
 */
GetRealAddr (s, addr)
	int addr;
{
	int top8;
	int point;
	int newaddr;

	if (addr < 0xffff)	/* first block */
		return (addr);

	point = (addr & 0x0fffff) >> 16;
	if (point >= num_banks[s])
		error(input, msg[23], addr);
	top8 = addr_conv[point-1].old;
	newaddr = (addr & 0xffff) | ((top8 & 0x7f) << 14);
#ifdef DEBUG
	fprintf (stderr, "GetRealAddr: addr = %x, point = %x, top8 = %x, newaddr = %x\n",
		addr, point, top8, newaddr);
#endif
	return (newaddr);
}

/*
 * GetNewAddr
 *	Given the address in wctyp, it returns the real address
 */
GetNewAddr (addr, flag)
	int addr;
	int flag;
{
	int top8;
	struct addr_conv *p = addr_conv;
	int newaddr;
	int i = 0;

	if ((addr & 0xffff0000) == 0)
		return (addr);

	top8 = ((addr >> 14) & 0xff) | 0x80;
	while (p->flg != -1) {
		if (p->old == top8)
			break;
		p++;
		i++;
	}

	if (p->flg == -1) {
#ifdef DDEBUG
	fprintf (stderr, "GetNewAddr: (%x)memory trap\n", top8);
#endif
		if (flag == OFF)
			return (-1);
		else 
			error (input, msg[24], addr);
	}
	newaddr = ((i+1) << 14+2) | (addr & LOW14);
	return (newaddr);
}

/*
 * debugging
 */
dumpconv()
{
	int i;

	fprintf (stderr, "\n DUMPING addr_conv.\n");
	for (i = 0; i < NUM_BANKS; ++i) {
		fprintf (stderr, "\taddr_conv[%x].flg = %x, old = %x\n", 
			i, addr_conv[i].flg, addr_conv[i].old);
	}
}
