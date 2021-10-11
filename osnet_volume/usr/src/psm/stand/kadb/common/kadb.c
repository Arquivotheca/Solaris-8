/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kadb.c	1.66	99/05/04 SMI"

/*
 * kadb - support glue for kadb
 */
#include <sys/types.h>
/*
 * These include files come from the adb source directory.
 */
#include "adb.h"
#include "ptrace.h"
#include "symtab.h"

#include <sys/pf.h>
#include <sys/debug/debugger.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/cpu.h>
#include <sys/fcntl.h>
#if !defined(i386) && !defined(__ia64)
#include <sys/comvec.h>
#include <sys/privregs.h>
#endif

/* Since we do not include libc anymore  */
#include <time.h>
#include <sys/types.h>
#include <ctype.h>
#include <tzfile.h>
#define	sbrk	_sbrk
#include "mallint.h"
void qsort();
static void qst();

#define	reset()	_longjmp(abort_jmp, 1)

union sunromvec *romp;		/* libprom.a wants one of these declared. */
short cputype;			/* one of these, too. */

extern int infile;
int eof;
char **environ;			/* hack- needed for libc setlocale call */

int wtflag = 2;			/* pretend we opened for r/w */
int debugging = 0;		/* patchable value */
int dottysync = 1;		/* call monitor's initgetkey() routine */
addr_t systrap;			/* address of kernel's trap() */

extern char *index();
static void doreset(void);
void error(char *);
extern int	_printf();	/* adb's printf */
extern void 	putchar();
#ifdef _LP64
extern int elf64mode;
#ifdef __sparcv9
extern int npf32;
extern struct pseudo_file pf32[];
#endif
#endif

extern int printf();
extern void *memset();

char myname_default[] = "kadb";	/* default name for debugger */
extern char *prompt;

#define	MAC_FD	0x10000

void
debuginit(int fd, Elf32_Ehdr *exec, Elf32_Phdr *phdr, char *name)
{
	char *s;

	s = index(name, ')');
	if (s)
		s++;
	else
		s = name;
	symfil = (char *)malloc(strlen(s) + 1);
	if (symfil)
		strcpy(symfil, s);
	else {
		(void) printf("malloc failed\n");
		symfil = "-";
	}
	corfil = "-";
	fsym = fd;		/* set fd for source file */
	fcor = -1;
	filhdr = *exec;		/* structure assignment */
	proghdr = phdr;
	(void) setsym();
	(void) setvar();
	pid = 0xface;		/* fake pid as the process is `alive' */

	(void) lookup_base("trap");	/* useful to know where this is */
	if (cursym)		/* won't find it if not debugging a kernel */
		systrap = (addr_t)cursym->s_value;
}

#ifdef _LP64
void
debuginit64(int fd, Elf64_Ehdr *exec, Elf64_Phdr *phdr, char *name)
{
	char *s;

	s = index(name, ')');
	if (s)
		s++;
	else
		s = name;
	symfil = (char *)malloc(strlen(s) + 1);
	if (symfil)
		strcpy(symfil, s);
	else {
		printf("malloc failed\n");
		symfil = "-";
	}
	corfil = "-";
	fsym = fd;		/* set fd for source file */
	fcor = -1;
	filhdr64 = *exec;
	proghdr64 = phdr;
	(void) setsym();
	(void) setvar();
	pid = 0xface;		/* fake pid as the process is `alive' */

	(void) lookup_base("trap");	/* useful to know where this is */
	if (cursym)		/* won't find it if not debugging a kernel */
		systrap = (addr_t)cursym->s_value;
}
#endif
static jmp_buf jb;
extern int cur_cpuid;	/* cpu currently running kadb */

void
debugcmd()
{
	void debugcmd_mainloop(void);

	/*
	 * The next block of code is for first time through only so
	 * that when we are first entered everything looks good.
	 */
	bpwait(PTRACE_CONT);
	abort_jmp = jb;

	if (executing)
		delbp();

	if (readreg(Reg_PC) != 0) {
		_printf("stopped at%16t");	/* use adb printf */
		print_dis(Reg_PC, 0);
	}
	executing = 0;
	(void) _setjmp(jb);
	debugcmd_mainloop();
}

void
debugcmd_mainloop(void)
{
	if (executing) {
		delbp();
		executing = 0;
	}
	for (;;) {
		killbuf();
		if (errflg) {
			printf("%s\n", errflg);
			errflg = 0;
		}
		if (interrupted) {
			interrupted = 0;
			lastcom = 0;
			printf("\n");
			(void) doreset();
			/* NOTREACHED */
		}
		if ((infile & MAC_FD) == 0)
#if !defined(sparc)
			printf("%s[%d]: ", prompt, cur_cpuid & 0xff);
#else
			printf("%s[%d]: ", prompt, cur_cpuid);
#endif
		lp = 0;
		(void) rdc();
		lp--;
		if (eof) {
#if defined(i386)
			eof = 0;
#endif
			if (infile) {
				iclose(-1, 0);
#if !defined(i386)
				eof = 0;
#endif
				reset();
				/* NOTREACHED */
			} else
				printf("eof?");
		}
		(void) command((char *)0, lastcom);
		if (lp && lastc != '\n')
			(void) error("newline expected");
	}
}

void
chkerr()
{
	if (errflg || interrupted)
		(void) error(errflg);
}

static void
doreset()
{
	iclose(0, 1);
	oclose();
	reset();
	/* NOTREACHED */
}

void
error(char *n)
{
	errflg = n;
	(void) doreset();
	/* NOTREACHED */
}


#define	NMACFILES	10	/* number of max open macro files */

struct open_file {
	struct pseudo_file *of_f;
	char *of_pos;
} filetab[NMACFILES];

static int
getfileslot()
{
	register struct open_file *fp;

	for (fp = filetab; fp < &filetab[NMACFILES]; fp++) {
		if (fp->of_f == NULL)
			return (fp - filetab);
	}
	return (-1);
}

