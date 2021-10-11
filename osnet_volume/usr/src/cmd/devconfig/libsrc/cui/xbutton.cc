#pragma ident "@(#)xbutton.cc   1.8     99/02/19 SMI"

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
//	RectButton, CheckBox class implementations
//
//	$RCSfile: xbutton.cc $ $Revision: 1.3 $ $Date: 1992/12/30 21:12:47 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_CURSVMEM_H
#include "cursvmem.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_EXCLUSIVE_H
#include "exclusive.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_XBUTTON_H
#include "xbutton.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// attributes to display RectButton labels

// resources to load

static CUI_StringId resList[] =
{
	setId,
    unselectId,
	setColorId,
    nullStringId
};


// does our parent have exclusive (as opposed to nonexclusive) semantics?

#define EXCLUSIVE_PARENT (parent && !(parent->flagValues() & CUI_MULTI))



//=============================================================================
//	RectButton class implementation
//=============================================================================

//
//	constructor
//

RectButton::RectButton(char *Name, Widget *Parent, CUI_Resource *resources,
					   CUI_WidgetId id)
	: Button(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	// assign default colors

	setColor = CUI_REVERSE_COLOR;

    loadResources(resList);
    setValues(resources);
	if(!label)
	{
		MEMHINT();
        label = CUI_newString("");
	}
}


//
//	realize
//

int RectButton::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

    // invoke Control's realize method (will create ETI field)

	Control::realize();

	// if we don't have a leading "[ ]",
	// construct the label, padding out on right to specified width

	if(strlen(label) < 3 || label[2] != ']')
	{
		char buffer[200];
		char *lptr = buffer;
		strcpy(lptr, label);
		lptr += strlen(label + 1) - 1;
		int diff = (int)width - (lptr - buffer) - 1;
		for(int i = 0; i < diff; i++)
			*lptr++ = ' ';
		*lptr++ = ']';
		*lptr = 0;

		MEMHINT();
		CUI_free(label);
		MEMHINT();
		label = CUI_newString(buffer);
	}

	// store label as ETI field's value

    return(set_field_buffer(field, CUI_INIT_BUFF, label));
}


//
//	resource routines
//

int RectButton::setValue(CUI_Resource *resource)
{
	int intValue = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case setId:
		{
			setOn(intValue == trueId);
			break;
		}
		case unselectId:
		{
			char *lname = strValue;
			addCallback(lname, CUI_UNSELECT_CALLBACK);
			break;
        }
		case setColorId:
		{
			return(setColorValue(strValue, setColor));
		}
        default:
			return(Button::setValue(resource));
	}
	return(0);
}

int RectButton::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		case setId:
			resource->value = (void *)isSet;
			break;
        default:
			return(Button::getValue(resource));
	}
	return(0);
}


//
//	set label string
//

int RectButton::setLabel(char *Label)
{
	if(Label && Label[0] != '[')
	{
		char buffer[200];

		// if we have an empty label, don't surround with spaces

		if(label[0] == 0 || (Label[0] == ' ' && Label[1] == 0))
			strcpy(buffer, "[ ]");
		else
			sprintf(buffer, "[ %s ]", Label);
		return(Control::setLabel(buffer));
	}
	return(0);
}


//
//	set value on/off, or toggle
//

void RectButton::setOn(bool on)
{
	if(on)
	{
		// anything to do?

		if(isSet)
			return;

		// if parent has exclusive semantics,
		// unset any member that is currently set

		if(EXCLUSIVE_PARENT)
			((ControlArea *)parent)->unsetCurrent();

		// if we have a parent, and it has a window, reshow our focus

		if(parent && parent->getWindow())
			showFocus(isCurrent());

		// invoke any callbacks

		doCallback(CUI_SELECT_CALLBACK);
		isSet = TRUE;
	}
	else
	{
		// anything to do?

		if(!isSet)
			return;

		// reshow our focus

		showFocus(isCurrent());

		// invoke any callbacks

		doCallback(CUI_UNSELECT_CALLBACK);
		isSet = FALSE;
	}
}

