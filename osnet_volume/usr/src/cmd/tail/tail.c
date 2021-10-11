/* Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/* All Rights Reserved					*/

/* THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/* The copyright notice above does not evidence any	*/
/* actual or intended publication of such source code.	*/

/* Parts of this product may be derived from OSF/1 1.0 and */
/* Berkeley 4.3 BSD systems licensed from */
/* OPEN SOFTWARE FOUNDATION, INC. and the University of California. */

/* Parts of this product may be derived from the systems licensed */
/* from International Business Machines Corp. and */
/* Bell Telephone Laboratories, Inc. */

/* Portions Copyright (c) 1988,1994,1996-1999 by Sun Microsystems, Inc. */
/* All Rights Reserved.					*/

#pragma	ident	"@(#)tail.c	1.34	99/09/17	SMI"	/* SVr4.0 1.14   */

/*
 * XCU4 compiant tail command
 *  NEW STYLE
 *     tail [-fr] [-c number | -n number] [file]
 *  option 'c' means tail number bytes
 *  option 'n' means tail number lines
 *  The number must be a decimal integer whose sign affects the location in
 *  the file, measured in bytes/lines, to begin the copying:
 *          Sign        Copying Starts
 *          +           Relative to the beginning of the file.
 *          -           Relative to the end of the file.
 *          none        Relative to the end of the file.
 *  The origin for couting is 1; that is, -c +1 represents the first byte
 *  of the file, -n -1 the last line.
 *  option 'f' means loop endlessly trying to read more bytes after the
 *  end of file, on the assumption that the file is growing.
 *  option 'r' means inlines in reverse order from end
 *    (for -r, default is entire buffer)
 *  OLD STYLE
 *     tail -[number][b|c|l|r][f] [file]
 *     tail +[number][b|c|l|r][f] [file]
 *  - means n lines before end
 *  + means nth line from beginning
 *  type 'b' means tail n blocks, not lines
 *  type 'c' means tail n characters(bytes?)
 *  type 'l' means tail n lines
 *  type 'r' means in lines in reverse order from end
 *    (for -r, default is entire buffer)
 *  option 'f' means loop endlessly trying to read more bytes after the
 *  end of file, on the assumption that the file is growing.
 *
 * Sun-traditional tail command
 *     tail where [file]
 *     where is [+|-]n[type]
 *     - means n lines before end
 *     + means nth line from beginning
 *     type 'b' means tail n blocks, not lines
 *     type 'c' means tail n bytes
 *     type 'r' means in lines in reverse order from end
 *      (for -r, default is entire buffer)
 *     option 'f' means loop endlessly trying to read more
 *             characters after the end of file, on the assumption
 *             that the file is growing
 *
 * Comments regarding possible (future) cleanup:
 *
 * XXX:	In some places, the input file is accessed through the decsriptor
 *	variable "fd"; in others, it's accessed through the constant "0".  The
 *	way the descriptor is named should be made uniform throughout.
 *
 * XXX:	Too many global variables!
 */
#include	<stdio.h>
#include	<ctype.h>
#include	<locale.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<limits.h>			/* for :INE_MAX and PATH_MAX */
#include	<string.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<sys/signal.h>
#include	<sys/types.h>
#include	<sys/sysmacros.h>
#include	<sys/stat.h>
#include	<sys/statvfs.h>
#include	<sys/param.h>	/* for DEV_BSHIFT and DEV_BSIZE */
#include	<sys/mman.h>
#include	<sys/uio.h>

/*
 * Amount of file buffered when it's not possible to mmap it or determine its
 * size in advance; the value reflects a somewhat arbitrary tradeoff between
 * memory consumption and providing a big enough window into the file to
 * accommodate the command line arguments without truncation (it could
 * reasonably be doubled or quadrupled).
 */
#ifdef XPG4
#define	LBIN	(LINE_MAX * 10)
#else /* !XPG4 */
#define	LBIN	65537		/* 64K + 1 for trailing NUL */
#endif /* XPG4 */

#ifdef XPG4
#define	NO_FLAG	0
#define	FLAG_C	1
#define	FLAG_N	2
#endif /* XPG4 */

char			staticbin[LBIN];	/* just in case malloc fails */
struct stat		statb;
struct statvfs	vstatb;
off_t				fcount;		/* size of the input file */
char			*mapp;		/* mmap'ped pointer to the input file */
char			*bin;

