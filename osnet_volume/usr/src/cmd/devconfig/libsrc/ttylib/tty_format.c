/*LINTLIBRARY*/
/*
 * Copyright (c) 1991,1992,1993 Sun Microsystems, Inc.  All Rights Reserved.
 * Sun considers its source code as an unpublished, proprietary trade secret,
 * and it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

#pragma	ident	"@(#)tty_format.c 1.2 94/02/17"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

static	char	*fmt(char *, int);
static	int	any(char, char	*);
static	void	prefix(char *);
static	void	split(char *);
static	void	pack(char *);
static	void	oflush(void);
static	void	tabulate(char *);
static	void	leadin(void);

extern	void	*xmalloc(size_t);

/*
 * Routine to format messages.
 */

char **
format_text(
	char	*string,	/* message to format */
	int	width)		/* desired indentation of formatted text */
{
	int n;
	char *s;
	char **return_array;

	/*
	 * Format the message.
	 * The formatted text is returned in a new buffer.
	 */

	string = fmt(string, width);

	/*
	 * count all the newlines so we know how much to
	 * allocate
	 */

	n = 0;
	s = string;
	while (*s) {
		if (*s++ == '\n')
			n++;
	}

	return_array = (char **)xmalloc((n + 1) * sizeof (char *));

	/*
	 * now go down the string and whenever a newline
	 * is found, it becomes a null to terminate the current line
	 * and the next character is pointed to as the beginning of the
	 * next line
	 */

	n = 0;
	s = string;
	return_array[n++] = s;	/* first one */

	while (*s) {
		if (*s == '\n') {
			*s++ = '\0';
			return_array[n++] = s;
		} else
			s++;
	}

	return_array[--n] = NULL;

	return (return_array);

}




/*
 * BELOW HERE IS AN ADAPTATION OF THE TEXT FORMATTER USED BY MAIL
 * AND VI, MODIFIED TO DO I/O ON STRINGS, RATHER THAN ON FILES.
 */




/*
 * Is ch any of the characters in str?
 */

static int
any(char ch, char *str)
{
	register char *f;
	register c;

	f = str;
	c = ch;
	while (*f)
		if (c == *f++)
			return (1);
	return (0);
}



/*
 * fmt -- format the concatenation of strings
 */

/* string equivalents for stdin, stdout */
static	char *sstdout;
static	int  _soutptr;
#define	sgetc(fi)	(_ptr < (int)strlen(fi) ? fi[_ptr++] : EOF)
#define	sputchar(c)	sstdout[_soutptr++] = c
#define	sputc(c, f)	sputchar(c)


#define	NOSTR	((char *) 0)	/* Null string pointer for lint */

static	char	outbuf[BUFSIZ];	/* Sandbagged output line image */
static	char	*outp;		/* Pointer in above */
static	int	filler;		/* Filler amount in outbuf */

static	int	pfx;		/* Current leading blank count */
static	int	lineno;		/* Current input line */
static	int	nojoin = 1;	/* split lines only, don't join */
				/* short ones */
static	int	width = 80;	/* Width that we will not exceed */

enum crown_type	{c_none, c_reset, c_head, c_lead, c_fixup, c_body};
static	enum crown_type	crown_state;	/* Crown margin state */
static	int	crown_head;		/* The header offset */
static	int	crown_body;		/* The body offset */



/*
 * Drive the whole formatter by managing input string.  Also,
 * cause initialization of the output stuff and flush it out
 * at the end.
 */


/*
 * Read up characters from the passed input string, forming lines,
 * doing ^H processing, expanding tabs, stripping trailing blanks,
 * and sending each line down for analysis.
 */

