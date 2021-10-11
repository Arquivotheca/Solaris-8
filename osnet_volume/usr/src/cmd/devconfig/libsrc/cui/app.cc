#pragma ident "@(#)app.cc   1.7     93/07/22 SMI"

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
//	Application class implementation
//
//	$RCSfile: app.cc $ $Revision: 1.22 $ $Date: 1992/09/23 00:53:42 $
//=============================================================================

// #define TIMETEST

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef  _CUI_DISPLAY_H
#include "display.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_HELPWIN_H
#include "helpwin.h"
#endif
#ifndef  _CUI_SPOTHELP_H
#include "spothelp.h"
#endif
#ifndef  _CUI_KEYBD_H
#include "keybd.h"
#endif

#endif	// PRE_COMPILED_HEADERS


#ifdef TIMETEST
#include <dos.h>
struct time startTime;
struct time endTime;
#endif


//
//	globals
//

Symtab		  *CUI_Symtab;			// the system Symtab
StringTable   *CUI_Stringtab;		// the system StringTable
ResourceTable *CUI_Restab;			// global ResourceTable
int vidMemWrite = FALSE;            // are we in video-memory-write mode?
SpotHelp   *CUI_spotHelp = NULL;	// SpotHelp object
CUI_Widget CUI_helpWindow = NULL;	// HelpWindow object
bool cursesInitialized = FALSE; 	// curses initialized?

// Static to ensure we only instantiate one Application.

int Application::count = 0;

extern int	vidMemWrite;			// curses in vidmem mode?
extern FILE *CUI_traceFile; 		// trace file
extern int	CUI_tracing;			// are we tracing?


//
//	external routines
//

extern void CUI_createStringTable(void);
extern int  CUI_initCallbacks(void);
extern int	CUI_cleanupControl(void);
extern int	CUI_cleanupControlArea(void);
static int initTty(void);


//
//	constructor
//

Application::Application(char *Name)
{
	// make sure we have only one instance

	if(count++)
		CUI_fatal(dgettext(CUI_MESSAGES, 
			"Only one instance of Application may be created"));

	// get environment variables and set globals accordingly

	// curses memory-mapped mode?

	char *mmap = getenv("CURSESMMAP");
	if(mmap && strcmp(mmap, "TRUE") == 0)
        vidMemWrite = TRUE;

	// trace mode?

	char *trace = getenv("CUI_TRACE");
	if(trace)
	{
		CUI_traceFile = fopen(trace, "w");
		if(!CUI_traceFile)
			CUI_fatal(dgettext(CUI_MESSAGES, 
				"Can't open trace file '%s'"), trace);
		CUI_tracing = TRUE;
	}

    // initialize curses

	initCurses();

    // save our name

	MEMHINT();
	name = CUI_newString(Name);

	// define standard callbacks
	// (this will create the Symtab if we don't already have one)

	CUI_initCallbacks();

	// make sure we have a StringTable

	if(!CUI_Stringtab)
		CUI_createStringTable();

    // create and initialize the global ResourceTable

	MEMHINT();
    CUI_Restab = new ResourceTable(Name);

	// initialize colors (must do this before we load resources,
	// since some resources we load will reference our default colors)

	 Color::initialize();

	// load resources (don't create any widgets but Colors)

    CUI_Restab->load(FALSE);

	// initialize SpotHelp and HelpWindow

	char buffer[200];
	strcpy(buffer, name);
	buffer[0] = tolower(buffer[0]);
#ifdef TIMETEST
gettime(&startTime);
#endif
    CUI_spotHelp = new SpotHelp(buffer);
#ifdef TIMETEST
gettime(&endTime);
int startSec  = startTime.ti_sec;
int startHund = startTime.ti_hund;
int endSec	  = endTime.ti_sec;
int endHund   = endTime.ti_hund;
printf("start: %d.%d, end %d.%d\n", startSec, startHund, endSec, endHund);
#endif
	if(!CUI_spotHelp->status())
	{
		delete(CUI_spotHelp);
		CUI_spotHelp = NULL;
	}
	if(CUI_spotHelp)
	{
		CUI_helpWindow = CUI_vaCreateWidget("CUI_helpWindow",
											CUI_HELPWIN_ID, NULL,
											nullStringId);
	}

	// create the global Keyboard object

	MEMHINT();
    CUI_keyboard = new Keyboard("CUI_keyboard", NULL, NULL);

    // read/save keystrokes from/to file?

	char *file = getenv("CUI_READKEYS");
	if(file)
	{
		CUI_keyboard->readFd() = fopen(file, "r");
		if(!CUI_keyboard->readFd())
			CUI_infoMessage(dgettext(CUI_MESSAGES, 
				"Warning: can't open keystroke file '%s'"), file);
    }
	file = getenv("CUI_SAVEKEYS");
	if(file)
	{
		CUI_keyboard->saveFd() = fopen(file, "w");
		if(!CUI_keyboard->saveFd())
			CUI_infoMessage(dgettext(CUI_MESSAGES, 
				"Warning: can't open keystroke file '%s'"), file);
	}

	// create the global Emanager

	MEMHINT();
    CUI_Emanager = new Emanager;

    // initialize display and select SCREEN window
	// (display constructor will also set CUI_display, since this
	// is accessed by routines called from within the constructor!)

	MEMHINT();
    CUI_display = new Display((Widget*)this);
	CUI_display->screen()->select();
}


