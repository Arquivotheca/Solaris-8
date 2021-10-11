#pragma ident "@(#)hyper.cc   1.6     93/07/23 SMI" 

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
//	HypertextPanel class implementation
//
//	$RCSfile: hyper.cc $ $Revision: 1.3 $ $Date: 1992/12/31 00:27:20 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_HYPER_H
#include "hyper.h"
#endif
#ifndef  _CUI_TOPIC_H
#include "topic.h"
#endif
#ifndef  _CUI_HELPWIN_H
#include "helpwin.h"
#endif
#ifndef  _CUI_SPOTHELP_H
#include "spothelp.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_STRINGID_H
#include "stringid.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_VCONTROL_H
#include "vcontrol.h"
#endif
#ifdef MSDOS
#include <alloc.h>
#endif

#endif	// PRE_COMPILED_HEADERS



// the global SpotHelp object (this provides our text)

extern SpotHelp *CUI_spotHelp;


// external flag to prevent ControlArea refreshes

extern bool CUI_doRefresh;


// external flag to say we're in the help system

extern bool CUI_doingHelp;


// BUMP value for links array

#define BUMP 8


// static for a simple link save/restore operation

HypertextLink *HypertextPanel::savedLink = NULL;


//
//	constructor
//

HypertextPanel::HypertextPanel(char *Name, Widget *Parent,
							   CUI_Resource *Resources, CUI_WidgetId id)
	: TextPanel(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	height = 10;
	setValues(Resources);
	width = parent->wWidth();
	flags &= ~CUI_CONSTRAINED;
}


//
//	destructor
//

HypertextPanel::~HypertextPanel(void)
{
	if(topics)
	{
		for(int i = 0; i < topicIndex; i++)
		{
			MEMHINT();
			delete(topics[i]);
		}
		MEMHINT();
		CUI_free(topics);
	}
}


//
//	focus
//

int HypertextPanel::focus(bool)
{
	if( ((PopupWindow *)parent)->isPoppedUp() )
	{
		// force a refresh of our parent now, so that we can tell our caller
		// not to (it might disturb our cursor)

		parent->refresh();
		CUI_doRefresh = FALSE;

		// focus our first link

		showFirstLink();
		syncCursor();
	}
    return(0);
}


//
//	go to start of screen, and highlight first link
//	(should really highlight a button - GoBack? - if no link on screen
//	and no text to scroll to)
//

int HypertextPanel::showFirstLink(void)
{
	// disable updating

	CUI_updateDisplay(FALSE);

    // first we show all links unfocussed

	showAllLinks();

	// go back to start of screen

	line = firstLine;
	column = 0;
	syncCursor();

	// enable screen updating

	CUI_updateDisplay(TRUE);

    // highlight first link
	// (nexlink skips if we're already on it, so handle this here...)

	if(inLink())
		showLink(TRUE);
	else
		nextLink(TRUE);

    return(0);
}


//
//	show all on-screen links unfocussed (doesn't update - caller is responsible)
//

int HypertextPanel::showAllLinks(void)
{
	// go to start of screen

	line = firstLine;
	column = 0;

	// don't update while we do this...

	bool save = CUI_updateDisplay(FALSE);

	// find each link and show it unfocussed

	while(nextLink(FALSE) == 0)
		column++;	// don't keep finding the same link...

	// restore saved update mode

	CUI_updateDisplay(save);

    return(0);
}


//
//	handle keystroke
//