/* ARGSUSED2 */
int
_open(path, flags, mode)
	const char *path;
	int flags, mode;
{
	register struct pseudo_file *pfp;
	register int fd;
	register char *name, *s;
	extern char *rindex();

	tryabort(1);
	if (flags != O_RDONLY) {
		errno = EROFS;
		return (-1);
	}
	/* find open file slot */
	if ((fd = getfileslot()) == -1) {
		errno = EMFILE;
		return (-1);
	}
	/*
	 * Scan ahead in the path past any directories
	 * and convert all '.' in file name to '_'.
	 */
	name = rindex(path, '/');
	if (name == NULL)
		name = (char *)path;
	else
		name++;
	while ((s = rindex(path, '.')) != NULL)
		*s = '_';
	/* try to find "file" in pseudo file list */
#ifdef __sparcv9
	if (!elf64mode) {
		db_printf(1, " searching for %name in pf32\n", name);
		for (pfp = pf32; pfp < &pf32[npf32]; pfp++) {
			if (strcmp(name, pfp->pf_name) == 0)
				break;
		}
		if (pfp >= &pf32[npf32]) {
			errno = ENOENT;
			return (-1);
		}
	} else {
#endif

	for (pfp = pf; pfp < &pf[npf]; pfp++) {
		if (strcmp(name, pfp->pf_name) == 0)
			break;
	}

	if (pfp >= &pf[npf]) {
		errno = ENOENT;
		return (-1);
	}
#ifdef __sparcv9
	}
#endif
	filetab[fd].of_f = pfp;
	filetab[fd].of_pos = pfp->pf_string;
	return (fd | MAC_FD);
}

_lseek(int d, off_t offset, int whence)
{
	register char *se;
	register int r;
	register struct pseudo_file *pfp;
	char *pos;

	tryabort(1);
	if (d & MAC_FD) {	/* pseudo file I/O for macro's */
		d &= ~MAC_FD;
		if ((pfp = filetab[d].of_f) == NULL) {
			r = -1;
			errno = EBADF;
			goto out;
		}
		se = pfp->pf_string + strlen(pfp->pf_string);
		switch (whence) {
		case 0:
			pos = pfp->pf_string + offset;
			break;
		case 1:
			pos = filetab[d].of_pos + offset;
			break;
		case 2:
			pos = se + offset;
			break;
		default:
			r = -1;
			errno = EINVAL;
			goto out;
		}
		if (pos < pfp->pf_string || pos > se) {
			r = -1;
			errno = EINVAL;
			goto out;
		}
		filetab[d].of_pos = pos;
		r = filetab[d].of_pos - pfp->pf_string;
	} else {
		r = lseek(d, offset, whence);
		if (r == -1)
			errno = EINVAL;
	}
out:
	return (r);
}

int
_read(int d, char *buf, int nbytes)
{
	static char line[LINEBUFSZ];
	static char *p;
	register struct open_file *ofp;
	register char *s, *t;
	register int r;

	if (d & MAC_FD) {
		d &= ~MAC_FD;
		ofp = &filetab[d];
		if (ofp->of_f == NULL || ofp->of_pos == NULL) {
			r = -1;
			errno = EBADF;
			goto out;
		}
		for (r = 0, t = buf, s = ofp->of_pos; *s && r < nbytes; r++) {
			if (*s == '\n')
				tryabort(1);
			*t++ = *s++;
		}
		ofp->of_pos = s;
	} else if (d != 0) {
		tryabort(1);
		r = read(d, buf, nbytes);
		if (r == -1)
			errno = EFAULT;
	} else {
#if defined(__ia64)
		/* workaround for compiler bug */
		prom_printf("", d != 0);
#endif
		/*
		 * Reading from stdin (keyboard).
		 * Call getsn() to read buffer (thus providing
		 * erase, kill, and interrupt functions), then
		 * return the characters as needed from buffer.
		 */
		r = 0;
		while (r < nbytes) {
			if (p == NULL)
				getsn(p = line, sizeof (line));
			else
				tryabort(1);
			if (*p == '\0') {
				buf[r++] = '\n';
				p = NULL;
				break;
			} else
				buf[r++] = *p++;
		}
	}
out:
	return (r);
}

int max_write = 20;

_write(int d, char *buf, int nbytes)
{
	register int r, e, sz;

	if (d & MAC_FD) {
		r = -1;
		errno = EBADF;
	} else {
		for (r = 0; r < nbytes; r += e) {
			sz = nbytes - r;
			if (sz > max_write)
				sz = max_write;
			trypause();
			(void) putchar(*(buf+r));
			trypause();
			e = 1;
			if (e == -1) {
				r = -1;
				errno = EFAULT;
				break;
			}
		}
	}
	return (r);
}

int
_close(int d)
{
	int r;

	tryabort(1);
	if (d & MAC_FD) {
		d &= ~MAC_FD;
		if (filetab[d].of_f == NULL) {
			r = -1;
		} else {
			filetab[d].of_f = NULL;
			filetab[d].of_pos = NULL;
		}
		return (0);
	} else {
		r = close(d);
	}
	if (r == -1)
		errno = EBADF;
	return (r);
}

int
creat(const char *path, mode_t mode)
{
	return (_open(path, O_RDWR | O_CREAT | O_TRUNC, (int)mode));
}

#define	NCOL	5
#define	SIZECOL	16

void
printmacros()
{
	register struct pseudo_file *pfp;
	register int j, i = 0;

	for (pfp = pf; pfp < &pf[npf]; pfp++) {
		_printf("%s", pfp->pf_name);
		if ((++i % NCOL) == 0)
			_printf("\n");
		else for (j = strlen(pfp->pf_name); j < SIZECOL; j++) {
			_printf(" ");
		}
	}
	_printf("\n");
}

/*
 * This routine is an attempt to resync the tty (to avoid getting
 * into repeat mode when it shouldn't be).  Because of a bug
 * in the sun2 PROM when dealing w/ certain devices, we skip
 * calling initgetkey() if dottysync is not set.
 */
void
ttysync()
{
}

/*
 * Stubs from ttycontrol.c
 */
void
newsubtty()
{
}

void
subtty()
{
}

void
adbtty()
{
}

/*
 *  Below are the routines that are stolen from libc
 *  so we don't have to link with libc anymore
 */

/*
 * Below is stolen from qsort.c from libc since we really ought not to
 * link libc in.
 */

/*
 * qsort.c:
 * Our own version of the system qsort routine which is faster by an average
 * of 25%, with lows and highs of 10% and 50%.
 * The THRESHold below is the insertion sort threshold, and has been adjusted
 * for records of size 48 bytes.
 * The MTHREShold is where we stop finding a better median.
 */

