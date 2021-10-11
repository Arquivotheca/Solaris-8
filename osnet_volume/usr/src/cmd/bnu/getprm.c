/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getprm.c	1.5	95/03/01 SMI"	/* from SVR4 bnu:getprm.c 2.9 */

#include "uucp.h"

#define LQUOTE	'('
#define RQUOTE ')'

static char *bal();

/*
 * get next parameter from s
 *	s	-> string to scan
 *	whsp	-> pointer to use to return leading whitespace
 *	prm	-> pointer to use to return token
 * return:
 *	 s	-> pointer to next character 
 *		NULL at end
 */
char *
getprm(s, whsp, prm)
register char *s, *whsp, *prm;
{
	register char *c;
	char rightq;		/* the right quote character */
	char *beginning;
	wchar_t	ch;
	int	width;

	beginning = prm;

	while ((width = mbtowc(&ch, s, MB_CUR_MAX)) &&
	    iswspace(ch) || (ch == '\n')) {
		if (whsp != (char *) NULL)
			while (width--)
				*whsp++ = *s++;
		else
			s += width;
	}

	if ( whsp != (char *) NULL )
		*whsp = '\0';

	while ((width = mbtowc(&ch, s, MB_CUR_MAX)) && ch) {
		if (iswspace(ch) || ch == '\n' || ch == '\0') {
			*prm = '\0';
			return(prm == beginning ? NULL : s);
		}
		switch (ch) {
		case '>':
			if ((prm == beginning + 1) && (*beginning == '2'))
				*prm++ = *s++;
			if ((prm == beginning + 1) && (*beginning == '1'))
				*beginning = *s++;
			if (prm == beginning) {
				width = mbtowc(&ch, s+1, MB_CUR_MAX);
				if ((ch == '>') || (ch == '&'))
					*prm++ = *s++;
				*prm++ = *s++;
			}
			*prm = '\0';
			return(s);
			/* NOTREACHED */
			break;
		case '<':
			if ((prm == beginning + 1) && (*beginning == '0'))
				*beginning = *s++;
			if (prm == beginning) {
				width = mbtowc(&ch, s+1, MB_CUR_MAX);
				if (ch == '<') {
					*prm++ = *s++;
					*prm++ = *s++;
					*prm = '\0';
					return;
				}
				*prm++ = *s++;
			}
			/* FALLTHRU */
		case '|':
		case ';':
		case '&':
		case '^':
		case '\\':
			if (prm == beginning)
				*prm++ = *s++;
			*prm = '\0';
			return(s);
			/* NOTREACHED */
			break;
		case '\'':
		case '(':
		case '`':
		case '"':
			if (prm == beginning) {
				rightq = ( *s == '(' ? ')' : *s );
				c = bal(s, rightq);
				(void) strncpy(prm, s, c-s+1);
				prm += c - s + 1;
				if ( *(s=c) == rightq)
					s++;
			}
			*prm = '\0';
			return(s);
			/* NOTREACHED */
			break;
		default:
			while (width--)
				*prm++ = *s++;
		}
	}

	*prm = '\0';
	return(prm == beginning ? NULL : s);
}

/*
 * bal - get balanced quoted string
 *
 * s - input string
 * r - right quote
 * Note: *s is the left quote
 * return:
 *  pointer to the end of the quoted string
 * Note:
 *	If the string is not balanced, it returns a pointer to the
 *	end of the string.
 */

static char *
bal(s, r)
register char *s;
char r;
{
	int	width;
	wchar_t	ch;
	short count = 1;
	char l;		/* left quote character */

	for (l = *s++; *s; s+=width) {
	    width = mbtowc(&ch, s, MB_CUR_MAX);
	    if (*s == r) {
		if (--count == 0)
		    break;	/* this is the balanced end */
	    }
	    else if (*s == l)
		count++;
	}
	return(s);
}

/*
 * split - split the name into parts:
 *	arg  - original string
 *	sys  - leading system name
 *	fwd  - intermediate destinations, if not NULL, otherwise
 *		only split into two parts.
 *	file - filename part
 */

int
split(arg, sys, fwd, file)
char *arg, *sys, *fwd, *file;
{
    register wchar_t *cl, *cr, *n;
    int retval = 0;
    wchar_t	wcbuf[MAXFULLNAME];
    wchar_t	tmpbuf[MAXFULLNAME];
    wchar_t	myname[MAXFULLNAME];

    *sys = *file = NULLCHAR;
    if ( fwd != (char *) NULL )
	*fwd = NULLCHAR;

    /* uux can use parentheses for output file names */
    /* we'll check here until  we can move it to uux */
    if (EQUALS(Progname,"uux") && (*arg == LQUOTE)) {
	char *c;
	c = bal(arg++, RQUOTE);
	(void) strncpy(file, arg, c-arg);
	file[c-arg] = NULLCHAR;
	return(retval);
	}
		

    mbstowcs(myname, Myname, MAXFULLNAME);
    mbstowcs(wcbuf, arg, MAXFULLNAME);
    for (n=wcbuf ;; n=cl+1) {
	cl = wcschr(n, (wchar_t)'!');
	if (cl == NULL) {
	    /* no ! in n */
	    (void) wcstombs(file, n, MAXFULLNAME);
	    return(retval);
	}

	retval = 1;
	if (cl == n)	/* leading ! */
	    continue;
	if (WEQUALSN(myname, n, cl - n) && myname[cl - n] == NULLCHAR)
	    continue;

	(void) wcsncpy(tmpbuf, n, cl-n);
	tmpbuf[cl-n] = NULLCHAR;
	(void) wcstombs(sys, tmpbuf, MAXFULLNAME);

	if (fwd != (char *) NULL) {
	    if (cl != (cr = wcsrchr(n, (wchar_t)'!'))) {
		/*  more than one ! */
		wcsncpy(tmpbuf, cl+1, cr-cl-1);
		tmpbuf[cr-cl-1] = NULL;
		(void) wcstombs(fwd, tmpbuf, MAXFULLNAME);
	    }
	} else {
	    cr = cl;
	}

	(void) wcstombs(file, cr+1, MAXFULLNAME);
	return(retval);
    }
    /*NOTREACHED*/
}

