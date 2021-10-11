/*	Copyright (c) 1988 AT&T	*/

/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)doprnt.c	1.70	98/09/01 SMI"

/*LINTLIBRARY*/
/*
 *	_doprnt: common code for printf, fprintf, sprintf
 */

#include "synonyms.h"
#include "shlib.h"
#include "mtlib.h"
#include "print.h"	/* parameters & macros for doprnt */
#include <wchar.h>
#include "libc.h"
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <values.h>
#include <nan.h>
#include "Qnan.h"
#include <memory.h>
#include <string.h>
#include <locale.h>
#include <widec.h>
#include "../i18n/_locale.h"
#include <errno.h>
#include <sys/types.h>
#include <libw.h>
#include <sys/localedef.h>
#include "mse.h"

#if defined(i386) || defined(__sparcv9)
#define	GETQVAL(arg)	(va_arg(arg, long double))
#else /* !defined(i386) && !defined(__sparcv9) */
#define	GETQVAL(arg)	*(va_arg(arg, long double *))
#endif /* !defined(i386) && !defined(__sparcv9) */

#ifdef	_WIDE
#define	STRCHR	wcschr
#define	STRSPN	wcsspn
#define	ATOI(x)	_watoi((wchar_t *)x)
#define	_P_HYPHEN	L"-"
#define	_P_PLUS		L"+"
#define	_P_BLANK	L" "
#define	_P_ZEROx	L"0x"
#define	_P_ZEROX	L"0X"
#define	_M_ISDIGIT(c)	(((c) >= 0) && ((c) < 256) && isdigit((c)))
#define	_M_ISUPPER(c)	(((c) >= 0) && ((c) < 256) && isupper((c)))
#else  /* _WIDE */
#define	STRCHR	strchr
#define	STRSPN	strspn
#define	ATOI(x)	atoi(x)
#define	_P_HYPHEN	"-"
#define	_P_PLUS		"+"
#define	_P_BLANK	" "
#define	_P_ZEROx	"0x"
#define	_P_ZEROX	"0X"
#define	_M_ISDIGIT(c)	isdigit((c))
#define	_M_ISUPPER(c)	isupper((c))
#endif /* _WIDE */

#ifdef	_WIDE
#define	WCTOMB(a, b)	METHOD(lc, wctomb)(lc, a, b)
#define	PUT(p, n) \
	{ \
		if (sflag) { \
			size_t	len; \
			len = (wchar_t *)bufferend - (wchar_t *)bufptr; \
			if (n > len) { \
				(void) wmemcpy((wchar_t *)bufptr, \
					(const wchar_t *)p, len);\
				iop->_ptr = (unsigned char *)bufferend; \
				return ((ssize_t)EOF); \
			} else { \
				(void) wmemcpy((wchar_t *)bufptr, \
					(const wchar_t *)p, (size_t)n); \
				bufptr = (unsigned char *) \
					(((wchar_t *)bufptr) + n); \
			} \
		} else { \
			unsigned char	*newbufptr; \
			wchar_t	*q; \
			char	*tmpp, *tmpq; \
			int	r; \
			size_t	len, i; \
			tmpp = (char *)malloc(sizeof (char) * (n + 1) \
				* MB_CUR_MAX); \
			if (tmpp == NULL) { \
				errno = ENOMEM; \
				return (EOF); \
			} \
			q = (wchar_t *)p; \
			tmpq = tmpp; \
			for (len = 0, i = 0; i < n; i++) { \
				r = WCTOMB(tmpq, *q++); \
				if (r == -1) { \
					free(tmpp); \
					errno = EILSEQ; \
					return (EOF); \
				} \
				len += r; \
				tmpq += r; \
			} \
			tmpq = tmpp; \
			newbufptr = bufptr + len; \
			if (newbufptr > bufferend) { \
				if (!_dowrite(tmpp, len, iop, &bufptr)) { \
					free(tmpp); \
					return (EOF); \
				} \
			} else { \
				switch (len) { \
				case 4: \
					*bufptr++ = *tmpp++; \
				case 3: \
					*bufptr++ = *tmpp++; \
				case 2: \
					*bufptr++ = *tmpp++; \
				case 1: \
					*bufptr = *tmpp++; \
					break; \
				default: \
					(void) memcpy(bufptr, tmpp, len); \
				} \
				bufptr = newbufptr; \
			} \
			free(tmpq); \
		} \
	}


#define	PAD(s, n)	PUT(s, n)

#else  /* _WIDE */
#define	PUT(p, n)	{ unsigned char *newbufptr; \
			if ((newbufptr = bufptr + n) > bufferend) { \
				if (!_dowrite(p, n, iop, &bufptr)) \
					return (EOF); \
			} else { \
				char *tmp = (char *)p; \
				switch (n) {\
				case 4: \
					*bufptr++ = *tmp++;   \
				case 3: \
					*bufptr++ = *tmp++;   \
				case 2: \
					*bufptr++ = *tmp++; \
				case 1: \
					*bufptr = *tmp; \
					break; \
				default: \
					(void) memcpy(bufptr, p, n); \
				} \
				bufptr = newbufptr; \
			} \
			}

#define	PAD(s, n)    { ssize_t nn; \
		    for (nn = n; nn > 20; nn -= 20) \
			if (!_dowrite(s, 20, iop, &bufptr)) \
				return (EOF); \
			PUT(s, nn); \
		}

#endif /* _WIDE */

#define	SNLEN	5	/* Length of string used when printing a NaN */

/* bit positions for flags used in doprnt */

#define	LENGTH	1	/* l */
#define	FPLUS	2	/* + */
#define	FMINUS	  4	/* - */
#define	FBLANK	  8	/* blank */
#define	FSHARP	 16	/* # */
#define	PADZERO  32	/* padding zeroes requested via '0' */
#define	DOTSEEN  64	/* dot appeared in format specification */
#define	SUFFIX	128	/* a suffix is to appear in the output */
#define	RZERO	256	/* there will be trailing zeros in output */
#define	LZERO	512	/* there will be leading zeroes in output */
#define	SHORT  1024	/* h */
#define	QUAD   2048	/* Q for long double */
#define	XLONG  4096	/* ll for long long */

#ifdef	_WIDE
static wchar_t *
insert_thousands_sep(wchar_t *bp, wchar_t *ep);
#else  /* _WIDE */
static char *
insert_thousands_sep(char *bp, char *ep);
#endif /* _WIDE */

static int
_rec_scrswidth(wchar_t *, ssize_t);

/*
 *	Positional Parameter information
 */
#define	MAXARGS	30	/* max. number of args for fast positional paramters */

static ssize_t
_dowrite(char *p, ssize_t n, FILE *iop, unsigned char **ptrptr);

/*
 * stva_list is used to subvert C's restriction that a variable with an
 * array type can not appear on the left hand side of an assignment operator.
 * By putting the array inside a structure, the functionality of assigning to
 * the whole array through a simple assignment is achieved..
 */
typedef struct stva_list {
	va_list	ap;
} stva_list;

#ifdef	_WIDE
static void _wmkarglst(wchar_t *, stva_list, stva_list []);
static void _wgetarg(wchar_t *, stva_list *, long);
#else  /* _WIDE */
void _mkarglst(char *, stva_list, stva_list []);
void _getarg(char *, stva_list *, long);
#endif /* _WIDE */




static int
_lowdigit(ssize_t *valptr)
{
	/* This function computes the decimal low-order digit of the number */
	/* pointed to by valptr, and returns this digit after dividing   */
	/* *valptr by ten.  This function is called ONLY to compute the */
	/* low-order digit of a long whose high-order bit is set. */

	ssize_t lowbit = *valptr & 1;
	long value = (*valptr >> 1) & ~HIBITL;

	*valptr = value / 5;
	value = value % 5 * 2 + lowbit + '0';
	return ((int) value);
}

static int
_lowlldigit(long long *valptr)
{
	ssize_t lowbit = *valptr & 1;
	long long value = (*valptr >> 1) & ~HIBITLL;
		*valptr = value / 5;
		value = value % 5 * 2 + lowbit + '0';
		return ((int) value);
}

/* The function _dowrite carries out buffer pointer bookkeeping surrounding */
/* a call to fwrite.  It is called only when the end of the file output */
/* buffer is approached or in other unusual situations. */

static ssize_t
_dowrite(char *p, ssize_t n, FILE *iop, unsigned char **ptrptr)
{
	if (!(iop->_flag & _IOREAD)) {
		iop->_cnt -= (*ptrptr - iop->_ptr);
		iop->_ptr = *ptrptr;
		_bufsync(iop, _bufend(iop));
		if (_FWRITE(p, 1, n, iop) != n) {
			return (0);
		}
		*ptrptr = iop->_ptr;
	} else {
		if (n > iop->_cnt)
			n = iop->_cnt;
		iop->_cnt -= n;
		*ptrptr = (unsigned char *) memcpy((char *) *ptrptr, p, n) + n;
		iop->_ptr = *ptrptr;
	}
	return (1);
}

#ifdef	_WIDE
	static const wchar_t _blanks[] = L"                    ";
	static const wchar_t _zeroes[] = L"00000000000000000000";
	static const wchar_t uc_digs[] = L"0123456789ABCDEF";
	static const wchar_t lc_digs[] = L"0123456789abcdef";
#else /* _WIDE */
	static char _blanks[] = "                    ";
	static char _zeroes[] = "00000000000000000000";
	static char uc_digs[] = "0123456789ABCDEF";
	static char lc_digs[] = "0123456789abcdef";
#endif /* _WIDE */
#ifdef	HANDLE_NaNINF
#ifdef	_WIDE
	static const wchar_t  lc_nan[] = L"nan0x";
	static const wchar_t  uc_nan[] = L"NAN0X";
	static const wchar_t  lc_inf[] = L"inf";
	static const wchar_t  uc_inf[] = L"INF";