#define		THRESH		4		/* threshold for insertion */
#define		MTHRESH		6		/* threshold for median */

static  int		(*qcmp)();		/* the comparison routine */
static  int		qsz;			/* size of each record */
static  int		thresh;			/* THRESHold in chars */
static  int		mthresh;		/* MTHRESHold in chars */

/*
 * qsort:
 * First, set up some global parameters for qst to share.  Then, quicksort
 * with qst(), and then a cleanup insertion sort ourselves.  Sound simple?
 * It's not...
 */
void
qsort(char *base, int n, int size, int (*compar)())
{
	register char c, *i, *j, *lo, *hi;
	char *min, *max;

	if (n <= 1)
		return;
	qsz = size;
	qcmp = compar;
	thresh = qsz * THRESH;
	mthresh = qsz * MTHRESH;
	max = base + n * qsz;
	if (n >= THRESH) {
		qst(base, max);
		hi = base + thresh;
	} else {
		hi = max;
	}
	/*
	 * First put smallest element, which must be in the first THRESH, in
	 * the first position as a sentinel.  This is done just by searching
	 * the first THRESH elements (or the first n if n < THRESH), finding
	 * the min, and swapping it into the first position.
	 */
	j = lo = base;
	while ((lo += qsz) < hi)
		if (qcmp(j, lo) > 0)
			j = lo;
	if (j != base) {
		/* swap j into place */
		i = base, hi = base + qsz;
		while (i < hi) {
			c = *j;
			*j++ = *i;
			*i++ = c;
		}
	}
	/*
	 * With our sentinel in place, we now run the following hyper-fast
	 * insertion sort.  For each remaining element, min, from [1] to [n-1],
	 * set hi to the index of the element AFTER which this one goes.
	 * Then, do the standard insertion sort shift on a character at a time
	 * basis for each element in the frob.
	 */
	min = base;
	while ((hi = min += qsz) < max) {
		while (qcmp(hi -= qsz, min) > 0)
			/* void */;
		if ((hi += qsz) != min) {
			lo = min + qsz;
			while (--lo >= min) {
				c = *lo;
				for (i = j = lo; (j -= qsz) >= hi; i = j)
					*i = *j;
				*i = c;
			}
		}
	}
}

/*
 * qst:
 * Do a quicksort
 * First, find the median element, and put that one in the first place as the
 * discriminator.  (This "median" is just the median of the first, last and
 * middle elements).  (Using this median instead of the first element is a big
 * win).  Then, the usual partitioning/swapping, followed by moving the
 * discriminator into the right place.  Then, figure out the sizes of the two
 * partions, do the smaller one recursively and the larger one via a repeat of
 * this code.  Stopping when there are less than THRESH elements in a partition
 * and cleaning up with an insertion sort (in our caller) is a huge win.
 * All data swaps are done in-line, which is space-losing but time-saving.
 * (And there are only three places where this is done).
 */

static void
qst(char *base, char *max)
{
	register char c, *i, *j, *jj;
	register int ii;
	char *mid, *tmp;
	int lo, hi;

	/*
	 * At the top here, lo is the number of characters of elements in the
	 * current partition.  (Which should be max - base).
	 * Find the median of the first, last, and middle element and make
	 * that the middle element.  Set j to largest of first and middle.
	 * If max is larger than that guy, then it's that guy, else compare
	 * max with loser of first and take larger.  Things are set up to
	 * prefer the middle, then the first in case of ties.
	 */
	lo = max - base;		/* number of elements as chars */
	do {
		mid = i = base + qsz * ((lo / qsz) >> 1);
		if (lo >= mthresh) {
			j = (qcmp((jj = base), i) > 0 ? jj : i);
			if (qcmp(j, (tmp = max - qsz)) > 0) {
				/* switch to first loser */
				j = (j == jj ? i : jj);
				if (qcmp(j, tmp) < 0)
					j = tmp;
			}
			if (j != i) {
				ii = qsz;
				do {
					c = *i;
					*i++ = *j;
					*j++ = c;
				} while (--ii);
			}
		}
		/*
		 * Semi-standard quicksort partitioning/swapping
		 */
		i = base, j = max - qsz;
		/* CONSTCOND */
		while (1) {
			while (i < mid && qcmp(i, mid) <= 0)
				i += qsz;
			while (j > mid) {
				if (qcmp(mid, j) <= 0) {
					j -= qsz;
					continue;
				}
				tmp = i + qsz;	/* value of i after swap */
				if (i == mid) {
					/* j <-> mid, new mid is j */
					mid = jj = j;
				} else {
					/* i <-> j */
					jj = j;
					j -= qsz;
				}
				goto swap;
			}
			if (i == mid) {
				break;
			} else {
				/* i <-> mid, new mid is i */
				jj = mid;
				tmp = mid = i;	/* value of i after swap */
				j -= qsz;
			}
		swap:
			ii = qsz;
			do {
				c = *i;
				*i++ = *jj;
				*jj++ = c;
			} while (--ii);
			i = tmp;
		}
		/*
		 * Look at sizes of the two partitions, do the smaller
		 * one first by recursion, then do the larger one by
		 * making sure lo is its size, base and max are update
		 * correctly, and branching back.  But only repeat
		 * (recursively or by branching) if the partition is
		 * of at least size THRESH.
		 */
		i = (j = mid) + qsz;
		if ((lo = j - base) <= (hi = max - i)) {
			if (lo >= thresh)
				qst(base, j);
			base = i;
			lo = hi;
		} else {
			if (hi >= thresh)
				qst(i, max);
			max = j;
		}
	} while (lo >= thresh);
}

/*
 * This localtime is a modified version of offtime from libc, which does not
 * bother to figure out the time zone from the kernel, from environment
 * varaibles, or from Unix files.  We just return things in GMT format.
 */

