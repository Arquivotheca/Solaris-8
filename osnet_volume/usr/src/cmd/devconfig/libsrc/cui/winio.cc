#pragma ident "@(#)winio.cc   1.3     92/11/25 SMI"

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
//	Window IO routines
//
//	$RCSfile: winio.cc $ $Revision: 1.7 $ $Date: 1992/09/12 15:27:05 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif // PRE_COMPILED_HEADERS


//
//	set/get cursor position
//

void Window::setcur(int Row, int Col)
{
	forceCoords(Row, Col);
	wmove(inner, Row, Col);

	//	sync the virtual cursor with the window's cursor

	getcurAbs(Row, Col);
	CUI_setCursor(Row, Col);
}

void Window::getcur(int &Row, int &Col)
{
    int _row, _col;
	getyx(inner, _row, _col); // getyx is a macro - no & required
	Row = _row;
	Col = _col;
}


//
//	clear window/row in window
//

void Window::clear(void)
{
	werase(inner);
	wmove(inner, 0, 0);
	refresh();
}

void Window::clearRow(int Row)
{
	wmove(inner, Row, 0);
	wclrtoeol(inner);
	refresh();
}


//
//	print string in window, interpreting embedded attribute controls
//

int Window::printf(char *format,...)
{
	char buffer[1024];

	// format the error message

	va_list argptr;
	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);
	return(print(buffer));
}

int Window::print(char *string)
{
	attribPrint(string);
	refresh();
	return(0);
}


//
//	scroll window up or down
//	(pass number of lines to scroll +ve = scroll up, -ve = down)
//

int Window::scroll(int count)
{
    int i;
    if(count > 0)
    {
		for(i = 0; i < count; i++)
		{
			wmove(inner, 0, 0);
			wdeleteln(inner);
		}
    }
    else
    {
		count = -count;
		for(i = 0; i < count; i++)
		{
			wmove(inner, 0, 0);
			winsertln(inner);
		}
    }
	refresh();
    return(0);
}


//
//	put char in window
//

void Window::putc(int ch)
{
	waddch(inner, (chtype)ch);
}


//
//	enable/disable scrolling
//

void Window::scrolling(bool mode)
{
	if(mode)
		scrollok(inner, TRUE);
	else
		scrollok(inner, FALSE);
}


/*===========================================================================
					  low-level helper routines
============================================================================*/


//
//	force row/col coordinates into window
//

void Window::forceCoords(int &Row, int &Col)
{
	if(Row < 0)
		Row = 0;
	if(Row >= height)
		Row = height;
	if(Col < 0)
		Col = 0;
	if(Col >= width)
		Col = width;
}


//
//	get window's cursor position in absolute terms
//	(WARNING! this function knows about curses WINDOW internals)
//

void Window::getcurAbs(int &Row, int &Col)
{
	WINDOW *window = inner;
	Row = window->_yoffset + window->_begy + window->_cury;
	Col = window->_begx + window->_curx;
}


//
//	set attribute for text output in window
//

void Window::setTextAttrib(short attrib)
{
	chtype cursesAttrib = Color::lookupValue(attrib);
	wattrset(inner, cursesAttrib);
}


//
//	output a string, interpreting embedded attribute control-codes
//

void Window::attribPrint(char *string)
{
#define BUFFSIZE 100

    char buffer[BUFFSIZE + 2];
	char *from = string;
	char *to   = buffer;
	char *end  = buffer + BUFFSIZE;

	while(*from)
	{
		switch(*from)
		{
			case '\\':  // output next char literally
			{
				from++;
				break;
			}
			case ATTRIB_CHANGE:  // change attributes
			{
				// get attrib-change code

				int code = *(from + 1) - '0';

				// if not a valid attrib-change code, copy char as normal

                if(code < 0 || code > 9)
				{
					*to++ = *from++;
				}
				else // we have an attrib-change code
				{
					// output what we have so far, and reset pointer

					*to = 0;
					waddstr(inner, buffer);
					to = buffer;

					// switch to the indicated attribute

					chtype attrib = Color::lookupValue(code);
					wattrset(inner, attrib);

					// bump past the attrib-change code

					from++;
					from++;
                }
				break;
			}
			default:  // by default, copy char to buffer
			{
				*to++ = *from++;
			}
		}

		// if our buffer is full, flush it

		if(to >= end)
		{
			*to = 0;
			waddstr(inner, buffer);
			to = buffer;
		}
    }

	// flush our buffer

	*to = 0;
	waddstr(inner, buffer);
}


#ifdef MSDOS

int beep(void)
{
	putc(7, stdout);
	return(0);
}

#endif

