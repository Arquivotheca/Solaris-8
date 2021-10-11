/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mse.c 1.11	99/11/03 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include "mbstatet.h"
#include "file64.h"
#include <sys/types.h>
#include <stdio.h>
#include <wchar.h>
#include <thread.h>
#include <synch.h>
#include <stdlib.h>
#include <string.h>
#include "mtlib.h"
#include "libc.h"
#include "stdiom.h"
#include "mse.h"

/*
 * DESCRIPTION:
 * This function/macro gets the orientation bound to the specified iop.
 *
 * RETURNS:
 * _WC_MODE	if iop has been bound to Wide orientation
 * _BYTE_MODE	if iop has been bound to Byte orientation
 * _NO_MODE	if iop has been bound to neither Wide nor Byte
 */
_IOP_orientation_t
_getorientation(FILE *iop)
{
	if (GET_BYTE_MODE(iop))
		return (_BYTE_MODE);
	else if (GET_WC_MODE(iop))
		return (_WC_MODE);

	return (_NO_MODE);
}

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
void
_setorientation(FILE *iop, _IOP_orientation_t mode)
{
	switch (mode) {
	case _NO_MODE:
		CLEAR_BYTE_MODE(iop);
		CLEAR_WC_MODE(iop);
		break;
	case _BYTE_MODE:
		CLEAR_WC_MODE(iop);
		SET_BYTE_MODE(iop);
		break;
	case _WC_MODE:
		CLEAR_BYTE_MODE(iop);
		SET_WC_MODE(iop);
		break;
	}
}

static mbstate_t	**__top_mbstates = NULL;
static mutex_t	__top_mbstates_lock = DEFAULTMUTEX;

void
_clear_internal_mbstate(void)
{
	int	i;

	(void) _mutex_lock(&__top_mbstates_lock);
	if (__top_mbstates) {
		for (i = 0; i <= _MAX_MB_FUNC; i++) {
			if (*(__top_mbstates + i)) {
				free(*(__top_mbstates + i));
			}
		}
		free(__top_mbstates);
		__top_mbstates = NULL;
	}
	(void) _mutex_unlock(&__top_mbstates_lock);
}

mbstate_t *
_get_internal_mbstate(int item)
{
	if (item < 0 || item > _MAX_MB_FUNC)
		return (NULL);

	(void) _mutex_lock(&__top_mbstates_lock);
	if (__top_mbstates == NULL) {
		__top_mbstates = (mbstate_t **)
			calloc(_MAX_MB_FUNC + 1, sizeof (mbstate_t *));
		if (__top_mbstates == NULL) {
			(void) _mutex_unlock(&__top_mbstates_lock);
			return (NULL);
		}
		*(__top_mbstates + item) = (mbstate_t *)
			calloc(1, sizeof (mbstate_t));
		if (*(__top_mbstates + item) == NULL) {
			(void) _mutex_unlock(&__top_mbstates_lock);
			return (NULL);
		}
		(void) _mutex_unlock(&__top_mbstates_lock);
		return (*(__top_mbstates + item));
	}
	if (*(__top_mbstates + item) == NULL) {
		*(__top_mbstates + item) = (mbstate_t *)
			calloc(1, sizeof (mbstate_t));
		if (*(__top_mbstates + item) == NULL) {
			(void) _mutex_unlock(&__top_mbstates_lock);
			return (NULL);
		}
	}
	(void) _mutex_unlock(&__top_mbstates_lock);
	return (*(__top_mbstates + item));
}

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

/*
 * void
 * _set_orientation_byte(FILE *iop)
 *
 * Note: this is now implemented as macro __SET_ORIENTATION_BYTE()
 *       (in libc/inc/mse.h) for performance improvement.
 */

int
_set_orientation_wide(FILE *iop, _LC_charmap_t **lcharmap)
{
	_IOP_orientation_t	orientation;
	mbstate_t	*mbst;

	orientation = _getorientation(iop);

	switch (orientation) {
	case _NO_MODE:
		_setorientation(iop, _WC_MODE);
		mbst = _getmbstate(iop);
		if (mbst == NULL) {
			return (-1);
		}
		mbst->__lc_locale = (void *)__lc_locale;
		*lcharmap = __lc_charmap;
		break;

	case _WC_MODE:
		mbst = _getmbstate(iop);
		if (mbst == NULL) {
			return (-1);
		}
		*lcharmap = ((_LC_locale_t *)mbst->__lc_locale)->lc_charmap;
		break;

	case _BYTE_MODE:
		mbst = _getmbstate(iop);
		if (mbst == NULL) {
			return (-1);
		}
		if (mbst->__lc_locale == NULL)
			*lcharmap = __lc_charmap;
		else
			*lcharmap =
			    ((_LC_locale_t *)mbst->__lc_locale)->lc_charmap;
	}
	return (0);
}

/* Returns the value of 'ps->__nconsumed' */
char
__mbst_get_nconsumed(const mbstate_t *ps)
{
	return (ps->__nconsumed);
}

/* Sets 'n' to 'ps->__nconsumed' */
void
__mbst_set_nconsumed(mbstate_t *ps, char n)
{
	ps->__nconsumed = n;
}

/* Copies 'len' bytes from '&ps->__consumed[index]' to 'str' */
int
__mbst_get_consumed_array(const mbstate_t *ps, char *str,
	size_t index, size_t len)
{
	if ((index + len) > 8) {
		/* The max size of __consumed[] is 8 */
		return (-1);
	}
	(void) memcpy((void *)str, (const void *)&ps->__consumed[index],
		len);
	return (0);
}

/* Copies 'len' bytes from 'str' to '&ps->__consumed[index]' */
int
__mbst_set_consumed_array(mbstate_t *ps, const char *str,
	size_t index, size_t len)
{
	if ((index + len) > 8) {
		/* The max size of __consumed[] is 8 */
		return (-1);
	}
	(void) memcpy((void *)&ps->__consumed[index], (const void *)str,
		len);
	return (0);
}

/* Returns 'ps->__lc_locale' */
void *
__mbst_get_locale(const mbstate_t *ps)
{
	return (ps->__lc_locale);
}

/* Sets 'loc' to 'ps->__lc_locale' */
void
__mbst_set_locale(mbstate_t *ps, const void *loc)
{
	ps->__lc_locale = (void *)loc;
}
