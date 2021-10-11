/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1988, 1996, 1997 Sun Microsystems, Inc.	*/
/*	All Rights Reserved.				*/

#ident	"@(#)uudecode.c	1.17	97/05/12 SMI"	/* from SVR4 bnu:uudecode.c 1.2 */
/*
 * uudecode [-p] [input]
 *
 * create the specified file, decoding as you go.
 * used with uuencode.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>
#include <iconv.h>
#include <limits.h>
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


#define	BUFSIZE	80

#define	isvalid(octet)	(octet <= 0x40)

/*
 * Encoding conversion table used to translate characters to the current
 * locale's character encoding...
 *
 * Table is initialized by init_decode_table()...
 *
 * (Size of 0x3f is large enough to convert a basic 6-bit data chunk.)
 */
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
	'\000',	/* NULL */
};

/* begin, end, and data convereted to local character set */
static char		end_line[12], begin_line[12];
static unsigned char	decode_table[UCHAR_MAX+1];

/* DEC is the basic 1 character decoding function */
#define	DEC(c)	decode_table[c]

static void	init_decode_table(char *);
static void	decode(FILE *, FILE *);
static int	outdec(unsigned char *, unsigned char *, int);

static char	*prog;
static wchar_t	line[1024];

void
main(int argc, char **argv)
{
	FILE *in, *out;
	int pipeout = 0;
	mode_t mode;
	char dest[BUFSIZ];
	char buf[BUFSIZ];
	char	*encoding;
	int	c, errflag = 0;
	struct stat sbuf;

	prog = argv[0];

	/* Set locale environment variables local definitions */
	encoding = setlocale(LC_CTYPE, "");
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

	init_decode_table(encoding);

	while ((c = getopt(argc, argv, "p")) != EOF) {
		switch (c) {
		case 'p':
			pipeout++;
			break;
		case '?':
			errflag++;
			break;
		}
	}
	argc -= optind;
	argv = &argv[optind];

	/* optional input arg */
	if (argc > 0) {
		if ((in = fopen(*argv, "r")) == NULL) {
			perror(*argv);
			exit(1);
		}
		argv++; argc--;
	} else {
		in = stdin;
		errno = 0;
		if (fstat(fileno(in), &sbuf) < 0) {
			perror("stdin");
			exit(1);
		}
	}

	if ((argc > 0) || errflag) {
		(void) fprintf(stderr,
		    gettext("Usage: %s -p [infile]\n"), prog);
		exit(2);
	}

	/* search for header line */
	for (;;) {
		if (fgets(buf, sizeof (buf), in) == NULL) {
			(void) fprintf(stderr, gettext("No begin line\n"));
			exit(3);
		}
		if (strncmp(buf, begin_line, strlen(begin_line)) == 0) {
			if (sscanf(buf+strlen(begin_line), "%lo %s",
			    &mode, dest) == 2) {
				break;
			}
		}
	}

	if (pipeout) {
		out = stdout;
	} else {
		/* handle ~user/file format */
		if (dest[0] == '~') {
			char *sl;
			struct passwd *user;
			char dnbuf[100];

			sl = strchr(dest, '/');
			if (sl == NULL) {
				(void) fprintf(stderr,
				    gettext("Illegal ~user\n"));
				exit(3);
			}
			*sl++ = 0;
			user = getpwnam(dest+1);
			if (user == NULL) {
				(void) fprintf(stderr,
				    gettext("No such user as %s\n"), dest);
				exit(4);
			}
			(void) strcpy(dnbuf, user->pw_dir);
			(void) strcat(dnbuf, "/");
			(void) strcat(dnbuf, sl);
			(void) strcpy(dest, dnbuf);
		}

		/* create output file */
		out = fopen(dest, "w");
		if (out == NULL) {
			perror(dest);
			exit(4);
		}
		(void) chmod(dest, mode & 0777);
	}

	decode(in, out);

	exit(0);
	/* NOTREACHED */
}

/*
 * copy from in to out, decoding as you go along.
 */

