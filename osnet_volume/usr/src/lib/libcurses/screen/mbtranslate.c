/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)mbtranslate.c 1.3 97/06/25 SMI"

#include	<stdlib.h>
#include	<sys/types.h>
#include	"curses_inc.h"


/*
**	Translate process code to byte-equivalent
**	Return the length of the byte-equivalent string
*/

/*
**	use _curs_wctomb() instead of _code2byte(code, bytes)
*/


/*
**	Translate a set of byte to a single process code
*/

/*
**	use _curs_mbtowc() instead of wchar_t _byte2code(bytes)
*/


/*
**	Translate a string of wchar_t to a byte string.
**	code: the input code string
**	byte: if not NULL, space to store the output string
**	n: maximum number of codes to be translated.
*/
char
*_strcode2byte(wchar_t *code, char *byte, int n)
{
	char		*bufp;
	wchar_t		*endcode;
	static char	*buf;
	static int	bufsize;

	/* compute the length of the code string */
	if (n < 0)
		for (n = 0; code[n] != 0; ++n)
			;

	/* get space to store the translated string */
	if (!byte && (n*CSMAX+1) > bufsize) {
		if (buf)
			free(buf);
		bufsize = n * CSMAX + 1;
		if ((buf = malloc(bufsize * sizeof (char))) == NULL)
			bufsize = 0;
		}

	/* no space to do it */
	if (!byte && !buf)
		return (NULL);

	/* start the translation */
	bufp = byte ? byte : buf;
	endcode = code+n;
	while (code < endcode && *code) {
		bufp += _curs_wctomb(bufp, *code & TRIM);
		++code;
	}
	*bufp = '\0';

	return (byte ? byte : buf);
}



/*
**	Translate a byte-string to a wchar_t string.
*/
wchar_t
*_strbyte2code(char *byte, wchar_t *code, int n)
{
	char		*endbyte;
	wchar_t		*bufp;
	static wchar_t	*buf;
	static int	bufsize;

	if (n < 0)
		for (n = 0; byte[n] != '\0'; ++n)
			;

	if (!code && (n + 1) > bufsize) {
		if (buf)
			free((char *)buf);
		bufsize = n + 1;
		if ((buf = (wchar_t *)malloc(bufsize * sizeof (wchar_t))) ==
		    NULL)
			bufsize = 0;
	}

	if (!code && !buf)
		return (NULL);

	bufp = code ? code : buf;
	endbyte = byte + n;

	while (byte < endbyte && *byte) {
		int		type, width;
		wchar_t		wchar;

		type = TYPE(*byte & 0377);
		width = cswidth[type];
		if (type == 1 || type == 2)
			width++;

		if (byte + width <= endbyte) {
			(void) _curs_mbtowc(&wchar, byte, width);
			*bufp++ = wchar;
		}

		byte += width;
	}
	*bufp = 0;

	return (code ? code : buf);
}