static char *
fmt(char *fi, int cols)
{
	char linebuf[BUFSIZ], canonb[BUFSIZ];
	register char *cp, *cp2;
	register int c, col;
	register int _ptr = 0;

	width = cols;
	if (width > cols)
		width = 80;


	/* open format buffer */
	sstdout = (char *)xmalloc(BUFSIZ);
	sstdout[0] = '\0';
	_soutptr = 0;

	c = (int)sgetc(fi);
	while (c != EOF) {

		/*
		 * Collect a line, doing ^H processing.
		 * Leave tabs for now.
		 */

		cp = linebuf;
		while (c != '\n' && c != EOF && cp-linebuf < BUFSIZ-1) {
			if (c == '\b') {
				if (cp > linebuf)
					cp--;
				c = (int)sgetc(fi);
				continue;
			}
			if ((c >= '\0' && c < ' ') && c != '\t') {
				c = (int)sgetc(fi);
				continue;
			}
			*cp++ = (char)c;
			c = (int)sgetc(fi);
		}
		*cp = '\0';

		/*
		 * Toss anything remaining on the input line.
		 */

		while (c != '\n' && c != EOF)
			c = (int)sgetc(fi);

		/*
		 * Expand tabs on the way to canonb.
		 */

		col = 0;
		cp = linebuf;
		cp2 = canonb;
		while ((c = *cp++) != NULL) {
			if (c != '\t') {
				col++;
				if (cp2-canonb < BUFSIZ-1)
					*cp2++ = (char)c;
				continue;
			}
			do {
				if (cp2-canonb < BUFSIZ-1)
					*cp2++ = ' ';
				col++;
			} while ((col & 07) != 0);
		}

		/*
		 * Swipe trailing blanks from the line.
		 */

		for (cp2--; cp2 >= canonb && *cp2 == ' '; cp2--)
			;
		*++cp2 = '\0';
		prefix(canonb);
		if (c != EOF)
			c = (int)sgetc(fi);
	}

	/*
	 * If anything partial line image left over,
	 * send it out now.
	 */
	if (outp) {
		*outp = '\0';
		outp = NOSTR;
		(void) strcat(sstdout, outbuf);
		(void) strcat(sstdout, "\n");
		outbuf[0] = '\0';
	}

	return (sstdout);
}

/*
 * Take a line devoid of tabs and other garbage and determine its
 * blank prefix.  If the indent changes, call for a linebreak.
 * If the input line is blank, echo the blank line on the output.
 * Finally, if the line minus the prefix is a mail header, try to keep
 * it on a line by itself.
 */

static void
prefix(char line[])
{
	register char *cp;
	register int np, h = 0;

	if (strlen(line) == 0) {
		oflush();
		sputchar('\n');
		sstdout[_soutptr] = '\0';
		if (crown_state != c_none)
			crown_state = c_reset;
		return;
	}
	for (cp = line; *cp == ' '; cp++)
		;
	np = cp - line;

	/*
	 * The following horrible expression attempts to avoid linebreaks
	 * when the indent changes due to a paragraph.
	 */

	if (crown_state == c_none && np != pfx && (np > pfx || abs(pfx-np) > 8))
		oflush();
	if (nojoin) {
		h = 1;
		oflush();
	}
	if (!h && (h = (*cp == '.')))
		oflush();
	pfx = np;
	switch (crown_state) {
	case c_reset:
		crown_head = pfx;
		crown_state = c_head;
		break;
	case c_lead:
		crown_body = pfx;
		crown_state = c_body;
		break;
	case c_fixup:
		crown_body = pfx;
		crown_state = c_body;
		if (outp) {
			char s[BUFSIZ];

			*outp = '\0';
			(void) strcpy(s, &outbuf[crown_head]);
			outp = NOSTR;
			split(s);
		}
	}
	split(cp);
	if (h)
		oflush();
	lineno++;
}

/*
 * Split up the passed line into output "words" which are
 * maximal strings of non-blanks with the blank separation
 * attached at the end.  Pass these words along to the output
 * line packer.
 */

