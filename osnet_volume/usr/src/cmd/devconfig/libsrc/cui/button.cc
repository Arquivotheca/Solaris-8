#pragma ident "@(#)button.cc   1.4     93/01/08 SMI"

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
//	Button, OblongButton class implementations
//
//	$RCSfile: button.cc $ $Revision: 1.3 $ $Date: 1992/12/30 02:14:59 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_BUTTON_H
#include "button.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
    nullStringId
};


//=============================================================================
//	Button class implementation
//=============================================================================

//
//	constructor
//

Button::Button(char* Name, Widget* Parent, CUI_Resource* Resources,
			   CUI_WidgetId id)
    : Control(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	loadResources(resList);
    setValues(Resources);
}


//
//	realize
//

int Button::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

    // invoke Control's realize method (will create ETI field)

	Control::realize();

	// store label as ETI field's value

	return(set_field_buffer(field, CUI_INIT_BUFF, label));
}


//
//	destructor
//

Button::~Button(void)
{
	// nothing to do
}


//
//	resource routines
//

int Button::setValue(CUI_Resource *resource)
{
    switch(resource->id)
	{
        default:
			return(Control::setValue(resource));
	}
}

int Button::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Control::getValue(resource));
	}
}


//
//	set label
//

int Button::setLabel(char *Label)
{
	if(Label && Label[0] != '(')
	{
		// don't put spaces around label if it's only 1 or 2 chars wide
		// (that is, if we're an AbbrevMenuButton)

		if(strlen(Label) <= 2)
			sprintf(CUI_buffer, "(%s)", Label);
		else
			sprintf(CUI_buffer, "( %s )", Label);
		return(Control::setLabel(CUI_buffer));
	}
	return(0);
}


//
//	handle key
//

int Button::doKey(int key, Widget *from)
{
	// if RETURN or SPACE, invoke callback(s)

	if(key == KEY_RETURN || key == ' ')
	{
		// doCallback returns FALSE to suspend - we ignore this for now...

		doCallback(CUI_SELECT_CALLBACK);
		return(0);
	}

	// if edit key (8 bit char but not control char, INS, DEL), swallow it

	if((key < 256 && !iscntrl(key)) || key == KEY_INS || key == KEY_DEL)
		return(0);

	// else let Control handle it

	return(Control::doKey(key, from));
}


//
//	locate cursor (move to 1st char in field to make ugly block cursor
//	a little less ugly)
//

void Button::locateCursor(void)
{
	FORM *form = ETIform();
	form_driver(form, REQ_BEG_LINE);
	form_driver(form, REQ_NEXT_CHAR);
}


//=============================================================================
//	OblongButton class implementation
//=============================================================================

//
//	constructor
//

OblongButton::OblongButton(char* Name, Widget* Parent, CUI_Resource* Resources,
						   CUI_WidgetId id)
	: Button(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	setValues(Resources);
}

