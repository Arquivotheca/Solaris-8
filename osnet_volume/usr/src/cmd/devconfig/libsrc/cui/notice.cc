#pragma ident "@(#)notice.cc   1.5     93/07/22 SMI"

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
//	Notice class implementation
//
//	$RCSfile: notice.cc $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_NOTICE_H
#include "notice.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif

#endif // PRE_COMPILED_HEADERS


static const long default_flags = CUI_CENTERED;


// Resources added by Notice:

static CUI_StringId resList[] =
{
	stringId,
	nullStringId
};


//
//	constructor/destructor
//

Notice::Notice(char* Name, Widget* Parent, CUI_Resource* Resources,
			   CUI_WidgetId id)
	: PopupWindow(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	flags |= default_flags;
	saveString = -1;

	// defaults for row & col to center window

	row = col = -1;

	// load our resources (if we have a 'string' resource it will be
	// saved temporarily in saveString)

	saveString = -1;
	loadResources(resList);
	setValues(Resources);

	// we want our first child to be the StaticText, yet we
	// can't create it till we realize (because we may be assigned
	// resources that belong to it after we exit the constructor).
	// So, 'reserve' a slot for it...

	numChildren = 1;
}


Notice::~Notice()
{
}


//
//	realize
//

int Notice::realize()
{
	short widen;

	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

    // set up arg list and create the StaticText
 
    int i;
    CUI_Resource resources[10];
 
    i = 0;
	widen = vPad;
    CUI_setArg(resources[i], rowId, widen);				++i;
	widen = hPad;
    CUI_setArg(resources[i], colId, widen);				++i;
    CUI_setArg(resources[i], heightId, height - 1);		++i;
    if(saveString != -1)
    {
        CUI_setArg(resources[i], stringId, saveString); ++i;
        saveString = -1;
    }
    CUI_setArg(resources[i], nullStringId, 0);
    sprintf(CUI_buffer, "%s@Text", name);
    MEMHINT();
    t = new StaticText(CUI_buffer, this, resources);
 
    // if we have an explicit width, pass it on to the StaticText
 
    if(width != 1)
        t->wWidth() = width;

	// if we still have a saved string value, pass on to StaticText

	if(saveString != -1)
	{
		CUI_Resource resources[2];
		CUI_setArg(resources[0], stringId, saveString);
		CUI_setArg(resources[1], nullStringId, 0);
		CUI_setValues(children[0], resources);
        saveString = -1;
	}

	// move the StaticText from the end of the child array
	// to the beginning


	children[0] = t;
	if(children[numChildren - 1] != t)
		CUI_fatal(dgettext( CUI_MESSAGES, "Miscalculation in Notice::realize()"));
	children[--numChildren] = NULL;

    // if the user has not added any buttons, stick in
	// a default OK that just pops down the window

	if(numChildren == 1)
	{
		int i;
		CUI_Resource resources[10];

		i = 0;
		char *label = dgettext( CUI_MESSAGES, "OK");
		CUI_setArg(resources[i], labelId, CUI_compileString(label)); ++i;
		CUI_setArg(resources[i], nullStringId, 0);

		// create OK button

		sprintf(CUI_buffer, "%s@OK", name);
		MEMHINT();
        ok = new OblongButton(CUI_buffer, this, resources);

		// assign the builtin popdownCallback to our OK button

		ok->addCallback(CUI_popdownCallback, CUI_SELECT_CALLBACK, this);
	}

	// ensure our buttons are centered and located below start of text
	// (so Composite's routines will set us up correctly)

	for(i = 1; i < numChildren; i++)
	{
		children[i]->wRow() = vPad + 1;
		children[i]->flagValues() |= CUI_ADJUST_CENTER;
	}

    // now call Shell's realize method

	return(Shell::realize());
}


//
//	resource routines
//

int Notice::setValue(CUI_Resource* resource)
{
    switch(resource->id)
	{
        case stringId:
		{
			int intValue = (int)resource->value;

			// if we havent' realized, save the compiled value
			// (we'll pass to our StaticText later)

			if(!(flags & CUI_REALIZED))
			{
				saveString = intValue;
			}
			else // pass on now to the StaticText
   			{
        		CUI_Resource resources[2];
        		CUI_setArg(resources[0], stringId, intValue);
        		CUI_setArg(resources[1], nullStringId, 0);
        		CUI_setValues(children[0], resources);
        		saveString = -1;
    		}
			break;
		}
		default:
			return(PopupWindow::setValue(resource));
	}
	return(0);
}

int Notice::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(PopupWindow::getValue(resource));
	}
}


//
//	adjust children's location (!!remove this later!!)
//

int Notice::adjustChildren()
{
	return(Composite::adjustChildren());
}

