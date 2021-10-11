#pragma ident "@(#)display.cc   1.3     92/11/25 SMI"

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
//	Display class implementation
//
//	$RCSfile: display.cc $ $Revision: 1.13 $ $Date: 1992/09/13 04:46:07 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_DISPLAY_H
#include "display.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// global pointer to the display

Display *CUI_display = NULL;

// static for screen-dump file

char *Display::dumpFile = NULL;


//
//	constructor/destructor
//

Display::Display(Widget *Parent)
{
	int i;
	CUI_Resource resources[10];

	// save pointer to self (will be needed when we construct windows)

	CUI_display = this;

	// save parent

	parent = Parent;

	// update mode ON, no message displayed

	updating	= TRUE;
	haveMessage = FALSE;

	int color = CUI_compileString("CUI_monoColor");

	// create and realize CUI_screenWindow
	// (hard-code interior color for this window; we want no changes)

    i = 0;
	CUI_setArg(resources[i], rowId, 		   0);		 i++;
	CUI_setArg(resources[i], colId, 		   0);		 i++;
	CUI_setArg(resources[i], heightId,		   LINES);	 i++;
	CUI_setArg(resources[i], widthId,		   COLS);	 i++;
	CUI_setArg(resources[i], interiorColorId,  color);	 i++;
	CUI_setArg(resources[i], nullStringId, 0);
	MEMHINT();
	screenWin = new Window("CUI_screenWindow", NULL, resources);
	screenWin->realize();

    // create and realize CUI_messageWindow

	infoRow = LINES - 1;

	i = 0;
    CUI_setArg(resources[i], rowId,            infoRow); i++;
	CUI_setArg(resources[i], colId, 		   0);		 i++;
	CUI_setArg(resources[i], heightId,		   1);		 i++;
	CUI_setArg(resources[i], widthId,		   COLS);	 i++;
	CUI_setArg(resources[i], interiorColorId,  color);	 i++;
    CUI_setArg(resources[i], nullStringId,     0);
	MEMHINT();
	messageWin = new Window("CUI_messageWindow", NULL, resources);
	messageWin->realize();
}

Display::~Display(void)
{
	// Can't delete our windows here since under cfront we no longer
	// have a display (this) pointer, and this will upset the Window
	// destructors.  CUI_exit() takes care of deleting them.
}


//
//	enable/disable screen updating
//

bool Display::update(bool mode)
{
	bool previous = updating;
	if(mode)
		updating = TRUE;
	else
		updating = FALSE;
	return(previous);
}


//
//	 refresh screen (if 'redraw' flag is true, force a redraw)
//

void Display::refresh(bool redraw)
{
	if(!redraw)
	{
		if(updating)
		{
			update_panels();
			doupdate();
		}
	}
	else // force redraw
	{
		//	curses has a bug such that if the last char drawn was not in
		//	'normal' attribute, or was a line-drawing char, then the screen
		//	is sometimes fouled-up on refresh; here we'll draw a normal space
		//	in a safe location, and force it to the screen, in the hope that
		//	we'll defeat the bug

		wmove(stdscr, 0, 0);
		wattrset(stdscr, A_NORMAL);
		waddch(stdscr, ' ');
		wrefresh(stdscr);

		//	say stdscr has changed, and screen should be redrawn in full
		//	tell the panel manager to update everything

		touchwin(stdscr);
		clearok(stdscr, TRUE);
		update_panels();

		// now do the physical update

		doupdate();
	}
}


//
//	set/get cursor position
//

void Display::setcur(short row, short col)
{
	setsyx(row, col);
	doupdate();
}

void Display::getcur(short &row, short &col)
{
	int tmpRow, tmpCol;
	getsyx(tmpRow, tmpCol);
	row = (short)tmpRow;
	col = (short)tmpCol;
}


//
//	display informational message (takes printf-style args)
//

void Display::infoMessage(char *format,...)
{
	short saveRow, saveCol;
	getcur(saveRow, saveCol);

    char buffer[240];

	// format the error message

	va_list argptr;
	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	// print it in REVERSE in message window

	Window *previous = Window::current();
	messageWin->select();
	messageWin->clear();

	chtype normal  = Color::lookupValue(messageWin->interiorAttrib());
	chtype reverse = Color::invert(normal);
	wattrset(messageWin->getInner(), reverse);
	update(TRUE);	  // !!! work-around a bug !!!
	messageWin->print(buffer);
	wattrset(messageWin->getInner(), normal);
	haveMessage = TRUE;
	refresh(FALSE);   // !!! another bug !!!

	if(previous)
		previous->select();
	setcur(saveRow, saveCol);
}


//
//  clear message window
//

void Display::clearMessage(void)
{
	short saveRow, saveCol;
	getcur(saveRow, saveCol);
	if(messageWin)
		messageWin->clear();
	haveMessage = FALSE;
	setcur(saveRow, saveCol);
}


//
//	save and restore display (we don't nest)
//

int Display::save(char *file)
{
	// if we weren't passed a file-name, generate one

	if(!file)
		file = tmpnam(NULL);
	if(!file)
		return(-1);

	// save file name for restore, then do the save

	dumpFile = file;
	return(scr_dump(dumpFile));
}

int Display::restore(char *file)
{
	// if we weren't passed a file-name, use saved name (dumpFile)

	if(!file)
		file = dumpFile;
	if(!file)
		return(-1);

	// load the saved screen and refresh

	int retcode = scr_restore(file);
	refresh(FALSE);

	// unconditionally delete and clear dumpFile

    unlink(dumpFile);
	dumpFile = NULL;

    return(retcode);
}