#else  /* _WIDE */
	static char  lc_nan[] = "nan0x";
	static char  uc_nan[] = "NAN0X";
	static char  lc_inf[] = "inf";
	static char  uc_inf[] = "INF";
#endif /* _WIDE */
#endif	/* defined(HANDLE_NaNINF) */


#ifdef	_WIDE
ssize_t
_wdoprnt(const wchar_t *format, va_list in_args, FILE *iop)
#else  /* _WIDE */
ssize_t
_doprnt(const char *format, va_list in_args, FILE *iop)
#endif /* _WIDE */
{

#ifdef	_WIDE
	int	sflag = 0;
	size_t	maxcount;
	mbstate_t	*mbst;
	_LC_charmap_t	*lc;
#endif /* _WIDE */
	/* bufptr is used inside of doprnt instead of iop->_ptr; */
	/* bufferend is a copy of _bufend(iop), if it exists.  For */
	/* dummy file descriptors (iop->_flag & _IOREAD), bufferend */
	/* may be meaningless. Dummy file descriptors are used so that */
	/* sprintf and vsprintf may share the _doprnt routine with the */
	/* rest of the printf family. */

	unsigned char *bufptr;
	unsigned char *bufferend;

#ifdef	_WIDE
	/* This variable counts output characters. */
	size_t	count = 0;
#else  /* _WIDE */
	/* This variable counts output characters. */
	int	count = 0;
#endif /* _WIDE */

#ifdef	_WIDE
	wchar_t	*wbp;
	wchar_t	*bp;
	wchar_t	*p;
	char	*cbp;
	char	*cp;

#else  /* _WIDE */
	/* Starting and ending points for value to be printed */
	char	*bp;
	char *p;
#endif /* _WIDE */
	/* Field width and precision */
	int	prec = 0;
	ssize_t width;
	ssize_t num;
	ssize_t sec_display;
	wchar_t *wp;
	ssize_t preco;
	ssize_t wcount = 0;
	char tmpbuf[10];
	char wflag;
	char lflag;
	int quote;		/* ' */
	int	retcode;


#ifdef	_WIDE
	/* Format code */
	wchar_t	fcode;
#else  /* _WIDE */
	/* Format code */
	char	fcode;
#endif /* _WIDE */

	/* Number of padding zeroes required on the left and right */
	ssize_t	lzero, rzero, rz, leadzeroes;


	/* Flags - bit positions defined by LENGTH, FPLUS, FMINUS, FBLANK, */
	/* and FSHARP are set if corresponding character is in format */
	/* Bit position defined by PADZERO means extra space in the field */
	/* should be padded with leading zeroes rather than with blanks */

	ssize_t	flagword;

#ifdef	_WIDE
	/* Values are developed in this buffer */
	wchar_t	buf[max(MAXLLDIGS, 1034)];
	wchar_t	wcvtbuf[DECIMAL_STRING_LENGTH];
#else  /* _WIDE */
	/* Values are developed in this buffer */
	char	buf[max(MAXLLDIGS, 1034)];
#endif /* _WIDE */
	char	cvtbuf[DECIMAL_STRING_LENGTH];

#ifdef	_WIDE
	/* Pointer to sign, "0x", "0X", or empty */
	wchar_t	*prefix;
	wchar_t	*suffix;
	/* Buffer to create exponent */
	wchar_t	expbuf[MAXESIZ + 1];
#else  /* _WIDE */
	/* Pointer to sign, "0x", "0X", or empty */
	char	*prefix;
	char	*suffix;
	/* Buffer to create exponent */
	char	expbuf[MAXESIZ + 1];
#endif /* _WIDE */

	/* Exponent or empty */



	/* Length of prefix and of suffix */
	ssize_t	prefixlength, suffixlength;

	/* Combined length of leading zeroes, trailing zeroes, and suffix */
	ssize_t 	otherlength;

	/* The value being converted, if integer */
	ssize_t	val;

	/* The value being converted, if long long */
	long long ll = 0LL;

	/* The value being converted, if real */
	double	dval;

#if !defined(_NO_LONG_DOUBLE)
	/* The value being converted, if long double */
	long double quadval;
#endif  /* _NO_LONG_DOUBLE */

	/* Output values from fcvt and ecvt */
	int	decpt, sign;

#ifdef	_WIDE
	/* Pointer to a translate table for digits of whatever radix */
	wchar_t	*tab;
#else  /* _WIDE */
	/* Pointer to a translate table for digits of whatever radix */
	char	*tab;
#endif /* _WIDE */

	/* Work variables */
	ssize_t	k, lradix, mradix;

#ifdef	HANDLE_INFNaN
	/* Variables used to flag an infinities and nans, resp. */
	/* Nan_flg is used with two purposes: to flag a NaN and */
	/* as the length of the string ``NAN0X'' (``nan0x'') */
	int	inf_nan = 0, NaN_flg = 0;

#ifdef	_WIDE
	/* Pointer to string "NAN0X" or "nan0x" */
	wchar_t	*SNAN;
#else  /* _WIDE */
	/* Pointer to string "NAN0X" or "nan0x" */
	char	*SNAN;
#endif /* _WIDE */

	/* Flag for negative infinity or NaN */
	int neg_in = 0;
#else	/* defined(HANDLE_INFNaN) */
	int	 inf_nan = 0;
#endif	/* defined(HANDLE_INFNaN) */

#ifdef	_WIDE
	/* variables for positional parameters */
	/* save the beginning of the format */
	wchar_t *sformat = (wchar_t *)format;
#else  /* _WIDE */
	/* variables for positional parameters */
	char *sformat = (char *) format; /* save the beginning of the format */
#endif

	int	fpos = 1;		/* 1 if first positional parameter */
	stva_list	args,	/* used to step through the argument list */
			sargs;	/* used to save the start of the arg list */
	stva_list	bargs;	/* used to restore args if positional width */
				/* or precision */
	stva_list	arglst[MAXARGS]; /* array giving appropriate values */
					/* for va_arg() to retrieve the */
					/* corresponding argument: */
					/* arglst[0] is the first arg */
					/* arglst[1] is the second arg, etc */

	int	starflg = 0;	/* set to 1 if * format specifier seen */
	/*
	 * Initialize args and sargs to the start of the argument list.
	 * We don't know any portable way to copy an arbitrary C object
	 * so we use a system-specific routine (probably a macro) from
	 * stdarg.h.  (Remember that if va_list is an array, in_args will
	 * be a pointer and &in_args won't be what we would want for
	 * memcpy.)
	 */
	va_copy(args.ap, in_args);
	sargs = args;

#ifdef	_WIDE
	if (iop->_flag == _IOREAD)
		sflag = 1;

	if (!sflag) {
		_LC_locale_t	*loc;

		mbst = _getmbstate(iop);
		if (mbst == NULL) {
			errno = EBADF;
			return (EOF);
		}
		if ((loc = (_LC_locale_t *)__mbst_get_locale(
			(const mbstate_t *)mbst)) == NULL) {
			lc = __lc_charmap;
		} else {
			lc = loc->lc_charmap;
		}
#endif /* _WIDE */
	/* if first I/O to the stream get a buffer */
	/* Note that iop->_base should not equal 0 for sprintf and vsprintf */
	if (iop->_base == 0)  {
	    if (_findbuf(iop) == 0)
		return (EOF);
	    /* _findbuf leaves _cnt set to 0 which is the wrong thing to do */
	    /* for fully buffered files */
	    if (!(iop->_flag & (_IOLBF|_IONBF)))
		iop->_cnt = _bufend(iop) - iop->_base;
	}
#ifdef	_WIDE
	}
#endif /* _WIDE */

#ifdef	_WIDE
	bufptr = iop->_ptr;
	if (sflag) {
		maxcount = (size_t)iop->_cnt;
		bufferend = (unsigned char *)(((wchar_t *)iop->_ptr) +
			maxcount);
	} else {
		bufferend = _bufend(iop);
	}
#else  /* _WIDE */
	/* initialize buffer pointer and buffer end pointer */
	/* _IOREAD && _cnt == MAXINT implies [v]sprintf (no boundschecking) */
	bufptr = iop->_ptr;
	bufferend = (iop->_flag & _IOREAD) && iop->_cnt == MAXINT ? \
	    (unsigned char *)((long) bufptr | (-1L & ~HIBITL)) \
	    : _bufend(iop);
#endif /* _WIDE */

	/*
	 *	The main loop -- this loop goes through one iteration
	 *	for each string of ordinary characters or format specification.
	 */
	for (; ; ) {
		ssize_t n;

		if ((fcode = *format) != '\0' && fcode != '%') {
#ifdef	_WIDE
			bp = (wchar_t *) format;
#else  /* _WIDE */
			bp = (char *) format;
#endif /* _WIDE */
			do {
				format++;
			} while ((fcode = *format) != '\0' && fcode != '%');

			count += (n = format - bp); /* n = no. of non-% chars */
			PUT(bp, n);
		}
		if (fcode == '\0') {  /* end of format; return */
			ssize_t nn = bufptr - iop->_ptr;

#ifdef	_WIDE
			if (sflag) {
				iop->_ptr = bufptr;
				return ((ssize_t)count);
			}
#endif /* _WIDE */

			iop->_cnt -= nn;
			iop->_ptr = bufptr;
			/* in case of interrupt during last several lines */
			if ((bufptr + iop->_cnt) > bufferend && !(iop->_flag \
			    & _IOREAD))
				_bufsync(iop, bufferend);
			if (iop->_flag & (_IONBF | _IOLBF) && \
			    (iop->_flag & _IONBF || \
			    memchr((char *)(bufptr+iop->_cnt), \
			    '\n', -iop->_cnt) != NULL))
				(void) _xflsbuf(iop);
#ifdef	_WIDE
			return (FERROR(iop) ? EOF : (ssize_t) count);
#else  /* _WIDE */
			return (FERROR(iop) ? EOF : (int) count);
#endif /* _WIDE */
		}

		/*
		 *	% has been found.
		 *	The following switch is used to parse the format
		 *	specification and to perform the operation specified
		 *	by the format letter.  The program repeatedly goes
		 *	back to this switch until the format letter is
		 *	encountered.
		 */
		width = prefixlength = otherlength = 0;
		flagword = suffixlength = 0;
		format++;
		wflag = 0;
		lflag = 0;
		sec_display = 0;
		quote = 0;

	charswitch:

		switch (fcode = *format++) {

		case '+':
			flagword |= FPLUS;
			goto charswitch;
		case '-':
			flagword |= FMINUS;
			flagword &= ~PADZERO; /* ignore 0 flag */
			goto charswitch;
		case ' ':
			flagword |= FBLANK;
			goto charswitch;
		case '\'':	/* XSH4 */
			quote++;
			goto charswitch;
		case '#':
			flagword |= FSHARP;
			goto charswitch;

		/* Scan the field width and precision */
		case '.':
			flagword |= DOTSEEN;
			prec = 0;
			goto charswitch;

		case '*':
			if (_M_ISDIGIT(*format)) {
				starflg = 1;
				bargs = args;
				goto charswitch;
			}
			if (!(flagword & DOTSEEN)) {
				width = va_arg(args.ap, int);
				if (width < 0) {
					width = -width;
					flagword |= FMINUS;
				}
			} else {
				prec = va_arg(args.ap, int);
				if (prec < 0) {
					prec = 0;
					flagword ^= DOTSEEN; /* ANSI sez so */
				}
			}
			goto charswitch;

		case '$':
			{
			ssize_t		position;
			stva_list	targs;
			if (fpos) {
#ifdef	_WIDE
				_wmkarglst(sformat, sargs, arglst);
#else  /* _WIDE */
				_mkarglst(sformat, sargs, arglst);
#endif /* _WIDE */
				fpos = 0;
			}
			if (flagword & DOTSEEN) {
				position = prec;
				prec = 0;
			} else {
				position = width;
				width = 0;
			}
			if (position <= 0) {
				/* illegal position */
				format--;
				continue;
			}
			if (position <= MAXARGS) {
				targs = arglst[position - 1];
			} else {
				targs = arglst[MAXARGS - 1];
#ifdef	_WIDE
				_wgetarg(sformat, &targs, position);
#else  /* _WIDE */
				_getarg(sformat, &targs, position);
#endif /* _WIDE */
			}
			if (!starflg)
				args = targs;
			else {
				starflg = 0;
				args = bargs;
				if (flagword & DOTSEEN) {
					prec = va_arg(targs.ap, int);
					if (prec < 0) {
						prec = 0;
						flagword ^= DOTSEEN; /* XSH */
					}
				} else {
					width = va_arg(targs.ap, int);
					if (width < 0) {
						width = -width;
						flagword |= FMINUS;
					}
				}
			}
			goto charswitch;
			}

		case '0':	/* obsolescent spec:  leading zero in width */
				/* means pad with leading zeros */
			if (!(flagword & (DOTSEEN | FMINUS)))
				flagword |= PADZERO;
			/* FALLTHROUGH */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			{ num = fcode - '0';
			while (_M_ISDIGIT(fcode = *format)) {
				num = num * 10 + fcode - '0';
				format++;
			}
			if (flagword & DOTSEEN)
				prec = num;
			else
				width = num;
			goto charswitch;
			}

		/* Scan the length modifier */
		case 'l':
			if (! (flagword & XLONG)) {
				if (flagword & LENGTH) {
					/* long long */
					flagword &= ~LENGTH;
					flagword |= XLONG;
				} else	/* long */
					flagword |= LENGTH;
			}
			lflag++;
			goto charswitch;
#if !defined(_NO_LONG_DOUBLE)
		case 'L':			/* long double */
			flagword |= QUAD;
			goto charswitch;
#endif  /* _NO_LONG_DOUBLE */
		case 'h':
			flagword |= SHORT;
			goto charswitch;

		/*
		 *	The character addressed by format must be
		 *	the format letter -- there is nothing
		 *	left for it to be.
		 *
		 *	The status of the +, -, #, and blank
		 *	flags are reflected in the variable
		 *	"flagword".  "width" and "prec" contain
		 *	numbers corresponding to the digit
		 *	strings before and after the decimal
		 *	point, respectively. If there was no
		 *	decimal point, then flagword & DOTSEEN
		 *	is false and the value of prec is meaningless.
		 *
		 *	The following switch cases set things up
		 *	for printing.  What ultimately gets
		 *	printed will be padding blanks, a
		 *	prefix, left padding zeroes, a value,
		 *	right padding zeroes, a suffix, and
		 *	more padding blanks.  Padding blanks
		 *	will not appear simultaneously on both
		 *	the left and the right.  Each case in
		 *	this switch will compute the value, and
		 *	leave in several variables the informa-
		 *	tion necessary to construct what is to
		 *	be printed.
		 *
		 *	The prefix is a sign, a blank, "0x",
		 *	"0X", or null, and is addressed by
		 *	"prefix".
		 *
		 *	The suffix is either null or an
		 *	exponent, and is addressed by "suffix".
		 *	If there is a suffix, the flagword bit
		 *	SUFFIX will be set.
		 *
		 *	The value to be printed starts at "bp"
		 *	and continues up to and not including
		 *	"p".
		 *
		 *	"lzero" and "rzero" will contain the
		 *	number of padding zeroes required on
		 *	the left and right, respectively.
		 *	The flagword bits LZERO and RZERO tell
		 *	whether padding zeros are required.
		 *
		 *	The number of padding blanks, and
		 *	whether they go on the left or the
		 *	right, will be computed on exit from
		 *	the switch.
		 */



		/*
		 *	decimal fixed point representations
		 *
		 *	HIBITL is 100...000
		 *	binary, and is equal to	the maximum
		 *	negative number.
		 *	We assume a 2's complement machine
		 */

		case 'i':
		case 'd':
			if ((flagword & PADZERO) && (flagword & DOTSEEN))
				flagword &= ~PADZERO; /* ignore 0 flag */

			/* Fetch the argument to be printed */
			if (flagword & XLONG) {		/* long long */
				ll = va_arg(args.ap, long long);

			/* Set buffer pointer to last digit */
			p = bp = buf + MAXLLDIGS;

			/* If signed conversion, make sign */
			if (ll < 0) {
				prefix = _P_HYPHEN;
				prefixlength = 1;
				/*
				 * Negate, checking in
				 * advance for possible
				 * overflow.
				 */
				if (ll != HIBITLL)
					ll = -ll;
				else
				/* number is -HIBITLL; convert last */
				/* digit now and get positive number */
					*--bp = _lowlldigit(&ll);
			} else if (flagword & FPLUS) {
				prefix = _P_PLUS;
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = _P_BLANK;
				prefixlength = 1;
			}
			} else {		/* not long long */
				if (flagword & LENGTH)
					val = va_arg(args.ap, long);
				else
					val = va_arg(args.ap, int);

				if (flagword & SHORT)
					val = (short) val;

				/* Set buffer pointer to last digit */
				p = bp = buf + MAXDIGS;

				/* If signed conversion, make sign */
				if (val < 0) {
					prefix = _P_HYPHEN;
				prefixlength = 1;
				/*
				 * Negate, checking in
				 * advance for possible
				 * overflow.
				 */
				if (val != HIBITL)
					val = -val;
				/* number is -HIBITL; convert last */
				/* digit now and get positive number */
				else
					*--bp = _lowdigit(&val);
			} else if (flagword & FPLUS) {
				prefix = _P_PLUS;
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = _P_BLANK;
				prefixlength = 1;
			}
			}

		decimal:
			{ long qval = val;
		long long lll = ll;
		long long tll;
		if (flagword & XLONG) {
				if (lll < 10LL) {
#ifdef	_WIDE
					if (lll != 0LL || !(flagword & DOTSEEN))
						*--bp = (wchar_t) lll + L'0';
#else  /* _WIDE */
					if (lll != 0LL || !(flagword & DOTSEEN))
						*--bp = (char) lll + '0';
#endif /* _WIDE */
				} else {
					do {
						tll = lll;
						lll /= 10;
#ifdef	_WIDE
						*--bp = (wchar_t)
							(tll - lll * 10 + '0');
#else  /* _WIDE */
						*--bp = (char) \
						    (tll - lll * 10 + '0');
#endif /* _WIDE */
					} while (lll >= 10);
#ifdef	_WIDE
					*--bp = (wchar_t) lll + '0';
#else  /* _WIDE */
					*--bp = (char) lll + '0';
#endif /* _WIDE */
				}
				} else {
				if (qval <= 9) {
#ifdef	_WIDE
					if (qval != 0 || !(flagword & DOTSEEN))
						*--bp = (wchar_t) qval + '0';
#else  /* _WIDE */
					if (qval != 0 || !(flagword & DOTSEEN))
						*--bp = (char) qval + '0';
#endif /* _WIDE */
				} else {
					do {
						n = qval;
						qval /= 10;
#ifdef	_WIDE
						*--bp = (wchar_t) \
						    (n - qval * 10 + '0');
#else  /* _WIDE */
						*--bp = (char) \
						    (n - qval * 10 + '0');
#endif /* _WIDE */
					} while (qval > 9);
#ifdef	_WIDE
					*--bp = (wchar_t) qval + '0';
#else  /* _WIDE */
					*--bp = (char) qval + '0';
#endif /* _WIDE */
					}
				}
			}
			/* Handle the ' flag */
			if (quote) {
				switch (fcode) {
					case 'd':
					case 'i':
					case 'u':
						p = insert_thousands_sep(bp, p);
						break;
				}
			}

			/* Calculate minimum padding zero requirement */
			if (flagword & DOTSEEN) {
				leadzeroes = prec - (p - bp);
				if (leadzeroes > 0) {
					otherlength = lzero = leadzeroes;
					flagword |= LZERO;
				}
			}
			break;

		case 'u':
			if ((flagword & PADZERO) && (flagword & DOTSEEN))
				flagword &= ~PADZERO; /* ignore 0 flag */

			/* Fetch the argument to be printed */
			if (flagword & XLONG) {
			ll = va_arg(args.ap, long long);

			p = bp = buf + MAXLLDIGS;

			if (ll & HIBITLL)
				*--bp = _lowlldigit(&ll);
			} else {
				if (flagword & LENGTH)
					val = va_arg(args.ap, long);
				else
					val = va_arg(args.ap, unsigned);

				if (flagword & SHORT)
					val = (unsigned short)val;

				p = bp = buf + MAXDIGS;

				if (val & HIBITL)
					*--bp = _lowdigit(&val);
			}

			goto decimal;

		/*
		 *	non-decimal fixed point representations
		 *	for radix equal to a power of two
		 *
		 *	"mradix" is one less than the radix for the conversion.
		 *	"lradix" is one less than the base 2 log
		 *	of the radix for the conversion. Conversion is unsigned.
		 *	HIBITL is 100...000
		 *	binary, and is equal to	the maximum
		 *	negative number.
		 *	We assume a 2's complement machine
		 */

		case 'o':
			mradix = 7;
			lradix = 2;
			goto fixed;

		case 'p':
			flagword &= ~(XLONG | SHORT);
			flagword |= LENGTH;

		case 'X':
		case 'x':
			mradix = 15;
			lradix = 3;

		fixed:
			if ((flagword & PADZERO) && (flagword & DOTSEEN))
				flagword &= ~PADZERO; /* ignore 0 flag */

#ifdef	_WIDE
			/* Set translate table for digits */
			tab = (wchar_t *)((fcode == 'X') ? uc_digs : lc_digs);
#else  /* _WIDE */
			/* Set translate table for digits */
			tab = (fcode == 'X') ? uc_digs : lc_digs;
#endif /* _WIDE */

			/* Fetch the argument to be printed */
			if (flagword & XLONG) {
				ll = va_arg(args.ap, long long);
			} else {
				if (flagword & LENGTH)
					val = va_arg(args.ap, long);
				else
					val = va_arg(args.ap, unsigned);

				if (flagword & SHORT)
					val = (unsigned short) val;
			}

#ifdef	HANDLE_INFNaN
			/* Entry point when printing a double which is a NaN */
		put_pc:
#endif	/* defined(HANDLE_INFNaN) */

			/* Develop the digits of the value */
			if (flagword & XLONG) {
				long long lll = ll;

				p = bp = buf + MAXLLDIGS;
				if (lll == 0LL) {
					if (!(flagword & DOTSEEN)) {
						otherlength = lzero = 1;
						flagword |= LZERO;
					}
				} else do {
					*--bp = tab[(ssize_t) (lll & mradix)];
					lll = ((lll >> 1) & ~HIBITLL) \
					    >> lradix;
				} while (lll != 0LL);
			} else {
				long qval = val;

				p = bp = buf + MAXDIGS;
				if (qval == 0) {
					if (!(flagword & DOTSEEN)) {
						otherlength = lzero = 1;
						flagword |= LZERO;
					}
				} else do {
					*--bp = tab[qval & mradix];
					qval = ((qval >> 1) & ~HIBITL) \
					    >> lradix;
				} while (qval != 0);
			}

			/* Calculate minimum padding zero requirement */
			if (flagword & DOTSEEN) {
				leadzeroes = prec - (p - bp);
				if (leadzeroes > 0) {
					otherlength = lzero = leadzeroes;
					flagword |= LZERO;
				}
			}

			/* Handle the # flag, (val != 0) for int and long */
			/* (ll!= 0) handles long long case */
			if ((flagword & FSHARP) &&
			    (((flagword & XLONG) == 0 && val != 0) ||
			    ((flagword & XLONG) == XLONG && ll != 0)))
				switch (fcode) {
				case 'o':
					if (!(flagword & LZERO)) {
						otherlength = lzero = 1;
						flagword |= LZERO;
					}
					break;
				case 'x':
					prefix = _P_ZEROx;
					prefixlength = 2;
					break;
				case 'X':
					prefix = _P_ZEROX;
					prefixlength = 2;
					break;
				}

			break;

		case 'E':
		case 'e':
			/*
			 * E-format.  The general strategy
			 * here is fairly easy: we take
			 * what ecvt gives us and re-format it.
			 * (qecvt for long double)
			 */

			/* Establish default precision */
			if (!(flagword & DOTSEEN))
				prec = 6;

#if !defined(_NO_LONG_DOUBLE)
			if (flagword & QUAD) {	/* long double */
				/* Fetch the value */
				quadval = GETQVAL(args.ap);

#ifdef	HANDLE_INFNaN
			/* Check for NaNs and Infinities */
			if (QIsNANorINF(quadval))  {
				if (QIsINF(quadval)) {
					if (QIsNegNAN(quadval))
						neg_in = 1;
					inf_nan = 1;
#ifdef	_WIDE
					bp = (wchar_t *)((fcode == 'E')? uc_inf:
						lc_inf);
#else  /* _WIDE */
					bp = (fcode == 'E')? uc_inf: lc_inf;
#endif /* _WIDE */
					p = bp + 3;
					break;
				} else {
					if (QIsNegNAN(quadval))
						neg_in = 1;
				inf_nan = 1;
				val  = QGETNaNPC(quadval);
				NaN_flg = SNLEN;
				mradix = 15;
				lradix = 3;
				if (fcode == 'E') {
#ifdef	_WIDE
					SNAN = (wchar_t *)uc_nan;
					tab =  (wchar_t *)uc_digs;
#else  /* _WIDE */
					SNAN = uc_nan;
					tab =  uc_digs;
#endif /* _WIDE */
				} else {
#ifdef	_WIDE
					SNAN = (wchar_t *)lc_nan;
					tab =  (wchar_t *)lc_digs;
#else /* _WIDE */
					SNAN =  lc_nan;
					tab =  lc_digs;
#endif /* _WIDE */
				}
				goto put_pc;
				}
			}
#endif	/* defined(HANDLE_INFNaN) */

#ifdef	_WIDE
			(void) qeconvert(&quadval, min(prec + 1, MAXECVT), \
			    &decpt, &sign, cvtbuf);
			{

				wchar_t	*wp;
				char	*cp;
				wp = wcvtbuf;
				cp = cvtbuf;
				while (*cp) {
					*wp++ = (wchar_t)*cp++;
				}
				*wp = L'\0';
				bp = wcvtbuf;
			}
#else  /* _WIDE */
			/* Develop the mantissa */
			bp = qeconvert(&quadval, min(prec + 1, MAXECVT), \
			    &decpt, &sign, cvtbuf);
#endif /* _WIDE */
#ifndef	HANDLE_INFNaN
			if (*bp > '9') {
				inf_nan = 1;
				break;
			}
#endif	/* !defined(HANDLE_INFNaN) */
			} else		/* double */
#endif  /* _NO_LONG_DOUBLE */
			{
			/* Fetch the value */
			dval = va_arg(args.ap, double);

#ifdef	HANDLE_INFNaN
			/* Check for NaNs and Infinities */
			if (IsNANorINF(dval))  {
				if (IsINF(dval)) {
					if (IsNegNAN(dval))
						neg_in = 1;
				inf_nan = 1;
#ifdef	_WIDE
				bp = (wchar_t *)((fcode == 'E')? uc_inf:
					lc_inf);
#else  /* _WIDE */
				bp = (fcode == 'E')? uc_inf: lc_inf;
#endif /* _WIDE */
				p = bp + 3;
				break;
				} else {
					if (IsNegNAN(dval))
						neg_in = 1;
				inf_nan = 1;
				val  = GETNaNPC(dval);
				NaN_flg = SNLEN;
				mradix = 15;
				lradix = 3;
				if (fcode == 'E') {
#ifdef	_WIDE
					SNAN = (wchar_t *)uc_nan;
					tab =  (wchar_t *)uc_digs;
#else  /* _WIDE */
					SNAN = uc_nan;
					tab =  uc_digs;
#endif /* _WIDE */
				} else {
#ifdef	_WIDE
					SNAN = (wchar_t *)lc_nan;
					tab =  (wchar_t *)lc_digs;
#else  /* _WIDE */
					SNAN =  lc_nan;
					tab =  lc_digs;
#endif /* _WIDE */
				}
				goto put_pc;
				}
			}
#endif	/* defined(HANDLE_INFNaN) */
#ifdef	_WIDE
			(void) econvert(dval, min(prec +1, MAXECVT), \
			    &decpt, &sign, cvtbuf);
			{
				wchar_t	*wp;
				char	*cp;
				wp = wcvtbuf;
				cp = cvtbuf;
				while (*cp) {
					*wp++ = (wchar_t)*cp++;
				}
				*wp = L'\0';
				bp = wcvtbuf;
			}
#else  /* _WIDE */
			/* Develop the mantissa */
			bp = econvert(dval, min(prec +1, MAXECVT), \
			    &decpt, &sign, cvtbuf);
#endif /* _WIDE */

#ifndef	HANDLE_INFNaN
			if (*bp > '9') {
				inf_nan = 1;
				break;
			}
#endif	/* !defined(HANDLE_INFNaN) */
			}

			/* Determine the prefix */
		e_merge:
			if (sign) {
				prefix = _P_HYPHEN;
				prefixlength = 1;
			} else if (flagword & FPLUS) {
				prefix = _P_PLUS;
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = _P_BLANK;
				prefixlength = 1;
			}

			/* Place the first digit in the buffer */
			p = &buf[0];
			*p++ = (*bp != '\0') ? *bp++ : '0';

			/* Put in a decimal point if needed */
			if (prec != 0 || (flagword & FSHARP))
				*p++ = _numeric[0];

			/* Create the rest of the mantissa */
			{ rz = prec;
				for (; rz > 0 && *bp != '\0'; --rz)
					*p++ = *bp++;
				if (rz > 0) {
					otherlength = rzero = rz;
					flagword |= RZERO;
				}
			}

			{
			long	is_zero;
#if !defined(_NO_LONG_DOUBLE)
			if (flagword & QUAD)
				is_zero = (quadval == 0.0) ? 1 : 0;
			else
#endif  /* _NO_LONG_DOUBLE */
				is_zero = (dval == 0.0) ? 1 : 0;
			bp = &buf[0];

			/* Create the exponent */
			*(suffix = &expbuf[MAXESIZ]) = '\0';
			if (! is_zero) {
				int nn = decpt - 1;
				if (nn < 0)
				    nn = -nn;
				for (; nn > 9; nn /= 10)
					*--suffix = todigit(nn % 10);
				*--suffix = todigit(nn);
			}

			/* Prepend leading zeroes to the exponent */
			while (suffix > &expbuf[MAXESIZ - 2])
				*--suffix = '0';

			/* Put in the exponent sign */
			*--suffix = (decpt > 0 || is_zero) ? '+' : '-';

			/* Put in the e */
			*--suffix = _M_ISUPPER(fcode) ? 'E'  : 'e';

			/* compute size of suffix */
			otherlength += (suffixlength = &expbuf[MAXESIZ] \
			    - suffix);
			flagword |= SUFFIX;
			}

			break;

		case 'f':
			/*
			 * F-format floating point.  This is a
			 * good deal less simple than E-format.
			 * The overall strategy will be to call
			 * fcvt, reformat its result into buf,
			 * and calculate how many trailing
			 * zeroes will be required.  There will
			 * never be any leading zeroes needed.
			 * (gcvt for long double)
			 */

			/* Establish default precision */
			if (!(flagword & DOTSEEN))
				prec = 6;

#if !defined(_NO_LONG_DOUBLE)
			if (flagword & QUAD) {	/* long double */
			/* Fetch the value */
				quadval = GETQVAL(args.ap);
#ifdef	HANDLE_INFNaN
			/* Check for NaNs and Infinities  */
			if (QIsNANorINF(quadval)) {
				if (QIsINF(quadval)) {
					if (QIsNegNAN(quadval))
						neg_in = 1;
					inf_nan = 1;
#ifdef	_WIDE
					bp = (wchar_t *)lc_inf;
#else  /* _WIDE */
					bp = lc_inf;
#endif /* _WIDE */
					p = bp + 3;
					break;
				} else {
					if (QIsNegNAN(quadval))
						neg_in = 1;
					inf_nan = 1;
					val  = QGETNaNPC(quadval);
					NaN_flg = SNLEN;
					mradix = 15;
					lradix = 3;
#ifdef	_WIDE
					tab =  (wchar_t *)lc_digs;
					SNAN = (wchar_t *)lc_nan;
#else  /* _WIDE */
					tab =  lc_digs;
					SNAN = lc_nan;
#endif /* _WIDE */
					goto put_pc;
				}
			}
#endif	/* defined HANDLE_INFNaN */
#ifdef	_WIDE
			(void) qfconvert(&quadval, min(prec, MAXFCVT), &decpt, \
			    &sign, cvtbuf);
			{
				wchar_t	*wp;
				char	*cp;
				wp = wcvtbuf;
				cp = cvtbuf;
				while (*cp) {
					*wp++ = (wchar_t)*cp++;
				}
				*wp = L'\0';
				bp = wcvtbuf;
			}
#else  /* _WIDE */
			/* Do the conversion */
			bp = qfconvert(&quadval, min(prec, MAXFCVT), &decpt, \
			    &sign, cvtbuf);
#endif /* _WIDE */

#ifndef	HANDLE_INFNaN
			if (*bp > '9') {
				inf_nan = 1;
				break;
			}
#endif	/* !defined(HANDLE_INFNaN) */
			if (bp[0] == 0) {
#ifdef	_WIDE
			(void) qeconvert(&quadval, min(prec + 1, MAXECVT), \
			    &decpt, &sign, cvtbuf);
			{
				wchar_t	*wp;
				char	*cp;
				wp = wcvtbuf;
				cp = cvtbuf;
				while (*cp) {
					*wp++ = (wchar_t)*cp++;
				}
				*wp = L'\0';
				bp = wcvtbuf;
			}
#else  /* _WIDE */
			/* qeconvert overflow internal decimal record */
			bp = qeconvert(&quadval, min(prec + 1, MAXECVT), \
			    &decpt, &sign, cvtbuf);
#endif /* _WIDE */

			goto e_merge;
				}
			} else  	/* double */
#endif  /* _NO_LONG_DOUBLE */
			{
			/* Fetch the value */
			dval = va_arg(args.ap, double);
#ifdef	HANDLE_INFNaN
			/* Check for NaNs and Infinities  */
			if (IsNANorINF(dval)) {
				if (IsINF(dval)) {
					if (IsNegNAN(dval))
						neg_in = 1;
				inf_nan = 1;
#ifdef	_WIDE
				bp = (wchar_t *)lc_inf;
#else  /* _WIDE */
				bp = lc_inf;
#endif /* _WIDE */
				p = bp + 3;
				break;
				} else {
					if (IsNegNAN(dval))
						neg_in = 1;
					inf_nan = 1;
					val  = GETNaNPC(dval);
					NaN_flg = SNLEN;
					mradix = 15;
					lradix = 3;
#ifdef	_WIDE
					tab =  (wchar_t *)lc_digs;
					SNAN = (wchar_t *)lc_nan;
#else  /* _WIDE */
					tab =  lc_digs;
					SNAN = lc_nan;
#endif /* _WIDE */
					goto put_pc;
				}
			}
#endif	/* defined(HANDLE_INFNaN) */
#ifdef	_WIDE
			(void) fconvert(dval, min(prec, MAXFCVT), &decpt, \
			    &sign, cvtbuf);
			{
				wchar_t	*wp;
				char	*cp;
				wp = wcvtbuf;
				cp = cvtbuf;
				while (*cp) {
					*wp++ = (wchar_t)*cp++;
				}
				*wp = L'\0';
				bp = wcvtbuf;
			}
#else  /* _WIDE */
			/* Do the conversion */
			bp = fconvert(dval, min(prec, MAXFCVT), &decpt, \
			    &sign, cvtbuf);
#endif /* _WIDE */

#ifndef	HANDLE_INFNaN
			if (*bp > '9') {
				inf_nan = 1;
				break;
			}
#endif	/* !defined(HANDLE_INFNaN) */
			}
			/* Determine the prefix */
		f_merge:
			if (sign) {
				prefix = _P_HYPHEN;
				prefixlength = 1;
			} else if (flagword & FPLUS) {
				prefix = _P_PLUS;
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = _P_BLANK;
				prefixlength = 1;
			}

			/* Initialize buffer pointer */
			p = &buf[0];

			{
			ssize_t nn = decpt;

			/* Emit the digits before the decimal point */
			k = 0;
			do {
				*p++ = (nn <= 0 || *bp == '\0' || \
				    k >= MAXFSIG) ? '0' : (k++, *bp++);
			} while (--nn > 0);

				if (quote)
					p = insert_thousands_sep(buf, p);

				/* Decide whether we need a decimal point */
				if ((flagword & FSHARP) || prec > 0)
					*p++ = _numeric[0];

				/* Digits (if any) after the decimal point */
				nn = min(prec, MAXFCVT);
				if (prec > nn) {
					flagword |= RZERO;
					otherlength = rzero = prec - nn;
				}
				while (--nn >= 0)
					*p++ = (++decpt <= 0 || *bp == '\0' || \
					    k >= MAXFSIG) ? '0' : (k++, *bp++);
			}

			bp = &buf[0];

			break;

		case 'G':
		case 'g':
			/*
			 * g-format.  We play around a bit
			 * and then jump into e or f, as needed.
			 */

			/* Establish default precision */
			if (!(flagword & DOTSEEN))
				prec = 6;
			else if (prec == 0)
				prec = 1;

#if !defined(_NO_LONG_DOUBLE)
			if (flagword & QUAD) {	/* long double */
			/* Fetch the value */
			quadval = GETQVAL(args.ap);

#ifdef	HANDLE_INFNaN
			/* Check for NaN and Infinities  */
			if (QIsNANorINF(quadval)) {
				if (QIsINF(quadval)) {
					if (QIsNegNAN(quadval))
						neg_in = 1;
#ifdef	_WIDE
				bp = (wchar_t *)((fcode == 'G') ? uc_inf :
					lc_inf);
#else  /* _WIDE */
				bp = (fcode == 'G') ? uc_inf : lc_inf;
#endif /* _WIDE */
				p = bp + 3;
				inf_nan = 1;
				break;
				} else {
					if (QIsNegNAN(quadval))
						neg_in = 1;
				inf_nan = 1;
				val  = QGETNaNPC(quadval);
				NaN_flg = SNLEN;
				mradix = 15;
				lradix = 3;
				if (fcode == 'G') {
#ifdef	_WIDE
					SNAN = (wchar_t *)uc_nan;
					tab = (wchar_t *)uc_digs;
#else /* _WIDE */
					SNAN = uc_nan;
					tab = uc_digs;
#endif /* _WIDE */
				} else {
#ifdef	_WIDE
					SNAN = (wchar_t *)lc_nan;
					tab =  (wchar_t *)lc_digs;
#else  /* _WIDE */
					SNAN = lc_nan;
					tab =  lc_digs;
#endif /* _WIDE */
				}
				goto put_pc;
				}
			}
#endif	/* defined(HANDLE_INFNaN) */
#ifdef	_WIDE
			(void) qeconvert(&quadval, min(prec, MAXECVT), &decpt, \
			    &sign, cvtbuf);
			{
				wchar_t	*wp;
				char	*cp;
				wp = wcvtbuf;
				cp = cvtbuf;
				while (*cp) {
					*wp++ = (wchar_t)*cp++;
				}
				*wp = L'\0';
				bp = wcvtbuf;
			}
#else  /* _WIDE */
			/* Do the conversion */
			bp = qeconvert(&quadval, min(prec, MAXECVT), &decpt, \
			    &sign, cvtbuf);
#endif /* _WIDE */

#ifndef	HANDLE_INFNaN
			if (*bp > '9') {
				inf_nan = 1;
				break;
			}
#endif	/* !defined(HANDLE_INFNaN) */
			if (quadval == 0)
				decpt = 1;
			} else  	/* double */
#endif  /* _NO_LONG_DOUBLE */
			{
			/* Fetch the value */
			dval = va_arg(args.ap, double);
#ifdef	HANDLE_INFNaN
			/* Check for NaN and Infinities  */
			if (IsNANorINF(dval)) {
				if (IsINF(dval)) {
					if (IsNegNAN(dval))
						neg_in = 1;
#ifdef	_WIDE
					bp = (wchar_t *)((fcode == 'G') ?
						uc_inf : lc_inf);
#else  /* _WIDE */
					bp = (fcode == 'G') ? uc_inf : lc_inf;
#endif /* _WIDE */
					p = bp + 3;
					inf_nan = 1;
					break;
				} else {
					if (IsNegNAN(dval))
						neg_in = 1;
					inf_nan = 1;
					val  = GETNaNPC(dval);
					NaN_flg = SNLEN;
					mradix = 15;
					lradix = 3;
					if (fcode == 'G') {
#ifdef	_WIDE
						SNAN = (wchar_t *)uc_nan;
						tab = (wchar_t *)uc_digs;
#else  /* _WIDE */
						SNAN = uc_nan;
						tab = uc_digs;
#endif /* _WIDE */
					} else {
#ifdef	_WIDE
						SNAN = (wchar_t *)lc_nan;
						tab =  (wchar_t *)lc_digs;
#else  /* _WIDE */
						SNAN = lc_nan;
						tab =  lc_digs;
#endif /* _WIDE */
					}
					goto put_pc;
				}
			}
#endif	/* defined(HANDLE_INFNaN) */
#ifdef	_WIDE
			(void) econvert(dval, min(prec, MAXECVT), &decpt, \
			    &sign, cvtbuf);
			{
				wchar_t	*wp;
				char	*cp;
				wp = wcvtbuf;
				cp = cvtbuf;
				while (*cp) {
					*wp++ = (wchar_t)*cp++;
				}
				*wp = L'\0';
				bp = wcvtbuf;
			}
#else  /* _WIDE */
			/* Do the conversion */
			bp = econvert(dval, min(prec, MAXECVT), &decpt, \
			    &sign, cvtbuf);
#endif /* _WIDE */

#ifndef	HANDLE_INFNaN
			if (*bp > '9') {
				inf_nan = 1;
				break;
			}
#endif	/* !defined(HANDLE_INFNaN) */
			if (dval == 0)
				decpt = 1;
			}

			{ int kk = prec;
				if (!(flagword & FSHARP)) {
#ifdef	_WIDE
					n = wslen(bp);
#else  /* _WIDE */
					n = strlen(bp);
#endif /* _WIDE */
					if (n < kk)
						kk = (int) n;
					while (kk >= 1 && bp[kk-1] == '0')
						--kk;
				}
				if (decpt < -3 || decpt > prec) {
					prec = kk - 1;
					goto e_merge;
				}
				prec = kk - decpt;
				goto f_merge;
			}

		case '%':
			buf[0] = fcode;
			goto c_merge;

#ifndef	_WIDE
		case 'w':
			wflag = 1;
			goto charswitch;
#endif /* _WIDE */


		case 'C': /* XPG XSH4 extention */
wide_C:
			{
				wchar_t	temp;

				temp = va_arg(args.ap, wchar_t);
#ifdef	_WIDE
				if (temp) {
					buf[0] = temp;
					p = (bp = buf) + 1;
				} else {
					buf[0] = 0;
					p = (bp = buf) + 1;
				}
				wcount = 1;
				wflag = 1;
#else  /* _WIDE */
				if (temp) {
					if ((retcode = wctomb(buf, temp))
						== -1) {
						errno = EILSEQ;
						return (EOF);
					} else {
						p = (bp = buf) + retcode;
					}
				} else { /* NULL character */
					buf[0] = 0;
					p = (bp = buf) + 1;
				}
				wcount = p - bp;
#endif /* _WIDE */
			}
			break;
		case 'c':
			if (lflag) {
				goto wide_C;
			}
#ifndef	_WIDE
			if (wflag) {
				wchar_t	temp;

				temp = va_arg(args.ap, wchar_t);
				if (temp) {
					if ((retcode = wctomb(buf, temp))
						== -1) {
						p = (bp = buf) + 1;
					} else {
						p = (bp = buf) + retcode;
					}
				} else { /* NULL character */
					buf[0] = 0;
					p = (bp = buf) + 1;
				}
				wcount = p - bp;
			} else {
#endif /* _WIDE */
				if (flagword & XLONG) {
					long long temp;
					temp = va_arg(args.ap, long long);
#ifdef	_WIDE
					buf[0] = (wchar_t) temp;
#else  /* _WIDE */
					buf[0] = (char) temp;
#endif /* _WIDE */
				} else
					buf[0] = va_arg(args.ap, int);
			c_merge:
				p = (bp = &buf[0]) + 1;
#ifdef	_WIDE
				wcount = 1;
				wflag = 1;
#endif /* _WIDE */
#ifndef	_WIDE
			}
#endif /* _WIDE */
			break;

		case 'S': /* XPG XSH4 extention */
wide_S:
#ifdef	_WIDE
			if (!lflag) {
				lflag++;
			}
			bp = va_arg(args.ap, wchar_t *);
			if (!(flagword & DOTSEEN)) {
				/* wide character handling */
				prec = MAXINT;
			}

			wp = bp;
			wcount = 0;
			while (*wp) {
				if ((prec - wcount - 1) >= 0) {
					wcount++;
					wp++;
				} else {
					break;
				}
			}
			p = wp;
			wflag = 1;
			break;
#else  /* _WIDE */
			if (!wflag)
				wflag++;
			bp = va_arg(args.ap, char *);
			if (!(flagword & DOTSEEN)) {
				/* wide character handling */
				prec = MAXINT;
			}

			wp = (wchar_t *)bp;
			wcount = 0;
			while (*wp) {
				int nbytes;

				nbytes = wctomb(tmpbuf, *wp);
				if (nbytes < 0) {
					errno = EILSEQ;
					return (EOF);
				}
				if ((prec - (wcount + nbytes)) >= 0) {
					wcount += nbytes;
					wp++;
				} else {
					break;
				}
			}
			sec_display = wcount;
			p = (char *)wp;
			break;
#endif /* _WIDE */
		case 's':
			if (lflag) {
				goto wide_S;
			}
#ifdef	_WIDE
			cbp = va_arg(args.ap, char *);
			if (!(flagword & DOTSEEN)) {
				size_t	nwc;
				wchar_t	*wstr;

				nwc = mbstowcs(NULL, cbp, 0);
				if (nwc == (size_t)-1) {
					errno = EILSEQ;
					return (EOF);
				}
				wstr = (wchar_t *)malloc(sizeof (wchar_t) *
					(nwc + 1));
				if (wstr == NULL) {
					errno = EILSEQ;
					return (EOF);
				}
				nwc = mbstowcs(wstr, cbp, MAXINT);
				wcount = nwc;
				bp = wstr;
				p = wstr + nwc;
			} else {
				size_t	nwc;
				wchar_t	*wstr;

				nwc = mbstowcs(NULL, cbp, 0);
				if (nwc == (size_t)-1) {
					errno = EILSEQ;
					return (EOF);
				}
				if (prec > nwc) {
					wstr = (wchar_t *)malloc(
						sizeof (wchar_t) * nwc);
					if (wstr == NULL) {
						errno = ENOMEM;
						return (EOF);
					}
					nwc = mbstowcs(wstr, cbp, nwc);
					cp = cbp + strlen(cbp);
					wcount = nwc;
					bp = wstr;
					p = wstr + nwc;
				} else {
					size_t	nnwc;
					int	len;
					char	*s;
					wchar_t	*wstr;

					wstr = (wchar_t *)malloc(
						sizeof (wchar_t) * prec);
					if (wstr == NULL) {
						errno = ENOMEM;
						return (EOF);
					}
					nwc = mbstowcs(wstr, cbp, prec);
					wcount = prec;
					bp = wstr;
					p = wstr + nwc;
				}
			}
			wflag = 1;
#else  /* _WIDE */
			bp = va_arg(args.ap, char *);
			if (!(flagword & DOTSEEN)) {
				if (wflag) {
					/* wide character handling */
					prec = MAXINT;
					goto wide_hand;
				}


				p = bp + strlen(bp);

				/*
				 * sec_display only needed if width
				 * is specified (ie, "%<width>s")
				 * Solaris behavior counts <width> in
				 * screen column width.  (If XPG4 behavior,
				 * <width> is counted in bytes.)
				 */
				if ((width > 0) && (__xpg4 == 0)) {
#define	NW	256
					wchar_t wbuff[NW];
					wchar_t *wp, *wptr;
					size_t nwc;

					wp = NULL;
					if ((nwc = mbstowcs(wbuff, bp,
							    NW)) == -1) {
						/* Estimate width */
						sec_display = strlen(bp);
						goto mbs_err;
					}
					if (nwc < NW) {
						wptr = wbuff;
					} else {
						/*
						 * If widechar does not fit into
						 * wbuff, allocate larger buffer
						 */
						if ((nwc = mbstowcs \
						    (NULL, bp, NULL)) == -1) {
							sec_display = \
							    strlen(bp);
							goto mbs_err;
						}
						if ((wp = (wchar_t *) \
						    malloc((nwc + 1) \
						    * sizeof (wchar_t))) \
						    == NULL) {
							errno = ENOMEM;
							return (EOF);
						}
						if ((nwc = mbstowcs(wp, \
						    bp, nwc)) == -1) {
							sec_display = \
							    strlen(bp);
							goto mbs_err;
						}
						wptr = wp;
					}
					if ((sec_display = wcswidth(wptr, nwc))
					    == -1) {
						sec_display =
							_rec_scrswidth
								(wptr, nwc);
					}
				mbs_err:
					if (wp)
						free(wp);
				}
			} else { /* a strnlen function would  be useful here! */
				if (wflag) {
					/* wide character handling */

				wide_hand:
					wp = (wchar_t *)bp;
					preco = prec;
					wcount = 0;
					while (*wp &&
					(prec -= scrwidth(*wp)) >= 0) {
					    if ((retcode = wctomb(tmpbuf, *wp))
									< 0)
						wcount++;
					    else
						wcount += retcode;
					    wp++;
					}
					if (*wp)
						prec += scrwidth(*wp);
					p = (char *)wp;
					sec_display = preco - prec;
				} else if (__xpg4 == 0) {
					/*
					 * Solaris behavior - count
					 * precision as screen column width
					 */
					char *qp = bp;
					int ncol, nbytes;
					wchar_t wc;

					ncol = 0;
					preco = prec;
					while (*qp) {
						if ((nbytes = mbtowc(&wc, qp,
							MB_LEN_MAX)) == -1) {
							/* print illegal char */
							nbytes = 1;
							ncol = 1;
						} else {
							if ((ncol =
							scrwidth(wc))
								== 0) {
								ncol = 1;
							}
						}

						if ((prec -= ncol) >= 0) {
							qp += nbytes;
							if (prec == 0)
								break;
						} else {
							break;
						}
					}
					if (prec < 0)
						prec += ncol;
					p = qp;
					sec_display = preco - prec;
				} else {
					/*
					 * XPG4 behavior - count
					 * precision as bytes
					 */
					size_t len;

					len = strlen(bp);
					if (prec < len) {
						p = bp + prec;
					} else {
						p = bp + len;
					}
				}
			}
#endif /* _WIDE */
			break;

		case 'n':
			{
			/*
			if (flagword & XLONG) {
				long long *svcount;
				svcount = va_arg(args.ap, long long *);
				*svcount = (long long)count;
			} else
			*/
			if (flagword & LENGTH) {
				long *svcount;
				svcount = va_arg(args.ap, long *);
				*svcount = (long)count;
			} else if (flagword & SHORT) {
				short *svcount;
				svcount = va_arg(args.ap, short *);
				*svcount = (short)count;
			} else {
				int *svcount;
				svcount = va_arg(args.ap, int *);
				*svcount = count;
			}
			continue;
		}
		default: /* this is technically an error; what we do is to */
			/* back up the format pointer to the offending char */
			/* and continue with the format scan */
			format--;
			continue;
		}

		if (inf_nan) {
#ifndef	HANDLE_INFNaN
		for (p = bp + 1; *p != '\0'; p++)
			;
		if (sign)
#else	/* !defined(HANDLE_INFNaN) */
		if (neg_in)
#endif	/* !defined(HANDLE_INFNaN) */
		{
			prefix = _P_HYPHEN;
			prefixlength = 1;
#ifdef	HANDLE_INFNaN
			neg_in = 0;
#endif	/* defined(HANDLE_INFNaN) */
		} else if (flagword & FPLUS) {
			prefix = _P_PLUS;
			prefixlength = 1;
		} else if (flagword & FBLANK) {
			prefix = _P_BLANK;
			prefixlength = 1;
		}
		inf_nan = 0;
		}

		/* Calculate number of padding blanks */
		n = p - bp; /* n == size of the converted value (in bytes) */

#ifdef	_WIDE
		k = n;
#else  /* _WIDE */
		if (sec_display) /* when format is %s or %ws or %S */
			k = sec_display;
		else
			k = n;
#endif /* _WIDE */
		/*
		 * k is the (screen) width or # of bytes of the converted value
		 */
		k += prefixlength + otherlength
#ifdef	HANDLE_INFNaN
			+ NaN_flg
#endif	/* defined(HANDLE_INFNaN) */
			;

#ifdef	_WIDE
		if (wflag) {
			count += wcount;
		} else {
			count += n;
		}
#else  /* _WIDE */
		/*
		 * update count which is the overall size of the output data
		 * and passed to memchr()
		 */
		if (wflag)
			/*
			 * when wflag != 0 (i.e. %ws or %wc), the size of the
			 * converted value is wcount bytes
			 */
			count += wcount;
		else
			/*
			 * when wflag == 0, the size of the converted
			 * value is n (= p-bp) bytes
			 */
			count += n;
#endif /* _WIDE */
		count += prefixlength + otherlength
#ifdef	HANDLE_INFNaN
			+ NaN_flg
#endif	/* defined(HANDLE_INFNaN) */
			;

		if (width > k) {
			count += (width - k);
			/*
			 * Set up for padding zeroes if requested
			 * Otherwise emit padding blanks unless output is
			 * to be left-justified.
			 */

			if (flagword & PADZERO) {
				if (!(flagword & LZERO)) {
					flagword |= LZERO;
					lzero = width - k;
				} else
					lzero += width - k;
				k = width; /* cancel padding blanks */
			} else
				/* Blanks on left if required */
				if (!(flagword & FMINUS))
					PAD(_blanks, width - k);
		}

		/* Prefix, if any */
		if (prefixlength != 0)
			PUT(prefix, prefixlength);

#ifdef	HANDLE_INFNaN
		/* If value is NaN, put string NaN */
		if (NaN_flg) {
			PUT(SNAN, SNLEN);
			NaN_flg = 0;
		}
#endif	/* defined(HANDLE_INFNaN) */

		/* Zeroes on the left */
		if ((flagword & LZERO)) /* && */
			/* (!(flagword & SHORT) || !(flagword & FMINUS)) */
			PAD(_zeroes, lzero);

#ifdef	_WIDE
		if (n > 0)
			PUT(bp, n);
		if ((fcode == 's') && !lflag) {
			if (bp)
				free(bp);
		}
#else  /* _WIDE */
		/* The value itself */
		if ((fcode == 's' || fcode == 'S') && wflag) {
			/* wide character handling */
			wchar_t *wp = (wchar_t *)bp;
			int cnt;
			char *bufp;
			long printn;
			printn = (wchar_t *)p - (wchar_t *)bp;
			bufp = buf;
			while (printn > 0) {
				if ((cnt = wctomb(buf, *wp)) < 0)
					cnt = 1;
			PUT(bufp, cnt);
				wp++;
				printn--;
			}
		} else {	/* non wide character value */
			if (n > 0)
				PUT(bp, n);
		}
#endif /* _WIDE */

		if (flagword & (RZERO | SUFFIX | FMINUS)) {
			/* Zeroes on the right */
			if (flagword & RZERO)
				PAD(_zeroes, rzero);

			/* The suffix */
			if (flagword & SUFFIX)
				PUT(suffix, suffixlength);

			/* Blanks on the right if required */
			if (flagword & FMINUS && width > k)
				PAD(_blanks, width - k);
		}
	}
}

