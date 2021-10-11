/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)copywin.c	1.9	97/08/22 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

/*
 * This routine writes parts of Srcwin onto Dstwin,
 * either non-destructively (over_lay = TRUE) or destructively
 * (over_lay = FALSE).
 */

#include	<string.h>
#include	<sys/types.h>
#include	"curses_inc.h"

int
copywin(WINDOW *Srcwin, WINDOW *Dstwin,
	int minRowSrc, int minColSrc, int minRowDst,
	int minColDst, int maxRowDst, int maxColDst,
	int over_lay)
{
	int		ySrc, yDst, which_copy, t;
	int		height = (maxRowDst - minRowDst) + 1,
			width = (maxColDst - minColDst) + 1;
	chtype		**_yDst = Dstwin->_y, **_ySrc = Srcwin->_y,
			bkSrc = Srcwin->_bkgd, atDst = Dstwin->_attrs,
			*spSrc, *spDst, *epSrc, *epDst, *savepS,
			*savepD, width_bytes, numcopied;

#ifdef	DEBUG
	if (outf)
		fprintf(outf, "copywin(%0.2o, %0.2o);\n", Srcwin, Dstwin);
#endif	/* DEBUG */

	/*
	* If we are going to be copying from curscr,
	* first offset into curscr the offset the Dstwin knows about.
	*/
	if (Srcwin == curscr)
		minRowSrc += Dstwin->_yoffset;

	/*
	* There are three types of copy.
	* 0 - Straight memcpy allowed
	* 1 - We have to first check to see if the source character is a blank
	* 2 - Dstwin has attributes or bkgd that must changed
	* on a char-by-char basis.
	*/
	if ((which_copy = (over_lay) ? 1 :
	    (2 * ((Dstwin->_attrs != A_NORMAL) ||
	    (Dstwin->_bkgd != _BLNKCHAR)))) == 0)
		width_bytes = width * (int) sizeof (chtype);

	/* for each Row */
	for (ySrc = minRowSrc, yDst = minRowDst; height-- > 0; ySrc++, yDst++) {
		if (which_copy) {
			spSrc = &_ySrc[ySrc][minColSrc];
			spDst = &_yDst[yDst][minColDst];
			numcopied = width;

			epSrc = savepS = &_ySrc[ySrc][maxColDst];
			epDst = savepD = &_yDst[yDst][maxColDst];
		/* only copy into an area bounded by whole characters */
			for (; spDst <= epDst; spSrc++, spDst++)
				if (!ISCBIT(*spDst))
					break;
			if (spDst > epDst)
				continue;
			for (; epDst >= spDst; --epDst, --epSrc)
				if (!ISCBIT(*epDst))
					break;
			t = _curs_scrwidth[TYPE(RBYTE(*epDst))] - 1;
			if (epDst+t <= savepD)
				epDst += t, epSrc += t;
			else
				epDst -= 1, epSrc -= 1;
			if (epDst < spDst)
				continue;
			/* don't copy partial characters */
			for (; spSrc <= epSrc; ++spSrc, ++spDst)
				if (!ISCBIT(*spSrc))
					break;
			if (spSrc > epSrc)
				continue;
			for (; epSrc >= spSrc; --epSrc, --epDst)
				if (!ISCBIT(*epSrc))
					break;
			t = _curs_scrwidth[TYPE(RBYTE(*epSrc))] - 1;
			if (epSrc+t <= savepS)
				epSrc += t, epDst += t;
			else
				epSrc -= 1, epDst -= 1;
			if (epSrc < spSrc)
				continue;
		/* make sure that the copied-to place is clean */
			if (ISCBIT(*spDst))
				(void) _mbclrch(Dstwin, minRowDst,
				    /*LINTED*/
				    (int) (spDst - *_yDst[yDst]));
			if (ISCBIT(*epDst))
				(void) _mbclrch(Dstwin, minRowDst,
				    /*LINTED*/
				    (int) (epDst - *_yDst[yDst]));
			/*LINTED*/
			numcopied = (chtype) (epDst - spDst + 1);

			if (which_copy == 1) {		/* overlay */
				for (; numcopied-- > 0; spSrc++, spDst++)
			/* Check to see if the char is a "blank/bkgd". */
					if (*spSrc != bkSrc)
						*spDst = *spSrc | atDst;
			} else {
				for (; numcopied-- > 0; spSrc++, spDst++)
					*spDst = *spSrc | atDst;
			}
		} else {
			/* ... copy all chtypes */
			(void) memcpy((char *) &_yDst[yDst][minColDst],
			    (char *) &_ySrc[ySrc][minColSrc], width_bytes);
		}

		/* note that the line has changed */
		if (minColDst < Dstwin->_firstch[yDst])
			/*LINTED*/
			Dstwin->_firstch[yDst] = (short) minColDst;
		if (maxColDst > Dstwin->_lastch[yDst])
			/*LINTED*/
			Dstwin->_lastch[yDst] = (short) maxColDst;
	}

#ifdef	_VR3_COMPAT_CODE
	if (_y16update) {
		(*_y16update)(Dstwin, (maxRowDst - minRowDst) + 1,
		    (maxColDst - minColDst) + 1, minRowDst, minColDst);
	}
#endif	/* _VR3_COMPAT_CODE */

	/* note that something in Dstwin has changed */
	Dstwin->_flags |= _WINCHANGED;

	if (Dstwin->_sync)
		wsyncup(Dstwin);

	return (Dstwin->_immed ? wrefresh(Dstwin) : OK);
}
