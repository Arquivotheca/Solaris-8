#pragma ident "@(#)error.cc   1.4     93/07/23 SMI"

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
//	Error-handling logic
//
//	$RCSfile: error.cc $ $Revision: 1.5 $ $Date: 1992/09/12 15:24:03 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#ifndef  _CUI_SYSCURSES_H
#endif
#include "syscurses.h"
#endif

#include "cuilib.h"

#endif	// PRE_COMPILED_HEADERS


int CUI_errno = 0;
extern bool cursesInitialized;


void CUI_warning(char *format,...)
{
    char buffer[240];

	// format the warning message

	va_list argptr;
	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	// display the error-message

	fprintf(stderr, "%s\r\n", buffer);
}

void CUI_fatal(char *format,...)
{
    char buffer[240];

	// format the error message

	va_list argptr;
	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	// if we've initialized curses, do minimal curses cleanup,
	// print the error-message, and abort

	if(cursesInitialized)
		reset_shell_mode();
	fprintf(stderr, "\007\007%s: %s\n", dgettext( CUI_MESSAGES, "FATAL ERROR"), buffer);
	abort();
}

