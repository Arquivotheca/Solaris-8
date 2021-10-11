#pragma ident "@(#)vcontrol.cc   1.4     93/01/08 SMI" 

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
//	VirtualControl class implementation
//
//	$RCSfile: vcontrol.cc $ $Revision: 1.2 $ $Date: 1992/12/29 22:52:57 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_VCONTROL_H
#include "vcontrol.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif

#endif	// PRE_COMPILED_HEADERS


//
//	constructor
//

VirtualControl::VirtualControl(char *Name, Widget *Parent,
							   CUI_Resource *resources, CUI_WidgetId id)
	: Control(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
    setValues(resources);
}


//
//	destructor
//

VirtualControl::~VirtualControl(void)
{
	// nothing to do
}


//
//	realize
//

int VirtualControl::realize(void)
{
	// invoke Control's realize method to create our ETI FIELD

	Control::realize();

	// make ourselves invisible

	field_opts_off(field, O_PUBLIC);

	// attach self to our Widget's parent's FORM
	// (we have no parent, so this wasn't done in Control::realize)

	Widget *parentWidget = widget->getParent();
	if(parentWidget)
	{
		ControlArea *parentArea = parentWidget->getControlArea();
		if(parentArea)
			parentArea->addField(field);
	}

	// sync our flags with our widget

	if(widget->flagValues() & CUI_SENSITIVE)
		flags |= CUI_SENSITIVE;
	else
	{
		flags &= ~CUI_SENSITIVE;
		field_opts_off(widget->ETIfield(), O_ACTIVE);
	}

    return(0);
}


//
//	handle keystroke
//

int VirtualControl::doKey(int key, Widget *from)
{
	// swallow the key if we have no widget

	if(!widget)
		return(0);

    switch(key)
	{
		// potential intra-field navigation keys
		// (our Widget gets first shot at these)

		case KEY_TAB:
		case KEY_BTAB:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_UP:
		case KEY_DOWN:
        case KEY_HOME:
		case KEY_END:
		case KEY_PGUP:
		case KEY_PGDN:
        {
			if(widget->doKey(key, from) == 0)
				return(0);

			// don't leave if we fail validation

			if(!valid())
				return(0);
			else
				return(Control::doKey(key, from));
        }

		// Widget unconditionally gets all other keys

		default:
			return(widget->doKey(key, from));
	}
}


//
//	validate current field before we leave it
//	(returns TRUE if OK, FALSE if not)
//

bool VirtualControl::valid(void)
{
	// (if form has no fields, don't call validation routine)

    FORM *form = widget->ETIform();
	if(form && form->maxfield && form_driver(form, REQ_VALIDATION) != E_OK)
		return(FALSE);
	return(TRUE);
}