/* amount to read from file at a crack */
u_long		blocksize = DEV_BSIZE;	/* fallback value */

int		follow;		/* -f flag */
int		piped;
int		bkwds;		/* -r flag  */
int		istty;		/* flag set for terminal device, e.g., stdin */
				/* avoid lseeking on terminal devices, */
				/* bugid 4204623 */

#ifdef XPG4
int		fromend, frombegin;
int		bylines;
off_t		num;
#endif /* XPG4 */

static char		*readbackline(char *, char *);
static void		readfromstart(int, off_t);
static void		readfromend(int, off_t);
static void		fexit(void);
static void		docopy(void);
static void		usage(void);
#ifdef XPG4
static void		setnum(char *, int);
#endif /* XPG4 */


main(int argc, char **argv)
{
	off_t	i, j, k;
	int	partial, lastnl, filefound;
	off_t	fsize = (off_t)SSIZE_MAX;
	char	*p, *arg;
	char	filename[PATH_MAX];
#ifdef XPG4
	int	cflag, nflag, bflag;
	char	c;
#else /* !XPG4 */
	off_t	num;
	int	fromend, frombegin;
	int	bylines;
#endif /* XPG4 */

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) lseek(0, (off_t)0, SEEK_CUR);
	piped = (errno == ESPIPE);
	num = -1;
	bylines = -1;
	follow = 0;
	bkwds = 0;
	istty = 0;
	fromend = 1;
	frombegin = 0;
	filefound = 0;

#ifdef XPG4
	if (argc == 1) {			/* just 'tail' */
		num = 10;
		bylines = 1;
		filefound = 0;
		goto file;
	}

	arg = *(argv + 1);
	if (*arg != '-' && *arg != '+') { /* 'tail filename' */
		if (argc > 2) {
			usage();
		} else {
			num = 10;
			bylines = 1;
			(void) strcpy(filename, arg);
			filefound = 1;
			goto file;
		}
	}

	cflag = 0;
	nflag = 0;
	bflag = 0;

	while (--argc > 0) {
		arg = *++argv;
		switch (*arg) {
		case '+':
			fromend = 0;
			frombegin = 1;
			if (nflag == 1) {
				setnum(arg, FLAG_N);
				nflag = 2;
			} else if (cflag == 1) {
				setnum(arg, FLAG_C);
				cflag = 2;
			} else {
				num = 10;
				setnum(arg, NO_FLAG);
			}
			break;
		case '-':
nextarg:
			c = *(arg + 1);
			if (c == 'f') {
				if (bkwds == 1) {
					usage();
				}
				follow = 1;
				if (cflag == 1) { /* OLD STYLE: 'tail -cf' */
					bylines = 0;
					num = 10;
					cflag = 2;
				}
			} else if (c == 'r') {
				if (follow == 1) {
					usage();
				}
				if (!fromend && num != -1)
					num++;
				if (bylines == 0) {
					usage();
				}
				if (cflag == 1) { /* 'tail -cr' */
					usage();
				}
				bkwds = 1;
				fromend = 1;
				bylines = 1;
			} else if (c == 'c') {
				if (!cflag && !nflag && !bflag) {
					cflag = 1;
				} else {
					usage();
				}
			} else if (c == 'n') {
				if (!cflag && !nflag && !bflag) {
					nflag = 1;
				} else {
					usage();
				}
			} else if (c == 'b') {
				if (!cflag && !nflag && !bflag) {
					bflag = 1;
				} else {
					usage();
				}
			} else if ((c == '-') && (*(arg + 2) == '\0')) {
				arg = *++argv;
				argc--;
				goto fileset;
			} else {
				frombegin = 0;
				fromend = 1;
				if (nflag == 1) {
					setnum(arg, FLAG_N);
					nflag = 2;
				} else if (cflag == 1) {
					setnum(arg, FLAG_C);
					cflag = 2;
				} else {
					setnum(arg, NO_FLAG);
				}
				break;
			}

			if (cflag || bflag) {
				if (bkwds) {
					usage();
				}
			}
			if (*(arg + 2) != '\0') {
				*(arg + 1) = '-';
				arg++;
				goto nextarg;
			}
			break;
		default:
fileset:
			if (nflag == 1) {
				fromend = 1;
				frombegin = 0;
				setnum(arg, FLAG_N);
				nflag = 2;
			} else if (cflag == 1) {
				fromend = 1;
				frombegin = 0;
				setnum(arg, FLAG_C);
				cflag = 2;
			} else {
				if (argc == 0) {
					filefound = 0;
				} else if (argc > 1) {
					usage();
				} else {
					(void) strcpy(filename, arg);
					filefound = 1;
				}
			}
			break;
		}
	}
