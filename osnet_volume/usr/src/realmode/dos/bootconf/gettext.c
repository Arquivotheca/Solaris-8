/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * gettext.c -- routines to perform string translations
 */

#ident "@(#)gettext.c   1.13   99/10/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

#include "debug.h"
#include "err.h"
#include "gettext.h"
#include "open.h"
#include "spmalloc.h"

#define	MAXPATH	1024

/*
 * Globals - common strings used multiple times
 */
char *Please_wait = "Please wait ...";
char *Auto_boot = "auto-boot?";
char *Auto_boot_cfg_num = "auto-boot-cfg-num";
char *Auto_boot_timeout = "auto-boot-timeout";

/* stuff that determines where the .txt file lives */
static char Suffix[] = "txt";

/* our internal version of the .txt file */
static char *Text;
static int Textsize;
static char **Keys;
static char **Values;
static Nentries;

/*
 * compile_text -- compile escape sequences in a string up to terminating char
 *
 * this string takes "escaped" characters (like \r) and converts them to
 * their corresponding character.  if any such escapes are found in the
 * given string, the resulting string will be shorter, but it will still
 * begin in the same spot in memory (i.e. we do the conversion "in place").
 * the terminating character must be escaped to appear in the string.  when
 * we find the terminating character, we replace it with a '\0' so that the
 * resulting string is NULL terminated.  the return value is the first
 * character we did NOT consume.
 *
 * a NULL character will force us to stop whether we've seen the terminating
 * character or not.
 */

char *
compile_text(char *src, char term)
{
	char *dst;		/* destination string */
	unsigned char num;	/* number created by an escape sequence */
	char *hexdigits = "0123456789abcdef";
	int i;
	enum {
		S_NONE,		/* just copying normal characters */
		S_BSLASH,	/* last character was a backslash */
		S_BSLASHX,	/* last 2 characters were \[xX] */
		S_BSLASHX1,	/* last 3 characters were \[xX][0-9a-fA-F] */
		S_BSLASH1,	/* last 2 characters were \[0-7] */
		S_BSLASH2	/* last 3 characters were \[0-7][0-7] */
	} state = S_NONE;

	/* note that src & dst overlap, but src is always >= dst */
	dst = src;

	/*CONSTANTCONDITION*/
	while (1) {
		if (*src == '\0')
			fatal("missing '%c' character in .txt file", term);
		else if (state == S_BSLASHX) {
			/* last 2 characters were \[xX] */
			for (i = 0; hexdigits[i]; i++)
				if (*src == hexdigits[i]) {
					num = (char)i;
					state = S_BSLASHX1;
					break;
				}
			if (hexdigits[i] == '\0') {
				/*
				 * we've seen \xc where c is not a hex
				 * digit.  emit a null.  (an ansi C compiler
				 * would print treat this as a fatal error,
				 * but it doesn't seem worth it here.)
				 */
				*dst++ = '\0';
				state = S_NONE;
			}
		} else if (state == S_BSLASHX1) {
			/* last 3 characters were \[xX][0-9a-fA-F] */
			for (i = 0; hexdigits[i]; i++)
				if (*src == hexdigits[i]) {
					num = (num * 16) + i;
					state = S_BSLASHX1;
					break;
				}
			*dst++ = num;
			state = S_NONE;
		} else if (state == S_BSLASH1) {
			/* last 2 characters were \[0-7] */
			if ((*src >= '0') && (*src <= '7')) {
				num = (num * 8) + (*src - '0');
				state = S_BSLASH2;
			} else {
				*dst++ = num;
				state = S_NONE;
			}
		} else if (state == S_BSLASH2) {
			/* last 3 characters were \[0-7][0-7] */
			if ((*src >= '0') && (*src <= '7')) {
				num = (num * 8) + (*src - '0');
				state = S_BSLASH2;
			}
			*dst++ = num;
			state = S_NONE;
		} else if (state == S_BSLASH) {
			/* previous character was a backslash */
			switch (*src) {
			case 'x':
			case 'X':
				state = S_BSLASHX;
				break;

			case 'a':
				/*
				 * sun's compiler prints annoying warning
				 * if we use \a in the next statement...
				 */
				*dst++ = '\007';
				state = S_NONE;
				break;

			case 'b':
				*dst++ = '\b';
				state = S_NONE;
				break;

			case 'f':
				*dst++ = '\f';
				state = S_NONE;
				break;

			case 'n':
				*dst++ = '\n';
				state = S_NONE;
				break;

			case 'r':
				*dst++ = '\r';
				state = S_NONE;
				break;

			case 't':
				*dst++ = '\t';
				state = S_NONE;
				break;

			case 'v':
				*dst++ = '\v';
				state = S_NONE;
				break;

			default:
				if ((*src >= '0') && (*src <= '7')) {
					num = *src - '0';
					state = S_BSLASH1;
				} else {
					/*
					 * unknown escape, just eat the
					 * backslash and emit the character
					 * (i.e. \c just stands for character
					 * 'c' if it isn't one of the above
					 * cases).  this is also how the
					 * termination character gets by us
					 * when it is escaped too.
					 */
					*dst++ = *src;
					state = S_NONE;
				}
			}
		} else if (*src == '\\')
			state = S_BSLASH;
		else if (*src == term) {
			src++;	/* eat the term character */
			break;
		} else
			*dst++ = *src;

		src++;
	}
	*dst++ = '\0';

	return (src);
}