int HypertextPanel::doKey(int key, Widget *from)
{
	if(!func)
		return(-1);

    switch(key)
	{
        // TextPanel handles these keys, but we must figure out whether
		// we'll leave the line, and if so, unfocus current link (if any)
		// !!! beware - we're duplicating logic from TextPanel::doKey !!!

		case KEY_UP:
		case KEY_DOWN:
        case KEY_PGUP:
        case KEY_PGDN:
		case KEY_HOME:
		case KEY_END:
        {
			bool willMove = FALSE;

			if(link)
			{
				switch(key)
				{
					case KEY_UP:
					case KEY_HOME:
						willMove = (line > 0);
						break;
					case KEY_DOWN:
					case KEY_END:
						willMove = (line < numLines - 1);
						break;
					case KEY_PGUP:
						willMove = (firstLine != 0);
						break;
					case KEY_PGDN:
					{
						int last = numLines - visibleLines;
						willMove = (numLines > 0 && firstLine < last);
						break;
					}
				}

				// if we'll move, unfocus current link

				if(willMove)
					leaveLink();
			}

			// process the key

			TextPanel::doKey(key, from);

			// make sure we re-focus if necessary

			switch(key)
			{
				case KEY_UP:
				case KEY_DOWN:
				{
					// if we're in a link, focus it

					focusIfInLink();
					break;
				}
				default:
				{
					// highlight first link

					showFirstLink();
					break;
				}
			}
			break;
		}

		// TextPanel can handle these too, but we can't be sure
		// in advance whether we'll move out of the current link

        case KEY_LEFT:
		case KEY_RIGHT:
		{
			// process the key

			TextPanel::doKey(key, from);

			// if we have a currrent link, and we're outside it...

			if(link)
			{
				if( line != link->line() ||
					column < link->start() ||
					column > link->end() )
				{
					int   saveLine = line;
					short saveCol  = column;
					leaveLink();
					line   = saveLine;
					column = saveCol;
					syncCursor();
				}
			}

			// if we're in a link, focus it

			focusIfInLink();
			break;
        }

		// these are hypertext-specific keys...

		case ' ':
		case KEY_RETURN:	// follow link at cursor (if any)
		{
			if(inLink())
				return(followLink());
		}
		case KEY_TAB:		// next link
		{
			leaveLink();

            // if we have a link to go to, do it,
			// else let caller handle the key

			if(nextLink(TRUE) == 0)
				return(0);
			else
				return(-1);
		}
		case KEY_BTAB:		// prev link
        {
			leaveLink();

            // if we have a link to go to, do it,
			// else let caller handle the key

			if(prevLink() == 0)
				return(0);
			else
				return(-1);
        }
		case KEY_CANCEL:	// exit Help
		{
			exitHelp();
			return(0);
		}
		case KEY_HELP:		// go to INDEX
		{
			// do nothing if we're already there

			if(strcmp(topicKey(), HELP_INDEX_KEY) == 0)
				return(-1);
			return(showTopic(HELP_INDEX_KEY, TRUE));
		}
		case KEY_BSPACE:	// back-up to previous Topic
		case 8: 				// if terminfo is broken...
		{
			return(prevTopic());
		}
        default:
		{
			return(0);
		}
	}
	return(0);
}


//
//	check whether cursor is in a link - set link if so
//

bool HypertextPanel::inLink(void)
{
	HypertextLink **links = topic->links();
	for(int i = 0; i < topic->numLinks(); i++)
	{
		if(line == links[i]->line()    &&
		   column >= links[i]->start() &&
		   column <= links[i]->end() )
		{
			link = links[i];
			return(TRUE);
		}
	}
	link = NULL;
	return(FALSE);
}


//
//	check whether cursor is in a link, and if so, focus it
//

void HypertextPanel::focusIfInLink(void)
{
	short saveCol = column;
	if(inLink())
		showLink(TRUE);
	column = saveCol;
	syncCursor();
}


//
//	search for and goto next on-screen link (focus/unfocus according to flag)
//

