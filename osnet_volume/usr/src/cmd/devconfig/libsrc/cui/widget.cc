#pragma ident "@(#)widget.cc   1.8     99/02/19 SMI"

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
//	Widget class implementation
//
//	$RCSfile: widget.cc $ $Revision: 1.2 $ $Date: 1992/12/30 01:31:50 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_PROTO_H
#include "cuiproto.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WIDGET_H
#include "widget.h"
#endif
#ifndef  _CUI_HELPWIN_H
#include "helpwin.h"
#endif

#ifdef MSDOS
#include <alloc.h>
#endif

#include "cuilib.h"

#endif	// PRE_COMPILED_HEADERS


// flag to say cursor is correct - don't adjust after callback

bool CUI_cursorOK = FALSE;

// static dummy color for virtual references

short Widget::dummyColor = -1;


//
//	widget names
//	WARNING! order must match defines in cuitypes.h
//

char *Widget::typeNames[] =
{
	"Null",
	"Widget",
    "Composite",
	"ControlArea",
	"Caption",
	"Control",
	"Button",
	"OblongButton",
	"RectButton",
	"MenuButton",
	"CheckBox",
	"TextEdit",
	"TextField",
	"StaticText",
	"NumericField",
	"Item",
	"Menu",
	"MenuItem",
	"Window",
	"ScrollBar",
    "Shell",
	"BaseWindow",
	"VirtualControl",
	"Exclusives",
	"NonExclusives",
	"ScrollingList",
	"ListItem",
	"Notice",
	"PopupWindow",
	"AbbrevMenuButton",
	"FooterPanel",
	"Separator",
	"SliderControl",
	"Gauge",
	"Slider",
	"TextPanel",
	"HypertextPanel",
	"HelpWindow",
	"Keyboard",
	"Color",
    0,
};


// resources to load

static CUI_StringId resList[] =
{
	rowId,
	colId,
	heightId,
	widthId,
	borderWidthId,
	sensitiveId,
	mappedWhenManagedId,
	defaultId,
	selectId,
	focusCallbackId,
	unfocusCallbackId,
	adjustId,
	helpId,
	helpCallbackId,
	popupOnSelectId,
	popdownOnSelectId,
	userPointerId,
	footerMessageId,
    nullStringId
};

//
//	constructor
//

Widget::Widget(char* Name, Widget* Parent, CUI_Resource* Resources,
			   CUI_WidgetId id)
{
	if(!ID)
		ID = id;
	callbacks = NULL;
	row 	  = 0;
	col 	  = 0;
	height	  = 1;
	width	  = 1;

	setName(Name);
	setParent(Parent);

    // unconditionally add a FOCUS callback to display footer-message

    extern int CUI_footerMsgCallback(CUI_Widget handle, void *, void *);
    addCallback(CUI_footerMsgCallback, CUI_FOCUS_CALLBACK, NULL);

	loadResources(resList);
	setValues(Resources);
}


//
//	destructor
//

Widget::~Widget(void)
{
	MEMHINT();
    CUI_free(help);

	// remove self from symtab and free name

	CUI_Symtab->remove(name);
	MEMHINT();
    CUI_free(name);
}


//
//	add callback method
//

int Widget::addCallback(CUI_CallbackProc callback, CUI_CallbackType type,
						void *data)
{
    CUI_CallbackEntry *entry;

    // allocate and initialize the entry

	MEMHINT();
	entry = (CUI_CallbackEntry *)CUI_malloc(sizeof(CUI_CallbackEntry));
	entry->callback   = callback;
	entry->type 	  = type;
	entry->clientData = data;
	entry->next 	  = NULL;

	// add it to the end of our list

	if(!callbacks)
		callbacks = entry;
	else
	{
		CUI_CallbackEntry *currentEntry = callbacks;
		while(currentEntry->next)
			currentEntry = currentEntry->next;
		currentEntry->next = entry;
	}
	return(0);
}


//
//	lookup callback by name and assign it
//

int Widget::addCallback(char *lname, CUI_CallbackType type)
{
	CUI_CallbackProc callback = CUI_lookupCallback(lname);
	if(!callback)
		CUI_fatal(dgettext( CUI_MESSAGES, "Can't find callback '%s'"), lname);
	return(addCallback(callback, type));
}