/*
 * fini_gettext -- free up any memory used by gettext module
 */

void
/*ARGSUSED*/
fini_gettext(void *arg, int exitcode)
{
	if (Text) {
		spcl_free(Text);
		Text = NULL;
	}

	if (Keys) {
		spcl_free(Keys);
		Keys = NULL;
	}

	if (Values) {
		spcl_free(Values);
		Values = NULL;
	}

	Textsize = Nentries = 0;
}

/*
 * init_gettext -- initialize the gettext module
 */

void
init_gettext(const char *progname)
{
	char *ptr;
	char *eptr;
	int fd;
	int i;
	struct _stat stbuf;
	unsigned int cc;

	/*
	 * see if we can open the .txt file.  it should be in the solaris
	 * subdirectory but we allow it to be "up here" for easier access
	 * during development.
	 */
	if (((fd = fn_open(NULL, 0, "solaris", progname,
	    Suffix, _O_RDONLY)) < 0) && ((fd = fn_open(NULL, 0, ".", progname,
	    Suffix, _O_RDONLY)) < 0))
		return;		/* no .txt file exists */

	/* get the size of the .txt file and read the whole thing in */
	if (_fstat(fd, &stbuf) < 0)
		fatal("can't stat .txt file: %!");

	/* remember length of malloc'd area */
	Textsize = (int)stbuf.st_size + 1;

	if ((Text = spcl_malloc(Textsize)) == NULL)
		MemFailure();
	if ((cc = _read(fd, Text, (unsigned int)stbuf.st_size)) == -1)
		fatal("can't read .txt file: %!");
	else if (cc != (unsigned int)stbuf.st_size)
		fatal("can't read .txt file: "
		    "only read %d bytes out of %d", cc,
		    (unsigned int)stbuf.st_size);

	/* NULL terminate the text buffer for our convenience */
	eptr = &Text[Textsize - 1];
	*eptr = '\0';

	/* zip through the buffer once and count the entries */
	for (ptr = Text; ptr < eptr; ptr++)
		if (((ptr == Text) && (*ptr == '[')) ||
		    (((*ptr == '\n') || (*ptr == '\r')) && (*(ptr + 1) == '[')))
			Nentries++;

	/* allocate arrays to point at the keys and the values */
	if ((Keys = (char **)spcl_calloc(Nentries, sizeof (char **))) == NULL) {
		spcl_free(Text);
		MemFailure();
	}
	if ((Values = (char **)spcl_calloc(Nentries, sizeof (char **))) ==
								NULL) {
		spcl_free(Keys);
		spcl_free(Text);
		MemFailure();
	}

	/* have our "free" routine called when the program completes */
	ondoneadd(fini_gettext, 0, CB_EVEN_FATAL);

	/*
	 * run through the buffer again, fill in Keys and Values, and
	 * "compile" any escaped characters
	 */
	i = 0;
	for (ptr = Text; ptr < eptr; )
		if (((ptr == Text) && (*ptr == '[')) ||
		    (((*ptr == '\n') || (*ptr == '\r')) &&
		    (*(ptr + 1) == '['))) {
			/* skip to beginning of key */
			if (ptr == Text)
				ptr++;
			else
				ptr += 2;
			Keys[i] = ptr;
			ptr = compile_text(ptr, ']');
			while (ptr < eptr)
				if (*ptr++ == '"')
					break;
			Values[i] = ptr;
			ptr = compile_text(ptr, '"');
			i++;
		} else
			ptr++;
}

/*
 * gettext -- translate a string
 */

const char *
gettext(const char *msg)
{
	int i;

	if ((Text == NULL) || (msg == NULL))
		return (msg);

	/* search for the key (simple linear search for now) */
	for (i = 0; i < Nentries; i++)
		if (strcmp(Keys[i], msg) == 0)
			return (Values[i]);

	/* didn't find key */
	return (msg);
}