int HypertextPanel::nextLink(bool showFocus)
{
	// figure out the last line currently displayed

	int maxLine = firstLine + visibleLines - 1;
	if(maxLine >= topic->numLines())
		maxLine = topic->numLines() - 1;

	// find the first link after current cursor location

	HypertextLink **links = topic->links();
	for(int i = 0; i < topic->numLinks(); i++)
	{
		int linkLine = links[i]->line();
		if(linkLine == line && links[i]->start() > column ||
		   linkLine > line && linkLine <= maxLine)
        {
			link = links[i];
			line = links[i]->line();
			column = links[i]->start();
			return(showLink(showFocus));
		}
	}
	link = NULL;
	return(-1);
}


//
//	search for, goto, and focus previous on-screen link
//

int HypertextPanel::prevLink(void)
{
	// find the first link before current cursor location

	HypertextLink **links = topic->links();
    for(int i = topic->numLinks() - 1; i >= 0; i--)
	{
		int linkLine = links[i]->line();
		if(linkLine == line && links[i]->start() < column ||
		   linkLine < line && linkLine >= firstLine)
        {
			link = links[i];
			line = links[i]->line();
			column = links[i]->start();
            return(showLink(TRUE));
		}
	}
	link = NULL;
	return(-1);
}


//
//	unfocus and unset current link, if any
//

void HypertextPanel::leaveLink(void)
{
	if(link)
	{
		showLink(FALSE);
		link = NULL;
    }
}


//
//	display current link (focus if flag is TRUE)
//

int HypertextPanel::showLink(bool showFocus)
{
	// remember whether we're updating

	bool updating = CUI_updateMode();

#ifdef MSDOS
	extern bool CUI_debuggingMemory;

	if(CUI_debuggingMemory)
	{
		unsigned long memFree = farcoreleft();
		CUI_infoMessage("%ld bytes free memory", memFree);
	}
#endif

	if(link)
	{
		short saveCol = column;
		short attrib;
		if(showFocus)
			attrib = parent->activeAttrib();
		else
			attrib = parent->setAttrib();
		line   = link->line();
		column = link->start();
		syncCursor();
		window->setTextAttrib(attrib);
		window->print(link->text());
		attrib = parent->interiorAttrib();
		window->setTextAttrib(attrib);
		if(!showFocus)
			column = saveCol;
        syncCursor();

		// why do we even need to do a wrefresh here???

		if(updating)
			wrefresh(window->getInner());

		CUI_cursorOK = TRUE;
		return(0);
	}
	return(-1);
}


//=============================================================================
// high-level methods to manage topic navigation
// these have no knowledge of how we display
//=============================================================================


//
//	follow current link
//

int HypertextPanel::followLink(void)
{
	char *target = CUI_spotHelp->target(link->text());
	if(target)
	{
		// save state in Topic stack for when we return

		topicFirstLine() = firstLine;
		topicLine() 	 = line;
		topicColumn()	 = column;

        // handle special targets:
			// !POPUP	  widget
			// !CALLBACK  routine

		if(strncmp(target, "!POPUP", 6) == 0)
		{
			target = CUI_ltrim(target + 6);
			Widget *widget = Widget::lookup(target);
			if(widget)
			{
				CUI_popup(widget, CUI_GRAB_EXCLUSIVE);
				syncCursor();
				return(0);
			}
			return(-1);
		}
		if(strncmp(target, "!CALLBACK", 9) == 0)
		{
			target = CUI_ltrim(target + 9);
			CUI_CallbackProc callback = CUI_lookupCallback(target);
			if(callback)
			{
				callback(NULL, NULL, NULL);
				syncCursor();
                return(0);
			}
			return(-1);
		}

		// by default it's a regular help topic

        return(showTopic(target, TRUE));
	}
	else
	{
		CUI_infoMessage(dgettext( CUI_MESSAGES, "Link %s has no target"), link->text());
		return(-1);
	}
}


//
//	show topic
//	  if push is TRUE, we're visiting a new topic; update display here
//	  else we're returning to a visited topic; caller will update display
//

