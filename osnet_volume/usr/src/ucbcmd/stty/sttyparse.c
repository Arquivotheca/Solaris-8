/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sttyparse.c	1.6	97/02/19 SMI"	/* SVr4.0 1.2	*/

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termio.h>
#include <sys/stermio.h>
#include <sys/termiox.h>
#include "stty.h"

static char	*s_arg;			/* s_arg: ptr to mode to be set */
static int	match;
static int gct(), eq(), encode();

/* set terminal modes for supplied options */
char *
sttyparse(argc, argv, term, ocb, cb, termiox, winsize)
int	argc;
char	*argv[];
int	term; /* type of tty device, -1 means allow all options, 
	       * no sanity check 
	       */
struct	termio	*ocb;
struct	termios	*cb;
struct	termiox	*termiox;
struct	winsize	*winsize;
{
	register i;
	extern	const struct	speeds	speeds[];
	extern	const struct	mds	lmodes[];
	extern	const struct	mds	nlmodes[];
	extern	const struct	mds	cmodes[];
	extern	const struct	mds	ncmodes[];
	extern	const struct	mds	imodes[];
	extern	const struct	mds	nimodes[];
	extern	const struct	mds	omodes[];
	extern	const struct	mds	hmodes[];
	extern	const struct	mds	clkmodes[];