//
//	determine whether we have a callback of specified type
//

bool Widget::hasCallback(CUI_CallbackType type)
{
	return(lookupCallback(type) != NULL);
}


//
//	return the first callback of specified type
//

CUI_CallbackProc Widget::lookupCallback(CUI_CallbackType type)
{
	CUI_CallbackEntry *entry = callbacks;
	while(entry)
	{
		if(entry->type == type)
			return(entry->callback);
		entry = entry->next;
	}
	return(NULL);
}


//
//	dispatch to callback(s) of specified type (if we have any)
//

int Widget::doCallback(CUI_CallbackType type, void *callData)
{
    CUI_CallbackEntry *entry = callbacks;
	while(entry)
	{
		// if we have a callback of specified type,
		// save cursor, invoke it, restore cursor

		if(entry->type == type)
		{
			short saveRow, saveCol;
			CUI_getCursor(&saveRow, &saveCol);
            CUI_CallbackProc callback = entry->callback;
			CUI_cursorOK = FALSE;
            callback(this, entry->clientData, callData);
			if(!CUI_cursorOK)
				CUI_setCursor(saveRow, saveCol);
        }
		entry = entry->next;
	}

	// callback may have suspended us;
	// if so return FALSE, else TRUE to continue processing

	if(flags & CUI_SUSPENDED)
		return(FALSE);
	else
		return(TRUE);
}


//
//	locate/resize
//

int Widget::locate(short Row, short Col)
{
	// should sanity-check...

	row = Row;
	col = Col;
	return(0);
}

int Widget::resize(short Height, short Width)
{
	// should sanity-check...

	height = Height;
	width  = Width;
	return(0);
}


//
//	message-handler (default actions are here)
//

int Widget::messageHandler(CUI_Message *message)
{
    switch(message->type)
	{
		case CUI_SHOW:
			return(show());
		case CUI_HIDE:
			return(hide());
		case CUI_SELECT:
			return(select());
		case CUI_UNSELECT:
			return(unselect());
		case CUI_CANCEL:
			return(cancel());
        case CUI_DONE:
			return(done());
		case CUI_INTERPRET:
			return(interpret((char *)message->args));
        default:
			badMessage(message->type);
    }
	return(0); // keep the compiler happy
}


//
//	assign name to widget (insert into symtab)
//

int Widget::setName(char *Name)
{
	// anything to do?

	if(!Name)
		return(0);

	// error if object already has a name

	if(name)
		CUI_fatal(dgettext( CUI_MESSAGES, "%s already has a name (%s)"), typeName(), name);

	// insert widget into symtab and save its name

	if(CUI_Symtab->insert(Name, this) == E_NOT_UNIQUE)
		CUI_fatal(dgettext( CUI_MESSAGES, "Name '%s' not unique for %s"), Name, typeName());
	MEMHINT();
    name = CUI_newString(Name);
	return(0);
}


//
//	assign parent to widget
//

int Widget::setParent(Widget *Parent)
{
	if(Parent)
	{
		if(parent)
			CUI_fatal(dgettext( CUI_MESSAGES, "%s '%s' already has a parent (%s)"),
					 typeName(), name, parent->getName());
        parent = Parent;
		Parent->addChild(this);
	}
	return(0);
}


//
//	assign help key to widget
//

int Widget::setHelp(char *Help)
{
	MEMHINT();
    CUI_free(help);
	MEMHINT();
    help = CUI_newString(Help);
	return(0);
}


//
//	add child to this Widget
//

int Widget::addChild(Widget *)
{
	// default is to do nothing

	return(0);
}


//
//	lookup a widget by name
//

Widget *Widget::lookup(char *name)
{
	return((Widget *)CUI_Symtab->lookup(name));
}


//
//	return CUI_WidgetId of specified name
//

CUI_WidgetId Widget::lookupName(char *name)
{
	for(int i = 0; typeNames[i]; i++)
	{
		if(strcmp(name, typeNames[i]) == 0)
			return((CUI_WidgetId)(i + CUI_BUMP));
	}
	return(CUI_NULL_ID);
}


//
//	lookup Widget type name by ID (for error-reporting)
//

char *Widget::typeName(CUI_WidgetId id)
{
	return(typeNames[id - CUI_BUMP]);
}


