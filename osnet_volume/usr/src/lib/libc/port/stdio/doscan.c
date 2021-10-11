/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)doscan.c	1.57	99/06/16 SMI"

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include "mtlib.h"
#include "file64.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <thread.h>
#include <synch.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <limits.h>
#include <wchar.h>
#include <unistd.h>
#include "libc.h"
#include "stdiom.h"

#define	NCHARS	(1 << BITSPERBYTE)

/* if the _IOWRT flag is set, this must be a call from sscanf */
#define	locgetc(cnt)	(cnt += 1, (iop->_flag & _IOWRT) ? \
				((*iop->_ptr == '\0') ? EOF : *iop->_ptr++) : \
				GETC(iop))
#define	locungetc(cnt, x) (cnt -= 1, (x == EOF) ? EOF : \
				((iop->_flag & _IOWRT) ? *(--iop->_ptr) : \
				    (++iop->_cnt, *(--iop->_ptr))))

#define	wlocgetc()	((iop->_flag & _IOWRT) ? \
				((*iop->_ptr == '\0') ? EOF : *iop->_ptr++) : \
				GETC(iop))
#define	wlocungetc(x) ((x == EOF) ? EOF : \
				((iop->_flag & _IOWRT) ? *(--iop->_ptr) : \
				    UNGETC(x, iop)))

#define	MAXARGS	30	/* max. number of args for fast positional paramters */

/*
 * stva_list is used to subvert C's restriction that a variable with an
 * array type can not appear on the left hand side of an assignment operator.
 * By putting the array inside a structure, the functionality of assigning to
 * the whole array through a simple assignment is achieved..
 */
typedef struct stva_list {
	va_list	ap;
} stva_list;

static int number(int *, int *, int, int, int, int, FILE *, va_list *);
static int readchar(FILE *, int *);
static int string(int *, int *, int, int, int, char *, FILE *, va_list *);
static int wstring(int *, int *, int, int, int, FILE *, va_list *);
static int	wbrstring(int *, int *, int, int, int, FILE *,
	unsigned char *, va_list *);
static int	brstring(int *, int *, int, int, int, FILE *,
	unsigned char *, va_list *);
static int _bi_getwc(FILE *);
static int _bi_ungetwc(wint_t, FILE *);

#ifdef	_WIDE
static int _mkarglst(const wchar_t *, stva_list, stva_list[]);
static wint_t	_wd_getwc(int *, FILE *);
static wint_t	_wd_ungetwc(int *, wchar_t, FILE *);
static int	_watoi(wchar_t *);
#else  /* _WIDE */
static int _mkarglst(const char *, stva_list, stva_list[]);
#endif /* _WIDE */

#ifndef	_WIDE
int
_doscan(FILE *iop, const char *fmt, va_list va_Alist)
{

	int ret = 0;
	rmutex_t *lk;

	if (iop->_flag & _IOWRT)
		ret = __doscan_u(iop, fmt, va_Alist);
	else {
		FLOCKFILE(lk, iop);
		ret = __doscan_u(iop, fmt, va_Alist);
		FUNLOCKFILE(lk);
	}
	return (ret);
}
#endif /* _WIDE */