int HypertextPanel::showTopic(char *key, bool push)
{
	if(!func)
		return(-1);

    // tell the TextFunc to load the topic (will set our topic pointer)
	// seek to first line, and retrieve number of lines in topic

	if(func(CUI_TEXTFUNC_NEW_TOPIC, (void *)key, (void *)&topic) != 0)
	{
		CUI_infoMessage(dgettext( CUI_MESSAGES, "Help topic '%s' not found"), key);
        return(-1);
	}
	seek(0);
	numLines = func(CUI_TEXTFUNC_SIZE, NULL, NULL);

	// set parent's title to the Topic's title

	if(parent)
	{
        CUI_vaSetValues(parent,
						titleId, CUI_STR_RESOURCE(topic->title()),
						nullStringId);
	}

	// determine whether the panel should be sensitive
	// (not sensitive if we have no links, and all text fits on-screen)

	bool sensitive = TRUE;
	if(topic->numLinks() == 0 && numLines <= visibleLines)
	{
		control->flagValues() &= ~CUI_SENSITIVE;
		sensitive = FALSE;
	}
	else
	{
		control->flagValues() |= CUI_SENSITIVE;
		sensitive = TRUE;
	}

	// if we're not sensitive, we must sync up with the form-driver
	// (since at this moment the Panel's VirtualControl is the current field)
	// issue two REQ_NEXT_FIELDs to make the GoBack button current)

	if(!sensitive)
	{
		FORM *parentForm = parent->ETIform();
		form_driver(parentForm, REQ_NEXT_FIELD);
		form_driver(parentForm, REQ_NEXT_FIELD);
	}

	// now that our VirtualControl is no longer current, we can change
	// its options for real (wouldn't work while it was current)

    control->syncOptions();

	// if we're visiting a new topic...

	if(push)
	{
		pushTopic(key);

		// re-set state variables for new topic

		line = column = 0;

		// disable updating for now

		bool save = CUI_updateDisplay(FALSE);

		// refresh our window contents

		refreshContents();

		// if we're sensitive, highlight first link

		if(sensitive)
			showFirstLink();

		// now we can update

		CUI_updateDisplay(save);
		syncCursor();
		refresh();
		if(!sensitive)
			parent->refresh();
		CUI_doRefresh = FALSE;
	}
	return(0);
}


//
//	push topic to stack
//

int HypertextPanel::pushTopic(char *key)
{
	// allocate array if necessary

	if(!topics)
	{
		topicIndex = 0;
		arraySize  = BUMP;
		int size   = arraySize * sizeof(TopicInfo *);
		MEMHINT();
		topics	   = (TopicInfo **) CUI_malloc(size);
	}

	// else grow array if necessary

	else if(topicIndex == arraySize - 1)  // keep final NULL!
	{
		MEMHINT();
		topics = (TopicInfo **) CUI_arrayGrow((char **)topics, BUMP);
		arraySize += BUMP;
    }

	// save topic (with our current state)

	topics[topicIndex++] = new TopicInfo(key, firstLine, line, column);

	return(0);
}


//
//	pop and discard topic from stack (this is the current topic)
//

int HypertextPanel::popTopic(void)
{
	if(topicIndex)
	{
		MEMHINT();
		delete(topics[--topicIndex]);
		topics[topicIndex] = NULL;
		return(0);
    }
	return(-1);
}


//
//	back-up to previous topic (if any)
//

int HypertextPanel::prevTopic(void)
{
	// do we have a previous topic?

	if(topicIndex < 2)
	{
		CUI_infoMessage(dgettext( CUI_MESSAGES, "No previous topic"));
		return(-1);
	}

	// pop topic from stack and then get current
	// go to newly-current topic (but don't push it to stack again!)

	popTopic();
	showTopic(topicKey(), FALSE);

	// showTopic doesn't display in no-push mode - we take care of this,
	// since we want to re-sync to the point at which we left this topic

	// disable updating for now

	CUI_updateDisplay(FALSE);

	// reset and seek to saved firstLine

	firstLine = topicFirstLine();
	seek(firstLine);

	// refresh screen contents and show all links unfocussed

	refreshContents();
	showAllLinks();

	// reset state of current link from saved TopicInfo

	line = topicLine();
	column = topicColumn();
	syncCursor();
	focusIfInLink();

	// now we can update

	CUI_updateDisplay(TRUE);
	refresh();
	syncCursor();
	CUI_doRefresh = TRUE;

	return(0);
}