	while(--argc > 0) {

		s_arg = *++argv;
		match = 0;
		if ((term & ASYNC) || term == -1) {
			if (eqarg("erase", argc) && --argc)
				cb->c_cc[VERASE] = gct(*++argv, term);
			else if (eqarg("intr", argc) && --argc)
				cb->c_cc[VINTR] = gct(*++argv, term);
			else if (eqarg("quit", argc) && --argc)
				cb->c_cc[VQUIT] = gct(*++argv, term);
			else if (eqarg("eof", argc) && --argc)
				cb->c_cc[VEOF] = gct(*++argv, term);
			else if (eqarg("min", argc) && --argc)
				cb->c_cc[VMIN] = atoi(*++argv);
			else if (eqarg("eol", argc) && --argc)
				cb->c_cc[VEOL] = gct(*++argv, term);
			else if (eqarg("brk", argc) && --argc)
				cb->c_cc[VEOL] = gct(*++argv, term);
			else if (eqarg("eol2", argc) && --argc)
				cb->c_cc[VEOL2] = gct(*++argv, term);
			else if (eqarg("time", argc) && --argc)
				cb->c_cc[VTIME] = atoi(*++argv);
			else if (eqarg("kill", argc) && --argc)
				cb->c_cc[VKILL] = gct(*++argv, term);
			else if (eqarg("swtch", argc) && --argc)
				cb->c_cc[VSWTCH] = gct(*++argv, term);
			if(match)
				continue;
			if((term & TERMIOS) || term == -1) {
				if (eqarg("start", argc) && --argc)
					cb->c_cc[VSTART] = gct(*++argv, term);
				else if (eqarg("stop", argc) && --argc)
					cb->c_cc[VSTOP] = gct(*++argv, term);
				else if (eqarg("susp", argc) && --argc)
					cb->c_cc[VSUSP] = gct(*++argv, term);
				else if (eqarg("dsusp", argc) && --argc)
					cb->c_cc[VDSUSP] = gct(*++argv, term);
				else if (eqarg("rprnt", argc) && --argc)
					cb->c_cc[VREPRINT] = gct(*++argv, term);
				else if (eqarg("flush", argc) && --argc)
					cb->c_cc[VDISCARD] = gct(*++argv, term);
				else if (eqarg("werase", argc) && --argc)
					cb->c_cc[VWERASE] = gct(*++argv, term);
				else if (eqarg("lnext", argc) && --argc)
					cb->c_cc[VLNEXT] = gct(*++argv, term);
			}
			if(match)
				continue;
			if (eq("ek")) {
				cb->c_cc[VERASE] = CERASE;
				cb->c_cc[VKILL] = CKILL;
			}
			else if (eq("crt") || eq("newcrt")) {
				cb->c_lflag &= ~ECHOPRT;
				cb->c_lflag |= ECHOE|ECHOCTL;
				if (cfgetospeed(cb) >= B1200)
					cb->c_lflag |= ECHOKE;
			}
			else if (eq("dec")) {
				cb->c_cc[VERASE] = 0177;
				cb->c_cc[VKILL] = CTRL('u');
				cb->c_cc[VINTR] = CTRL('c');
				cb->c_lflag &= ~ECHOPRT;
				cb->c_lflag |= ECHOE|ECHOCTL|IEXTEN;
				if (cfgetospeed(cb) >= B1200)
					cb->c_lflag |= ECHOKE;
			}
			else if (eqarg("line", argc) && (!(term & TERMIOS) || term == -1) && --argc) {
				ocb->c_line = atoi(*++argv);
				continue;
			}
			else if (eq("raw") || eq("cbreak")) {
				cb->c_cc[VMIN] = 1;
				cb->c_cc[VTIME] = 0;
			}
			else if (eq("-raw") || eq("-cbreak") || eq("cooked")) {
				cb->c_cc[VEOF] = CEOF;
				cb->c_cc[VEOL] = CNUL;
			}
			else if(eq("sane")) {
				cb->c_cc[VERASE] = CERASE;
				cb->c_cc[VKILL] = CKILL;
				cb->c_cc[VQUIT] = CQUIT;
				cb->c_cc[VINTR] = CINTR;
				cb->c_cc[VEOF] = CEOF;
				cb->c_cc[VEOL] = CNUL;
							   /* SWTCH purposely not set */
			}
			else if((term & TERMIOS) && eqarg("ospeed", argc) && --argc) { 
				s_arg = *++argv;
				match = 0;
				for(i=0; speeds[i].string; i++)
					if(eq(speeds[i].string))
					    cfsetospeed(cb, speeds[i].speed);
				if(!match)
					return s_arg;
				continue;
			}
			else if((term & TERMIOS) && eqarg("ispeed", argc) && --argc) { 
				s_arg = *++argv;
				match = 0;
				for(i=0; speeds[i].string; i++)
					if(eq(speeds[i].string))
					    cfsetispeed(cb, speeds[i].speed);
				if(!match)
					return s_arg;
				continue;
			}
			else if (argc == 0) {
				(void) fprintf(stderr, "stty: No argument for \"%s\"\n", s_arg);
				exit(1);
			}
			for(i=0; speeds[i].string; i++)
				if(eq(speeds[i].string)) {
					cfsetospeed(cb, B0);
					cfsetispeed(cb, B0);
					cfsetospeed(cb, speeds[i].speed);
				}
		}
		if ((!(term & ASYNC) || term == -1) && eqarg("ctab", argc) && --argc) {
			cb->c_cc[7] = gct(*++argv, term);
			continue;
		}
		else if (argc == 0) {
			(void) fprintf(stderr, "stty: No argument for \"%s\"\n", s_arg);
			exit(1);
		}
			
		for(i=0; imodes[i].string; i++)
			if(eq(imodes[i].string)) {
				cb->c_iflag &= ~imodes[i].reset;
				cb->c_iflag |= imodes[i].set;
			}
		if((term & TERMIOS) || term == -1) {
			for(i=0; nimodes[i].string; i++)
				if(eq(nimodes[i].string)) {
					cb->c_iflag &= ~nimodes[i].reset;
					cb->c_iflag |= nimodes[i].set;
				}
		}

		for(i=0; omodes[i].string; i++)
			if(eq(omodes[i].string)) {
				cb->c_oflag &= ~omodes[i].reset;
				cb->c_oflag |= omodes[i].set;
			}
		if((!(term & ASYNC) || term == -1) && eq("sane")) {
			cb->c_oflag |= TAB3;
			continue;
		}
		for(i=0; cmodes[i].string; i++)
			if(eq(cmodes[i].string)) {
				cb->c_cflag &= ~cmodes[i].reset;
				cb->c_cflag |= cmodes[i].set;
			}
		if((term & TERMIOS) || term == -1)
			for(i=0; ncmodes[i].string; i++)
				if(eq(ncmodes[i].string)) {
					cb->c_cflag &= ~ncmodes[i].reset;
					cb->c_cflag |= ncmodes[i].set;
				}
		for(i=0; lmodes[i].string; i++)
			if(eq(lmodes[i].string)) {
				cb->c_lflag &= ~lmodes[i].reset;
				cb->c_lflag |= lmodes[i].set;
			}
		if((term & TERMIOS) || term == -1)
			for(i=0; nlmodes[i].string; i++)
				if(eq(nlmodes[i].string)) {
					cb->c_lflag &= ~nlmodes[i].reset;
					cb->c_lflag |= nlmodes[i].set;
				}
		if((term & FLOW) || term == -1) {
			for(i=0; hmodes[i].string; i++)
				if(eq(hmodes[i].string)) {
					termiox->x_hflag &= ~hmodes[i].reset;
					termiox->x_hflag |= hmodes[i].set;
				}
			for(i=0; clkmodes[i].string; i++)
				if(eq(clkmodes[i].string)) {
					termiox->x_cflag &= ~clkmodes[i].reset;
					termiox->x_cflag |= clkmodes[i].set;
				}
			
		}
		if(eqarg("rows", argc) && --argc)
			winsize->ws_row = atoi(*++argv);
		else if((eqarg("columns", argc) || eqarg("cols", argc)) && --argc)
			winsize->ws_col = atoi(*++argv);
		else if(eqarg("xpixels", argc) && --argc)
			winsize->ws_xpixel = atoi(*++argv);
		else if(eqarg("ypixels", argc) && --argc)
			winsize->ws_ypixel = atoi(*++argv);
		else if (argc == 0) {
			(void) fprintf(stderr, "stty: No argument for \"%s\"\n", s_arg);
			exit(1);
		}
		if(!match)
			if(!encode(cb, term)) {
				return(s_arg); /* parsing failed */
			}
	}
	return((char *)0);
}

