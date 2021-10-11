/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)expand.c	1.11	95/05/09 SMI"	/* SVr4.0 1.18.3.1	*/
/*
 *	UNIX shell
 *
 */

#include	"defs.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<dirent.h>



/*
 * globals (file name generation)
 *
 * "*" in params matches r.e ".*"
 * "?" in params matches r.e. "."
 * "[...]" in params matches character class
 * "[...a-z...]" in params matches a through z.
 *
 */
static int	addg();

expand(as, rcnt)
	unsigned char	*as;
{
	int	count;
	DIR	*dirf;
	BOOL	dir = 0;
	unsigned char	*rescan = 0;
	unsigned char 	*slashsav = 0;
	register unsigned char	*s, *cs;
	unsigned char *s2 = 0;
	struct argnod	*schain = gchain;
	BOOL	slash;
	int	len;
	wchar_t	wc;

	if (trapnote & SIGSET)
		return (0);
	s = cs = as;
	/*
	 * check for meta chars
	 */
	{
		register BOOL open;

		slash = 0;
		open = 0;
		do
		{
			if ((len = mbtowc(&wc, (char *)cs, MB_LEN_MAX)) <= 0) {
				len = 1;
				wc = (unsigned char)*cs;
			}

			cs += len;
			switch (wc) {
			case 0:
				if (rcnt && slash)
					break;
				else
					return (0);

			case '/':
				slash++;
				open = 0;
				continue;

			case '[':
				open++;
				continue;

			case ']':
				if (open == 0)
					continue;

			case '?':
			case '*':
				if (rcnt > slash)
					continue;
				else
					cs--;
				break;

			case '\\':
				cs++;
			default:
				continue;
			}
			break;
		} while (TRUE);
	}

	for (;;)
	{
		if (cs == s)
		{
			s = (unsigned char *)nullstr;
			break;
		} else if (*--cs == '/')
		{
			*cs = 0;
			if (s == cs)
				s = (unsigned char *)"/";
			else {
				/*
				 * push trimmed copy of directory prefix
				 * onto stack
				 */
				s2 = cpystak(s);
				trim(s2);
				s = s2;
			}
			break;
		}
	}

	if ((dirf = opendir(*s ? (char *)s : (char *)".")) != 0)
		dir++;

	/* Let s point to original string because it will be trimmed later */
	if (s2)
		s = as;
	count = 0;
	if (*cs == 0)
		slashsav = cs++; /* remember where first slash in as is */

	/* check for rescan */
	if (dir)
	{
		register unsigned char *rs;
		struct dirent *e;

		rs = cs;
		do /* find next / in as */
		{
			if (*rs == '/')
			{
				rescan = rs;
				*rs = 0;
				gchain = 0;
			}
		} while (*rs++);

		while ((e = readdir(dirf)) && (trapnote & SIGSET) == 0)
		{
			if (e->d_name[0] == '.' && *cs != '.')
				continue;

			if (gmatch(e->d_name, cs))
			{
				addg(s, e->d_name, rescan, slashsav);
				count++;
			}
		}
		(void) closedir(dirf);

		if (rescan)
		{
			register struct argnod	*rchain;

			rchain = gchain;
			gchain = schain;
			if (count)
			{
				count = 0;
				while (rchain)
				{
					count += expand(rchain->argval,
							slash + 1);
					rchain = rchain->argnxt;
				}
			}
			*rescan = '/';
		}
	}

	if (slashsav)
		*slashsav = '/';
	return (count);
}

static int
addg(as1, as2, as3, as4)
unsigned char	*as1, *as2, *as3, *as4;
{
	register unsigned char	*s1, *s2;
	register int	c;
	int		len;
	wchar_t		wc;

	s2 = locstak() + BYTESPERWORD;
	s1 = as1;
	if (as4) {
		while (c = *s1++)
		{
			if (s2 >= brkend)
				growstak(s2);
			*s2++ = c;
		}
		/*
		 * Restore first slash before the first metacharacter
		 * if as1 is not "/"
		 */
		if (as4 + 1 == s1) {
			if (s2 >= brkend)
				growstak(s2);
			*s2++ = '/';
		}
	}
/* add matched entries, plus extra \\ to escape \\'s */
	s1 = as2;
	for (;;)
	{
		if ((len = mbtowc(&wc, (char *)s1, MB_LEN_MAX)) <= 0) {
			len = 1;
			wc = (unsigned char)*s1;
		}
		if (s2 >= brkend)
			growstak(s2);

		if (wc == 0) {
			*s2 = *s1++;
			break;
		}

		if (wc == '\\') {
			*s2++ = '\\';
			if (s2 >= brkend)
				growstak(s2);
			*s2++ = '\\';
			s1++;
			continue;
		}
		if ((s2 + len) >= brkend)
			growstak(s2 + len);
		memcpy(s2, s1, len);
		s2 += len;
		s1 += len;
	}
	if (s1 = as3)
	{
		if (s2 >= brkend)
			growstak(s2);
		*s2++ = '/';
		do
		{
			if (s2 >= brkend)
				growstak(s2);
		}
		while (*s2++ = *++s1);
	}
	makearg(endstak(s2));
}

makearg(args)
	register struct argnod *args;
{
	args->argnxt = gchain;
	gchain = args;
}