static int mon_lengths[2][MONS_PER_YEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int year_lengths[2] = {
	DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

struct tm *
localtime(const time_t *clock)
{
	register struct tm *tmp;
	register long days;
	register long rem;
	register int y;
	register int yleap;
	register int *ip;
	static struct tm tm;

	tmp = &tm;
	days = *clock / SECS_PER_DAY;
	rem = *clock % SECS_PER_DAY;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}
	tmp->tm_hour = (int)(rem / SECS_PER_HOUR);
	rem = rem % SECS_PER_HOUR;
	tmp->tm_min = (int)(rem / SECS_PER_MIN);
	tmp->tm_sec = (int)(rem % SECS_PER_MIN);
	tmp->tm_wday = (int)((EPOCH_WDAY + days) % DAYS_PER_WEEK);
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYS_PER_WEEK;
	y = EPOCH_YEAR;
	if (days >= 0)
	for (;;) {
		yleap = isleap(y);
		if (days < (long)year_lengths[yleap])
			break;
		++y;
		days = days - (long)year_lengths[yleap];
	} else {
		do {
			--y;
			yleap = isleap(y);
			days = days + (long)year_lengths[yleap];
		} while (days < 0);
	}
	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int)days;
	ip = mon_lengths[yleap];
	for (tmp->tm_mon = 0; days >= (long)ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long)ip[tmp->tm_mon];
	tmp->tm_mday = (int)(days + 1);
	tmp->tm_isdst = 0;
	/*
	 * tm_zone and and tm_gmoff are not defined in the tm struct anymore.
	 * Find out what to do about this.
	 */
#ifdef notdef
	tmp->tm_zone = "GMT";
	tmp->tm_gmtoff = 0;
#endif
	return (tmp);
}

/*
 * So is ctime...
 */


/*
 * This routine converts time as follows.
 * The epoch is 0000 Jan 1 1970 GMT.
 * The argument time is in seconds since then.
 * The localtime(t) entry returns a pointer to an array
 * containing
 *  seconds (0-59)
 *  minutes (0-59)
 *  hours (0-23)
 *  day of month (1-31)
 *  month (0-11)
 *  year-1970
 *  weekday (0-6, Sun is 0)
 *  day of the year
 *  daylight savings flag
 *
 * The routine corrects for daylight saving
 * time and will work in any time zone provided
 * "timezone" is adjusted to the difference between
 * Greenwich and local standard time (measured in seconds).
 * In places like Michigan "daylight" must
 * be initialized to 0 to prevent the conversion
 * to daylight time.
 * There is a table which accounts for the peculiarities
 * undergone by daylight time in 1974-1975.
 *
 * The routine does not work
 * in Saudi Arabia which runs on Solar time.
 *
 * asctime(tvec)
 * where tvec is produced by localtime
 * returns a ptr to a character string
 * that has the ascii time in the form
 *	Thu Jan 01 00:00:00 1970\n\0
 *	01234567890123456789012345
 *	0	  1	    2
 *
 * ctime(t) just calls localtime, then asctime.
 *
 * tzset() looks for an environment variable named
 * TZ.
 * If the variable is present, it will set the external
 * variables "timezone", "altzone", "daylight", and "tzname"
 * appropriately. It is called by localtime, and
 * may also be called explicitly by the user.
 */



#define	dysize(A) (((A)%4)? 365: 366)
#define	CBUFSIZ 26

/*
 * POSIX.1c standard version of the function asctime_r.
 * User gets it via static asctime_r from the header file.
 */
char *
__posix_asctime_r(const struct tm *t, char *cbuf)
{
	register char *cp;
	register const char *ncp;
	register const int *tp;
	static char	*ct_numb();
	const char *Date = "Day Mon 00 00:00:00 1900\n";
	const char *Day  = "SunMonTueWedThuFriSat";
	const char *Month = "JanFebMarAprMayJunJulAugSepOctNovDec";

	cp = cbuf;
	for (ncp = Date; *cp++ = *ncp++; /* */);
	ncp = Day + (3*t->tm_wday);
	cp = cbuf;
	*cp++ = *ncp++;
	*cp++ = *ncp++;
	*cp++ = *ncp++;
	cp++;
	tp = &t->tm_mon;
	ncp = Month + ((*tp) * 3);
	*cp++ = *ncp++;
	*cp++ = *ncp++;
	*cp++ = *ncp++;
	cp = ct_numb(cp, *--tp);
	cp = ct_numb(cp, *--tp+100);
	cp = ct_numb(cp, *--tp+100);
	cp = ct_numb(cp, *--tp+100);
	if (t->tm_year >= 100) {
		cp[1] = '2';
		cp[2] = '0';
	}
	cp += 2;
	cp = ct_numb(cp, t->tm_year+100);
	return (cbuf);
}

/*
 * POSIX.1c Draft-6 version of the function asctime_r.
 * It was implemented by Solaris 2.3.
 */
char *
_asctime_r(const struct tm *t, char *cbuf, int buflen)
{
	if (buflen < CBUFSIZ) {
		errno = ERANGE;
		return (NULL);
	}
	return (__posix_asctime_r(t, cbuf));
}

char *
ctime(const time_t *t)
{
	return (asctime(localtime(t)));
}


char *
asctime(const struct tm *t)
{
	static char cbuf[CBUFSIZ];

	return (_asctime_r(t, cbuf, CBUFSIZ));
}


static char *
ct_numb(char *cp, int n)
{
	cp++;
	if (n >= 10)
		*cp++ = (n/10)%10 + '0';
	else
		*cp++ = ' ';		/* Pad with blanks */
	*cp++ = n%10 + '0';
	return (cp);
}



unsigned char __ctype[129] =
{
	0, /* EOF */
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_S|_C,	_S|_C,	_S|_C,	_S|_C,	_S|_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_S|_B,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_N|_X,	_N|_X,	_N|_X,	_N|_X,  _N|_X,	_N|_X,	_N|_X,	_N|_X,
	_N|_X,	_N|_X,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P,
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C,
};

char *
memcpy(char *s, char *s0, size_t n)
{
	if (n != 0) {
		register char *s1 = s;
		register  char *s2 = s0;
		do {
			*s1++ = *s2++;
		} while (--n != 0);
	}
	return (s);
}

void
perror(const char *error)
{
	/* Do nothing for now */
}

/* Below directly stolen from malloc.c */