static void
split(char line[])
{
	register char *cp, *cp2;
	char word[BUFSIZ];

	cp = line;
	while (*cp) {
		cp2 = word;

		/*
		 * Collect a 'word,' allowing it to contain escaped
		 * white space.
		 */

		while (*cp && *cp != ' ') {
			if (*cp == '\\' && isspace(cp[1]))
				*cp2++ = *cp++;
			*cp2++ = *cp++;
		}

		/*
		 * Guarantee a space at end of line.
		 * Two spaces after end of sentence punctuation.
		 */

		if (*cp == '\0' && cp[-1] != ' ') {
			*cp2++ = ' ';
			if (any(cp[-1], ".:!?"))
				*cp2++ = ' ';
		}
		while (*cp == ' ')
			*cp2++ = *cp++;
		*cp2 = '\0';
		pack(word);
	}
}

/*
 * Output section.
 * Build up line images from the words passed in.  Prefix
 * each line with correct number of blanks.  The buffer "outbuf"
 * contains the current partial line image, including prefixed blanks.
 * "outp" points to the next available space therein.  When outp is NOSTR,
 * there ain't nothing in there yet.  At the bottom of this whole mess,
 * leading tabs are reinserted.
 */


/*
 * Pack a word onto the output line.  If this is the beginning of
 * the line, push on the appropriately-sized string of blanks first.
 * If the word won't fit on the current line, flush and begin a new
 * line.  If the word is too long to fit all by itself on a line,
 * just give it its own and hope for the best.
 */

static void
pack(char word[])
{
	register char *cp;
	register int s, t, w;

	if (outp == NOSTR)
		leadin();
	t = strlen(word);
	s = outp-outbuf;
	if (t+s <= width) {

		/*
		 * In like flint!
		 */

		for (cp = word; *cp; *outp++ = *cp++)
			;
		return;
	}
	if (t <= width-filler) {	/* fits by itself so start new line */
		oflush();
		leadin();
		s = outp-outbuf;
	}
	w = width - (s + 1);			/* allow for '-' at end */
	for (cp = word, t = 1; *cp; *outp++ = *cp++, t++) {
		if (t > w && cp[1]) {
			*outp++ = '-';		/* hyphenate line */
			oflush();		/* start new line */
			leadin();		/* add leading white space */
			t = 1;			/* reset count */
			s = outp-outbuf;
			w = width - (s + 1);
		}
	}
}

/*
 * If there is anything on the current output line, send it on
 * its way.  Set outp to NOSTR to indicate the absence of the current
 * line prefix.
 */

static void
oflush(void)
{
	if (outp == NOSTR)
		return;
	*outp = '\0';
	tabulate(outbuf);
	outp = NOSTR;
}

/*
 * Take the passed line buffer, insert leading tabs where possible, and
 * output on standard output (finally).
 */

static void
tabulate(char line[])
{
	register char *cp;
	register int b, t;

	/*
	 * Toss trailing blanks in the output line.
	 */

	cp = line + strlen(line) - 1;
	while (cp >= line && *cp == ' ')
		cp--;

	/*
	 * Add a single blank after anything that
	 * might be a user prompt.
	 */

	if (any(*cp++, ":?"))
		*cp++ = ' ';
	*cp = '\0';

	/*
	 * Count the leading blank space and tabulate.
	 */

	for (cp = line; *cp == ' '; cp++)
		;
	b = cp-line;
	t = b >> 3;
	b &= 07;
	if (t > 0)
		do
			sputc('\t', stdout);
		while (--t);
	if (b > 0)
		do
			sputc(' ', stdout);
		while (--b);
	while (*cp)
		sputc(*cp++, stdout);
	sputc('\n', stdout);
	sstdout[_soutptr] = '\0';
}

/*
 * Initialize the output line with the appropriate number of
 * leading blanks.
 */

static void
leadin(void)
{
	register int b;
	register char *cp;
	register int l;

	switch (crown_state) {
	case c_head:
		l = crown_head;
		crown_state = c_lead;
		break;

	case c_lead:
	case c_fixup:
		l = crown_head;
		crown_state = c_fixup;
		break;

	case c_body:
		l = crown_body;
		break;

	default:
		l = pfx;
		break;
	}
	filler = l;
	for (b = 0, cp = outbuf; b < l; b++)
		*cp++ = ' ';
	outp = cp;
}