#else /* !XPG4 */
	arg = *(argv + 1);
	if (argc <= 1 || *arg != '-' && *arg != '+') {
		arg = "-10l";
		argc++;
		argv--;
	}
	fromend = (*arg == '-');
	frombegin = (*arg == '+');
	arg++;
	if (isdigit((int)*arg)) {
		num = 0;
		while (isdigit((int)*arg)) {
			num = num * 10 + *arg++ - '0';
		}
	} else if (frombegin) {		/* option was '+' without an integer */
		num = 10;
	} else {
		num = -1;
	}
	if (argc > 2) {
		(void) strcpy(filename, *(argv + 2));
		filefound = 1;
	} else {
		filefound = 0;
	}
	bylines = -1;
	bkwds = 0;
	follow = 0;
	while (*arg) {
		switch (*arg++) {
		case 'b':
			if (num == -1) {
				num = 10;
			}
			num <<= DEV_BSHIFT;
			if (bylines != -1 || bkwds == 1) {
				usage();
			}
			bylines = 0;
			break;
		case 'c':
			if (bylines != -1 || bkwds == 1) {
				usage();
			}
			bylines = 0;
			break;
		case 'f':
			if (bkwds == 1) {
				usage();
			}
			follow = 1;
			break;
		case 'r':
			if (follow == 1) {
				usage();
			}
			if (!fromend && num != -1)
				num++;
			if (bylines == 0) {
				usage();
			}
			bkwds = 1;
			fromend = 1;
			bylines = 1;
			break;
		case 'l':
			if (bylines != -1 && bylines == 1) {
				usage();
			}
			bylines = 1;
			break;
		default:
			usage();
			break;
		}
	}

#endif /* XPG4 */

file:
	if (filefound) {
		int		fd;

		(void) close(0);	/* fd 0 will be used for next open */
		piped = 0;
		if ((fd = open(filename, 0)) == -1) { /* fd must be 0 */
			(void) fprintf(stderr,
			    gettext("tail: cannot open input\n"));
			exit(2);
		}
		if (fstat(fd, &statb) == -1) {
			(void) fprintf(stderr, gettext(
			    "tail: cannot determine length of %s\n"), filename);
			exit(2);
		}
		fcount = statb.st_size;
		fsize = fcount + 1;
		/* Get optimal read chunk size. */
		if (fstatvfs(fd, &vstatb) == 0 && vstatb.f_bsize != 0) {
			blocksize = vstatb.f_bsize;
		}
		if (!follow && fcount <= SIZE_MAX) {
			if ((mapp = (char *)mmap((caddr_t)NULL,
			    (size_t)fcount, PROT_READ,
			    MAP_PRIVATE, fd, (off_t)0)) == MAP_FAILED) {
				/*
				 * Fall back to use read instead of mmap.
				 */
				mapp = NULL;
			}
		}
	}

	if (bkwds && num == -1)
		num = fsize;

	istty = isatty(0);

	if (follow || (mapp == NULL &&
		((fsize > (off_t)SSIZE_MAX) ||
		((bin = (char *)malloc((size_t)fsize)) == NULL)))) {
		bin = staticbin;
		if (fsize > (off_t)LBIN)
			fsize = (off_t)LBIN;
	}

	if (!fromend && num > 0) {
		num--;
	}
	if (num == -1) {
		num = 10;
	}
	if (bylines == -1) {
		bylines = 1;
	}
	if (bkwds) {
		follow = 0;
	}
	if (fromend) {
		if (mapp) {
			readfromend(bylines, num);
			/* NOTREACHED */
		}
		goto keep;
	}

	/*
	 * seek from beginning
	 */
	if (mapp) {
		readfromstart(bylines, num);
		/* NOTREACHED */
	}

	if (bylines) {
		/*
		 * Read and discard num lines from the beginning of the file,
		 * and then transcribe the residual amount read.
		 */
		j = 0;
		while (num-- > 0) {
			do {
				if (j-- <= 0) {
					p = bin;
					j = read(0, p, blocksize);
					if (j-- <= 0) {
						fexit();
					}
				}
			} while (*p++ != '\n');
		}
		(void) write(1, p, j);
	} else if (num > 0) {
		/*
		 * See whether it's possible to seek directly to the desired
		 * position; if not, read up to that point (discarding the
		 * data).
		 */
		if (!piped) {
			(void) fstat(0, &statb);
		}
		if (piped || (statb.st_mode & S_IFMT) == S_IFCHR) {
			while (num > 0) {
				i = MIN(num, blocksize);
				i = read(0, bin, i);
				if (i <= 0) {
					fexit();
				}
				num -= i;
			}
		} else {
			(void) lseek(0, (off_t)num, SEEK_SET);
		}
	}
	docopy();
	/* NOTREACHED */

	/* seek from end */
