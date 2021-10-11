/*
 * Copyright (c) 1984,1985,1989,1994,1995,1996,1999  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Operating system dependent routines.
 *
 * Most of the stuff in here is based on Unix, but an attempt
 * has been made to make things work on other operating systems.
 * This will sometimes result in a loss of functionality, unless
 * someone rewrites code specifically for the new operating system.
 *
 * The makefile provides defines to decide whether various
 * Unix features are present.
 */

#include "less.h"
#include <signal.h>
#include <setjmp.h>
#if HAVE_TIME_H
#include <time.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_VALUES_H
#include <values.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_TIME_T
#define time_type	time_t
#else
#define	time_type	long
#endif

/*
 * BSD setjmp() saves (and longjmp() restores) the signal mask.
 * This costs a system call or two per setjmp(), so if possible we clear the
 * signal mask with sigsetmask(), and use _setjmp()/_longjmp() instead.
 * On other systems, setjmp() doesn't affect the signal mask and so
 * _setjmp() does not exist; we just use setjmp().
 */
#if HAVE__SETJMP && HAVE_SIGSETMASK
#define SET_JUMP	_setjmp
#define LONG_JUMP	_longjmp
#else
#define SET_JUMP	setjmp
#define LONG_JUMP	longjmp
#endif

public int reading;

static jmp_buf read_label;

extern int sigs;

/*
 * Like read() system call, but is deliberately interruptible.
 * A call to intread() from a signal handler will interrupt
 * any pending iread().
 */
	public int
iread(fd, buf, len)
	int fd;
	char *buf;
	unsigned int len;
{
	register int n;

#if MSDOS_COMPILER==WIN32C
	if (ABORT_SIGS())
		return (READ_INTR);
#else
#if MSDOS_COMPILER && MSDOS_COMPILER != DJGPPC
	if (kbhit())
	{
		int c;
		
		c = getch();
		if (c == '\003')
			return (READ_INTR);
		ungetch(c);
	}
#endif
#endif
	if (SET_JUMP(read_label))
	{
		/*
		 * We jumped here from intread.
		 */
		reading = 0;
#if HAVE_SIGSETMASK
		sigsetmask(0);
#else
#ifdef _OSK
		sigmask(~0);
#endif
#endif
		return (READ_INTR);
	}

	flush();
	reading = 1;
#if MSDOS_COMPILER==DJGPPC
	if (isatty(fd))
	{
		/*
		 * Don't try reading from a TTY until a character is
		 * available, because that makes some background programs
		 * believe DOS is busy in a way that prevents those
		 * programs from working while "less" waits.
		 */
		fd_set readfds;

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		if (select(fd+1, &readfds, 0, 0, 0) == -1)
			return (-1);
	}
#endif
	n = read(fd, buf, len);
#if 1
	/*
	 * This is a kludge to workaround a problem on some systems
	 * where terminating a remote tty connection causes read() to
	 * start returning 0 forever, instead of -1.
	 */
	{
		extern int ignore_eoi;
		if (!ignore_eoi)
		{
			static int consecutive_nulls = 0;
			if (n == 0)
				consecutive_nulls++;
			else
				consecutive_nulls = 0;
			if (consecutive_nulls > 20)
				quit(QUIT_ERROR);
		}
	}
#endif
	reading = 0;
	if (n < 0)
		return (-1);
	return (n);
}

/*
 * Interrupt a pending iread().
 */
	public void
intread()
{
	LONG_JUMP(read_label, 1);
}

/*
 * Return the current time.
 */
#if HAVE_TIME
	public long
get_time()
{
	time_type t;

	time(&t);
	return (t);
}
#endif


#if !HAVE_STRERROR
/*
 * Local version of strerror, if not available from the system.
 */
	static char *
strerror(err)
	int err;
{
#if HAVE_SYS_ERRLIST
	static char buf[16];
	extern char *sys_errlist[];
	extern int sys_nerr;
  
	if (err < sys_nerr)
		return sys_errlist[err];
	sprintf(buf, "Error %d", err);
	return buf;
#else
	return ("cannot open");
#endif
}
#endif

/*
 * errno_message: Return an error message based on the value of "errno".
 */
	public char *
errno_message(filename)
	char *filename;
{
	register char *p;
	register char *m;
#if HAVE_ERRNO
#if MUST_DEFINE_ERRNO
	extern int errno;
#endif
	p = strerror(errno);
#else
	p = "cannot open";
#endif
	m = (char *) ecalloc(strlen(filename) + strlen(p) + 3, sizeof(char));
	sprintf(m, "%s: %s", filename, p);
	return (m);
}

/*
 * Return the largest possible number that can fit in a long.
 */
	static long
get_maxlong()
{
#ifdef LONG_MAX
	return (LONG_MAX);
#else
#ifdef MAXLONG
	return (MAXLONG);
#else
	long n, n2;

	/*
	 * Keep doubling n until we overflow.
	 * {{ This actually only returns the largest power of two that
	 *    can fit in a long, but percentage() doesn't really need
	 *    it any more accurate than that. }}
	 */
	n2 = 128;  /* Hopefully no maxlong is less than 128! */
	do {
		n = n2;
		n2 *= 2;
	} while (n2 / 2 == n);
	return (n);
#endif
#endif
}

/*
 * Return the ratio of two POSITIONS, as a percentage.
 * {{ Assumes a POSITION is a long int. }}
 */
	public int
percentage(num, den)
	POSITION num, den;
{
	if (num <= get_maxlong() / 100)
		return ((100 * num) / den);
	else
		return (num / (den / 100));
}

/*
 * Return the specified percentage of a POSITION.
 * {{ Assumes a POSITION is a long int. }}
 */
	public POSITION
percent_pos(pos, percent)
	POSITION pos;
	int percent;
{
	if (pos <= get_maxlong() / 100)
		return ((percent * pos) / 100);
	else
		return (percent * (pos / 100));
}

#ifdef _OSK_MWC32

/*
 * This implements an ANSI-style intercept setup for Microware C 3.2
 */
	public int 
os9_signal(type, handler)
	int type;
	RETSIGTYPE (*handler)();
{
	intercept(handler);
}

#include <sgstat.h>

	public int 
isatty(f)
	int f;
{
	struct sgbuf sgbuf;

	if (_gs_opt(f, &sgbuf) < 0)
		return -1;
	return (sgbuf.sg_class == 0);
}
	
#endif
