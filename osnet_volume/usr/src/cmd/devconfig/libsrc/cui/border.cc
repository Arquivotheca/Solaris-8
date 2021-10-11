#pragma ident "@(#)border.cc   1.5     99/02/19 SMI"

/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved. Sun
 * considers its source code as an unpublished, proprietary trade secret, and
 * it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

//=============================================================================
//  Window methods concerning border, title, boxes, lines, etc.
//
//  $RCSfile: border.cc $ $Revision: 1.13 $ $Date: 1992/09/12 15:21:18 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_CURSVMEM_H
#include "cursvmem.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// #define TESTING_SCROLLBAR


//
//   draw border and all accoutrements
//

int Window::drawBorder(void)
{
	if(flags & CUI_BORDER)
    {
		chtype attrib = Color::lookupValue(borderColor);
		wattrset(outer, attrib);
        box(outer, (chtype)0, (chtype)0);
	}
	else if(flags & CUI_HLINE)
    {
		// do we always have a parentWindow???

		if(parentWindow)
		{
			int start = row - parentWindow->wRow() - 1;
			parentWindow->drawHline(start);
			parentWindow->drawHline(start + height + 1);
		}
    }

    // now draw the title, if any

	return(drawTitle());
}


//
//	draw window title (with or without focus)
//

int Window::drawTitle(bool focus)
{
    // draw the title

    if(title && *title)
    {
		int title_col = (width + 2 - strlen(title)) / 2;
		char buffer[80];

		chtype attrib;
		if(focus)
			attrib = Color::lookupValue(titleColor);
		else
			attrib = Color::lookupValue(borderColor);
        wattrset(outer, attrib);

		short cursorRow, cursorCol;
		CUI_getCursor(&cursorRow, &cursorCol);
        wmove(outer, 0, title_col);
		waddstr(outer, title);
		CUI_setCursor(cursorRow, cursorCol);

		attrib = Color::lookupValue(interiorColor);
		wattrset(outer, attrib);
    }

    if(flags & CUI_SYSMENU)
    {
		wmove(outer, 0, 1);
		waddstr(outer, "(*)");
    }

    refresh();
    return(0);
}


//
//  display scroll-bar on window
//		current is current row number in window
//		count	is total number of lines to display
//		borderWidth is resource value of our parent, and determines whether
//		(and how) we draw on our own border or on parent window's border

int Window::drawScrollBar(int current, int count, int borderWidth)
{
	// first we figure out which window we'll draw to

	WINDOW *win;
	if(borderWidth == 2)
		win = outer;
	else
		win = parentWindow->outer;

	int bump	= 0;
	int thumb	= 0;
	int tmpRow, tmpCol;
    chtype thumbChar;

	chtype normal  = Color::lookupValue(borderColor);
	chtype reverse = Color::invert(normal);

	// do nothing if we fit in window, or if window is not tall enough

	if(count < height || height < 3)
		return(0);

	// redraw right-hand border

	tmpRow	  = 1;
	tmpCol	  = width + 1;
	if(borderWidth != 2)
	{
		tmpRow	  += (row - parentWindow->wRow());
		tmpCol	  = parentWindow->wWidth() + 1;
#ifdef TESTING_SCROLLBAR
		tmpCol--;	// so we can see what's happening
#endif
	}
	if(borderWidth == 0)
		tmpRow--;
    wmove(win, tmpRow, tmpCol);
	wattrset(win, normal);
	wvline(win, ACS_VLINE, height);

	// now calculate where to draw the thumb

	//	'bump' is the number of lines in file associated with each
	//	one-line movement of the thumb.  Since we only use the top
	//	and bottom positions when we're at the very top or bottom
	//	of the file, subtract two.

	bump = count / (height - 2);
	if(bump == 0)
		bump = 1;	// don't divide by zero!

	// thumb position starts 1 row past top of the line we redrew
	// and is incremented every 'bump' lines

	int start = tmpRow + 1;
	thumb = start + (current / bump);

	// in VIDMEM mode, we use a ACS_UDARROW, else ACS_CKBOARD

	if(vidMemWrite)
		thumbChar = ACS_UDARROW | A_ALTCHARSET;
	else
		thumbChar = ACS_CKBOARD | A_ALTCHARSET;

	// if we're on first (last) line, adjust thumb to very top (bottom)
	// and substitute alternate thumb-style of the appropriate fat-arrow

	if(current == 0)
	{
		thumb = start - 1;
		if(vidMemWrite)
			thumbChar = ACS_FAT_DARROW | A_ALTCHARSET;
	}
	if(current == count - height)
	{
		thumb = start + height - 2;
		if(vidMemWrite)
			thumbChar = ACS_FAT_UARROW | A_ALTCHARSET;
	}

    // finally we can draw the thumb

	wmove(win, thumb, tmpCol);
	wattrset(win, reverse);
	waddch(win, thumbChar);

	// and refresh the window

	wrefresh(win);
	return(0);
}


//
//	draw a horizontal line at row "n" of specified window
//	using the appropriate line-drawing characters
//

int Window::drawHline(int win_row)
{
	chtype attrib = Color::lookupValue(borderColor);

	// previous logic saved and restored 'current vstate' - necessary?

	wattrset(outer, attrib);
	wmove(outer, win_row + 1, 0);
	waddch(outer, (chtype)ACS_LTEE);
	whline(outer, (chtype)ACS_HLINE, width);
	wmove(outer, win_row + 1, width + 1);
	waddch(outer, (chtype)ACS_RTEE);

	refresh();
    return(0);
}