/*
 * *********************************************************************
 *	Memory management: malloc(), realloc(), free().
 *
 *	The following #-parameters may be redefined:
 *	SEGMENTED: if defined, memory requests are assumed to be
 *		non-contiguous across calls of GETCORE's.
 *	GETCORE: a function to get more core memory. If not SEGMENTED,
 *		GETCORE(0) is assumed to return the next available
 *		address. Default is 'sbrk'.
 *	ERRCORE: the error code as returned by GETCORE.
 *		Default is ((char *)(-1)).
 *	CORESIZE: a desired unit (measured in bytes) to be used
 *		with GETCORE. Default is (1024*ALIGN).
 *
 *	This algorithm is based on a  best fit strategy with lists of
 *	free elts maintained in a self-adjusting binary tree. Each list
 *	contains all elts of the same size. The tree is ordered by size.
 *	For results on self-adjusting trees, see the paper:
 *		Self-Adjusting Binary Trees,
 *		DD Sleator & RE Tarjan, JACM 1985.
 *
 *	The header of a block contains the size of the data part in bytes.
 *	Since the size of a block is 0%4, the low two bits of the header
 *	are free and used as follows:
 *
 *		BIT0:	1 for busy (block is in use), 0 for free.
 *		BIT1:	if the block is busy, this bit is 1 if the
 *			preceding block in contiguous memory is free.
 *			Otherwise, it is always 0.
 * **********************************************************************
 */


static TREE	*Root,		/* root of the free tree */
		*Bottom,	/* the last free chunk in the arena */
		*_morecore();	/* function to get more core */

static char	*Baddr;		/* current high address of the arena */
static char	*Lfree;		/* last freed block with data intact */

static void	t_delete();
static void	t_splay();
static void	realfree();
static void	cleanfree();
static void	*_malloc_unlocked();
extern void	_free_unlocked();

#define	FREESIZE (1<<5) /* size for preserving free blocks until next malloc */
#define	FREEMASK FREESIZE - 1

static void *flist[FREESIZE];	/* list of blocks to be freed on next malloc */
static int freeidx;		/* index of free blocks in flist % FREESIZE */

/*
 *	Allocation of small blocks
 */

static TREE	*List[MINSIZE/WORDSIZE-1]; /* lists of small blocks */

static void *
_smalloc(size_t size)
{
	register TREE	*tp;
	register size_t	i;

	/* want to return a unique pointer on malloc(0) */
	if (size == 0)
		size = WORDSIZE;

	/* list to use */
	i = size/WORDSIZE - 1;

	if (List[i] == NULL) {
		register TREE *np;
		register int n;
		/* number of blocks to get at one time */
#define	NPS (WORDSIZE*8)

		/* get NPS of these block types */
		if ((List[i] = (TREE *)_malloc_unlocked(
					(size + WORDSIZE) * NPS)) == NULL)
			return (NULL);

		/* make them into a link list */
		for (n = 0, np = List[i]; n < NPS; ++n) {
			tp = np;
			SIZE(tp) = size;
			np = NEXT(tp);
			AFTER(tp) = np;
		}
		AFTER(tp) = NULL;
	}

	/* allocate from the head of the queue */
	tp = List[i];
	List[i] = AFTER(tp);
	SETBIT0(SIZE(tp));
	return (DATA(tp));
}


void *
malloc(size_t size)
{
	void *ret;
	/* _mutex_lock(&__malloc_lock); */
	ret = _malloc_unlocked(size);
	/* _mutex_unlock(&__malloc_lock); */
	return (ret);
}


void *
_malloc_unlocked(size_t size)
{
	register size_t	n;
	register TREE	*tp, *sp;
	register size_t	o_bit1;


	/* make sure that size is 0 mod ALIGN */
	ROUND(size);

	/* see if the last free block can be used */
	if (Lfree) {
		sp = BLOCK(Lfree);
		n = SIZE(sp);
		CLRBITS01(n);
		if (n == size) {	/* exact match, use it as is */
			freeidx = (freeidx + FREESIZE - 1) & FREEMASK;
								/* one back */
			flist[freeidx] = Lfree = NULL;
			return (DATA(sp));
		} else if (size >= MINSIZE && n > size) {
			/* got a big enough piece */
			freeidx = (freeidx + FREESIZE - 1) & FREEMASK;
								/* one back */
			flist[freeidx] = Lfree = NULL;
			o_bit1 = SIZE(sp) & BIT1;
			SIZE(sp) = n;
			goto leftover;
		}
	}
	o_bit1 = 0;

	/* perform free's of space since last malloc */
	cleanfree(NULL);

	/* small blocks */
	if (size < MINSIZE)
		return (_smalloc(size));

	/* search for an elt of the right size */
	sp = NULL;
	n  = 0;
	if (Root) {
		tp = Root;
		while (1) {
			/* branch left */
			if (SIZE(tp) >= size) {
				if (n == 0 || n >= SIZE(tp)) {
					sp = tp;
					n = SIZE(tp);
				}
				if (LEFT(tp))
					tp = LEFT(tp);
				else
					break;
			} else { /* branch right */
				if (RIGHT(tp))
					tp = RIGHT(tp);
				else
					break;
			}
		}

		if (sp)	{
			t_delete(sp);
		} else if (tp != Root) {
			/* make the searched-to element the root */
			t_splay(tp);
			Root = tp;
		}
	}

	/* if found none fitted in the tree */
	if (!sp) {
		if (Bottom && size <= SIZE(Bottom)) {
			sp = Bottom;
			CLRBITS01(SIZE(sp));
		} else if ((sp = _morecore(size)) == NULL) /* no more memory */
			return (NULL);
	}

	/* tell the forward neighbor that we're busy */
	CLRBIT1(SIZE(NEXT(sp)));



leftover:
	/* if the leftover is enough for a new free piece */
	if ((n = (SIZE(sp) - size)) >= MINSIZE + WORDSIZE) {
		n -= WORDSIZE;
		SIZE(sp) = size;
		tp = NEXT(sp);
		SIZE(tp) = n|BIT0;
		realfree(DATA(tp));
	} else if (BOTTOM(sp))
		Bottom = NULL;

	/* return the allocated space */
	SIZE(sp) |= BIT0 | o_bit1;
	return (DATA(sp));
}


/*
 *	realloc().
 *	If the block size is increasing, we try forward merging first.
 *	This is not best-fit but it avoids some data recopying.
 */
