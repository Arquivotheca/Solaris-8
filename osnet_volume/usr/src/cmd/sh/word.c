/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)word.c	1.18	98/05/06 SMI"	/* SVr4.0 1.11.2.2	*/
/*
 * UNIX shell
 */

#include	"defs.h"
#include	"sym.h"
#include	<errno.h>
#include	<fcntl.h>

static 	int readb();

/* ========	character handling for command lines	======== */


word()
{
	register unsigned int	c, d, cc;
	struct argnod	*arg = (struct argnod *)locstak();
	register unsigned char	*argp = arg->argval;
	unsigned char	*oldargp;
	int		alpha = 1;
	unsigned char *pc;

	wdnum = 0;
	wdset = 0;

	while (1)
	{
		while (c = nextwc(), space(c))		/* skipc() */
			;

		if (c == COMCHAR)
		{
			while ((c = readwc()) != NL && c != EOF);
			peekc = c;
		}
		else
		{
			break;	/* out of comment - white space loop */
		}
	}
	if (!eofmeta(c))
	{
		do
		{
			if (c == LITERAL)
			{
				oldargp = argp;
				while ((c = readwc()) && c != LITERAL){
					/*
					 * quote each character within
					 * single quotes
					 */
					pc = readw(c);
					if (argp >= brkend)
						growstak(argp);
					*argp++='\\';
				/* Pick up rest of multibyte character */
					if (c == NL)
						chkpr();
					while (c = *pc++) {
						if (argp >= brkend)
							growstak(argp);
						*argp++ = (unsigned char)c;
					}
				}
				if (argp == oldargp) { /* null argument - '' */
				/*
				 * Word will be represented by quoted null
				 * in macro.c if necessary
				 */
					if (argp >= brkend)
						growstak(argp);
					*argp++ = '"';
					if (argp >= brkend)
						growstak(argp);
					*argp++ = '"';
				}
			}
			else
			{
				if (c == 0) {
					if (argp >= brkend)
						growstak(argp);
					*argp++ = 0;
				} else {
					pc = readw(c);
					while (*pc) {
						if (argp >= brkend)
							growstak(argp);
						*argp++ = *pc++;
					}
				}
				if (c == '\\') {
					if ((cc = readwc()) == 0) {
						if (argp >= brkend)
							growstak(argp);
						*argp++ = 0;
					} else {
						pc = readw(cc);
						while (*pc) {
							if (argp >= brkend)
								growstak(argp);
							*argp++ = *pc++;
						}
					}
				}
				if (c == '=')
					wdset |= alpha;
				if (!alphanum(c))
					alpha = 0;
				if (qotchar(c))
				{
					d = c;
					for (;;)
					{
						if ((c = nextwc()) == 0) {
							if (argp >= brkend)
								growstak(argp);
							*argp++ = 0;
						} else {
							pc = readw(c);
							while (*pc) {
								if (argp >= brkend)
									growstak(argp);
								*argp++ = *pc++;
							}
						}
						if (c == 0 || c == d)
							break;
						if (c == NL)
							chkpr();
						/*
						 * don't interpret quoted
						 * characters
						 */
						if (c == '\\') {
							if ((cc = readwc()) == 0) {
								if (argp >= brkend)
									growstak(argp);
								*argp++ = 0;
							} else {
								pc = readw(cc);
								while (*pc) {
									if (argp >= brkend)
										growstak(argp);
									*argp++ = *pc++;
								}
							}
						}
					}
				}
			}
		} while ((c = nextwc(), !eofmeta(c)));
		argp = endstak(argp);
		if (!letter(arg->argval[0]))
			wdset = 0;

		peekn = c | MARK;
		if (arg->argval[1] == 0 &&
		    (d = arg->argval[0], digit(d)) &&
		    (c == '>' || c == '<'))
		{
			word();
			wdnum = d - '0';
		}else{ /* check for reserved words */
			if (reserv == FALSE ||
			    (wdval = syslook(arg->argval,
					reserved, no_reserved)) == 0) {
				wdval = 0;
			}
			/* set arg for reserved words too */
			wdarg = arg;
		}
	}else if (dipchar(c)){
		if ((d = nextwc()) == c)
		{
			wdval = c | SYMREP;
			if (c == '<')
			{
				if ((d = nextwc()) == '-')
					wdnum |= IOSTRIP;
				else
					peekn = d | MARK;
			}
		}
		else
		{
			peekn = d | MARK;
			wdval = c;
		}
	}
	else
	{
		if ((wdval = c) == EOF)
			wdval = EOFSYM;
		if (iopend && eolchar(c))
		{
			struct ionod *tmp_iopend;
			tmp_iopend = iopend;
			iopend = 0;
			copy(tmp_iopend);
		}
	}
	reserv = FALSE;
	return (wdval);
}