//
//	exit Help
//

void HypertextPanel::exitHelp(void)
{
	// clear the visited topics stack

	while(popTopic() == 0)
		;

	// make sure panel has focus next time we pop-up
	// (we don't want to refresh display now...)

	CUI_updateDisplay(FALSE);
	((HelpWindow *)parent)->focusPanel(TRUE);

    // tell our TextFunc we're exiting (will delete topic)

	func(CUI_TEXTFUNC_EXIT, NULL, NULL);
	topic = NULL;
	link  = NULL;

	// clear the window so we don't re-show old text next time

    clear();

	// now pop down our parent & make sure cursor isn't disturbed

	CUI_popdown(parent);
	CUI_cursorOK = TRUE;

	// say we're no longer doing help

	CUI_doingHelp = FALSE;

	// now we can update

	CUI_updateDisplay(TRUE);
	CUI_refreshDisplay(FALSE);
}


//
//	restore current link from saved link pointer and focus
//

void HypertextPanel::restoreLink(void)
{
	if(savedLink)
	{
		leaveLink();
		link = savedLink;
		savedLink = NULL;
		line = link->line();
		column = link->start();
		showLink(TRUE);
	}
}


//=============================================================================
//	builtin textFunc routine to handle hypertext
//=============================================================================

int CUI_hypertextFunc(CUI_TextFuncOp op, void *arg1, void *arg2)
{
	static HypertextTopic *topic = NULL;
	static int index = 0;

	switch(op)
	{
		case CUI_TEXTFUNC_INIT:
		{
			// nothing to do yet...

			return(0);
        }
		case CUI_TEXTFUNC_SIZE:
		{
			if(topic)
				return(topic->numLines());
			else
				return(-1);
        }
        case CUI_TEXTFUNC_HOME:
		{
			if(topic)
			{
				index = 0;
				return(0);
			}
			else
				return(-1);
        }
		case CUI_TEXTFUNC_NEXT:
		{
			if(topic && index < topic->numLines() - 1)
			{
                index++;
				return(0);
			}
            else
				return(-1);
		}
		case CUI_TEXTFUNC_PREV:
		{
			if(topic && index > 0)
			{
				index--;
				return(0);
			}
            else
				return(-1);
        }
		case CUI_TEXTFUNC_READ:
		{
			if(topic)
			{
				// cast arg (1st is pointer to char *)

				char **ptr = (char **)arg1;

				// set caller's pointer to the current line

				*ptr = topic->text()[index];
				return(0);
			}
			else
				return(-1);
        }
        case CUI_TEXTFUNC_EXIT:
        {
			MEMHINT();
			delete(topic);
			topic = NULL;
			break;
        }

		// hypertext-specific operation

		case CUI_TEXTFUNC_NEW_TOPIC:
		{
			// cast args (1st is key, 2nd is pointer to caller's Topic pointer)

			char *key = (char *)arg1;
			HypertextTopic **topicPtr = (HypertextTopic **)arg2;

			// attempt to create new topic - check whether we succeeded

			HypertextTopic *newTopic = new HypertextTopic(key);
			if(!newTopic->ok())
			{
				CUI_infoMessage(dgettext( CUI_MESSAGES, "Help topic '%s' not found"), key);
				return(-1);
			}

			// success - delete previous topic and save this
			// also pass it back to caller

			MEMHINT();
			delete(topic);
			topic = newTopic;
			*topicPtr = newTopic;
		}
	}
	return(0);
}