static void
decode(FILE *in, FILE *out)
{
	char	buf[BUFSIZE];
	char	*ibp, *obp, ch;
	size_t	len;
	int	n, octets, warned;
	longlong_t line;

	warned = 0;
	for (line = 1; ; line++) {
		/* for each input line */
		if (fgets(buf, sizeof (buf), in) == NULL) {
			(void) fprintf(stderr, gettext("No end line\n"));
			exit(5);
		}

		/* Is line == 'end\n'? */
		if (strcmp(buf, end_line) == 0) {
			break;
		}

		n = DEC(buf[0]);

		if (n < 0)
			continue;

		/*
		 * Decode data lines.
		 *
		 * Note that uuencode/uudecode uses only the portable
		 * character set for encoded data and the portable character
		 * set characters must be represented in a single byte.  We
		 * use this knowledge to reuse buffer space while decoding.
		 */
		octets = n;
		obp = &buf[0];
		ibp = &buf[1];
		while (octets > 0) {
			if ((ch = outdec((unsigned char *)obp,
			    (unsigned char *)ibp, octets)) != 0x20) {
				/* invalid characters where detected */
				if (!warned) {
					warned = 1;
					fprintf(stderr, gettext("Invalid "
					    "character (0x%x) on line %lld\n"),
					    ch, line);
				}
				break;
			}
			ibp += 4;
			obp += 3;
			octets -= 3;
		}
		/*
		 * Only write out uncorrupted lines
		 */
		if (octets <= 0) {
			fwrite(buf, n, 1, out);
		}
	}
}


/*
 * output a group of 3 bytes (4 input characters).
 * the input chars are pointed to by p, they are to
 * be output to file f.  n is used to tell us not to
 * output all of them at the end of the file.
 */

static int
outdec(unsigned char *out, unsigned char *in, int n)
{
	unsigned char	b0 = DEC(*(in++));
	unsigned char	b1 = DEC(*(in++));
	unsigned char	b2 = DEC(*(in++));
	unsigned char	b3 = DEC(*in);

	if (!isvalid(b0)) {
		return (*(in-3));
	}
	if (!isvalid(b1)) {
		return (*(in-2));
	}

	*(out++) = (b0 << 2) | (b1 >> 4);

	if (n >= 2) {
		if (!isvalid(b2)) {
			return (*(in - 1));
		}

		*(out++) = (b1 << 4) | (b2 >> 2);

		if (n >= 3) {
			if (!isvalid(b3)) {
				return (*in);
			}
			*out = (b2 << 6) | b3;
		}
	}
	return (0x20); /* a know good value */
}


/*
 * We don't really know how to get the current character encoding name at this
 * moment in time.  We do assume, however, that we will probably use iconv
 * for the actual conversion so we provide code using this but we have
 * it commented out for now...
 *
 */

static void
init_decode_table(char *encoding)
{
	char		ebuf[BUFSIZE];
	unsigned char	buf_646[0x40+1];
	unsigned char	buf[0x40+1];
	iconv_t		conv;
	unsigned char	i;
	char		*decode_addr;
	char		*in_buf, *out_buf;
	size_t		in_len, out_len;

	/*
	 * Load tables with ISO/IEC 646: 1991 standard encoded characters
	 * that we will be using.
	 */
	for (i = 0; i <= 0x40; i++) {
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
		out_buf = begin_line;
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
		out_buf = end_line;
		in_len = out_len = strlen(end_646);
		if (iconv(conv, (const char **)&in_buf, &in_len,
		    &out_buf, &out_len) == (size_t) -1) {
			(void) sprintf(ebuf, gettext("%s: %s to %s conversion"),
			    prog, encoding, ISO_646);
			perror(ebuf);
			iconv_close(conv);
			goto noconv;
		}
		in_buf = (char *)buf_646;
		out_buf = (char *)buf;
		in_len = out_len = sizeof (buf);
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
		strcpy(begin_line, begin_646);
		strcpy(end_line, end_646);
		memcpy(buf, buf_646, sizeof (buf));
	}

	/*
	 * Setup translation table given current encoding.
	 * We go ahead and decode 0x40 (') and have it converted
	 * to 0 (& 0x3f) to work around a common convention
	 * of using the '\'' character instead of the ' ' character
	 * to encode a 0 octet.
	 */
	(void) memset(decode_table, 0xff, sizeof (decode_table));
	for (i = 0; i <= 0x40; i++) {
		decode_table[buf[i]] = (unsigned char)i & 0x3f;
	}
}
