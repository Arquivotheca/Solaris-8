#pragma ident "@(#)footer.cc   1.5     93/02/25 SMI"

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
//	FooterPanel class implementation
//
//	$RCSfile: footer.cc $ $Revision: 1.4 $ $Date: 1992/09/12 15:19:38 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdlib.h>
#ifndef  _CUI_FOOTER_H
#include "footer.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
    nullStringId
};


//
//	constructor
//

FooterPanel::FooterPanel(char* Name, Widget* Parent, CUI_Resource* Resources,
						 CUI_WidgetId id)
	: ControlArea(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	loadResources(resList);
	setValues(Resources);

	// location and size are fixed

	if (row <= 1 )
		row    = 9999;
	col    = 0;
	height = 2;
	width  = 1;
}


//
//	realize
//

int FooterPanel::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// location and size are fixed (row has now been calculated)

	col    = 0;
	height = 2;
	width  = parent->wWidth() - 2;

	// set vPad to force our children to be located on row 1

	vPad = 1;

	// if we have no children, create a single StaticText child
	// and set its width to our width

	if(!numChildren)
	{
	    Widget *child;

        char buffer[80];
        sprintf(buffer, "%s@%s", name, "text");
        child = (Widget *)CUI_vaCreateWidget(buffer, CUI_STATIC_TEXT_ID, this,
                                    heightId,   CUI_RESOURCE(1),
									widthId,    CUI_RESOURCE(width),
                                    nullStringId);
        child->normalAttrib() = borderColor;
	}

	// tell ControlArea to do its job

	return(ControlArea::realize());
}


//
//	show/hide
//

int FooterPanel::show(void)
{
	ControlArea::show();
	parent->getWindow()->drawHline(row);
	return(refresh());
}

int FooterPanel::hide(void)
{
	// should really erase the hline...

	ControlArea::hide();
    return(0);
}


//
//  get pointer to StaticText child, if any (must be first child...)
//

StaticText *FooterPanel::getStaticText(void)
{
	if(numChildren && children[0]->getId() == CUI_STATIC_TEXT_ID)
		return((StaticText *)children[0]);
	return(NULL);
}

