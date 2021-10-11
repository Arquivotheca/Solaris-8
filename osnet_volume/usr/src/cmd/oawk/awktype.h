#ident	"@(#)awktype.h	1.3	92/07/14 SMI"

/*
 * There are additional "wctype" and "ctype" function(macros) to
 * support blank character without dependecy to locale.  This should
 * include after "ctype.h" and "wctype.h".
 */

#ifndef lint
#ifndef iswblank
#	define	iswblank(c)	((c) > 127 ? _iswctype(c, _B) \
				: isblank(c))
#endif
#ifndef isblank
#	define	isblank(c)	((__ctype + 1)[(c)&0xff] & _B)
#endif
#endif