keep:
	if (num <= 0) {
		fexit();
	}
	if (!piped) {
		/*
		 * Go to offset from end of file
		 */
		(void) fstat(0, &statb);
		if (!bylines) {
			if (!istty) {
				if (num < statb.st_size)
					(void) lseek(0, (off_t)-num, SEEK_END);
				else
					(void) lseek(0, (off_t)0, SEEK_SET);
			}
		/*
		 * If it's not necessary to find line boundaries, it suffices
		 * to transcribe the rest of the file.
		 */
			docopy();
		} else {
			/*
			 * Seek back the size of the buffer we'll be
			 * reading into to find the line boundaries
			 * at the end of the file.
			 */
			if (!istty)
				(void) lseek(0, (off_t)-(fsize - 1), SEEK_END);
		}
	}
	/*
	 * Suck up data until there's no more available, wrapping circularly
	 * through the buffer.  Set partial to indicate whether or not it was
	 * possible to fill the buffer.
	 */
	partial = 1;
	for (;;) {
		i = 0;
		do {
			j = read(0, &bin[i], fsize - i);
			if (j <= 0) {
				goto brka;
			}
			i += j;
		} while (i < fsize);
		partial = 0;
	}
brka:
	/*
	 * i now is the offset of the "oldest" data in the (circular) buffer.
	 *
	 * Set k to the offset of the beginning of the tail data within the
	 * circular buffer.
	 */
	if (!bylines) {
		k =
			num <= i ? i - num :
			partial ? 0 :
			num >= fsize ? i + 1 :
			i - num + fsize;
		k--;
	} else {
		/*
		 * Scan the buffer for the indicated number of line breaks; j
		 * counts the number found so far.
		 */
		if (bkwds && bin[i == 0 ? fsize - 1 : i - 1] != '\n') {
			/*
			 * Force a trailing newline, resetting the boundary
			 * between the two halves of the buffer if necessary.
			 */
			bin[i] = '\n';
			if (++i >= fsize) {
				i = 0;
				partial = 0;
			}
		}
		k = i;
		j = 0;
		do {
			lastnl = k;
			do {
				if (--k < 0) {
					if (partial) {
						if (bkwds) {
							(void) write(1, bin,
								lastnl + 1);
						}
						goto brkb;
					}
					k = fsize - 1;
				}
			} while (bin[k] != '\n' && k != i);
			if (bkwds && j > 0) {
				if (k < lastnl) {
					(void) write(1, &bin[k + 1],
					    lastnl - k);
				} else {
					(void) write(1, &bin[k + 1],
					    fsize - k - 1);
					(void) write(1, bin, lastnl + 1);
				}
			}
		} while (j++ < num && k != i);
brkb:
		if (bkwds) {
			exit(0);
		}
		if (k == i) {
			do {
				if (++k >= fsize)
					k = 0;
			} while (bin[k] != '\n' && k != i);
		}
	}
	if (k < i) {
		(void) write(1, &bin[k + 1], i - k - 1);
	} else {
		(void) write(1, &bin[k + 1], fsize - k - 1);
		(void) write(1, bin, i);
	}
	fexit();
}

/*
 * Not used when input file is mmap'ped.
 */
static void
fexit()
{
	register int    n;
	if (!follow || piped) {
		exit(0);
	}
	for (;;) {
		(void) sleep(1);
		while ((n = read(0, bin, blocksize)) > 0) {
			(void) write(1, bin, n);
		}
	}
}

/*
 * Not used when input file is mmap'ped.
 */
static void
docopy()
{
	int bytes;

	while ((bytes = read(0, bin, blocksize)) > 0) {
		(void) write(1, bin, bytes);
	}
	fexit();
	/* NOT REACHED */
}

/*
 * Used only when the input file is mmap'ped.
 *
 * Skips over nunits (lines or characters depending on "bylines")
 * of the input file from the beginning and writes out
 * the remainder.
 */