//
//	destructor
//

Application::~Application(void)
{
	// free and null our name so Widget destructor won't try to remove us
	// from a non-existent symbol table!

	MEMHINT();
    CUI_free(name);
	name = NULL;

	// cleanup other CUI stuff

	CUI_cleanupControl();
	CUI_cleanupControlArea();

	// display can't delete its own windows (since its this ptr is
	// cleared immediately under cfront, and Window destructor references
	// display), so we must do it here...
	// (and simplify the delete expressions for HCR C++!)

    Window *tmp = CUI_display->screen();
	MEMHINT();
    delete(tmp);
	tmp = CUI_display->message();
	MEMHINT();
    delete(tmp);
	MEMHINT();
    delete(CUI_display);

	// delete the Keyboard object

	MEMHINT();
    delete(CUI_keyboard);

	// delete Symtab and StringTable

	MEMHINT();
    delete(CUI_Symtab);
	MEMHINT();
    delete(CUI_Stringtab);

    // delete the global Emanager

	MEMHINT();
    delete(CUI_Emanager);

	// delete the global ResourceTable

	MEMHINT();
    delete(CUI_Restab);

	// try and force change of tty modes (we seem to have
	// buffering problems that prevent this from taking effect
	// even though we exit curses)

  	reset_shell_mode();

    // cleanup curses

    exitCurses();
}


//
//	initialize/cleanup curses
//

int Application::initCurses(void)
{
	initscr();
#ifdef NECCESSARY
	initTty();
#endif
	start_color();
	cursesInitialized = TRUE;
    typeahead(-1);
    cbreak();
    noecho();
#ifdef EMANAGER_SELECT  // we no longer do select() in emanager.cc
	nodelay(stdscr, TRUE);
#endif
    keypad(stdscr, TRUE);
	return(0);
}

int Application::exitCurses(void)
{
    endwin();
	return(0);
}


#ifdef NECESSARY // more work needed if so...

static char terminalTimeout = 5;
static char saveTerminalTimeout;
extern struct term *cur_term;

                              
//
//  initialize tty modes
//

static int initTty(void)
{
    struct termio ttyModes;
 
    ioctl(cur_term->Filedes, TCGETA, &ttyModes);
    saveTerminalTimeout = ttyModes.c_cc[VTIME];
    ttyModes.c_cc[VTIME] = terminalTimeout;
    ioctl(cur_term->Filedes, TCSETA, &ttyModes);
    return(0);
}
 
#endif // NECESSARY

