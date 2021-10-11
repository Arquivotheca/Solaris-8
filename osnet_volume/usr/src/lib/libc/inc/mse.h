/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mse.h 1.12	99/11/03 SMI"

#ifndef	_MSE_H
#define	_MSE_H
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "stdiom.h"

typedef enum {
	_NO_MODE,					/* not bound */
	_BYTE_MODE,					/* Byte orientation */
	_WC_MODE					/* Wide orientation */
} _IOP_orientation_t;

/*
 * DESCRIPTION:
 * This function gets the pointer to the mbstate_t structure associated
 * with the specified iop.
 *
 * RETURNS:
 * If the associated mbstate_t found, the pointer to the mbstate_t is
 * returned.  Otherwise, (mbstate_t *)NULL is returned.
 */
extern mbstate_t	*_getmbstate(FILE *);

/*
 * DESCRIPTION:
 * This function/macro gets the orientation bound to the specified iop.
 *
 * RETURNS:
 * _WC_MODE	if iop has been bound to Wide orientation
 * _BYTE_MODE	if iop has been bound to Byte orientation
 * _NO_MODE	if iop has been bound to neither Wide nor Byte
 */
extern _IOP_orientation_t	_getorientation(FILE *);

/*
 * DESCRIPTION:
 * This function/macro sets the orientation to the specified iop.
 *
 * INPUT:
 * flag may take one of the following:
 *	_WC_MODE	Wide orientation
 *	_BYTE_MODE	Byte orientation
 *	_NO_MODE	Unoriented
 */
extern void	_setorientation(FILE *, _IOP_orientation_t);

/*
 * From page 32 of XSH5
 * Once a wide-character I/O function has been applied
 * to a stream without orientation, the stream becomes
 * wide-orientated.  Similarly, once a byte I/O function
 * has been applied to a stream without orientation,
 * the stream becomes byte-orientated.  Only a call to
 * the freopen() function or the fwide() function can
 * otherwise alter the orientation of a stream.
 */
extern int	_set_orientation_wide(FILE *, _LC_charmap_t **);

#define	_SET_ORIENTATION_BYTE(iop) \
{ \
	if (GET_NO_MODE(iop)) \
		_setorientation(iop, _BYTE_MODE); \
}

/* The following is specified in the argument of _get_internal_mbstate() */
#define	_MBRLEN		0
#define	_MBRTOWC	1
#define	_WCRTOMB	2
#define	_MBSRTOWCS	3
#define	_WCSRTOMBS	4
#define	_MAX_MB_FUNC	_WCSRTOMBS

extern void	_clear_internal_mbstate(void);
extern mbstate_t	*_get_internal_mbstate(int);

#define	MBSTATE_INITIAL(ps)	MBSTATE_RESTART(ps)
#define	MBSTATE_RESTART(ps) \
	(void) memset((void *)ps, 0, sizeof (mbstate_t))

/* This macro checks for control character set 1 */
#define	IS_C1(c)	(((c) >= 0x80) && ((c) <= 0x9f))

#endif	/* _MSE_H */
