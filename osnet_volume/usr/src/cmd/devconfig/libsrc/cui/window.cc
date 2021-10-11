#pragma ident "@(#)window.cc   1.5     93/07/23 SMI"

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
//	Window class implementation
//
//	$RCSfile: window.cc $ $Revision: 1.24 $ $Date: 1992/09/24 00:16:22 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif
#ifndef  _CUI_DISPLAY_H
#include "display.h"
#endif

#endif // PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
	titleId,
	parentWindowId,
	vScrollId,
	borderColorId,
	titleColorId,
	interiorColorId,
    nullStringId
};


Window::Window(char* Name, Widget* Parent, CUI_Resource* Resources,
			   CUI_WidgetId id)
	: Widget(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	// set default colors

	borderColor   = CUI_NORMAL_COLOR;
	titleColor	  = CUI_REVERSE_COLOR;
	interiorColor = CUI_NORMAL_COLOR;

    loadResources(resList);
	setValues(Resources);
}


int Window::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// if we have a title, make sure we're wide enough to show it

	if(title)
	{
		int len = strlen(title) + 2;
		if(width < len)
			width = len;
	}

    // make sure we'll fit on-screen

	if((!name || strcmp(name, "CUI_screenWindow") != 0) &&
		height > CUI_screenRows() - 1)
	{
		height = CUI_screenRows() - 1;
	}
	if(width  > CUI_screenCols())
		width = CUI_screenCols();

	// special-case CUI_screenWindow - try to load from dump-file
	// (since the screen was dumped via scr_dump() and not putwin(),
	// we must skip past some initial junk - logic copied from scr_set.c)

#define SVR3_DUMP_MAGIC_NUMBER	0434

	FILE *fp = NULL;
    if(name && strcmp(name, "CUI_screenWindow") == 0)
	{
		// if we're inheriting a dump-file, load it

		char *dumpFile = getenv("CUI_SCREEN_DUMP");
		if(dumpFile)
		{
			fp = fopen(dumpFile, "r");
			if(fp)
			{
				short magic;
				long  ttytime;

				// read the magic number

				if (fread((char *) &magic, sizeof(short), 1, fp) != 1)
					goto err;
				if (magic != SVR3_DUMP_MAGIC_NUMBER)
					goto err;

				// skip past modification time of image in file

				if (fread((char *) &ttytime, sizeof(long), 1, fp) != 1)
					goto err;

				// read the window

				outer = getwin(fp);
			}
		}
err:
		// if for any reason we didn't load from a file, just create new

		if(!outer)
			outer = newwin(height, width, row, col);
    }

	// unconditionally close dump-file

	fclose(fp);

	// else if we have a parentWindow, we're a subwindow of it

	if(!outer)
	{
		if(parentWindow)
			outer = subwin(parentWindow->inner, height, width, row, col);

		// else if we're not a popup, we're a subwindow of CUI_screenWin

		else if(!(flags & CUI_POPUP))
		{
			WINDOW *pWindow = CUI_display->screen()->getOuter();
			outer = subwin(pWindow, height, width, row, col);
		}

		// else we're a regular window

		else
			outer = newwin(height, width, row, col);
    }

	// did we succeed?

	if(!outer)
		CUI_fatal(dgettext( CUI_MESSAGES, "Can't create window"));

	// OK...

	if(flags & CUI_BORDER)
    {
		height -= 2;
		width  -= 2;
		row++;
		col++;
    }
	else if(flags & CUI_HLINE)
    {
		height -= 2;
		row++;
    }
    inner = subwin(outer, height, width, row, col);
	if(!inner)
		CUI_fatal("Can't create window");
	if(flags & CUI_BORDER)
    {
		row--;
		col--;
    }
	else if(flags & CUI_HLINE)
		row--;
    if(flags & CUI_VSCROLL)
		scrollok(inner, TRUE);
	else
		scrollok(inner, FALSE);

	// set background and interior attributes

	chtype attrib = Color::lookupValue(interiorColor);
	wbkgd(outer, attrib);	// needed?
	wbkgd(inner, attrib);

	// create panel and hide it (disable display while we do this)

	bool saveMode = CUI_updateDisplay(FALSE);
	panel = new_panel(outer);
	if(!panel)
		CUI_fatal("Can't create window");
	hide_panel(panel);
	CUI_updateDisplay(saveMode);
	if(saveMode)
		CUI_refreshDisplay(FALSE);

	// store back-pointer to self in panel's userptr

	set_panel_userptr(panel, (char *)this);

	flags |= CUI_REALIZED;
    return(0);
}


//
//	destructor
//

Window::~Window(void)
{
	// unselect self, then hide, just in case we're current

	unselect();
	hide();

	// delete the panel, then the associated window and sub-window

	del_panel(panel);
	delwin(outer);
	delwin(inner);

	// delete our stuff

	MEMHINT();
    CUI_free(title);
}