int RectButton::toggle(void)
{
	if(isSet) // if we're currently set...
    {
		// if our parent is Exclusive, and noneSet is FALSE, do nothing

		if(EXCLUSIVE_PARENT && !(parent->flagValues() & CUI_NONE_SET))
			return(-1);

		// else unset

		setOn(FALSE);
	}
	else // currently unset...
	{
		setOn(TRUE);
	}
    return(0);
}


//
//	handle key
//

int RectButton::doKey(int key, Widget *from)
{
	// if RETURN or SPACE, toggle...

	if(key == KEY_RETURN || key == ' ')
	{
		int retcode = toggle();
		if(retcode == 0)
		{
			// need a callback??
		}
		return(retcode);
	}
	else
		return(Button::doKey(key, from));
}


//
//	modify focus/unfocus attribute
//

int RectButton::showFocus(bool current)
{
	// invoke callback, if any

	if(current)
		doCallback(CUI_FOCUS_CALLBACK);
	else
		doCallback(CUI_UNFOCUS_CALLBACK);

	if(field)
	{
		chtype attrib;
		if(current && ((ControlArea *)parent)->isCurrent())
		{
			if(isSet && ID == CUI_RECTBUTTON_ID)
                attrib = Color::lookupValue(setColor);
			else
				attrib = Color::lookupValue(activeColor);
		}
		else
		{
			if(isSet && ID == CUI_RECTBUTTON_ID)
                attrib = Color::lookupValue(setColor);
			else
				attrib = Color::lookupValue(normalColor);
		}
		set_field_back(field, attrib);

		// set field status to changed, and refresh form

		set_field_status(field, TRUE);
		bool save = CUI_updateDisplay(FALSE);
		parent->refresh();
		CUI_updateDisplay(save);
		return(0);
    }
	return(-1);
}


//=============================================================================
//	CheckBox class implementation
//=============================================================================

#define ASCII_CHECKMARK 	'*'
#define VIDMEM_CHECKMARK	ACS_CHECKMARK


//
//	constructor
//

CheckBox::CheckBox(char *Name, Widget *Parent, CUI_Resource *resources,
				   CUI_WidgetId id)
	: RectButton(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
    setValues(resources);
}


//
//	realize
//

int CheckBox::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	RectButton::realize();
	updateCheck(isSet);
	return(0);
}


//
//  set label
//

int CheckBox::setLabel(char *Label)
{
	if(Label && Label[0] != ' ' && Label[0] != '[')
	{
		char buffer[200];

		// handle empty label

		if(Label[0] == 0)
			strcpy(buffer, "[ ]");

		// form label depending on whether parent is Exclusive or NonExclusive

		else if(EXCLUSIVE_PARENT)
			sprintf(buffer, "[  %s ]", Label);
		else
			sprintf(buffer, "[ ]  %s", Label);

		return(Control::setLabel(buffer));
    }
	return(-1);
}


//
//	set value on/off, or toggle
//

void CheckBox::setOn(bool on)
{
	RectButton::setOn(on);
	if(flags & CUI_REALIZED)
		updateCheck(on);
}

int CheckBox::toggle(void)
{
	int wasSet	= isSet;
	int retcode = RectButton::toggle();
	if(retcode == 0)
	{
		if(wasSet)
			updateCheck(FALSE);
		else
			updateCheck(TRUE);
	}
	return(0);
}


//
//	update checkmark
//

void CheckBox::updateCheck(bool on)
{
	if(label)
	{
		unsigned char checkmark;

		if(vidMemWrite)
			checkmark = VIDMEM_CHECKMARK;
		else
			checkmark = ASCII_CHECKMARK;

		if(!label)
			CUI_fatal(dgettext( CUI_MESSAGES, "Null label for Checkbox %s"), name);
		if(on)
			label[1] = checkmark;
		else
			label[1] = ' ';

		set_field_buffer(field, CUI_INIT_BUFF, label);
        showFocus(isCurrent());
	}
}

