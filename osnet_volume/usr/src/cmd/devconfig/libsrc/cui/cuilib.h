#pragma ident "@(#)cuilib.h   1.5     93/07/23 SMI"

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

//============================== -*-Mode: c++;-*- =============================
//	Widget class definition
//
//	$RCSfile: cuilib.h $ $Revision: 1.2 $ $Date: 1992/12/29 21:46:01 $
//=============================================================================

#ifndef _CUI_LIB_H
#define _CUI_LIB_H


//
//	miscellaneous definitions
//

#define TMP_BUFF_LEN 1023	// size of temp buffer
#define ATTRIB_CHANGE '@'   // char that signals embedded attribute code
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif
#ifndef NO_REF
#define NO_REF(x) (x = x)	// keep the compiler happy
#endif


//
//	our own assert macro (HCR C++ brings in streams, which we don't want)
//

#define ASSERT(expr) {if (!(expr)){CUI_fatal( \
					 "Assertion failed: %s, file %s, line %d", \
					 #expr, __FILE__, __LINE__ );}}


//
// aliases for curses key values
//

#define KEY_TAB             '\t'
#define KEY_PGDN            KEY_NPAGE
#define KEY_PGUP			KEY_PPAGE
#define KEY_RETURN			'\r'
#define KEY_INS 			KEY_IC
#define KEY_DEL 			KEY_DC
#define KEY_BSPACE			KEY_BACKSPACE


//
// keep compiler quiet
//

#if !defined(CURSES) /* if not internal to curses source */
struct screen
{
	int _nobody_;
};
#endif


//
//	pointers to functions
//

#ifndef NULLPFI
typedef int 	   (*PFI)(void);	// pointer to function returning int
typedef void *	   (*PFV)(void);	// pointer to function returning void*
#define NULLPFI    ((PFI)0) 		// null function pointer
#define NULLPFV    ((PFV)0) 		// null function pointer
#endif


//
//	application commands and meta-keys
//

#define REQ_CANCEL				(MAX_COMMAND + 1)
#define REQ_DONE                (MAX_COMMAND + 2)
#define REQ_NEXT_MENU			(MAX_COMMAND + 3)
#define REQ_PREV_MENU			(MAX_COMMAND + 4)
#define REQ_HELP				(MAX_COMMAND + 5)
#define REQ_LOCAL_HOME			(MAX_COMMAND + 6)
#define REQ_LOCAL_END			(MAX_COMMAND + 7)
#define REQ_GOBACK				(MAX_COMMAND + 8)
#define REQ_KILL_LINE			(MAX_COMMAND + 9)
#define REQ_NULL				(MAX_COMMAND + 10)

//
// Localization identifier
//
#define CUI_MESSAGES "SUNW_INSTALL_CUILIB"


//
//	miscellaneous global data
//

extern char CUI_buffer[];			// temp buffer
extern int	CUI_errno;				// errno
extern int	CUI_messageDisplayed;	// info message displayed?
extern int	vidMemWrite;			// curses in video-memory mode?
extern FILE *CUI_traceFile; 		// trace file
extern int  CUI_tracing;            // are we tracing?

#endif // _CUI_LIB_H


