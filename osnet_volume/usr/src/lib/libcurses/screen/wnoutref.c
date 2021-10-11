/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wnoutref.c	1.11	97/08/22 SMI"	/* SVr4.0 1.12	*/

/*LINTLIBRARY*/

#include	<string.h>
#include	<sys/types.h>
#include	"curses_inc.h"

/* Like refresh but does not output */

int
wnoutrefresh(WINDOW *win)
{
	short	*bch, *ech, *sbch, *sech;
	chtype	**wcp, **scp, *wc, *sc;
	int	*hash;
	short	y, x, xorg, yorg, scrli, scrco,
		boty, sminy, smaxy, minx, maxx, lo, hi;
	bool	doall;

	if (win->_parent)
		wsyncdown(win);

	doall = win->_clear;

	sminy = SP->Yabove;
	smaxy = sminy + LINES;
	scrli = curscr->_maxy;
	scrco = curscr->_maxx;

	yorg = win->_begy + win->_yoffset;
	xorg = win->_begx;

	/* save flags, cursor positions */
	SP->virt_scr->_leave = win->_leave;
	if ((!win->_leave && (win->_flags & (_WINCHANGED | _WINMOVED))) &&
	    ((y = win->_cury + yorg) >= 0) && (y < scrli) &&
	    ((x = win->_curx + xorg) >= 0) && (x < scrco)) {
		_virtscr->_cury = y;
		_virtscr->_curx = x;
	}
	if (!(win->_use_idc))
		_virtscr->_use_idc = FALSE;
	if (win->_use_idl)
		_virtscr->_use_idl = TRUE;
	if (win->_clear) {
		_virtscr->_clear = TRUE;
		win->_clear = FALSE;
		win->_flags |= _WINCHANGED;
	}

	if (!(win->_flags & _WINCHANGED))
		goto done;

	/* region to update */
	boty = win->_maxy+yorg;
	if (yorg >= sminy && yorg < smaxy && boty >= smaxy)
		boty = smaxy;
	else
		if (boty > scrli)
			boty = scrli;
	boty -= yorg;

	minx = 0;
	if ((maxx = win->_maxx+xorg) > scrco)
		maxx = scrco;
	maxx -= xorg + 1;

	/* update structure */
	bch = win->_firstch;
	ech = win->_lastch;
	wcp = win->_y;

	hash = _VIRTHASH + yorg;
	sbch = _virtscr->_firstch + yorg;
	sech = _virtscr->_lastch + yorg;
	scp  = _virtscr->_y + yorg;

	/* first time around, set proper top/bottom changed lines */
	if (curscr->_sync) {
		_VIRTTOP = scrli;
		_VIRTBOT = -1;
	}

	/* update each line */
	for (y = 0; y < boty; ++y, ++hash, ++bch, ++ech, ++sbch,
	    ++sech, ++wcp, ++scp) {
		if (!doall && *bch == _INFINITY)
			continue;

		lo = (doall || *bch == _REDRAW || *bch < minx) ? minx : *bch;
		hi = (doall || *bch == _REDRAW || *ech > maxx) ? maxx : *ech;

		wc = *wcp;
		sc = *scp;
		/* adjust lo and hi so they contain whole characters */
		if (_scrmax > 1) {
			if (ISCBIT(wc[lo])) {
				for (x = lo - 1; x >= minx; --x)
					if (!ISCBIT(wc[x]))
						break;
					if (x < minx) {
						for (x = lo+1; x <= maxx; ++x)
							if (!ISCBIT(wc[x]))
								break;
						if (x > maxx)
							goto nextline;
					}
				lo = x;
			}
			if (ISMBIT(wc[hi])) {
				int		w;
				unsigned char	rb;
				for (x = hi; x >= lo; --x)
					if (!ISCBIT(wc[x]))
						break;
				/* LINTED */
				rb = (unsigned char) RBYTE(wc[x]);
				w = _curs_scrwidth[TYPE(rb)];
				hi = (x+w) <= maxx+1 ? x+w-1 : x;
			}
		}

		if (hi < lo)
			continue;

		/* clear partial multi-chars about to be overwritten */
		if (_scrmax > 1) {
			if (ISMBIT(sc[lo + xorg]))
				(void) _mbclrch(_virtscr, y + yorg, lo + xorg);
			if (ISMBIT(sc[hi + xorg]))
				(void) _mbclrch(_virtscr, y + yorg, hi + xorg);
		}

		/* update the change structure */
		if (*bch == _REDRAW || *sbch == _REDRAW)
			*sbch =  _REDRAW;
		else {
			if (*sbch > lo+xorg)
				*sbch = lo+xorg;
			if (*sech < hi+xorg)
				*sech = hi+xorg;
		}
		if ((y + yorg) < _VIRTTOP)
			_VIRTTOP = y+yorg;
		if ((y + yorg) > _VIRTBOT)
			_VIRTBOT = y + yorg;

		/* update the image */
		wc = *wcp + lo;
		sc = *scp + lo + xorg;
		(void) memcpy((char *) sc, (char *) wc, (size_t)
		    (((hi - lo) + 1) * sizeof (chtype)));

		/* the hash value of the line */
		*hash = _NOHASH;

nextline:
		*bch = _INFINITY;
		*ech = -1;
	}

done:
	_virtscr->_flags |= _WINCHANGED;
	win->_flags &= ~(_WINCHANGED | _WINMOVED | _WINSDEL);
	return (OK);
}