static void
readfromstart(int bylines, off_t nunits)
{
	if (bylines) {
		for (; nunits > 0 && fcount > 0; fcount--) {
			if (*mapp++ == '\n') {
				nunits--;
			}
		}
	} else {
		/*
		 * skip nunits characters
		 */
		mapp += nunits;
		fcount -= nunits;
	}
	if (fcount > 0) {
		(void) write(1, mapp, fcount);
	}
	exit(0);
}

/*
 * Used only when the input file is mmap'ped.
 *
 * Writes out nunits (depending on bylines) of the input file counting
 * from the end.
 */
static void
readfromend(int bylines, off_t nunits)
{
	off_t	cnt;
	off_t	end, start, mark;

	if (fcount <= 0 || nunits <= 0) {
		exit(0);
	}

	start = (off_t)mapp;	/* save start of file */
	mapp += fcount;		/* move mapp to the end */
	end = (off_t)mapp;	/* mark the end */

	if (bylines) {
		while (nunits-- > 0) {		/* for each line */
			mark = (off_t)mapp--;	/* set mark & move back one */
			mapp = readbackline(mapp, (char *)start);
			cnt = mark - (off_t)mapp;
			if (cnt == 0) {		/* done? */
				break;
			}
			if (bkwds) {
				(void) write(1, mapp, cnt);
				end = (off_t)mapp;
				continue;
			}
		}
	} else {
		/*
		 * bump pointer up to end - nunits.
		 */
		if ((end - start) < nunits)
			mapp = (char *)start;
		else
			mapp = (char *)(end - nunits);
	}

	cnt = end - (off_t)mapp;
	if (cnt > 0) {
		(void) write(1, mapp, cnt);
	}
	exit(0);
}

/*
 * Reads a line backward (or up to limit) and returns the pointer to the
 * beginning of the line.
 */
static char *
readbackline(char *mapp, char *limit)
{
	if (mapp <= limit) {
		return (limit);
	}

	while (*--mapp != '\n') {
		if (mapp == limit) {
			return (limit);
		}
	}

	return (mapp + 1);
}

static void
usage()
{
#ifdef XPG4
	(void) fprintf(stderr,
	    gettext("usage: tail [-f|-r] [-c number | -n number] [file]\n"
		    "       tail [+/-[number][lbc][f]] [file]\n"
		    "       tail [+/-[number][l][r|f]] [file]\n"));
#else /* !XPG4 */
	(void) fprintf(stderr, gettext("usage: tail [+/-[n][lbc][f]] [file]\n"
				"       tail [+/-[n][l][r|f]] [file]\n"));
#endif /* XPG4 */
	exit(2);
}

#ifdef XPG4
static void
setnum(char *arg, int flags)
{

	if ((*arg == '-') || (*arg == '+')) {
		arg++;
	}
	if (flags != NO_FLAG) {		/* '-c num' or '-n num' */

		num = 0;
		while (*arg) {
			if (!isdigit((int)*arg)) {
				/* 'num' must be a decimal integer */
				usage();
			} else {
				num = num * 10 + *arg++ - '0';
			}
		}
		if (flags == FLAG_N) {
			bylines = 1;
		} else {
			bylines = 0;
		}
	} else {
		/* '-[num][bcfrl]' or '+[num][bcfrl]' */
		if (isdigit((int)*arg)) {
			num = 0;
			while (isdigit((int)*arg)) {
				num = num * 10 + *arg++ - '0';
			}
		}
		bylines = -1;
		while (*arg) {
			switch (*arg++) {
			case 'l':
				if (bylines != -1) {
					usage();
				}
				bylines = 1;
				break;
			case 'b':
				if (bylines != -1) {
					usage();
				}
				bylines = 0;
				if (num == -1) {
					num = 10;
				}
				num <<= DEV_BSHIFT;
				break;
			case 'c':
				if (bylines != -1) {
					usage();
				}
				bylines = 0;
				break;
			case 'f':
				if (bkwds == 1) {
					usage();
				}
				follow = 1;
				break;
			case 'r':
				if (follow == 1) {
					usage();
				}
				if (num != -1 && !fromend) {
					num++;
				}
				if (bylines == 0) {
					usage();
				}
				bkwds = 1;
				fromend = 1;
				bylines = 1;
				break;
			default:
				usage();
				break;
			}
		}
	}
}	
#endif /* XPG4 */