#ifdef	_WIDE
int
__wdoscan_u(FILE *iop, const wchar_t *fmt, va_list va_Alist)
#else  /* _WIDE */
int
__doscan_u(FILE *iop, const char *sfmt, va_list va_Alist)
#endif /* _WIDE */
{
#ifdef	_WIDE
	wchar_t	ch;
	wchar_t	inchar, size;
	int	nmatch = 0, len, stow;
#else  /* _WIDE */
	int	ch;
	int		nmatch = 0, len, inchar, stow, size;
#endif /* _WIDE */

	unsigned char	*bracket_str = NULL;
	int		chcount, flag_eof;
	char	tab[NCHARS];

	/* variables for postional parameters */
#ifdef	_WIDE
	const wchar_t	*sformat = fmt;	/* save the beginning of the format */
#else  /* _WIDE */
	const unsigned char	*fmt = (const unsigned char *)sfmt;
	const char	*sformat = sfmt; /* save the beginning of the format */
#endif /* _WIDE */
	int		fpos = 1;	/* 1 if first postional parameter */
	stva_list	args,	/* used to step through the argument list */
		sargs;	/* used to save the start of the argument list */
	stva_list	arglst[MAXARGS];
					/*
					 * array giving the appropriate values
					 * for va_arg() to retrieve the
					 * corresponding argument:
					 * arglst[0] is the first argument
					 * arglst[1] is the second argument,etc.
					 */
	/* Check if readable stream */
	if (!(iop->_flag & (_IOREAD | _IORW))) {
		errno = EBADF;
		return (EOF);
	}

	/*
	 * Initialize args and sargs to the start of the argument list.
	 * We don't know any portable way to copy an arbitrary C object
	 * so we use a system-specific routine(probably a macro) from
	 * stdarg.h.  (Remember that if va_list is an array, in_args will
	 * be a pointer and &in_args won't be what we would want for
	 * memcpy.)
	 */
	va_copy(args.ap, va_Alist);
	sargs = args;

	chcount = 0; flag_eof = 0;

	/*
	 ******************************************************
	 * Main loop: reads format to determine a pattern,
	 *		and then goes to read input stream
	 *		in attempt to match the pattern.
	 ******************************************************
	 */
	for (; ; ) {
		if ((ch = *fmt++) == '\0') {
			return (nmatch); /* end of format */
		}
#ifdef	_WIDE
		if (iswspace(ch)) {
			if (!flag_eof) {
				while (iswspace(inchar =
					    _wd_getwc(&chcount, iop)))
					;
				if (_wd_ungetwc(&chcount, inchar, iop) == WEOF)
					flag_eof = 1;
			}
			continue;
		}
		if (ch != '%' || (ch = *fmt++) == '%') {
			if ((inchar = _wd_getwc(&chcount, iop)) == ch)
				continue;
			if (_wd_ungetwc(&chcount, inchar, iop) != WEOF) {
				return (nmatch); /* failed to match input */
			}
			break;
		}
#else  /* _WIDE */
		if (isspace(ch)) {
			if (!flag_eof) {
				while (isspace(inchar = locgetc(chcount)))
					;
				if (locungetc(chcount, inchar) == EOF)
					flag_eof = 1;

			}
			continue;
		}
		if (ch != '%' || (ch = *fmt++) == '%') {
			if ((inchar = locgetc(chcount)) == ch)
				continue;
			if (locungetc(chcount, inchar) != EOF) {
				return (nmatch); /* failed to match input */
			}
			break;
		}
#endif /* _WIDE */

charswitch:	/* target of a goto 8-( */

		if (ch == '*') {
			stow = 0;
			ch = *fmt++;
		} else
			stow = 1;

#ifdef	_WIDE
		for (len = 0; ((ch >= 0) && (ch < 256) && isdigit(ch));
								ch = *fmt++)
			len = len * 10 + ch - '0';
#else  /* _WIDE */
		for (len = 0; isdigit(ch); ch = *fmt++)
			len = len * 10 + ch - '0';
#endif /* _WIDE */

		if (ch == '$') {
			/*
			 * positional parameter handling - the number
			 * specified in len gives the argument to which
			 * the next conversion should be applied.
			 * WARNING: This implementation of positional
			 * parameters assumes that the sizes of all pointer
			 * types are the same. (Code similar to that
			 * in the portable doprnt.c should be used if this
			 * assumption does not hold for a particular
			 * port.)
			 */
			if (fpos) {
				if (_mkarglst(sformat, sargs, arglst) != 0) {
					return (EOF);
				} else {
					fpos = 0;
				}
			}
			if (len <= MAXARGS) {
				args = arglst[len - 1];
			} else {
				args = arglst[MAXARGS - 1];
				for (len -= MAXARGS; len > 0; len--)
					(void) va_arg(args.ap, void *);
			}
			len = 0;
			ch = *fmt++;
			goto charswitch;
		}

		if (len == 0)
			len = MAXINT;
#ifdef	_WIDE
		if ((size = ch) == 'l' || (size == 'h') || (size == 'L'))
			ch = *fmt++;
#else  /* _WIDE */
		if ((size = ch) == 'l' || (size == 'h') || (size == 'L') ||
			(size == 'w'))
			ch = *fmt++;
#endif /* _WIDE */
		if (size == 'l' && ch == 'l') {
			size = 'm';		/* size = 'm' if long long */
			ch = *fmt++;
		}
		if (ch == '\0') {
			return (EOF);		/* unexpected end of format */
		}
		if (ch == '[') {
#ifdef	_WIDE
			wchar_t	c;
			size_t	len;
			int	negflg = 0;
			wchar_t	*p;
			wchar_t	*wbracket_str;
			size_t	wlen, clen;

			/* p points to the address of '[' */
			p = (wchar_t *)fmt - 1;
			len = 0;
			if (*fmt == '^') {
				len++;
				fmt++;
				negflg = 1;
			}
			if (((c = *fmt) == ']') || (c == '-')) {
				len++;
				fmt++;
			}
			while ((c = *fmt) != ']') {
				if (c == '\0') {
					return (EOF); /* unexpected EOF */
				} else {
					len++;
					fmt++;
				}
			}
			fmt++;
			len += 2;
			wbracket_str = (wchar_t *)
				malloc(sizeof (wchar_t) * (len + 1));
			if (wbracket_str == NULL) {
				errno = ENOMEM;
				return (EOF);
			} else {
				(void) wmemcpy(wbracket_str,
						(const wchar_t *)p, len);
				*(wbracket_str + len) = L'\0';
				if (negflg && *(wbracket_str + 1) == '^') {
					*(wbracket_str + 1) = L'!';
				}
			}
			wlen = wcslen(wbracket_str);
			clen = wcstombs((char *)NULL, wbracket_str, 0);
			if (clen == (size_t)-1)
				return (EOF);
			bracket_str = (unsigned char *)
				malloc(sizeof (unsigned char) * (clen + 1));
			if (bracket_str == NULL)
				return (EOF);
			clen = wcstombs((char *)bracket_str, wbracket_str,
								wlen + 1);
			free(wbracket_str);
			if (clen == (size_t)-1) {
				free(bracket_str);
				return (EOF);
			}
#else  /* _WIDE */
			if (size == 'l') {
				int	c, len, i;
				int	negflg = 0;
				unsigned char 	*p;

				p = (unsigned char *)(fmt - 1);
				len = 0;
				if (*fmt == '^') {
					len++;
					fmt++;
					negflg = 1;
				}
				if (((c = *fmt) == ']') || (c == '-')) {
					len++;
					fmt++;
				}
				while ((c = *fmt) != ']') {
					if (c == '\0') {
						return (EOF);
					} else if (isascii(c)) {
						len++;
						fmt++;
					} else {
						i = mblen((const char *)fmt,
							MB_CUR_MAX);
						if (i <= 0) {
							return (EOF);
						} else {
							len += i;
							fmt += i;
						}
					}
				}
				fmt++;
				len += 2;
				bracket_str = (unsigned char *)
					malloc(sizeof (unsigned char) *
					(len + 1));
				if (bracket_str == NULL) {
					errno = ENOMEM;
					return (EOF);
				} else {
					(void) strncpy((char *)bracket_str,
						(const char *)p, len);
					*(bracket_str + len) = '\0';
					if (negflg &&
					    *(bracket_str + 1) == '^') {
						*(bracket_str + 1) = '!';
					}
				}
			} else {
				int	t = 0;
				int	b, c, d;

				if (*fmt == '^') {
					t++;
					fmt++;
				}
				(void) memset(tab, !t, NCHARS);
				if ((c = *fmt) == ']' || c == '-') {
					tab[c] = t;
					fmt++;
				}

				while ((c = *fmt) != ']') {
					if (c == '\0') {
						return (EOF);
					}
					b = *(fmt - 1);
					d = *(fmt + 1);
					if ((c == '-') && (d != ']') &&
								(b < d)) {
						(void) memset(&tab[b], t,
								d - b + 1);
						fmt += 2;
					} else {
						tab[c] = t;
						fmt++;
					}
				}
				fmt++;
			}
#endif /* _WIDE */
		}

#ifdef	_WIDE
		if ((ch >= 0) && (ch < 256) &&
			isupper((int)ch)) { /* no longer documented */
			if (_lib_version == c_issue_4) {
				if (size != 'm' && size != 'L')
					size = 'l';
			}
			ch = _tolower((int)ch);
		}
		if (ch != 'n' && !flag_eof) {
			if (ch != 'c' && ch != 'C' && ch != '[') {
				while (iswspace(inchar =
						_wd_getwc(&chcount, iop)))
					;
				if (_wd_ungetwc(&chcount, inchar, iop) == WEOF)
					break;

			}
		}
#else  /* _WIDE */
		if (isupper(ch)) { /* no longer documented */
			if (_lib_version == c_issue_4) {
				if (size != 'm' && size != 'L')
					size = 'l';
			}
			ch = _tolower(ch);
		}
		if (ch != 'n' && !flag_eof) {
			if (ch != 'c' && ch != 'C' && ch != '[') {
				while (isspace(inchar = locgetc(chcount)))
					;
				if (locungetc(chcount, inchar) == EOF)
					break;
			}
		}
#endif /* _WIDE */

		switch (ch) {
		case 'C':
		case 'S':
		case 'c':
		case 's':
#ifdef	_WIDE
			if ((size == 'l') || (size == 'C') || (size == 'S')) {
#else  /* _WIDE */
			if ((size == 'w') || (size == 'l') || (size == 'C') ||
				(size == 'S')) {
#endif /* _WIDE */
				size = wstring(&chcount, &flag_eof, stow,
					(int)ch, len, iop, &args.ap);
			} else {
				size = string(&chcount, &flag_eof, stow,
					(int)ch, len, tab, iop, &args.ap);
			}
			break;
		case '[':
			if (size == 'l') {
				size = wbrstring(&chcount, &flag_eof, stow,
					(int)ch, len, iop, bracket_str,
					&args.ap);
			} else {
#ifdef	_WIDE
				size = brstring(&chcount, &flag_eof, stow,
					(int)ch, len, iop, bracket_str,
					&args.ap);
#else  /* _WIDE */
				size = string(&chcount, &flag_eof, stow,
					ch, len, tab, iop, &args.ap);
#endif /* _WIDE */
			}
			break;
		case 'n':
			if (stow == 0)
				continue;
			if (size == 'h')
				*va_arg(args.ap, short *) = (short)chcount;
			else if (size == 'l')
				*va_arg(args.ap, long *) = (long)chcount;
			else if (size == 'm') /* long long */
				*va_arg(args.ap, long long *) =
					(long long) chcount;
			else
				*va_arg(args.ap, int *) = (int)chcount;
			continue;
		case 'i':
		default:
			size = number(&chcount, &flag_eof, stow, (int)ch,
				len, (int)size, iop, &args.ap);
			break;
		}
		if (size)
			nmatch += stow;
		else {
			return ((flag_eof && !nmatch) ? EOF : nmatch);
		}
		continue;
	}
	return (nmatch != 0 ? nmatch : EOF); /* end of input */
}

/* ****************************************************************** */
/* Functions to read the input stream in an attempt to match incoming */
/* data to the current pattern from the main loop of _doscan(). */
/* ****************************************************************** */
static int
number(int *chcount, int *flag_eof, int stow, int type, int len, int size,
	FILE *iop, va_list *listp)
{
	char	numbuf[64];
	char	*np = numbuf;
	int	c, base, inchar, lookahead;
	int		digitseen = 0, floater = 0, negflg = 0;
	long long	lcval = 0LL;

	switch (type) {
	case 'e':
	case 'f':
	case 'g':
		floater++;
		/* FALLTHROUGH */
	case 'd':
	case 'u':
	case 'i':
		base = 10;
		break;
	case 'o':
		base = 8;
		break;
	case 'p':
#ifdef	_LP64
		size = 'l'; /* pointers are long in LP64 */
#endif	/*	_LP64	*/
	case 'x':
		base = 16;
		break;
	default:
		return (0); /* unrecognized conversion character */
	}

	if (floater != 0) {
		/*
		 * Handle floating point with
		 * file_to_decimal.
		 */
		decimal_mode    dm;
		decimal_record  dr;
		fp_exception_field_type efs;
		enum decimal_string_form form;
		char	*echar;
		int		nread, ic;
		char	buffer[1024];
		char	*nb = buffer;

		if (len > 1024)
			len = 1024;
		file_to_decimal(&nb, len, 0, &dr, &form, &echar, iop, &nread);
		if (stow && (form != invalid_form)) {
#if defined(i386) || defined(__ia64)
			dm.rd = __xgetRD();
#elif defined(sparc)
			dm.rd = _QgetRD();
#else
#error Unknown architecture!
#endif
			if (size == 'l') {		/* double */
				decimal_to_double((double *)
					va_arg(*listp, double *),
					&dm, &dr, &efs);
			} else if (size == 'L') {	/* quad */
#if defined(i386) || defined(__ia64)
				decimal_to_extended((extended *)
					va_arg(*listp, quadruple *),
					&dm, &dr, &efs);
#elif defined(sparc)
				decimal_to_quadruple((quadruple *)
					va_arg(*listp, double *),
					&dm, &dr, &efs);
#else
#error Unknown architecture!
#endif /* ! defined(i386) */
			} else {	/* single */
				decimal_to_single((float *)
					va_arg(*listp, float *),
					&dm, &dr, &efs);
			}
			if ((efs & (1 << fp_overflow)) != 0) {
				errno = ERANGE;
			}
			if ((efs & (1 << fp_underflow)) != 0) {
				errno = ERANGE;
			}
		}
		(*chcount) += nread;	/* Count characters read. */
		c = *nb;		/* Get first unused character. */
		ic = c;
		if (c == NULL) {
			ic = locgetc((*chcount));
			c = ic;
			/*
			 * If null, first unused may have been put back
			 * already.
			 */
		}
		if (ic == EOF) {
			(*chcount)--;
			*flag_eof = 1;
		} else if (locungetc((*chcount), c) == EOF)
			*flag_eof = 1;
		return ((form == invalid_form) ? 0 : 1);
				/* successful match if non-zero */
	}

	switch (c = locgetc((*chcount))) {
	case '-':
		negflg++;
		/* FALLTHROUGH */
	case '+':
		if (--len <= 0)
			break;
		if ((c = locgetc((*chcount))) != '0')
			break;
		/* FALLTHROUGH */
	case '0':
		/*
		 * If %i or %x, the characters 0x or 0X may optionally precede
		 * the sequence of letters and digits (base 16).
		 */
		if ((type != 'i' && type != 'x') || (len <= 1))
			break;
		if (((inchar = locgetc((*chcount))) == 'x') ||
			(inchar == 'X')) {
			lookahead = readchar(iop, chcount);
			if (isxdigit(lookahead)) {
				base = 16;

				if (len <= 2) {
					locungetc((*chcount), lookahead);
					/* Take into account the 'x' */
					len -= 1;
				} else {
					c = lookahead;
					/* Take into account '0x' */
					len -= 2;
				}
			} else {
				locungetc((*chcount), lookahead);
				locungetc((*chcount), inchar);
			}
		} else {
			/* inchar wans't 'x'. */
			locungetc((*chcount), inchar); /* Put it back. */
			if (type == 'i') /* Only %i accepts an octal. */
				base = 8;
		}
	}
	for (; --len  >= 0; *np++ = (char)c, c = locgetc((*chcount))) {
		if (np > numbuf + 62) {
		    errno = ERANGE;
		    return (0);
		}
		if (isdigit(c) || base == 16 && isxdigit(c)) {
			int digit = c - (isdigit(c) ? '0' :
				isupper(c) ? 'A' - 10 : 'a' - 10);
			if (digit >= base)
				break;
			if (stow)
				lcval = base * lcval + digit;
			digitseen++;
			continue;
		}
		break;
	}

	if (stow && digitseen) {
		/* suppress possible overflow on 2's-comp negation */
		if (negflg && lcval != (1ULL << 63))
			lcval = -lcval;
		switch (size) {
			case 'm':
				*va_arg(*listp, long long *) = lcval;
				break;
			case 'l':
				*va_arg(*listp, long *) = (long)lcval;
				break;
			case 'h':
				*va_arg(*listp, short *) = (short)lcval;
				break;
			default:
				*va_arg(*listp, int *) = (int)lcval;
				break;
		}
	}
	if (locungetc((*chcount), c) == EOF)
	    *flag_eof = 1;
	return (digitseen); /* successful match if non-zero */
}

/* Get a character. If not using sscanf and at the buffer's end */
/* then do a direct read(). Characters read via readchar() */
/* can be  pushed back on the input stream by locungetc((*chcount),) */
/* since there is padding allocated at the end of the stream buffer. */
static int
readchar(FILE *iop, int *chcount)
{
	int	inchar;
	char	buf[1];

	if ((iop->_flag & _IOWRT) || (iop->_cnt != 0))
		inchar = locgetc((*chcount));
	else {
		if (read(FILENO(iop), buf, 1) != 1)
			return (EOF);
		inchar = (int)buf[0];
		(*chcount) += 1;
	}
	return (inchar);
}

static int
string(int *chcount, int *flag_eof, int stow, int type, int len, char *tab,
	FILE *iop, va_list *listp)
{
	int	ch;
	char	*ptr;
	char	*start;

	start = ptr = stow ? va_arg(*listp, char *) : NULL;
	if (((type == 'c') || (type == 'C')) && len == MAXINT)
		len = 1;
#ifdef	_WIDE
	while ((ch = locgetc((*chcount))) != EOF &&
		!(((type == 's') || (type == 'S')) && isspace(ch))) {
#else  /* _WIDE */
	while ((ch = locgetc((*chcount))) != EOF &&
		!(((type == 's') || (type == 'S')) &&
			isspace(ch) || type == '[' && tab[ch])) {
#endif /* _WIDE */
		if (stow)
			*ptr = (char)ch;
		ptr++;
		if (--len <= 0)
			break;
	}
	if (ch == EOF) {
		(*flag_eof) = 1;
		(*chcount) -= 1;
	} else if (len > 0 && locungetc((*chcount), ch) == EOF)
		(*flag_eof) = 1;
	if (ptr == start)
		return (0);	/* no match */
	if (stow && ((type != 'c') && (type != 'C')))
		*ptr = '\0';
	return (1);	/* successful match */
}

/* This function initializes arglst, to contain the appropriate */
/* va_list values for the first MAXARGS arguments. */
/* WARNING: this code assumes that the sizes of all pointer types */
/* are the same. (Code similar to that in the portable doprnt.c */
/* should be used if this assumption is not true for a */
/* particular port.) */

#ifdef	_WIDE
static int
_mkarglst(const wchar_t *fmt, stva_list args, stva_list arglst[])
#else  /* _WIDE */
static int
_mkarglst(const char *fmt, stva_list args, stva_list arglst[])
#endif /* _WIDE */
{
#ifdef	_WIDE
#define	STRCHR	wcschr
#define	STRSPN	wcsspn
#define	ATOI(x)	_watoi((wchar_t *)x)
#define	SPNSTR1	L"01234567890"
#define	SPNSTR2	L"# +-.0123456789hL$"
#else  /* _WIDE */
#define	STRCHR	strchr
#define	STRSPN	strspn
#define	ATOI(x)	atoi(x)
#define	SPNSTR1	"01234567890"
#define	SPNSTR2	"# +-.0123456789hL$"
#endif /* _WIDE */

	int maxnum, curargno;
	size_t n;

	maxnum = -1;
	curargno = 0;

	while ((fmt = STRCHR(fmt, '%')) != NULL) {
		fmt++;	/* skip % */
		if (*fmt == '*' || *fmt == '%')
			continue;
		if (fmt[n = STRSPN(fmt, SPNSTR1)] == L'$') {
			/* convert to zero base */
			curargno = ATOI(fmt) - 1;
			fmt += n + 1;
		}

		if (maxnum < curargno)
			maxnum = curargno;
		curargno++;	/* default to next in list */

		fmt += STRSPN(fmt, SPNSTR2);
		if (*fmt == '[') {
			int		i;
			fmt++; /* has to be at least on item in scan list */
			if (*fmt == ']') {
				fmt++;
			}
			while (*fmt != ']') {
				if (*fmt == L'\0') {
					return (-1); /* bad format */
#ifdef	_WIDE
				} else {
					fmt++;
				}
#else  /* _WIDE */
				} else if (isascii(*fmt)) {
					fmt++;
				} else {
					i = mblen((const char *)
						fmt, MB_CUR_MAX);
					if (i <= 0) {
						return (-1);
					} else {
						fmt += i;
					}
				}
#endif /* _WIDE */
			}
		}
	}
	if (maxnum > MAXARGS)
		maxnum = MAXARGS;
	for (n = 0; n <= maxnum; n++) {
		arglst[n] = args;
		(void) va_arg(args.ap, void *);
	}
	return (0);
}


/*
 * For wide character handling
 */
#ifdef	_WIDE
static int
wstring(int *chcount, int *flag_eof, int stow, int type,
	int len, FILE *iop, va_list *listp)
{
	wint_t	wch;
	wchar_t	*ptr;
	wchar_t	*wstart;
	int	dummy;

	wstart = ptr = stow ? va_arg(*listp, wchar_t *) : NULL;

	if ((type == 'c') && len == MAXINT)
		len = 1;
	while (((wch = _wd_getwc(chcount, iop)) != WEOF) &&
		!(type == 's' && iswspace(wch))) {
		if (stow)
			*ptr = wch;
		ptr++;
		if (--len <= 0)
			break;
	}
	if (wch == WEOF) {
		*flag_eof = 1;
		(*chcount) -= 1;
	} else {
		if (len > 0 && _wd_ungetwc(chcount, wch, iop) == WEOF)
			*flag_eof = 1;
	}
	if (ptr == wstart)
		return (0); /* no match */
	if (stow && (type != 'c'))
		*ptr = '\0';
	return (1); /* successful match */
}
#else  /* _WIDE */
static int
wstring(int *chcount, int *flag_eof, int stow, int type, int len, FILE *iop,
	va_list *listp)
{
	int	wch;
	wchar_t	*ptr;
	wchar_t	*wstart;

	wstart = ptr = stow ? va_arg(*listp, wchar_t *) : NULL;

	if ((type == 'c') && len == MAXINT)
		len = 1;
	while (((wch = _bi_getwc(iop)) != EOF) &&
		!(type == 's' && (isascii(wch) ? isspace(wch) : 0))) {
		(*chcount) += scrwidth((wchar_t)wch);
		if (stow)
			*ptr = wch;
		ptr++;
		if (--len <= 0)
			break;
	}
	if (wch == EOF) {
		(*flag_eof) = 1;
		(*chcount) -= 1;
	} else {
		if (len > 0 && _bi_ungetwc(wch, iop) == EOF)
			(*flag_eof) = 1;
	}
	if (ptr == wstart)
		return (0); /* no match */
	if (stow && (type != 'c'))
		*ptr = '\0';
	return (1); /* successful match */
}
#endif /* _WIDE */

#ifdef	_WIDE
static wint_t
_wd_getwc(int *chcount, FILE *iop)
{
	wint_t	wc;
	int	len;

	if (!(iop->_flag & _IOWRT)) {
		/* call from fwscanf, wscanf */
		wc = __fgetwc_xpg5(iop);
		(*chcount)++;
		return (wc);
	} else {
		/* call from swscanf */
		if (*iop->_ptr == '\0')
			return (WEOF);
		len = mbtowc((wchar_t *)&wc, (const char *)iop->_ptr,
								MB_CUR_MAX);
		if (len == -1)
			return (WEOF);
		iop->_ptr += len;
		(*chcount)++;
		return (wc);
	}
}

static wint_t
_wd_ungetwc(int *chcount, wchar_t wc, FILE *iop)
{
	wint_t	ret;
	int	len;
	char	*mbs;

	if (wc == WEOF)
		return (WEOF);

	if (!(iop->_flag & _IOWRT)) {
		/* call from fwscanf, wscanf */
		ret = __ungetwc_xpg5((wint_t)wc, iop);
		if (ret != (wint_t)wc)
			return (WEOF);
		(*chcount)--;
		return (ret);
	} else {
		/* call from swscanf */
		mbs = (char *)malloc(sizeof (char) * MB_CUR_MAX);
		if (mbs == NULL)
			return (WEOF);
		len = wctomb(mbs, wc);
		if (len == -1)
			return (WEOF);
		iop->_ptr -= len;
		(*chcount)--;
		return ((wint_t)wc);
	}
}

static int
_watoi(wchar_t *fmt)
{
	int	n = 0;
	wchar_t	ch;

	ch = *fmt;
	if ((ch >= 0) && (ch < 256) && isdigit((int)ch)) {
		n = ch - '0';
		while (((ch = *++fmt) >= 0) && (ch < 256) &&
				isdigit((int)ch)) {
			n *= 10;
			n += ch - '0';
		}
	}
	return (n);
}
#endif /* _WIDE */

static int
wbrstring(int *chcount, int *flag_eof, int stow, int type,
	int len, FILE *iop, unsigned char *brstr, va_list *listp)
{
	wint_t	wch;
	int	i;
	char	*str;
	wchar_t	*ptr, *start;
#ifdef	_WIDE
	int	dummy;
#endif /* _WIDE */

	start = ptr = stow ? va_arg(*listp, wchar_t *) : NULL;

	str = (char *)malloc(sizeof (char) * (MB_CUR_MAX + 1));
	if (str == NULL) {
		return (0);
	}
#ifdef	_WIDE
	while ((wch = _wd_getwc(&dummy, iop)) != WEOF) {
#else  /* _WIDE */
	while ((wch = _bi_getwc(iop)) != WEOF) {
#endif /* _WIDE */
		i = wctomb(str, (wchar_t)wch);
		if (i == -1) {
			free(str);
			return (0);
		}
		str[i] = '\0';
		if (fnmatch((const char *)brstr, (const char *)str,
			FNM_NOESCAPE)) {
			break;
		} else {
			if (len > 0) {
#ifdef	_WIDE
				(*chcount)++;
#else  /* _WIDE */
				(*chcount) += scrwidth(wch);
#endif /* _WIDE */
				len--;
				if (stow) {
					*ptr = wch;
				}
				ptr++;
				if (len <= 0)
					break;
			} else {
				break;
			}
		}
	}
	if (wch == WEOF) {
		*flag_eof = 1;
	} else {
#ifdef	_WIDE
		if (len > 0 && _wd_ungetwc(&dummy, wch, iop) == WEOF)
#else  /* _WIDE */
		if (len > 0 && _bi_ungetwc(wch, iop) == WEOF)
#endif /* _WIDE */
			*flag_eof = 1;
	}
	free(str);
	if (ptr == start)
		return (0);				/* no match */
	if (stow)
		*ptr = L'\0';
	return (1);					/* successful match */
}

#ifdef	_WIDE
static int
brstring(int *chcount, int *flag_eof, int stow, int type,
	int len, FILE *iop, unsigned char *brstr, va_list *listp)
{
	wint_t	wch;
	int	i;
	char	*str;
	char	*ptr, *start, *p;
	int	dummy;

	start = ptr = stow ? va_arg(*listp, char *) : NULL;

	str = (char *)malloc(sizeof (char) * (MB_CUR_MAX + 1));
	if (str == NULL) {
		return (0);
	}
	while ((wch = _wd_getwc(&dummy, iop)) != WEOF) {
		p = str;
		i = wctomb(str, (wchar_t)wch);
		if (i == -1) {
			free(str);
			return (0);
		}
		str[i] = '\0';
		if (fnmatch((const char *)brstr, (const char *)str,
			FNM_NOESCAPE)) {
			break;
		} else {
			if (len >= i) {
				(*chcount)++;
				len -= i;
				if (stow) {
					while (i-- > 0) {
						*ptr++ = *p++;
					}
				} else {
					while (i-- > 0) {
						ptr++;
					}
				}
				if (len <= 0)
					break;
			} else {
				break;
			}
		}
	}
	if (wch == WEOF) {
		*flag_eof = 1;
	} else {
		if (len > 0 && _wd_ungetwc(&dummy, wch, iop) == WEOF)
			*flag_eof = 1;
	}
	free(str);
	if (ptr == start)
		return (0);				/* no match */
	if (stow)
		*ptr = '\0';
	return (1);					/* successful match */
}
#endif /* _WIDE */

/*
 * Locally define getwc and ungetwc
 */
static int
_bi_getwc(FILE *iop)
{
	int c;
	wchar_t intcode;
	int i, nbytes, cur_max;
	char buff[MB_LEN_MAX];

	if ((c = wlocgetc()) == EOF)
		return (WEOF);

	if (isascii(c))	/* ASCII code */
		return ((wint_t)c);

	buff[0] = (char)c;

	cur_max = (int)MB_CUR_MAX;
	/* MB_CUR_MAX doen't exeed the value of MB_LEN_MAX */
	/* So we use MB_CUR_MAX instead of MB_LEN_MAX for */
	/* improving the performance. */
	for (i = 1; i < cur_max; i++) {
		c = wlocgetc();
		if (c == '\n') {
			wlocungetc(c);
			break;
		}
		if (c == EOF) {
			/* this still may be a valid multibyte character */
			break;
		}
		buff[i] = (char)c;
	}

	if ((nbytes = mbtowc(&intcode, buff, i)) == -1) {
		/*
		 * If mbtowc fails, the input was not a legal character.
		 *	ungetc all but one character.
		 *
		 * Note:  the number of pushback characters that
		 *	ungetc() can handle must be >= (MB_LEN_MAX - 1).
		 *	In Solaris 2.x, the number of pushback
		 *	characters is 4.
		 */
		while (i-- > 1) {
			wlocungetc((signed char)buff[i]);
		}
		errno = EILSEQ;
		return (WEOF); /* Illegal EUC sequence. */
	}

	while (i-- > nbytes) {
		/*
		 * Note:  the number of pushback characters that
		 *	ungetc() can handle must be >= (MB_LEN_MAX - 1).
		 *	In Solaris 2.x, the number of pushback
		 *	characters is 4.
		 */
		wlocungetc((signed char)buff[i]);
	}
	return ((int)intcode);
}

static int
_bi_ungetwc(wint_t wc, FILE *iop)
{
	char mbs[MB_LEN_MAX];
	unsigned char *p;
	int n;

	if (wc == WEOF)
		return (WEOF);

	if ((iop->_flag & _IOREAD) == 0 || iop->_ptr <= iop->_base) {
		if (iop->_base != NULL && iop->_ptr == iop->_base &&
		    iop->_cnt == 0)
			++iop->_ptr;
		else
			return (WEOF);
	}

	n = wctomb(mbs, (wchar_t)wc);
	if (n <= 0)
		return (WEOF);
	p = (unsigned char *)(mbs+n-1); /* p points the last byte */
	/* if _IOWRT is set to iop->_flag, it means this is */
	/* an invocation from sscanf(), and in that time we */
	/* don't touch iop->_cnt.  Otherwise, which means an */
	/* invocation from fscanf() or scanf(), we touch iop->_cnt */
	if ((iop->_flag & _IOWRT) == 0) {
		/* scanf() and fscanf() */
		iop->_cnt += n;
		while (n--) {
			*--iop->_ptr = *(p--);
		}
	} else {
		/* sscanf() */
		iop->_ptr -= n;
	}
	return (wc);
}
