/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uuencode.c	1.16	96/10/17 SMI"	/* from SVR4 bnu:uuencode.c 1.3 */

/*
 *		PROPRIETARY NOTICE-(Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * (c) 1986, 1987, 1988, 1989, 1996  Sun Microsystems, Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		    All rights reserved.
 */

/*
 * uuencode [input] output
 *
 * Encode a file so it can be mailed to a remote system.
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>
#include <iconv.h>
#include <errno.h>


/*
 * Name of ISO/IEC 646: 1991 encoding on this system
 *
 * We don't know what the final name to use for iconv will be, so,
 * guess at a name and allow it to be changed in the makefile if
 * necessary.
 *
 * We also rely on nl_langinfo to return an empty string if CODESET
 * is not implemented.
 *
 * The goal here is to minimize the amount of work necessary when a final
 * approach is decided upon...
 */
#ifndef ISO_646
#define	ISO_646	"646"
#endif

/*
 * Encoding conversion table used to translate characters to the current
 * locale's character encoding...
 *
 * Table is initialized by init_encode_table()...
 *
 * (Size of TABLE_SIZE octal is large enough to convert a basic 6-bit
 * data chunk.)
 */
#define		TABLE_SIZE	0100

static char	end_646[] = {
	'\145',	/* 'e' */
	'\156',	/* 'n' */
	'\144',	/* 'd' */
	'\012',	/* '\n' */
	'\000',	/* NULL */
};

static char	begin_646[] = {
	'\142',	/* 'b' */
	'\145',	/* 'e' */
	'\147',	/* 'g' */
	'\151',	/* 'i' */
	'\156',	/* 'n' */
	'\040', /* ' ' */
	'\045', /* '%' */
	'\154', /* 'l' */
	'\157', /* 'o' */
	'\040', /* ' ' */
	'\045', /* '%' */
	'\163', /* 's' */
	'\012',	/* '\n' */
	'\000',	/* NULL */
};

static char	newline_646[] = {
	'\012',	/* 'n' */
	'\000',	/* NULL */
};


static char	encode_begin[sizeof (begin_646) + 4];
static char	encode_end[sizeof (end_646) + 4];
static char	encode_newline[sizeof (newline_646) + 4];
static char	encode_table[TABLE_SIZE];

/* ENC is the basic 1 character encoding function to make a char printing */
#define	ENC(c)	encode_table[(c) & 077]

static void	init_encode_table(char *);
static void	encode(FILE *, FILE *);

static char	*prog;

void
main(int argc, char **argv)
{
	FILE *in;
	struct stat sbuf;
	mode_t mode = 0;
	char	*encoding;
	int	c, errflag = 0;

	prog = argv[0];

	/* Set locale environment variables local definitions */
	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*
	 * If the codeset is unknown we default to 646.
	 */
	encoding = nl_langinfo(CODESET);
	if (strcmp(encoding, "") == 0) {
		encoding = ISO_646;
	}

	init_encode_table(encoding);

	while ((c = getopt(argc, argv, "")) != EOF) {
		if (c == '?') {
			errflag++;
		}
	}
	argc -= optind;
	argv = &argv[optind];

	/* optional 1st argument */
	if (argc > 1) {
		if ((in = fopen(*argv, "r")) == NULL) {
			perror(*argv);
			exit(1);
		}
		argv++; argc--;
	} else {
		in = stdin;
		mode = 0777;
	}

	if ((argc != 1) || errflag) {
		(void) fprintf(stderr,
		    gettext("Usage: %s [infile] remotefile\n"), prog);
		exit(2);
	}
	/* figure out the input file mode */
	errno = 0;
	if (fstat(fileno(in), &sbuf) < 0 || !S_ISREG(sbuf.st_mode)) {
		mode = 0666 & ~umask(0666);
	} else {
		mode = sbuf.st_mode & 0777;
	}

	(void) printf(encode_begin, (long) mode, *argv);

	encode(in, stdout);

	(void) fwrite(encode_end, strlen(encode_end), 1, stdout);
	exit(0);
	/* NOTREACHED */
}

/*
 * copy from in to out, encoding as you go along.
 */
