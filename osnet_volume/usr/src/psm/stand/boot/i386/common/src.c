/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)src.c	1.19	99/02/04 SMI"

/* boot shell command source handling routines */

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/bootdef.h>
#include <sys/booti386.h>
#include <sys/fcntl.h>
#include <sys/salib.h>
#include <sys/bootvfs.h>

#define	__ctype _ctype		/* Incredibly stupid hack used by	*/
#include <ctype.h>		/* ".../stand/lib/i386/subr_i386.c"	*/

/* DOS end-of-file character is <ctrl-z> */
#define	DOS_EOF	0x1A

#ifdef	USERTEST
#include <stdio.h>
FILE *stream = stdin;
#define	getcons(s, n) fgets(s, n, stream)
#else
#define	getcons(s, n) cgets((char *)s, n)
#endif

extern void wait100ms();
extern int ischar();
extern void *memcpy(void *s1, void *s2, size_t n);
extern void put_arg();
extern void prom_panic();
extern int subst_var();
extern void putchar();
extern unsigned char config_source[];
extern unsigned char boot_source[];
static void readg_cmd(int timeout, int argc, char **argv);

extern unsigned char *var_ops();
unsigned char getsrcchar();
int cgets(char *, int), src_command();
static int on_or_off(char *);
static void pushsrcchar();

extern int boot_device;
int verbose_flag;
int singlestep_flag;
unsigned char linbuf[LINBUFSIZ];	/* initial console line buffer */
int srcx = 2;
struct src src[SRCSIZ] =	/* command source table */
{
	{ SRC_CONSOLE, LINBUFSIZ, linbuf, linbuf, 0 },
	{ SRC_VARIABLE, 0, boot_source, boot_source, 0},
	{ SRC_VARIABLE, 0, config_source, config_source, 0}
};

/* get_command() - main loop for reading words of a command */

static unsigned char *Wordbuf;
static int Wordbufsize;

static int
grow_wordbuf(unsigned char **wp)
{
	unsigned char *newbuf;

	if (!Wordbuf) {
		if (Wordbuf = (unsigned char *)bkmem_alloc(WORDSIZ))
			Wordbufsize = WORDSIZ;
		return ((int)Wordbuf);
	} else {
		int dist;
		if (newbuf =
		    (unsigned char *)bkmem_alloc(Wordbufsize + WORDSIZ)) {
			(void) memcpy(newbuf, Wordbuf, Wordbufsize);
			dist = *wp - Wordbuf;
			bkmem_free((caddr_t)Wordbuf, Wordbufsize);
			Wordbufsize += WORDSIZ;
			*wp = newbuf + dist;
			return ((int)(Wordbuf = newbuf));
		} else {
			return (0);
		}
	}
}

void
get_command(argp)
	struct arg *argp;
{
	unsigned char	*wordp;
	int		wordsiz;
	unsigned char	c, *cp;
	int		word_too_big;

	if (!Wordbuf && !grow_wordbuf(NULL))
		prom_panic("No space for command interpretation!");