//
//	compare two Widgets in terms of their 'closeness' to row 0, col 0,
//	such that we end up with a left-to-right, top-to-bottom ordering
//

int Widget::compare(Widget *one, Widget *two)
{
	short oneRank = (one->row * 80) + one->col;
	short twoRank = (two->row * 80) + two->col;

	if(oneRank == twoRank)
		return(0);
	else if(oneRank < twoRank)
		return(-1);
	else
		return(1);
}


//
//	process help message
//

int Widget::doHelp(void)
{
	// if we have a HELP callback, invoke it...

	if(hasCallback(CUI_HELP_CALLBACK))
	{
		doCallback(CUI_HELP_CALLBACK);
		return(0);
	}

	// else look for a help key (either associated with self,
	// or with one of our ancestors)

	char *helpKey = NULL;
	Widget *widget = this;
	while(widget)
	{
		helpKey = widget->getHelp();
		if(helpKey)
			break;
		widget = widget->getParent();
	}

	// if we have a help key and a HelpWindow...

	if(helpKey && CUI_helpWindow)
	{
		return(((HelpWindow *)CUI_helpWindow)->showHelp(helpKey));
	}
	else
		CUI_infoMessage(dgettext( CUI_MESSAGES, "No help is currently available"));

    return(0);
}


//
//	adjust location to be relative to containing ControlArea
//	(recurses upwards till we meet a ControlArea)
//

void Widget::adjustLocation(short &Row, short &Col)
{
	Row += row;
	Col += col;
	if(flags & CUI_BORDER)
	{
		Row++;
		Col++;
	}
	else if(flags & CUI_HLINE)
		Row++;
    if(parent)
		parent->adjustLocation(Row, Col);
}


//
//	adjust row/col values to make them absolute
//

void Widget::makeAbsolute(short &Row, short &Col)
{
	if(parent)
    {
		CUI_WidgetId id = parent->getId();
		if(id != CUI_CAPTION_ID && id != CUI_ABBREV_ID)
		{
			Row += parent->wRow();
			Col += parent->wCol();
		}
		if(parent->flagValues() & CUI_BORDER)
        {
			Row++;
			Col++;
		}
		else if(parent->flagValues() & CUI_HLINE)
			Row++;
    }
	if(parent)
		parent->makeAbsolute(Row, Col);
}


//===========================================================================
//				verification and error-reporting routines
//===========================================================================

//
//	verify that we have a real widget
//

Widget *Widget::verify(Widget *object)
{
	if(!object)
		CUI_fatal(dgettext( CUI_MESSAGES, "NULL widget ID"));
	int id = (int)object->ID;
	if(id < CUI_FIRST_WIDGET_ID || id > CUI_LAST_WIDGET_ID)
		CUI_fatal(dgettext( CUI_MESSAGES, "Bad widget ID"));
	return(object);
}


//
//	verify that widget is of specified type
//

Widget *Widget::verifyIsA(Widget *object, CUI_WidgetId type)
{
	Widget *widget = verify(object);
	if(widget->ID != type)
	{
		CUI_fatal(dgettext( CUI_MESSAGES, "Bad widget ID: expected %s, found %s"),
				   typeNames[type - CUI_BUMP],
				   typeNames[widget->ID - CUI_BUMP]);
	}
    return(widget);
}


//
//	verify that widget is derived from specified type
//

Widget *Widget::verifyIsKindOf(Widget *object, CUI_WidgetId type)
{
	Widget *widget = verify(object);
	if(!(widget->isKindOf(type)))
	{
		CUI_fatal(dgettext( CUI_MESSAGES, "Bad widget ID: expected %s or derived class, found %s"),
				   typeNames[type - CUI_BUMP],
				   typeNames[widget->ID - CUI_BUMP]);
	}
	return(widget);
}


void Widget::badMessage(CUI_MessageId message)
{
	CUI_fatal(dgettext( CUI_MESSAGES, "%s object can't respond to '%s' message"),
			  typeName(), CUI_MessageNames[message]);
}

void Widget::badCommand(char *command)
{
	CUI_fatal(dgettext( CUI_MESSAGES, "%s object can't interpret '%s' command"),
			  typeName(), command);
}

