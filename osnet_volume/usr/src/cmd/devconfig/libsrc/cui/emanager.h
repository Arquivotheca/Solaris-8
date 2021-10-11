#pragma ident "@(#)emanager.h   1.4     92/11/25 SMI"

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
//	Event-manager class definition
//
//	$RCSfile: emanager.h $ $Revision: 1.5 $ $Date: 1992/09/12 15:29:51 $
//=============================================================================


#ifndef _CUI_EMANAGER_H
#define _CUI_EMANAGER_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_MESSAGE_H
#include "message.h"
#endif

#endif // PRE_COMPILED_HEADERS


#define CUI_MAX_CLIENTS 10
#define CUI_MAX_MESSAGE 20

class Widget;
class Window;

#define CUI_KEYS	  1
#define CUI_MOUSE	  2


//
//	client structure
//

typedef struct
{
	Widget *widget; 	// widget to receive events
	int    events;		// types of events
} CUI_Client;


//
//	event types
//

#define CUI_KEYS	  1
#define CUI_MOUSE	  2


//
//	the event-manager
//

class Emanager
{
	protected:

		CUI_Client clients[CUI_MAX_CLIENTS];
		int  clientIndex;
		CUI_Message queue[CUI_MAX_MESSAGE];
		int  messagePutIndex;
		int  messageGetIndex;

		int  find(Widget *widget);
		int  remove(Widget *widget);
		void waitForEvent(void);
		int messageQueued(void) // do we have a queued message?
		{
			return(queue[messageGetIndex].type != CUI_NOP);
		}
		int hasKeyClient(void)
		{
			return(clientIndex >= 0 && (clients[clientIndex].events & CUI_KEYS));
		}

    public:

		Emanager(void);
		~Emanager(void) 	{ /* nothing */ }

		Widget *currentWidget(void);
		Window *currentWindow(void);
		int  activate(Widget *widget, int events);
		int  deactivate(Widget *widget);
		int  setMask(Widget *widget, int events);
		int  postMessage(Widget *from, Widget *to, CUI_MessageId type, void **args);
        void mainLoop(void);
		int  dispatchMessage(void);
		static CUI_KeyFilter filterKeys(CUI_KeyFilter);
		void locateCursor(void);
};

// the global Emanager

extern Emanager *CUI_Emanager;


#endif // _CUI_EMANAGER_H