	/* printf("get_command() "); */
	for (;;) {	/* loop through words until end of command */
nextword:
	    wordp = Wordbuf;
	    wordsiz = 0;	/* current size of word string */
	    word_too_big = 0;

	    /* loop through characters in word */
	    for (;;) {

		int base = 8;
		int maxesc = 3;
		static unsigned char hextab[] = "0123456789ABCDEF";

		c = getsrcchar();
dochar:
		switch (c) {

		case '#':   /* comment */
		    while ((c = getsrcchar()) != '\n')
			;
		    goto dochar;

		/* white space */
		case '\r':  /* carriage return */
		case ' ':   /* blank */
		case '\t':  /* tab */
		    if (wordsiz != 0)
			goto endword;
		    else
			continue;		/* skip white space */

		case '\n':  /* newline */
		    if (wordsiz != 0) {		/* if there is an arg word */
			*wordp = '\0';		/* null terminate */
			put_arg(Wordbuf, argp, wordsiz); /* install arg word */
		    }
		    if (argp->argc != 0)    /* command terminated by NL */
			return;
		    else
			continue;	/* no command */

		case '\'':    /* single quote */
		    /* collect text between single quotes */
		    while ((c = getsrcchar()) != '\'') {
			if (c == '\\') {	/* backslash quote */
			    switch (c = getsrcchar()) {
			    case '\n':
				    continue;	/* ignore \ NL	*/
			    case '\\':
			    case '\'':
				    break;
			    }
			}
			if (wordsiz >= Wordbufsize - 1 &&
			    !grow_wordbuf(&wordp)) {
				    ++word_too_big;
			} else {
			    *wordp++ = c;	/* put char in word */
			    wordsiz++;
			}
		    }
		    continue;		/* continue word */

		case '"':    /* double quote */
		    /* collect text between double quotes */
		    while ((c = getsrcchar()) != '\"') {
			if (c == '\\') {	/* backslash quote */
			    switch (c = getsrcchar()) {
			    case '\n':
				continue;	/* ignore \ NL	*/
			    case '\\':
			    case '"':
			    case '$':
				break;
			    }
			} else if (c == '$') {
			    if (subst_var()) /* variable substitution */
				continue;
			}
			if (wordsiz >= Wordbufsize - 1 &&
			    !grow_wordbuf(&wordp)) {
			    ++word_too_big;
			} else {
			    *wordp++ = c;
			    wordsiz++;
			}
		    }
		    continue;	/* continue word */

		case '$':   /* variable substitution */
		    if (! subst_var())
			goto addchar;
		    break;

		case '\\':   /* backslash */
		    switch (c = getsrcchar()) {

		    case 't': c = '\t'; break;
		    case 'b': c = '\b'; break;
		    case 'r': c = '\r'; break;
		    case 'n': c = '\n'; break;

		    case '\n':
			/* treat backslash - new-line as a blank */
			c = ' ';
			goto dochar;

		    case 'x':
			base = 16;
			maxesc = 2;
			c = getsrcchar();
			/*FALLTHROUGH*/

		    default:

			cp = (unsigned char *)strchr((char *)hextab,
				toupper(c));
			if (cp && ((cp - hextab) < base)) {
				/*
				 * Next input char is a valid
				 * octal/hex digit. Build the new
				 * char (in "j" register) from the
				 * escaped ASCII digits that follow.
				 */
				int j = 0;
				int k = (cp - hextab);

				do {
					/*
					 * Compute this digit value,
					 * read next one
					 */
					j = (j << ((base == 8) ? 3 : 4)) + k;
					c = getsrcchar();

				} while (--maxesc &&
				    (cp =
					(unsigned char *)strchr((char *)hextab,
					toupper(c))) &&
				    ((k = (cp - hextab)) < base));
				pushsrcchar(c);
				c = (unsigned char)j;
			}

			/* other characters fall through */
			break;
		    }

		default:    /* add character to word */
addchar:
		    if (wordsiz >= Wordbufsize - 1 &&
			!grow_wordbuf(&wordp)) {
			++word_too_big;
		    } else {
			*wordp++ = c;
			wordsiz++;
		    }
		    continue;	/* continue word */
		}
	    }
endword:
	    *wordp = '\0';	/* null-terminate word */
	    put_arg(Wordbuf, argp, wordsiz);    /* install arg word */
	    if (word_too_big != 0)
		printf("boot: word too big (%s...)\n", Wordbuf);
	}
}

unsigned char
getsrcchar()
{
	struct src *srcp;
	unsigned char c;
	unsigned char *cp;
	unsigned char *bufend;

again:
	srcp = &src[srcx];

	if ((c = srcp->pushedchars) != '\0') {
		srcp->pushedchars >>= 8;
		return (c);
	}

	switch (srcp->type) {

	case SRC_CONSOLE:
		if ((c = *srcp->nextchar++) != 0)
			return (c);
		/* end of line - get next line */
		*(srcp->nextchar = srcp->buf) = '\0';
		printf("> ");
		if (getcons(srcp->buf, srcp->bufsiz) == -1) {
			if (srcx == 0)
				printf("boot: bottom of source stack\n");
			else {
				bkmem_free((caddr_t)srcp->buf, srcp->bufsiz);
				--srcx;
			}
		}
		break;

	case SRC_FILE:
		/*
		 * The DOS_EOF test here is because sometimes DOS writes
		 * its EOF character into a file rather than terminating the
		 * input.  Discard the EOF character if it is at the end
		 * of a file.
		 */
		if (srcp->nextchar >= srcp->buf + srcp->bufsiz ||
				(*srcp->nextchar == DOS_EOF &&
				srcp->nextchar + 1 ==
				srcp->buf + srcp->bufsiz)) {
			bkmem_free((caddr_t)srcp->buf, srcp->bufsiz);
			--srcx;
			break;
		}
		if (verbose_flag && ((srcp->nextchar == srcp->buf) ||
		    (*(srcp->nextchar - 1) == '\n')))
			/* display next source line */
			for (cp = srcp->nextchar,
				bufend = srcp->buf + srcp->bufsiz;
				cp < bufend; ++cp) {
				putchar(c = *cp);
				if (c == '\n')
					break;
			}
		c = *srcp->nextchar++;
		return (c);

	case SRC_VARIABLE:
		if ((c = *srcp->nextchar++) != 0)
			return (c);
		/* end of var string */
		--srcx;
		break;

	default:
		printf("boot: logic error - illegal src type\n");
		prom_panic("getsrcchar");
	}
	goto again;
}