static void
encode(FILE *in, FILE *out)
{
	char in_buf[80];
	char out_buf[112];
	char *iptr, *optr;
	int i, n;

	for (;;) {
		iptr = in_buf;
		optr = out_buf;

		/* 1 (up to) 45 character line */
		n = fread(iptr, 1, 45, in);

		*(optr++) = ENC(n);

		for (i = 0; i < n; i += 3) {
			*(optr++) = ENC(*iptr >> 2);
			*(optr++) = ENC((*iptr << 4) & 060 |
			    (*(iptr + 1) >> 4) & 017);
			*(optr++) = ENC((*(iptr + 1) << 2) & 074 |
			    (*(iptr + 2) >> 6) & 03);
			*(optr++) = ENC(*(iptr + 2) & 077);
			iptr += 3;
		}

		*(optr++) = *encode_newline;

		fwrite(out_buf, 1, optr - out_buf, out);
		if (n <= 0)
			break;
	}
}


/*
 * We don't really know how to get the current character encoding name at this
 * moment in time.  We do assume, however, that we will probably use iconv
 * for the actual conversion so we provide code using this but we have
 * it commented out for now...
 *
 * Note that uuencode/uudecode uses only the portable character set for
 * encoded data and the portable character set characters must be represented
 * in a single byte.  We use this knowledge to reuse buffer space while
 * encoding.
 */

static void
init_encode_table(char *encoding)
{
	char		ebuf[128];
	unsigned char	buf_646[TABLE_SIZE];
	iconv_t		conv;
	unsigned char	i;
	char		*decode_addr;
	char		*in_buf, *out_buf;
	size_t		in_len, out_len;

	/* Load tables with ISO/IEC 646: 1991 standard encoded characters */
	for (i = 0; i < TABLE_SIZE; i++) {
		buf_646[i] = i + 0x20;
	}

	/*
	 * Translate characters into current locale's encoding if needed.
	 */
	if (strcmp(ISO_646, encoding) != 0) {
		/*
		 * Convert to locale specific character set.
		 */
		if ((conv = iconv_open(encoding, ISO_646)) == (iconv_t) -1) {
			(void) sprintf(ebuf, gettext("%s: %s to %s conversion"),
			    prog, encoding, ISO_646);
			perror(ebuf);
			goto noconv;
		}

		in_buf = begin_646;
		out_buf = encode_begin;
		in_len = out_len = strlen(begin_646);
		if (iconv(conv, (const char **)&in_buf, &in_len,
		    &out_buf, &out_len) == (size_t) -1) {
			(void) sprintf(ebuf, gettext("%s: %s to %s conversion"),
			    prog, encoding, ISO_646);
			perror(ebuf);
			iconv_close(conv);
			goto noconv;
		}

		in_buf = end_646;
		out_buf = encode_end;
		in_len = out_len = strlen(end_646);
		if (iconv(conv, (const char **)&in_buf, &in_len,
		    &out_buf, &out_len) == (size_t) -1) {
			(void) sprintf(ebuf, gettext("%s: %s to %s conversion"),
			    prog, encoding, ISO_646);
			perror(ebuf);
			iconv_close(conv);
			goto noconv;
		}

		in_buf = newline_646;
		out_buf = encode_newline;
		in_len = out_len = strlen(newline_646);
		if (iconv(conv, (const char **)&in_buf, &in_len,
		    &out_buf, &out_len) == (size_t) -1) {
			(void) sprintf(ebuf, gettext("%s: %s to %s conversion"),
			    prog, encoding, ISO_646);
			perror(ebuf);
			iconv_close(conv);
			goto noconv;
		}

		in_buf = (char *)buf_646;
		out_buf = (char *)encode_table;
		in_len = out_len = sizeof (encode_table);
		if (iconv(conv, (const char **)&in_buf, &in_len,
		    &out_buf, &out_len) == (size_t) -1) {
			(void) sprintf(ebuf, gettext("%s: %s to %s conversion"),
			    prog, encoding, ISO_646);
			perror(ebuf);
			iconv_close(conv);
			goto noconv;
		}

		iconv_close(conv);
	} else {
		/*
		 * No conversion will take place
		 */
noconv:
		strcpy(encode_begin, begin_646);
		strcpy(encode_end, end_646);
		strcpy(encode_newline, newline_646);
		memcpy(encode_table, buf_646, sizeof (encode_table));
	}
}
