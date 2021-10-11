#pragma ident "@(#)keybd.cc   1.5     93/01/08 SMI"

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

//============================================================================
//	 keyboard routines
//
//	 $RCSfile: keybd.cc $ $Revision: 1.2 $ $Date: 1992/12/29 20:40:17 $
//============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <curses.h>
#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_KEYBD_H
#include "keybd.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_DISPLAY_H
#include "display.h"
#endif
#ifdef MSDOS
#include <dos.h>	// for sleep
#else
#include <unistd.h>	// for sleep
#endif

#endif	// PRE_COMPILED_HEADERS


#define CTRL_KEY(x) x - '@'     // safe for internationalization?


// resources to load

static CUI_StringId resList[] =
{
	tabKeyId,
	stabKeyId,
	leftKeyId,
	rightKeyId,
	upKeyId,
	downKeyId,
	pgUpKeyId,
	pgDownKeyId,
	homeKeyId,
	endKeyId,
	helpKeyId,
	insKeyId,
	delKeyId,
	cancelKeyId,
	refreshKeyId,
	killLineKeyId,
    nullStringId
};


//
//	constructor
//

Keyboard::Keyboard(char *Name, Widget *Parent, CUI_Resource *, // never used
				   CUI_WidgetId id)
	: Widget(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	saveKey = 0;
	readKeyFd = saveKeyFd = NULL;

	// assign default values to alternate keys

	leftAlt 	  = CTRL_KEY('B');    // Backwards
	rightAlt	  = CTRL_KEY('F');    // Forwards
	upAlt		  = CTRL_KEY('P');    // Previous
	downAlt 	  = CTRL_KEY('N');    // Next
	homeAlt 	  = CTRL_KEY('A');    // Start
	endAlt		  = CTRL_KEY('E');    // End
	pgUpAlt 	  = CTRL_KEY('U');    // Up
	pgDnAlt 	  = CTRL_KEY('D');    // Down
	tabAlt		  = CTRL_KEY('I');    // Tab
	btabAlt 	  = CTRL_KEY('R');    // Reverse-tab
	insAlt		  = CTRL_KEY('O');    // Overlay
	delAlt		  = CTRL_KEY('X');    // X-out
	cancelAlt	  = CTRL_KEY('C');    // interrupt
	refreshAlt	  = CTRL_KEY('L');    // (same as default)
	helpAlt 	  = CTRL_KEY('W');    // What
	killLine	  = CTRL_KEY('K');    // Kill line
    emergencyExit = CTRL_KEY('Q');    // no mnemonic!

	// load resources

	loadResources(resList);
}


//
//	destructor
//

Keyboard::~Keyboard(void)
{
	if(readKeyFd)
		fclose(readKeyFd);
	if(saveKeyFd)
		fclose(saveKeyFd);
}


//
//	set resource values
//

int Keyboard::setValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		case tabKeyId:
		case stabKeyId:
		case leftKeyId:
		case rightKeyId:
		case upKeyId:
		case downKeyId:
		case pgUpKeyId:
		case pgDownKeyId:
		case homeKeyId:
		case endKeyId:
		case helpKeyId:
		case insKeyId:
		case delKeyId:
		case cancelKeyId:
		case refreshKeyId:
			return(setAlternateKey(resource->id, (int)(resource->value)));
        default:
			return(-1);
	}
}

//
//	check whether a key is ready
//

int Keyboard::keyReady(void)
{
    int ch;

	// if we have a saved key, return it

	if(saveKey)
		return(saveKey);

	// if we're reading from a keystroke file...

	if(readKeyFd)
	{
		// pause so we can see what's happening

	    sleep(1);

		// read keycode from file;
		// if no more, close file and clear descriptor

		if(fscanf(readKeyFd, "%d\n", &ch) == 0)
		{
			ch = ERR;
			fclose(readKeyFd);
			readKeyFd = NULL;
		}
	}
	else // get key from keyboard...
	{
		ch = getch();
	}

	// if no key, return FALSE

        if(ch == ERR)
		return(FALSE);

	// if we're saving keys to a file, do it

	if(saveKeyFd)
		fprintf(saveKeyFd, "%d\n", ch);

	// save keystroke for later retrieval and return success

	saveKey = ch;
	return(TRUE);
}