static void
pushsrcchar(c)
	unsigned char c;
{
	src[srcx].pushedchars = (src[srcx].pushedchars << 8) | c;
}


/*
 * subst_var: variable substitution
 *	   - returns true if variable value substituted
 *	   - returns false if variable name is null, e.g. $ or $.
 */

int
subst_var()
{
	unsigned char c;
	int in_braces;
	unsigned char	*varp;
	int		varsiz;
	unsigned char	var[VARSIZ];
	int		var_too_big;
	unsigned char	*valp;
	struct src *srcp;

	varp = var;
	varsiz = 0;
	in_braces = 0;
	var_too_big = 0;

	/* first char must be a letter or { */
firstchar:
	c = getsrcchar();
	if ((c >= 'a' && c <= 'z') ||
	    (c >= 'A' && c <= 'Z')) {
		*varp++ = c;
		++varsiz;
	} else if (c == '{') {
		++in_braces;
		goto firstchar;
	} else {
		pushsrcchar(c);
		return (0);
	}

	/* get the rest of the characters */
	for (;;) {
		c = getsrcchar();
		if ((c >= 'a' && c <= 'z') ||
		    (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') ||
		    (in_braces && c == '-') ||
		    (in_braces && c == '?') ||
		    (c == '_')) {
			if (varsiz >= (VARSIZ-1))
				++var_too_big;
			else {
				*varp++ = c;
				++varsiz;
			}
			continue;
		} else if (c == '}' && in_braces) {
			break;
		} else {
			pushsrcchar(c);
			break;
		}
	}

	*varp = 0;	/* null terminated string */
	if (var_too_big)
		printf("boot: variable name too long = %s...\n", var);

	/* find variable */

	if ((valp = var_ops(var, NULL, FIND_VAR)) == NULL)
		return (1);

	/* initialize next source struct */
	if (srcx >= (SRCSIZ-1)) {
		printf("boot: too many active sources");
		return (1);
	}
	srcp = &src[++srcx];
	srcp->type = SRC_VARIABLE;
	srcp->bufsiz = 0;
	srcp->buf = srcp->nextchar = valp;
	srcp->pushedchars = 0;

	return (1);
}

/* source_cmd() - pushes a new source file on source stack */

void
source_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	static int sourcecnt = 0;
	int	handle;
	size_t	size;
	struct stat statbuf;
	struct src *srcp;
	char	*addr;

	sourcecnt++;

	if (srcx >= (SRCSIZ-1)) {
		printf("boot: source: too many active sources");
		return;
	}

	if (argc != 2) {
		printf("boot: source: bad arg count = %d\n", argc);
		return;
	}

	if ((handle = open(argv[1], O_RDONLY)) == -1) {
		/*
		 *  Don't complain if the very first 'source' fails due
		 *  to a missing file.	This likely means we are booting
		 *  an older system that doesn't have the
		 *  /platform/i86pc/boot area.
		 */
		if (sourcecnt != 1)
			printf("boot: source: open of '%s' failed\n", argv[1]);
		return;
	}

	if (fstat(handle, &statbuf)) {
		printf("boot: source: fstat of '%s' failed\n", argv[1]);
		(void) close(handle);
		return;
	}

	size = statbuf.st_size;		/* file size */
	if (size > FILSIZ) {
		printf("boot: source: file '%s' too large = %d\n",
			argv[1], size);
		(void) close(handle);
		return;
	} else if (size == 0) {
		(void) close(handle);
		return;
	}

	if ((addr = bkmem_alloc(size)) == NULL) {
		printf("boot: source: can't get %d bytes for file '%s'\n",
			size, argv[1]);
		(void) close(handle);
		return;
	}

	if (boot_device == BOOT_FROM_DISK) {
		if (read(handle, addr, size) != size) {
			printf("boot: source: error reading file '%s'\n",
				argv[1]);
			bkmem_free(addr, size);
			(void) close(handle);
			return;
		}
	}

	if (boot_device == BOOT_FROM_NET) {
		(void) read(handle, addr, size);
	}

	(void) close(handle);

	/* initialize next source struct */
	srcp = &src[++srcx];
	srcp->type = SRC_FILE;
	srcp->bufsiz = size;
	srcp->buf = srcp->nextchar = (unsigned char *) addr;
	srcp->pushedchars = 0;
}


