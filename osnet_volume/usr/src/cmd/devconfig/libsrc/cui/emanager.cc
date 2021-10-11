#pragma ident "@(#)emanager.cc   1.6     92/11/25 SMI"

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
//	event-manager
//
//	$RCSfile: emanager.cc $ $Revision: 1.17 $ $Date: 1992/09/22 21:32:27 $
//
//	This event-manager is designed to handle both keystrokes and 'messages'
//	(but not mouse events).  Currently we have no way of receiving (nor
//	generating) asynchronous messages, but we do make limited use of
//	messages (which are posted by widgets in the process of responding to
//	keystrokes).  Eventually we may, for example, implement a CUI server
//	that receives messages from one or more clients;  when we do so, we'll
//	need to figure out a way to wait on both a message and a keystroke
//	(which implies that we'll either have to use a file descriptor to
//  deliver messages, or at least to signal that a message is waiting,
//  so that we can use select() to wait on both stdin and this fd).
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef  _CUI_KEYBD_H
#include "keybd.h"
#endif

#endif // PRE_COMPILED_HEADERS

#ifdef EMANAGER_SELECT
#if !defined(MSDOS)   // for select()

#include <sys/types.h>
#include <sys/time.h>

#endif
#endif


// the global Emanager, and flag to say whether we're done

Emanager *CUI_Emanager;
static bool done = FALSE;

// function prototypes

extern void CUI_traceMessage(Widget *to, CUI_Message *message);
extern void CUI_traceKey(Widget *to, int key);

// key-filter routine

static CUI_KeyFilter filterFunc = NULL;

// flag used to control cursor update

bool updateCursor = TRUE;


//
//	constructor
//

Emanager::Emanager(void)
{
	// initialize our arrays

	int i;
	for(i = 0; i < CUI_MAX_CLIENTS; i++)
	{
	   clients[i].widget = NULL;
	   clients[i].events = CUI_NOP;
	}
	for(i = 0; i < CUI_MAX_MESSAGE; i++)
	   queue[i].type = CUI_NOP;

    clientIndex = -1;
	messageGetIndex = messagePutIndex = 0;
}


//
//	look for specified widget in client list
//

int Emanager::find(Widget *widget)
{
	int i;
	for(i = 0; i <= clientIndex; i++)
	{
		if(clients[i].widget == widget)
			return(i);
	}
	return(-1);
}


//
//	remove specified widget from client list
//

int Emanager::remove(Widget *widget)
{
	int i = find(widget);
	if(i < 0)
		return(-1); // not found

	// shuffle down to remove the client

	for( ; i < clientIndex; i++)
	{
		clients[i].widget = clients[i + 1].widget;
		clients[i].events = clients[i + 1].events;
	}

	// decrement clientIndex and return success

	clientIndex--;
	return(0);
}


//
//	activate a widget (register to receive events)
//

int Emanager::activate(Widget *widget, int events)
{
	//	remove widget from list (we don't care if it's not there)
	//	(re)add widget at head of list

	remove(widget);
	if(++clientIndex == CUI_MAX_CLIENTS)
		return(-1); // no room

	clients[clientIndex].widget = widget;
	clients[clientIndex].events = events;
    return(0);
}


//
//	deactivate a widget (will receive no more events)
//

int Emanager::deactivate(Widget *widget)
{
	return(remove(widget));
}


//
//	set client's event mask
//

int Emanager::setMask(Widget *widget, int events)
{
	// look for widget in list

	int i = find(widget);
	if(i < 0)
		return(-1); /* not found */

	// set its event mask

	clients[clientIndex].events = events;
	return(0);
}


//
//	post a message to the queue
//

int Emanager::postMessage(Widget *from, Widget *to, CUI_MessageId type, void **args)
{
	// queue the message

	if(queue[messagePutIndex].type != CUI_NOP)
		beep();
	queue[messagePutIndex].from = from;
	queue[messagePutIndex].to	= to;
	queue[messagePutIndex].type = type;
	queue[messagePutIndex].args = args;
	if(++messagePutIndex == CUI_MAX_MESSAGE)
		messagePutIndex = 0;

    return(0);
}


//
//	dispatch next message (returns TRUE if APP_EXIT, else FALSE)
//