#ifdef	_WIDE
static int
_watoi(wchar_t *fmt)
{
	int	n = 0;
	wchar_t	ch;

	ch = *fmt;
	if (_M_ISDIGIT(ch)) {
		n = ch - '0';
		ch = *++fmt;
		while (_M_ISDIGIT(ch)) {
			n *= 10;
			n += ch - '0';
			ch = *++fmt;
		}
	}
	return (n);
}
#endif /* _WIDE */

/*
 * This function initializes arglst, to contain the appropriate va_list values
 * for the first MAXARGS arguments.
 */

/*
 * Type modifier flags:
 *  0x01	for long
 *  0x02	for int
 *  0x04	for long long
 *  0x08	for long double
 */

#define	FLAG_LONG	0x01
#define	FLAG_INT	0x02
#define	FLAG_LONG_LONG	0x04
#define	FLAG_LONG_DBL	0x08

#ifdef	_WIDE
static void
_wmkarglst(wchar_t *fmt, stva_list args, stva_list arglst[])
#else  /* _WIDE */
void
_mkarglst(char *fmt, stva_list args, stva_list arglst[])
#endif /* _WIDE */
{
#ifdef	_WIDE
	static const wchar_t	digits[] = L"01234567890";
	static const wchar_t	skips[] = L"# +-.'0123456789h$";
#else  /* _WIDE */
	static char digits[] = "01234567890", skips[] = "# +-.'0123456789h$";
#endif /* _WIDE */
	enum types {INT = 1, LONG, CHAR_PTR, DOUBLE, LONG_DOUBLE, VOID_PTR,
		LONG_PTR, INT_PTR, LONG_LONG /* , LONG_LONG_PTR */};
	enum types typelst[MAXARGS], curtype;
	ssize_t n;
	int  maxnum, curargno, flags;

	/*
	* Algorithm	1. set all argument types to zero.
	*		2. walk through fmt putting arg types in typelst[].
	*		3. walk through args using va_arg(args.ap, typelst[n])
	*		   and set arglst[] to the appropriate values.
	* Assumptions:	Cannot use %*$... to specify variable position.
	*/

	(void) memset((void *) typelst, 0, sizeof (typelst));
	maxnum = -1;
	curargno = 0;
	while ((fmt = STRCHR(fmt, '%')) != 0) {
		fmt++;	/* skip % */
		if (fmt[n = STRSPN(fmt, digits)] == '$') {
			/* convert to zero base */
			curargno = ATOI(fmt) - 1;
			if (curargno < 0)
				continue;
			fmt += n + 1;
		}
		flags = 0;
	again:;
		fmt += STRSPN(fmt, skips);
		switch (*fmt++) {
		case '%':	/* there is no argument! */
			continue;
		case 'l':
			if (flags & (FLAG_LONG | FLAG_LONG_LONG)) {
				flags |= FLAG_LONG_LONG;
				flags &= ~FLAG_LONG;
			} else {
				flags |= FLAG_LONG;
			}
			goto again;
		case '*':	/* int argument used for value */
			/* check if there is a positional parameter */
#ifdef	_WIDE
			if ((*fmt >= 0) && (*fmt < 256) &&
				isdigit(*fmt)) {
#else  /* _WIDE */
			if (isdigit(*fmt)) {
#endif /* _WIDE */
				int	targno;
				targno = ATOI(fmt) - 1;
				fmt += STRSPN(fmt, digits);
				if (*fmt == '$')
					fmt++; /* skip '$' */
				if (targno >= 0 && targno < MAXARGS) {
					typelst[targno] = INT;
					if (maxnum < targno)
						maxnum = targno;
				}
				goto again;
			}
			flags |= FLAG_INT;
			curtype = INT;
			break;
		case 'L':
			flags |= FLAG_LONG_DBL;
			goto again;
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
			if (flags & FLAG_LONG_DBL)
				curtype = LONG_DOUBLE;
			else
				curtype = DOUBLE;
			break;
		case 's':
			curtype = CHAR_PTR;
			break;
		case 'p':
			curtype = VOID_PTR;
			break;
		case 'n':
			/*
			if (flags & FLAG_LONG_LONG)
				curtype = LONG_LONG_PTR;
			else
			*/
			if (flags & FLAG_LONG)
				curtype = LONG_PTR;
			else
				curtype = INT_PTR;
			break;
		default:
			if (flags & FLAG_LONG_LONG)
				curtype = LONG_LONG;
			else if (flags & FLAG_LONG)
				curtype = LONG;
			else
				curtype = INT;
			break;
		}
		if (curargno >= 0 && curargno < MAXARGS) {
			typelst[curargno] = curtype;
			if (maxnum < curargno)
				maxnum = curargno;
		}
		curargno++;	/* default to next in list */
		if (flags & FLAG_INT)	/* took care of *, keep going */
		{
			flags ^= FLAG_INT;
			goto again;
		}
	}
	for (n = 0; n <= maxnum; n++) {
		arglst[n] = args;
		if (typelst[n] == 0)
			typelst[n] = INT;

		switch (typelst[n]) {
		case INT:
			(void) va_arg(args.ap, int);
			break;
		case LONG:
			(void) va_arg(args.ap, long);
			break;
		case CHAR_PTR:
			(void) va_arg(args.ap, char *);
			break;
		case DOUBLE:
			(void) va_arg(args.ap, double);
			break;
		case LONG_DOUBLE:
			(void) GETQVAL(args.ap);
			break;
		case VOID_PTR:
			(void) va_arg(args.ap, void *);
			break;
		case LONG_PTR:
			(void) va_arg(args.ap, long *);
			break;
		case INT_PTR:
			(void) va_arg(args.ap, int *);
			break;
		case LONG_LONG:
			(void) va_arg(args.ap, long long);
			break;
		/*
		case LONG_LONG_PTR:
			(void) va_arg(args.ap, long long *);
			break;
		*/
		}
	}
}

/*
 * This function is used to find the va_list value for arguments whose
 * position is greater than MAXARGS.  This function is slow, so hopefully
 * MAXARGS will be big enough so that this function need only be called in
 * unusual circumstances.
 * pargs is assumed to contain the value of arglst[MAXARGS - 1].
 */
#ifdef	_WIDE
static void
_wgetarg(wchar_t *fmt, stva_list *pargs, long argno)
#else  /* _WIDE */
void
_getarg(char *fmt, stva_list *pargs, long argno)
#endif /* _WIDE */
{

#ifdef	_WIDE
	static const wchar_t	digits[] = L"01234567890";
	static const wchar_t	skips[] = L"# +-.'0123456789h$";
	wchar_t	*sfmt = fmt;
#else  /* _WIDE */
	static char digits[] = "01234567890", skips[] = "# +-.'0123456789h$";
	char	*sfmt = fmt;
#endif /* _WIDE */
	ssize_t n;
	int i, curargno, flags;
	int	found = 1;

	i = MAXARGS;
	curargno = 1;
	while (found) {
		fmt = sfmt;
		found = 0;
		while ((i != argno) && (fmt = STRCHR(fmt, '%')) != 0) {
			fmt++;	/* skip % */
			if (fmt[n = STRSPN(fmt, digits)] == '$') {
				curargno = ATOI(fmt);
				if (curargno <= 0)
					continue;
				fmt += n + 1;
			}

			/* find conversion specifier for next argument */
			if (i != curargno) {
				curargno++;
				continue;
			} else
				found = 1;
			flags = 0;
		again:;
			fmt += STRSPN(fmt, skips);
			switch (*fmt++) {
			case '%':	/* there is no argument! */
				continue;
			case 'l':
				if (flags & (FLAG_LONG | FLAG_LONG_LONG)) {
					flags |= FLAG_LONG_LONG;
					flags &= ~FLAG_LONG;
				} else {
					flags |= FLAG_LONG;
				}
				goto again;
			case 'L':
				flags |= FLAG_LONG_DBL;
				goto again;
			case '*':	/* int argument used for value */
				/*
				 * check if there is a positional parameter;
				 * if so, just skip it; its size will be
				 * correctly determined by default
				 */
				if (_M_ISDIGIT(*fmt)) {
					fmt += STRSPN(fmt, digits);
					if (*fmt == '$')
						fmt++; /* skip '$' */
					goto again;
				}
				flags |= FLAG_INT;
				(void) va_arg((*pargs).ap, int);
				break;
			case 'e':
			case 'E':
			case 'f':
			case 'g':
			case 'G':
				if (flags & FLAG_LONG_DBL)
					(void) GETQVAL((*pargs).ap);
				else
					(void) va_arg((*pargs).ap, double);
				break;
			case 's':
				(void) va_arg((*pargs).ap, char *);
				break;
			case 'p':
				(void) va_arg((*pargs).ap, void *);
				break;
			case 'n':
				/*
				if (flags & FLAG_LONG_LONG)
					(void) va_arg((*pargs).ap, long long *);
				else
				*/
				if (flags & FLAG_INT)
					(void) va_arg((*pargs).ap, long *);
				else
					(void) va_arg((*pargs).ap, int *);
				break;
			default:
				if (flags & FLAG_LONG_LONG)
					(void) va_arg((*pargs).ap, long long);
				else if (flags & FLAG_INT)
					(void) va_arg((*pargs).ap, long int);
				else
					(void) va_arg((*pargs).ap, int);
				break;
			}
			i++;
			curargno++;	/* default to next in list */
			if (flags & FLAG_INT)	/* took care of *, keep going */
			{
				flags ^= FLAG_INT;
				goto again;
			}
		}

		/* missing specifier for parameter, assume param is an int */
		if (!found && i != argno) {
			(void) va_arg((*pargs).ap, int);
			i++;
			curargno = i;
			found = 1;
		}
	}
}

#ifdef	_WIDE
static wchar_t *
insert_thousands_sep(wchar_t *bp, wchar_t *ep)
#else  /* _WIDE */
static char *
insert_thousands_sep(char *bp, char *ep)
#endif /* _WIDE */
{
	char thousep;
	struct lconv *locptr;
	ssize_t buf_index;
	int i;
#ifdef	_WIDE
	wchar_t *obp = bp;
	wchar_t buf[371];
	wchar_t *bufptr = buf;
#else  /* _WIDE */
	char *obp = bp;
	char buf[371];
	char *bufptr = buf;
#endif /* _WIDE */
	char *grp_ptr;

	/* get the thousands sep. from the current locale */
	locptr = localeconv();
	thousep	= *locptr->thousands_sep;
	grp_ptr = locptr->grouping;

	/* thousands sep. not use in this locale or no grouping required */
	if (!thousep || (*grp_ptr == '\0'))
		return (ep);

	buf_index = ep - bp;
	for (;;) {
		if (*grp_ptr == CHAR_MAX) {
			for (i = 0; i < buf_index--; i++)
				*bufptr++ = *(bp + buf_index);
			break;
		}
		for (i = 0; i < *grp_ptr && buf_index-- > 0; i++)
			*bufptr++ = *(bp + buf_index);

		if (buf_index > 0) {
#ifdef	_WIDE
			*bufptr++ = (wchar_t)thousep;
#else  /* _WIDE */
			*bufptr++ = thousep;
#endif /* _WIDE */
			ep++;
		}
		else
			break;
		if (*(grp_ptr + 1) != '\0')
			++grp_ptr;
	}

	/* put the string in the caller's buffer in reverse order */
	--bufptr;
	while (buf <= bufptr)
		*obp++ = *bufptr--;
	return (ep);
}


/*
 *  Recovery scrswidth function -
 *  this variant of wcswidth() accepts non-printable or illegal
 *  widechar characters.
 */
static int
_rec_scrswidth(wchar_t *wp, ssize_t n)
{
	int col;
	int i;

	col = 0;
	while (*wp && (n-- > 0)) {
		if ((i = scrwidth(*wp++)) == 0)
			i = 1;
		col += i;
	}
	return (col);
}
