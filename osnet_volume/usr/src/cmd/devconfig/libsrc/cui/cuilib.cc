#pragma ident "@(#)cuilib.cc   1.5     93/07/23 SMI"

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
//	Miscellaneous library routines
//
//	$RCSfile: cuilib.cc $ $Revision: 1.9 $ $Date: 1992/09/23 00:50:10 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <signal.h>
#include <curses.h>
#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_STRTAB_H
#include "strtab.h"
#endif
#ifndef  _CUI_STRINGID_H
#include "stringid.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_DISPLAY_H
#include "display.h"
#endif

#endif	// PRE_COMPILED_HEADERS


#ifdef MSDOS
unsigned _ovrbuffer = 0x2000;
#endif


//
//	calculate number of rows and columns from layout and measure resources
//	sets rows and cols to calculated values
//

void calcRowsAndCols(int &rows, int &cols, CUI_StringId layout, short measure,
					 short numChildren)
{
    if(layout == fixedRowsId)
	{
		rows = measure;
		cols = numChildren / rows;
		if(cols * rows != numChildren)
			cols++;
    }
	else // fixedCols
	{
		cols = measure;
		rows = numChildren / cols;
		if(cols * rows != numChildren)
			rows++;
    }
}


#ifdef NO_GETTEXT

//
//	dummy gettext routine
//

char *dgettext( CUI_MESSAGES,char *text)
{
	return(text);
}

#endif // NO_GETTEXT

#ifdef NO_LOCALE

//
//  dummy setlocale routine
//
 
char *setlocale(int category, const char *locale)
{
    return("C");
}
 
#endif // NO_LOCALE



//
//	exec a sub-process, optionally saving and restoring screen
//	flag can be:
//		CUI_EXEC_QUIET:  just do it
//		CUI_EXEC_WAIT:	 save/restore keyboard/screen - wait for keystroke
//		CUI_EXEC_NOWAIT: save/restore keyboard/screen - don't wait for keystroke
//		CUI_EXEC_CUI:	 run a CUI application
//

int CUI_execCommand(char *command, CUI_ExecFlag flag)
{
	static char envBuffer[256];
	char *screenFile = NULL;
#ifdef MSDOS
    void (*saveIntHandler)(int);
	void (*saveQuitHandler)(int);
#else
	SIG_PF saveIntHandler;
	SIG_PF saveQuitHandler;
#endif

	// if requested, save screen and reset tty modes

	if(!(flag == CUI_EXEC_QUIET))
    {
		// generate temp file name for screen dump

		screenFile = tmpnam(NULL);
		if(!screenFile)
			return(-1);

		// reset tty, and save the screen

		reset_shell_mode();
		CUI_display->save(screenFile);

		// if we're not calling a CUI program, clear the screen for action
		// else store name of screen file in environment

		if(!(flag == CUI_EXEC_CUI))
		{
			attrset(A_NORMAL);
			wclear(stdscr);
			CUI_setCursor(0, 0);
			wrefresh(stdscr);
		}
		else
		{
			sprintf(envBuffer, "CUI_SCREEN_DUMP=%s", screenFile);
			putenv(envBuffer);
		}
    }

	// allow called program to be interrupted

	saveIntHandler = signal(SIGINT,  SIG_DFL);
#ifndef MSDOS
	saveQuitHandler = signal(SIGQUIT, SIG_DFL);
#endif

	// run the specified program

	int retcode = system(command);

	// restore our signals

	signal(SIGINT, saveIntHandler);
#ifndef MSDOS
	signal(SIGQUIT, saveQuitHandler);
#endif

	// exit status is in the high 8 bits of 16-bit return-code
	// (lower is signal, if process was interrupted)
	// make the adjustment...

    if(retcode >= 256)
	retcode = retcode/256;

	// if necessary, restore tty modes
	// (do this now so CUI_getKey() works)

	if(!(flag == CUI_EXEC_QUIET))
        reset_prog_mode();

    // if we were asked to wait for a keystroke, do it

	if(flag == CUI_EXEC_WAIT)
    {
		printf("\n%s\n", dgettext( CUI_MESSAGES,"Press any key to continue..."));
		CUI_getKey();
    }

	// if necessary, restore screen

	if(!(flag == CUI_EXEC_QUIET))
		CUI_display->restore(screenFile);

	CUI_refreshDisplay(TRUE);

	// return the exec'd program's exit code

	return(retcode);
}


//
//	create StringTable
//

void CUI_createStringTable(void)
{
	extern StringTable *CUI_Stringtab;		// system StringTable
	extern int CUI_compileStrings(void);	// compile 'known strings'

	if(!CUI_Stringtab)
	{
		MEMHINT();
		CUI_Stringtab = new StringTable(257);
		CUI_compileStrings();
	}
}


//
//	create Symbol table
//

void CUI_createSymtab(void)
{
	extern Symtab *CUI_Symtab;	// system Symtab

	if(!CUI_Symtab)
	{
		MEMHINT();
		CUI_Symtab = new Symtab(257);
	}
}