int Emanager::dispatchMessage(void)
{
	// anything to do?

	CUI_Message message = queue[messageGetIndex];
	if(message.type == CUI_NOP)
		return(0);

	// remove from queue

	queue[messageGetIndex].type = CUI_NOP;
	if(++messageGetIndex == CUI_MAX_MESSAGE)
		messageGetIndex = 0;

	// trace the message

	if(CUI_tracing)
		CUI_traceMessage(message.to, &message);

    // if message is CUI_APP_EXIT we're done, else dispatch

	if(message.type == CUI_APP_EXIT)
		return(TRUE);
	else
	{
		message.to->messageHandler(&message);
		return(FALSE);
	}
}


//
//	wait for an event
//
//	This routine should cause us to sleep until either a message or a
//	keystroke arrives.	Since we don't currently deal with asynchronous
//	messages, we simply do a select on stdin. 
//

void Emanager::waitForEvent(void)
{
#ifdef MSDOS

	return; 	// no need to wait in a single-process system

#else

//
//  select() interferes with curses' logic that assembles
//  keyboard escape sequences - now we set curses into delay (blocking)
//  mode.  This works fine with the current design, but will need
//  rethinking if we ever go to asynchronous messages.
//

#ifdef EMANAGER_SELECT
	fd_set readFiles;
	FD_SET(0, &readFiles);					// we're interested in stdin
	select(1, &readFiles, 0, 0, NULL);		// wait for ever
#endif 

#endif
}


//===========================================================================
//	the main event loop
//===========================================================================

//
//	process events until an CUI_APP_EXIT message is received
//

void Emanager::mainLoop(void)
{
	int key;

	while(!done)
	{
		Widget *to = currentWidget();

		// if we need to, refresh the current widget
		// (we do so at start, and after every message or keystroke)

		if(updateCursor && to)
		{
			to->locateCursor();
			updateCursor = FALSE;
		}

		// queued messages take priority

		if(messageQueued())
		{
			done = dispatchMessage();
			updateCursor = TRUE;
		}

		// no waiting message - try for a keystroke

		else if(key = CUI_keyReady())
        {
			// process 'system keys'

			switch(key)
			{
#ifdef TEST
				case 3:
				{
					done = TRUE;
					continue;
                }
#endif
			}

			// if client at head of list is receiving keystrokes...

			if(hasKeyClient())
			{
				// if there's a keystroke waiting...

				if(CUI_keyReady())
				{
					// get the keystroke and process it

					int key = CUI_getKey();

#ifdef TEST
					// for testing, always exit with emergency-exit key

					if(key == CUI_keyboard->emergencyExitKey())
                    {
						done = TRUE;
						continue;
                    }
#endif
					// handle screen refresh here...

					if(key == KEY_REFRESH)
					{
						short saveRow, saveCol;
						CUI_getCursor(&saveRow, &saveCol);
						CUI_refreshDisplay(TRUE);
						CUI_setCursor(saveRow, saveCol);
                        continue;
                    }

					// trace the keystroke

					if(CUI_tracing)
						CUI_traceKey(to, key);

                    // if we have a key-filter, invoke it (indirectly);
					// if it returns TRUE we're done, else process as usual

					if(filterFunc)
					{
						if(to->filterKey(filterFunc, key))
						{
							updateCursor = TRUE;
                            continue;
						}
					}

					// dispatch the keystroke

					to->doKey(key);
					updateCursor = TRUE;
                }

			} // end selected client is accepting keys

		} // end process keystrokes

		// else (no messages queued and no key) - wait for an event

		else
			waitForEvent();

    } // end while !done

	if(CUI_traceFile)
		fclose(CUI_traceFile);
}


//
//	insert a key-filter
//

CUI_KeyFilter Emanager::filterKeys(CUI_KeyFilter func)
{
	CUI_KeyFilter prev = filterFunc;
	filterFunc = func;
	return(prev);
}


//
//	return current widget
//

Widget *Emanager::currentWidget(void)
{
	if(clientIndex < 0)
		return(NULL);
	else
		return(clients[clientIndex].widget);
}


//
//	return the Window associated with the Shell of the current Widget
//

Window *Emanager::currentWindow(void)
{
	Widget *widget = currentWidget();
	while(widget && widget->getParent())
		widget = widget->getParent();
	if(widget)
		return(widget->getWindow());
	else
		return(NULL);
}


//
//	locate cursor in current widget
//

void Emanager::locateCursor(void)
{
	Widget *to = currentWidget();
	if(to)
		to->locateCursor();
}