//
//	return current window
//

Window *Window::current(void)
{
	return((Window *)panel_userptr(panel_below(NULL)));
}


//
//	resource routines
//

int Window::setValue(CUI_Resource *resource)
{
	int intValue   = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
        case titleId:
		{
			setTitle(strValue);
			break;
        }
		case parentWindowId:
		{
			// strValue is name of parent whose window is to be our parentWindow

			Widget *widget = Widget::lookup(strValue);
			if(widget)
				parentWindow = widget->getWindow();
			break;
        }
		case vScrollId:
		{
			setFlagValue(intValue, CUI_VSCROLL);
			break;
		}
		case borderColorId:
		{
			return(setColorValue(strValue, borderColor));
		}
		case titleColorId:
		{
			return(setColorValue(strValue, titleColor));
		}
		case interiorColorId:
		{
			return(setColorValue(strValue, interiorColor));
		}
        default:
			return(Widget::setValue(resource));
	}
	return(0);
}

int Window::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Widget::getValue(resource));
	}
}


//
//	set title
//

int Window::setTitle(char *Title)
{
	MEMHINT();
    CUI_free(title);
	MEMHINT();
	title = CUI_newString(Title);
	if(isVisible())
	{
		drawBorder();
		wrefresh(outer);
	}
	return(0);
}


//
//	select window
//

int Window::select(void)
{
	// realize if necessary (why make the programmer remember to do this?)

	if(!(flags & CUI_REALIZED))
		realize();

	// sanity-check

	if(!panel)
		CUI_fatal(dgettext( CUI_MESSAGES, "Null panel found in window %s"), name);

	// set interior attribute

	chtype attrib = Color::lookupValue(interiorColor);
	wattrset(inner, attrib);

	// show self and refresh the screen

    show();
	CUI_refreshDisplay(FALSE);
	return(0);
}


//
//	unselect window
//

int Window::unselect(void)
{
	// if our panel is on top, promote the panel beneath it

	if(isTop())
		top_panel(panel_below(panel));

	// refresh the screen

	CUI_refreshDisplay(FALSE);
    return(0);
}


//
//	show window
//

int Window::show(void)
{
	// realize if necessary (why make the programmer remember to do this?)

	if(!(flags & CUI_REALIZED))
		realize();

	// sanity-check

	if(!panel)
		CUI_fatal(dgettext( CUI_MESSAGES, "Null panel found in window %s"), name);

    // if our panel is hidden, show it

    if(panel_hidden(panel))
		show_panel(panel);

	// make sure we're top, draw border, and refresh screen

	top_panel(panel);
	drawBorder();
    CUI_refreshDisplay(FALSE);
	return(0);
}


//
//	hide window
//

int Window::hide(void)
{
	// can't hide SCREEN...

	if(this == CUI_display->screen())
		return(-1);

	// unselect self (we should be current)

	unselect();

	// hide our panel and refresh screen

	hide_panel(panel);
	CUI_refreshDisplay(FALSE);

    return(0);
}


//
//	locate window
//

int Window::locate(short Row, short Col)
{
	row = Row;
	col = Col;
	if(panel)
	{
		move_panel(panel, row, col);

		//	curses doesn't always move inner window successfully
		//	(depends on whether/when we do a refresh)

		if(flags & CUI_BORDER)
			mvwin(inner, row + 1, col + 1);
		else if(flags & CUI_HLINE)
			mvwin(inner, row + 1, col);
		else
			mvwin(inner, row, col);

		CUI_refreshDisplay(FALSE);
    }
    return(0);
}


//
//	refresh window
//

int Window::refresh(void)
{
	bool save = CUI_updateDisplay(FALSE);  // turn off screen update & save mode

	// panel refresh logic looks at outer window's change flag,
	// so we propagate inner's flag to outer...

	if(inner->_flags & _WINCHANGED)
		outer->_flags |= _WINCHANGED;

	// if we're updating...

    if(save)
	{
		// and we're not hidden...

		if(!panel_hidden(panel))
			wrefresh(inner);	// refresh the window
    }

	// restore update mode

	CUI_updateDisplay(save);

    return(0);
}


//===========================================================================
//						protected helper routines
//===========================================================================


//
//	adjust row/col/height/width for popup window
//

void Window::adjust(short &Row, short &Col, short &Height, short &Width)
{
	short myrow = Row;
	short mycol = Col;
	short nrows = CUI_screenRows();
	short ncols = CUI_screenCols();

	// check width to see if there's room

	while( (mycol + Width) > ncols - 2)
        mycol--;
    if(mycol < 0)
    {
		mycol = 0;
		Width = ncols - 2;
    }
	Col = mycol;

	// check height to see if there's room

	while( (myrow + Height) > nrows - 3)	  // keep error-line free
        myrow--;
    if(myrow < 0)
    {
		myrow  = 0;
		Height = nrows - 3;
    }
	Row = myrow;
}