void *
realloc(void *old, size_t size)
{
	register TREE	*tp, *np;
	register size_t	ts;
	register char	*new;


	/* pointer to the block */
	/* _mutex_lock(&__malloc_lock); */
	if (old == NULL) {
		new = _malloc_unlocked(size);
		/* _mutex_unlock(&__malloc_lock); */
		return (new);
	}

	/* perform free's of space since last malloc */
	cleanfree(old);

	/* make sure that size is 0 mod ALIGN */
	ROUND(size);

	tp = BLOCK(old);
	ts = SIZE(tp);

	/* if the block was freed, data has been destroyed. */
	if (!ISBIT0(ts)) {
		/* _mutex_unlock(&__malloc_lock); */
		return (NULL);
	}

	/* nothing to do */
	CLRBITS01(SIZE(tp));
	if (size == SIZE(tp)) {
		SIZE(tp) = ts;
		/* _mutex_unlock(&__malloc_lock); */
		return (old);
	}

	/* special cases involving small blocks */
	if (size < MINSIZE || SIZE(tp) < MINSIZE)
		goto call_malloc;

	/* block is increasing in size, try merging the next block */
	if (size > SIZE(tp)) {
		np = NEXT(tp);
		if (!ISBIT0(SIZE(np))) {
			SIZE(tp) += SIZE(np)+WORDSIZE;
			if (np != Bottom)
				t_delete(np);
			else
				Bottom = NULL;
			CLRBIT1(SIZE(NEXT(np)));
		}

#ifndef SEGMENTED
		/* not enough & at TRUE end of memory, try extending core */
		if (size > SIZE(tp) && BOTTOM(tp) && GETCORE(0) == Baddr) {
			Bottom = tp;
			if ((tp = _morecore(size)) == NULL)
				tp = Bottom;
		}
#endif /* !SEGMENTED */
	}

	/* got enough space to use */
	if (size <= SIZE(tp)) {
		register size_t n;

chop_big:;
		if ((n = (SIZE(tp) - size)) >= MINSIZE + WORDSIZE) {
			n -= WORDSIZE;
			SIZE(tp) = size;
			np = NEXT(tp);
			SIZE(np) = n|BIT0;
			realfree(DATA(np));
		} else if (BOTTOM(tp))
			Bottom = NULL;

		/* the previous block may be free */
		SETOLD01(SIZE(tp), ts);
		/* _mutex_unlock(&__malloc_lock); */
		return (old);
	}

	/* call malloc to get a new block */
call_malloc:;
	SETOLD01(SIZE(tp), ts);
	if ((new = _malloc_unlocked(size)) != NULL) {
		CLRBITS01(ts);
		if (ts > size)
			ts = size;
		MEMCOPY(new, old, ts);
		_free_unlocked(old);
		/* _mutex_unlock(&__malloc_lock); */
		return (new);
	}

	/*
	 * Attempt special case recovery allocations since malloc() failed:
	 *
	 * 1. size <= SIZE(tp) < MINSIZE
	 *	Simply return the existing block
	 * 2. SIZE(tp) < size < MINSIZE
	 *	malloc() may have failed to allocate the chunk of
	 *	small blocks. Try asking for MINSIZE bytes.
	 * 3. size < MINSIZE <= SIZE(tp)
	 *	malloc() may have failed as with 2.  Change to
	 *	MINSIZE allocation which is taken from the beginning
	 *	of the current block.
	 * 4. MINSIZE <= SIZE(tp) < size
	 *	If the previous block is free and the combination of
	 *	these two blocks has at least size bytes, then merge
	 *	the two blocks copying the existing contents backwards.
	 */

	CLRBITS01(SIZE(tp));
	if (SIZE(tp) < MINSIZE) {
		if (size < SIZE(tp)) {		/* case 1. */
			SETOLD01(SIZE(tp), ts);
			/* _mutex_unlock(&__malloc_lock); */
			return (old);
		} else if (size < MINSIZE) {	/* case 2. */
			size = MINSIZE;
			goto call_malloc;
		}
	} else if (size < MINSIZE) {		/* case 3. */
		size = MINSIZE;
		goto chop_big;
	} else if (ISBIT1(ts) && (SIZE(np = LAST(tp)) + SIZE(tp) + WORDSIZE) >=
									size) {
		t_delete(np);
		SIZE(np) += SIZE(tp) + WORDSIZE;
		/*
		 * Since the copy may overlap, use memmove() if available.
		 * Otherwise, copy by hand.
		 */
		{
			register WORD *src = (WORD *)old;
			register WORD *dst = (WORD *)DATA(np);
			register size_t  n = SIZE(tp) / WORDSIZE;

			do {
				*dst++ = *src++;
			} while (--n > 0);
		}
		old = DATA(np);
		tp = np;
		CLRBIT1(ts);
		goto chop_big;
	}
	SETOLD01(SIZE(tp), ts);
	/* _mutex_unlock(&__malloc_lock); */
	return (NULL);
}



/*
 *	realfree().
 *	Coalescing of adjacent free blocks is done first.
 *	Then, the new free block is leaf-inserted into the free tree
 *	without splaying. This strategy does not guarantee the amortized
 *	O(nlogn) behaviour for the insert/delete/find set of operations
 *	on the tree. In practice, however, free is much more infrequent
 *	than malloc/realloc and the tree searches performed by these
 *	functions adequately keep the tree in balance.
 */