//
//	 wait for and get a single keystroke
//	 BEWARE! only call this without first checking CUI_keyReady() if you
//	 really want to wait (we won't process events while we wait)
//

int Keyboard::getKey(void)
{
	int key;

	// spin till we have a key

	while(!keyReady())
		;

	if(saveKey)
	{
		key = saveKey;
		saveKey = 0;
	}
	else
		key = getch();

	// clear any infomsg

	if(CUI_display->messageDisplayed())
		CUI_display->clearMessage();

	// do some simple transformations

	switch(key)
	{
		// first some simple transformations...

		case 8: 					// if terminfo is broken...
			return(KEY_BSPACE);
        case '\n':
			return '\r';
		case KEY_F(1):
			return(KEY_HELP);
		case 27:
			return(KEY_CANCEL);
		case 127:					// Sparc console DEL key
#ifdef DEL_KEY
			return(KEY_DC);
#else
			return(KEY_BSPACE);
#endif
#ifndef DEL_KEY
		case KEY_DC:				// regular DEL key
			return(KEY_BSPACE);
#endif
        default:
		{
			// now we map control-key alternates
			// (can't use switch, since that requires constant expression)

			if(key == leftAlt)
				key = KEY_LEFT;
			else if(key == rightAlt)
				key = KEY_RIGHT;
			else if(key == upAlt)
				key = KEY_UP;
			else if(key == downAlt)
				key = KEY_DOWN;
			else if(key == homeAlt)
				key = KEY_HOME;
			else if(key == endAlt)
				key = KEY_END;
			else if(key == pgUpAlt)
				key = KEY_PGUP;
			else if(key == pgDnAlt)
				key = KEY_PGDN;
			else if(key == tabAlt)
				key = KEY_TAB;
			else if(key == btabAlt)
				key = KEY_BTAB;
			else if(key == insAlt)
				key = KEY_INS;
			else if(key == delAlt)
				key = KEY_DEL;
			else if(key == cancelAlt)
				key = KEY_CANCEL;
			else if(key == refreshAlt)
				key = KEY_REFRESH;
			else if(key == helpAlt)
				key = KEY_HELP;

			// this assignment goes last, so it can be over-ridden

			else if (key == CTRL_KEY('L'))
				return KEY_REFRESH;

			// that's all folks!

			return(key);
		}
	}
}


//
//	redefine alternate key
//

int Keyboard::setAlternateKey(CUI_StringId key, int value)
{
	if(value > 31) // ??? safe for internationalization ???
		return(-1);

	switch(key)
	{
		case tabKeyId:
			tabAlt = value;
			break;
        case stabKeyId:
			btabAlt = value;
			break;
        case leftKeyId:
			leftAlt = value;
			break;
        case rightKeyId:
			rightAlt = value;
			break;
        case upKeyId:
			upAlt = value;
			break;
        case downKeyId:
			downAlt = value;
			break;
        case pgUpKeyId:
			pgUpAlt = value;
			break;
        case pgDownKeyId:
			pgDnAlt = value;
			break;
        case homeKeyId:
			homeAlt = value;
			break;
        case endKeyId:
			endAlt = value;
			break;
        case insKeyId:
			insAlt = value;
			break;
        case delKeyId:
			delAlt = value;
			break;
        case cancelKeyId:
			cancelAlt = value;
			break;
        case refreshKeyId:
			refreshAlt = value;
			break;
		case helpKeyId:
			helpAlt = value;
			break;
		case killLineKeyId:
			killLine = value;
			break;
    }
	return(-1);
}


//============================================================================
//	a single, global Keyboard object, and API routines to access it
//============================================================================

Keyboard *CUI_keyboard;

int CUI_keyReady(void)
{
	return(CUI_keyboard->keyReady());
}

int CUI_getKey(void)
{
	return(CUI_keyboard->getKey());
}


#ifdef NEEDED

//
//	flush typeahead if NOFLUSH is non-NULL
//

int _ui_io_flushkeys()
{
    if(NOFLUSH == NULL)
	flushinp();
    return(0);
}

#endif // NEEDED