/*ARGSUSED*/
void
console_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	struct src *srcp;
	unsigned char *newlinbuf;

	if (srcx >= (SRCSIZ-1)) {
		printf("boot: console: too many active sources");
		return;
	}
	if ((newlinbuf = (unsigned char *)bkmem_alloc(LINBUFSIZ)) == NULL) {
		printf("boot: console: no memory for line buffer\n");
		return;
	}
	/* initialize next source struct for console input */
	srcp = &src[++srcx];
	srcp->type = SRC_CONSOLE;
	srcp->bufsiz = LINBUFSIZ;
	srcp->buf = srcp->nextchar = newlinbuf;
	srcp->pushedchars = 0;
	*newlinbuf = '\0';
}

void
singlestep()
{
	unsigned char buf[2];

	if (singlestep_flag) {
		printf("step ?");
		(void) getcons(buf, 2);
	}
}

void
singlestep_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	int	flag;

	if (argc == 1) {
		singlestep_flag = 1;
		return;
	}
	if (argc != 2) {
		printf("boot: singlestep: bad arg count = %d\n", argc);
		return;
	}
	if ((flag = on_or_off(argv[1])) >= 0)
		singlestep_flag = flag;
	else
		printf("boot: singlestep: bad arg = %s\n", argv[1]);
}


void
verbose_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	int	flag;

	if (argc == 1) {
		verbose_flag = 1;
		return;
	}
	if (argc != 2) {
		printf("boot: verbose: bad arg count = %d\n", argc);
		return;
	}
	if ((flag = on_or_off(argv[1])) >= 0)
		verbose_flag = flag;
	else
		printf("boot: verbose: bad arg = %s\n", argv[1]);
}

static int
on_or_off(s)
	char *s;
{
	if (strcmp(s, "on") == 0)
		return (1);
	else if (strcmp(s, "off") == 0)
		return (0);
	else
		return (-1);
}

/* read_cmd(): read words from console and assign to shell variables */
void
read_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	readg_cmd(-1, argc, argv);
}

/* readt_cmd(): same as readt with a timeout built in */
void
readt_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
#define	NOLIBC
#ifdef	NOLIBC
	register	char	*ptr = argv[1];
	int	timeout = 0;
	while (*ptr >= 0x30 && *ptr <= 0x39) {
		/* while we get decimal digits */
		timeout = timeout*10 + *ptr - 0x30;
		ptr++;
	}
#else
	int	timeout = atoi(argv[1]);
#endif
	readg_cmd(timeout, argc-1, ++argv);
}

/*
 * Generic read command which supports the old read command as well as the
 * new readt command (read with timeout).
 */

static void
readg_cmd(int timeout, int argc, char **argv)
{
	unsigned char lbuf[LINBUFSIZ]; /* local console line buffer */
	unsigned char *lbufend;
	int endoftheline;
	unsigned char *lp;
	unsigned char c;
	unsigned char word[WORDSIZ];
	unsigned char *wordp;
	int word_too_big;
	int wordsiz;

	/* read line from console */
	*(lp = lbuf) = '\0';
#ifndef	USERTEST
	if (timeout >= 0) {
		timeout = timeout*10;
		while (timeout-- && (ischar() == FALSE)) {
			wait100ms();	/* wait for a char */
		}
		if (ischar() == FALSE)
			*lbuf = '\n';
		else
			(void) getcons(lbuf, LINBUFSIZ);
	}
	else
		(void) getcons(lbuf, LINBUFSIZ);
#else /* USERTEST */
	(void) getcons(lbuf, LINBUFSIZ);
#endif /* USERTEST */
	lbufend = lbuf + LINBUFSIZ;
	endoftheline = 0;

	/* assign words to shell variables */
	while (--argc >= 1) {
	    word_too_big = 0;
	    wordp = word;
	    wordsiz = 0;
	    for (;;) {
		if (lp < lbufend && !endoftheline)
		    c = *lp++;
		else
		    c = '\0';
		switch (c) {
		case ' ':
		case '\t':
		    if (wordsiz == 0)
			continue;
		    else
			goto setvar;
		case '\n':
		case '\0':
		    ++endoftheline;
setvar:		    /* set variable to word */
		    *wordp = '\0';	/* null-terminate word */
		    if (word_too_big != 0)
			printf("boot: word too big: %s...\n", word);
		    (void) var_ops((u_char *)*++argv, word, SET_VAR);
		    goto nextarg;
		default:	/* put characters in word */
		    if (wordsiz >= (WORDSIZ-1))
			++word_too_big;
		    else {
			*wordp++ = c;
			wordsiz++;
		    }
		    continue;
		}
	    }
nextarg:
		;
	}
}