static void
realfree(void *old)
{
	register TREE	*tp, *sp, *np;
	register size_t	ts, size;


	/* pointer to the block */
	tp = BLOCK(old);
	ts = SIZE(tp);
	if (!ISBIT0(ts))
		return;
	CLRBITS01(SIZE(tp));

	/* small block, put it in the right linked list */
	if (SIZE(tp) < MINSIZE) {
		ts = SIZE(tp)/WORDSIZE - 1;
		AFTER(tp) = List[ts];
		List[ts] = tp;
		return;
	}

	/* see if coalescing with next block is warranted */
	np = NEXT(tp);
	if (!ISBIT0(SIZE(np))) {
		if (np != Bottom)
			t_delete(np);
		SIZE(tp) += SIZE(np)+WORDSIZE;
	}

	/* the same with the preceding block */
	if (ISBIT1(ts)) {
		np = LAST(tp);
		t_delete(np);
		SIZE(np) += SIZE(tp)+WORDSIZE;
		tp = np;
	}

	/* initialize tree info */
	PARENT(tp) = LEFT(tp) = RIGHT(tp) = LINKFOR(tp) = NULL;

	/* the last word of the block contains self's address */
	*(SELFP(tp)) = tp;

	/* set bottom block, or insert in the free tree */
	if (BOTTOM(tp))
		Bottom = tp;
	else {
		/* search for the place to insert */
		if (Root) {
			size = SIZE(tp);
			np = Root;
			while (1) {
				if (SIZE(np) > size) {
					if (LEFT(np))
						np = LEFT(np);
					else {
						LEFT(np) = tp;
						PARENT(tp) = np;
						break;
					}
				} else if (SIZE(np) < size) {
					if (RIGHT(np))
						np = RIGHT(np);
					else {
						RIGHT(np) = tp;
						PARENT(tp) = np;
						break;
					}
				} else {
					if ((sp = PARENT(np)) != NULL) {
						if (np == LEFT(sp))
							LEFT(sp) = tp;
						else
							RIGHT(sp) = tp;
						PARENT(tp) = sp;
					} else
						Root = tp;

					/* insert to head of list */
					if ((sp = LEFT(np)) != NULL)
						PARENT(sp) = tp;
					LEFT(tp) = sp;

					if ((sp = RIGHT(np)) != NULL)
						PARENT(sp) = tp;
					RIGHT(tp) = sp;

					/* doubly link list */
					LINKFOR(tp) = np;
					LINKBAK(np) = tp;
					SETNOTREE(np);

					break;
				}
			}
		} else
			Root = tp;
	}

	/* tell next block that this one is free */
	SETBIT1(SIZE(NEXT(tp)));
}


/*
 *	Get more core. Gaps in memory are noted as busy blocks.
 */
static TREE *
_morecore(size_t size)
{
	register TREE	*tp;
	register size_t	n, offset;
	register char	*addr;

	/* compute new amount of memory to get */
	tp = Bottom;
	n = size + 2*WORDSIZE;
	addr = GETCORE(0);

	if (addr == ERRCORE)
		return (NULL);

	/* need to pad size out so that addr is aligned */
	if ((((size_t)addr) % ALIGN) != 0)
		offset = ALIGN - (size_t)addr%ALIGN;
	else
		offset = 0;

#ifndef SEGMENTED
	/* if not segmented memory, what we need may be smaller */
	if (addr == Baddr) {
		n -= WORDSIZE;
		if (tp != NULL)
			n -= SIZE(tp);
	}
#endif /* !SEGMENTED */

	/* get a multiple of CORESIZE */
	n = ((n - 1)/CORESIZE + 1) * CORESIZE;
	if ((addr = GETCORE(n + offset)) == ERRCORE)
		return (NULL);

	/* contiguous memory */
	if (addr == Baddr) {
		if (tp) {
			addr = ((char *)tp);
			n += SIZE(tp) + 2*WORDSIZE;
		} else {
			addr = Baddr-WORDSIZE;
			n += WORDSIZE;
		}
	} else
		addr += offset;

	/* new bottom address */
	Baddr = addr + n;

	/* new bottom block */
	tp = ((TREE *) addr);
	SIZE(tp) = n - 2*WORDSIZE;

	/* reserved the last word to head any noncontiguous memory */
	SETBIT0(SIZE(NEXT(tp)));

	/* non-contiguous memory, free old bottom block */
	if (Bottom && Bottom != tp) {
		SETBIT0(SIZE(Bottom));
		realfree(DATA(Bottom));
	}

	return (tp);
}



/*
 *	Tree rotation functions (BU: bottom-up, TD: top-down)
 */

#define	LEFT1(x, y)	if ((RIGHT(x) = LEFT(y))) PARENT(RIGHT(x)) = x;\
			if ((PARENT(y) = PARENT(x)))\
				if (LEFT(PARENT(x)) == x) LEFT(PARENT(y)) = y;\
				else RIGHT(PARENT(y)) = y;\
			LEFT(y) = x; PARENT(x) = y

#define	RIGHT1(x, y)	if ((LEFT(x) = RIGHT(y))) PARENT(LEFT(x)) = x;\
			if ((PARENT(y) = PARENT(x)))\
				if (LEFT(PARENT(x)) == x) LEFT(PARENT(y)) = y;\
				else RIGHT(PARENT(y)) = y;\
			RIGHT(y) = x; PARENT(x) = y

#define	BULEFT2(x, y, z)	if ((RIGHT(x) = LEFT(y))) PARENT(RIGHT(x)) = x;\
			if ((RIGHT(y) = LEFT(z))) PARENT(RIGHT(y)) = y;\
			if ((PARENT(z) = PARENT(x)))\
				if (LEFT(PARENT(x)) == x) LEFT(PARENT(z)) = z;\
				else RIGHT(PARENT(z)) = z;\
			LEFT(z) = y; PARENT(y) = z; LEFT(y) = x; PARENT(x) = y

#define	BURIGHT2(x, y, z)	if ((LEFT(x) = RIGHT(y))) PARENT(LEFT(x)) = x;\
			if ((LEFT(y) = RIGHT(z))) PARENT(LEFT(y)) = y;\
			if ((PARENT(z) = PARENT(x)))\
				if (LEFT(PARENT(x)) == x) LEFT(PARENT(z)) = z;\
				else RIGHT(PARENT(z)) = z;\
			RIGHT(z) = y; PARENT(y) = z; RIGHT(y) = x; PARENT(x) = y

#define	TDLEFT2(x, y, z)	if ((RIGHT(y) = LEFT(z))) PARENT(RIGHT(y)) = y;\
			if ((PARENT(z) = PARENT(x)))\
				if (LEFT(PARENT(x)) == x) LEFT(PARENT(z)) = z;\
				else RIGHT(PARENT(z)) = z;\
			PARENT(x) = z; LEFT(z) = x;

#define	TDRIGHT2(x, y, z)	if ((LEFT(y) = RIGHT(z))) PARENT(LEFT(y)) = y;\
			if ((PARENT(z) = PARENT(x)))\
				if (LEFT(PARENT(x)) == x) LEFT(PARENT(z)) = z;\
				else RIGHT(PARENT(z)) = z;\
			PARENT(x) = z; RIGHT(z) = x;



/*
 *	Delete a tree element
 */