static int eq(string)
char *string;
{
	register i;

	if(!s_arg)
		return(0);
	i = 0;
loop:
	if(s_arg[i] != string[i])
		return(0);
	if(s_arg[i++] != '\0')
		goto loop;
	match++;
	return(1);
}

/* Checks for options that require an argument */
static int
eqarg(string, argc)
char *string;
int argc;
{
	int status;

	if ((status = eq(string)) == 1) {
		if (argc <= 1) {
			(void) fprintf(stderr, "stty: No argument for \"%s\"\n",
					 	s_arg);
			exit(1);
		}
	}
	return(status);
}

/* get pseudo control characters from terminal */
/* and convert to internal representation      */
static int gct(cp, term)
register char *cp;
int term;
{
	register c;

	c = *cp++;
	if (c == '^') {
		c = *cp;
		if (c == '?')
			c = 0177;		/* map '^?' to DEL */
		else if (c == '-')
			c = (term & TERMIOS) ? _POSIX_VDISABLE : 0200;		/* map '^-' to undefined */
		else
			c &= 037;
	}
	return(c);
}

/* get modes of tty device and fill in applicable structures */
int 
get_ttymode(fd, termio, termios, stermio, termiox, winsize)
int fd;
struct termio *termio;
struct termios *termios;
struct stio *stermio;
struct termiox *termiox;
struct winsize *winsize;
{
	int i;
	int term = 0;
	if(ioctl(fd, STGET, stermio) == -1) {
		term |= ASYNC;
		if(ioctl(fd, TCGETS, termios) == -1) {
			if(ioctl(fd, TCGETA, termio) == -1) 
				return -1;
			termios->c_lflag = termio->c_lflag;
			termios->c_oflag = termio->c_oflag;
			termios->c_iflag = termio->c_iflag;
			termios->c_cflag = termio->c_cflag;
			for(i = 0; i < NCC; i++)
				termios->c_cc[i] = termio->c_cc[i];
		} else
			term |= TERMIOS;
	}
	else {
		termios->c_cc[7] = (unsigned)stermio->tab;
		termios->c_lflag = stermio->lmode;
		termios->c_oflag = stermio->omode;
		termios->c_iflag = stermio->imode;
	}
	
	if(ioctl(fd, TCGETX, termiox) == 0)
		term |= FLOW;

	if(ioctl(fd, TIOCGWINSZ, winsize) == 0)
		term |= WINDOW;
	return term;
}

/* set tty modes */
int 
set_ttymode(fd, term, termio, termios, stermio, termiox, winsize, owinsize)
int fd, term;
struct termio *termio;
struct termios *termios;
struct stio *stermio;
struct termiox *termiox;
struct winsize *winsize, *owinsize;
{
	int i;
	if (term & ASYNC) {
		if(term & TERMIOS) {
			if(ioctl(fd, TCSETSW, termios) == -1)
				return -1;
		} else {
			termio->c_lflag = termios->c_lflag;
			termio->c_oflag = termios->c_oflag;
			termio->c_iflag = termios->c_iflag;
			termio->c_cflag = termios->c_cflag;
			for(i = 0; i < NCC; i++)
				termio->c_cc[i] = termios->c_cc[i];
			if(ioctl(fd, TCSETAW, termio) == -1)
				return -1;
		}
			
	} else {
		stermio->imode = termios->c_iflag;
		stermio->omode = termios->c_oflag;
		stermio->lmode = termios->c_lflag;
		stermio->tab = termios->c_cc[7];
		if (ioctl(fd, STSET, stermio) == -1)
			return -1;
	}
	if(term & FLOW) {
		if(ioctl(fd, TCSETXW, termiox) == -1)
			return -1;
	}
	if((owinsize->ws_col != winsize->ws_col  
	   || owinsize->ws_row != winsize->ws_row) 
	   && ioctl(0, TIOCSWINSZ, winsize) != 0)
		return -1;
	return 0;
}

static int encode(cb, term)
struct	termios	*cb;
int term;
{
	unsigned long grab[20], i;
	int last;
	i = sscanf(s_arg, 
	"%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx",
	&grab[0],&grab[1],&grab[2],&grab[3],&grab[4],&grab[5],&grab[6],
	&grab[7],&grab[8],&grab[9],&grab[10],&grab[11],
	&grab[12], &grab[13], &grab[14], &grab[15], 
	&grab[16], &grab[17], &grab[18], &grab[19]);

	if((term & TERMIOS) && i < 20 && term != -1 || i < 12) 
		return(0);
	cb->c_iflag = grab[0];
	cb->c_oflag = grab[1];
	cb->c_cflag = grab[2];
	cb->c_lflag = grab[3];

	if(term & TERMIOS)
		last = NCCS - 1;
	else
		last = NCC;
	for(i=0; i<last; i++)
		cb->c_cc[i] = (unsigned char) grab[i+4];
	return(1);
}

