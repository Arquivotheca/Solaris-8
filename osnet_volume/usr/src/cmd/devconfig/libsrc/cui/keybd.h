#pragma ident "@(#)keybd.h   1.3     92/11/25 SMI"

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
//	Keyboard class definition
//	(this is derived from Widget only so that we can load resources)
//
//	$RCSfile: keybd.h $ $Revision: 1.3 $ $Date: 1992/09/12 15:28:51 $
//=============================================================================


#ifndef _CUI_KEYBD_H
#define _CUI_KEYBD_H

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef _CUI_WIDGET_H
#include "widget.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Keyboard : public Widget
{
	private:

		int    saveKey;
		FILE   *readKeyFd;	// filedes for keystroke file (read)
		FILE   *saveKeyFd;	// filedes for keystroke file (save)

		// alternate control-key values

		int   tabAlt;
		int   btabAlt;
		int   leftAlt;
		int   rightAlt;
		int   upAlt;
		int   downAlt;
		int   pgUpAlt;
		int   pgDnAlt;
		int   homeAlt;
		int   endAlt;
		int   insAlt;
		int   delAlt;
		int   cancelAlt;
		int   refreshAlt;
		int   helpAlt;
		int   emergencyExit;
		int   killLine;

    protected:

		virtual int  setValue(CUI_Resource *);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Keyboard(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				 CUI_WidgetId = CUI_KEYBD_ID);
		virtual ~Keyboard(void);

		// class-specific routines

		FILE * &readFd(void)  { return(readKeyFd); }
		FILE * &saveFd(void)  { return(saveKeyFd); }
		int    keyReady(void);
		int    getKey(void);
		int    setAlternateKey(CUI_StringId key, int value);
		int    emergencyExitKey(void) { return(emergencyExit); }
		int    killLineKey(void) { return(killLine); }
};

extern Keyboard *CUI_keyboard;	// the global Keyboard object


#endif // _CUI_KEYBD_H