static void
t_delete(register TREE *op)
{
	register TREE	*tp, *sp, *gp;

	/* if this is a non-tree node */
	if (ISNOTREE(op)) {
		tp = LINKBAK(op);
		if ((sp = LINKFOR(op)) != NULL)
			LINKBAK(sp) = tp;
		LINKFOR(tp) = sp;
		return;
	}

	/* make op the root of the tree */
	if (PARENT(op))
		t_splay(op);

	/* if this is the start of a list */
	if ((tp = LINKFOR(op)) != NULL) {
		PARENT(tp) = NULL;
		if ((sp = LEFT(op)) != NULL)
			PARENT(sp) = tp;
		LEFT(tp) = sp;

		if ((sp = RIGHT(op)) != NULL)
			PARENT(sp) = tp;
		RIGHT(tp) = sp;

		Root = tp;
		return;
	}

	/* if op has a non-null left subtree */
	if ((tp = LEFT(op)) != NULL) {
		PARENT(tp) = NULL;

		if (RIGHT(op)) {
			/* make the right-end of the left subtree its root */
			while ((sp = RIGHT(tp)) != NULL) {
				if ((gp = RIGHT(sp)) != NULL) {
					TDLEFT2(tp, sp, gp);
					tp = gp;
				} else {
					LEFT1(tp, sp);
					tp = sp;
				}
			}

			/* hook the right subtree of op to the above elt */
			RIGHT(tp) = RIGHT(op);
			PARENT(RIGHT(tp)) = tp;
		}
	} else if ((tp = RIGHT(op)) != NULL) /* no left subtree */
		PARENT(tp) = NULL;

	Root = tp;
}


/*
 *	Bottom up splaying (simple version).
 *	The basic idea is to roughly cut in half the
 *	path from Root to tp and make tp the new root.
 */
static void
t_splay(TREE *tp)
{
	register  TREE	*pp, *gp;

	/* iterate until tp is the root */
	while ((pp = PARENT(tp)) != NULL) {
		/* grandparent of tp */
		gp = PARENT(pp);

		/* x is a left child */
		if (LEFT(pp) == tp) {
			if (gp && LEFT(gp) == pp) {
				BURIGHT2(gp, pp, tp);
			} else {
				RIGHT1(pp, tp);
			}
		} else {
			if (gp && RIGHT(gp) == pp) {
				BULEFT2(gp, pp, tp);
			} else {
				LEFT1(pp, tp);
			}
		}
	}
}



/*
 *	free().
 *	Performs a delayed free of the block pointed to
 *	by old. The pointer to old is saved on a list, flist,
 *	until the next malloc or realloc. At that time, all the
 *	blocks pointed to in flist are actually freed via
 *	realfree(). This allows the contents of free blocks to
 *	remain undisturbed until the next malloc or realloc.
 */

void
free(void *old)
{
	/* _mutex_lock(&__malloc_lock); */
	_free_unlocked(old);
	/* _mutex_unlock(&__malloc_lock); */
}


void
_free_unlocked(void *old)
{
	int	i;

	if (old == NULL)
		return;

	/*
	 * Make sure the same data block is not freed twice.
	 * 3 cases are checked.  It returns immediately if either
	 * one of the conditions is true.
	 *	1. Last freed.
	 *	2. Not in use or freed already.
	 *	3. In the free list.
	 */
	if (old == Lfree)
		return;
	if (!ISBIT0(SIZE(BLOCK(old))))
		return;
	for (i = 0; i < freeidx; i++)
		if (old == flist[i])
			return;

	if (flist[freeidx] != NULL)
		realfree(flist[freeidx]);
	flist[freeidx] = Lfree = old;
	freeidx = (freeidx + 1) & FREEMASK; /* one forward */
}



/*
 *	cleanfree() frees all the blocks pointed to be flist.
 *
 *	realloc() should work if it is called with a pointer
 *	to a block that was freed since the last call to malloc() or
 *	realloc(). If cleanfree() is called from realloc(), ptr
 *	is set to the old block and that block should not be
 *	freed since it is actually being reallocated.
 */
static void
cleanfree(void *ptr)
{
	register	 char	**flp;

	flp = (char **)&(flist[freeidx]);
	for (;;) {
		if (flp == (char **)&(flist[0]))
			flp = (char **)&(flist[FREESIZE]);
		if (*--flp == NULL)
			break;
		if (*flp != (char *)ptr)
			realfree(*flp);
		*flp = NULL;
	}
	freeidx = 0;
	Lfree = NULL;
}

void *
calloc(size_t num, size_t size)
{
	register void *mp;
	unsigned long total;

	if (num == 0 || size == 0)
		total = 0;
	else {
		total = (unsigned long) num * size;

		/* check for overflow */
		if (total / num != size)
			return (NULL);
	}
	return ((mp = malloc(total)) ? memset(mp, 0, total) : mp);
}

void *
memset(void *sp1, int c, size_t n)
{
	if (n != 0) {
		register unsigned char *sp = sp1;
		do {
			*sp++ = (unsigned char) c;
		} while (--n != 0);
	}
	return (sp1);
}

/*
 * Read a line into the given buffer and handles
 * erase (^H or DEL), kill (^U), and interrupt (^C) characters.
 */
void
getsn(char buf[], size_t bufsize)
{
	extern int getchar(void);
	extern void putchar(int);
	extern void dointr(int);
	register char *lp = buf;
	register int c;

	for (;;) {
		c = getchar() & 0177;
		switch (c)	{
		case '[':
		case ']':
			if (lp != buf)
				goto defchar;
			putchar('\n');
			*lp++ = c;
			/* FALLTHROUGH */
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
			lp--;
			if (lp < buf)
				lp = buf;
			continue;
		case 'u'&037:			/* ^U */
			lp = buf;
			putchar('^');
			putchar('U');
			putchar('\n');
			continue;
		case 'c'&037:
			dointr(1);
			/* MAYBE REACHED */
			/* fall through */
		default:
		defchar:
			if (lp < &buf[bufsize-1]) {
				*lp++ = c;
			} else {
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			break;
		}
	}
}

/*
 * Read a line into the given buffer using getsn().
 * This routine ASSUMES a maximum input line size of LINEBUFSZ
 * to guard against overflow of the buffer from obnoxious users.
 */
void
gets(char buf[])
{
	getsn(buf, (size_t)LINEBUFSZ);
}