unsigned int skipwc()
{
	register unsigned int c;

	while (c = nextwc(), space(c))
		;
	return (c);
}

unsigned int nextwc()
{
	register unsigned int	c, d;

retry:
	if ((d = readwc()) == ESCAPE) {
		if ((c = readwc()) == NL) {
			chkpr();
			goto retry;
		}
		peekc = c | MARK;
	}
	return (d);
}

unsigned char *readw(d)
wchar_t	d;
{
	static unsigned char c[MULTI_BYTE_MAX + 1];
	int length;
	wchar_t l;
	if (isascii(d)) {
		c[0] = d;
		c[1] = '\0';
		return (c);
	}

	length = wctomb((char *)c, d);
	if (length <= 0) {
		c[0] = (unsigned char)d;
		length = 1;
	}
	c[length] = '\0';
	return (c);
}

unsigned int
readwc()
{
	wchar_t		c;
	register int	len;
	register struct fileblk *f;
	int		rest;

	if (peekn)
	{
		c = peekn & 0x7fffffff;
		peekn = 0;
		return (c);
	}
	if (peekc)
	{
		c = peekc & 0x7fffffff;
		peekc = 0;
		return (c);
	}
	f = standin;

retry:
	if (rest = f->fend - f->fnxt) {
		if (*f->fnxt == 0) {
			f->fnxt++;
			f->nxtoff++;
			if (f->feval == 0)
				goto retry;	/* = c = readc(); */
			if (estabf(*f->feval++))
				c = EOF;
			else
				c = SPACE;
			if (flags & readpr && standin->fstak == 0)
				prc(c);
			if (c == NL)
				f->flin++;
			return (c);
		}

		if (isascii(c = (unsigned char)*f->fnxt)) {
			f->fnxt++;
			f->nxtoff++;
			if (flags & readpr && standin->fstak == 0)
				prc(c);
			if (c == NL)
				f->flin++;
			return (c);
		}
			
		if (rest < MB_LEN_MAX) {
			len = readb();
			f->fend = (f->fnxt = f->fbuf) + len;
			f->nxtoff = 0;
			f->endoff = len;
			rest = len;
		}

		if ((len = mbtowc(&c, (char *)f->fnxt, rest)) <= 0) {
			c = (unsigned char)*f->fnxt;
			len = 1;
		}
		f->fnxt += len;
		f->nxtoff += len;
		if (flags & readpr && standin->fstak == 0)
			prwc(c);
		if (c == NL)
			f->flin++;
		return (c);
	}

	if (f->feof || f->fdes < 0){
		c = EOF;
		f->feof++;
		return (c);
	}

	if ((len = readb()) <= 0){
		if (f->fdes != input || !isatty(input)) {
			close(f->fdes);
			f->fdes = -1;
		}
		f->feof++;
		c = EOF;
		return (c);
	}
	f->fend = (f->fnxt = f->fbuf) + len;
	f->nxtoff = 0;
	f->endoff = len;
	goto retry;
}

static
readb()
{
	register struct fileblk *f = standin;
	register int	len;
	int		rest, fflags;

	if (rest = f->fend - f->fnxt) {
		memcpy(f->fbuf, f->fnxt, rest);
		if (f->fbuf[rest - 1] == '\n')
			return(rest);
	}
			
retry:
	do
	{
		if (trapnote & SIGSET)
		{
			newline();
			sigchk();
		}else if ((trapnote & TRAPSET) && (rwait > 0)){
			newline();
			chktrap();
			clearup();
		}
	} while ((len = read(f->fdes, f->fbuf + rest, f->fsiz - rest)) < 0 && trapnote);
	/*
	 * if child sets O_NDELAY or O_NONBLOCK on stdin
	 * and exited then turn the modes off and retry
	 */
	if (len == 0) {
		if (((flags & intflg) ||
		    ((flags & oneflg) == 0 && isatty(input) &&
		    (flags & stdflg))) &&
		    ((fflags = fcntl(f->fdes, F_GETFL, 0)) & O_NDELAY)) {
			fflags &= ~O_NDELAY;
			fcntl(f->fdes, F_SETFL, fflags);
			goto retry;
		}
	} else if (len < 0) {
		if (errno == EAGAIN) {
			fflags = fcntl(f->fdes, F_GETFL, 0);
			fflags &= ~O_NONBLOCK;
			fcntl(f->fdes, F_SETFL, fflags);
			goto retry;
		}
		len = 0;
	}
	return (len + rest);
}
