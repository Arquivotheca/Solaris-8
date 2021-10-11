#pragma ident "@(#)basewin.cc   1.3     92/11/25 SMI"

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
//	BaseWindow class implementation
//
//	$RCSfile: basewin.cc $ $Revision: 1.11 $ $Date: 1992/09/12 15:21:15 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_BASEWIN_H
#include "basewin.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif

#endif	// PRE_COMPILED_HEADERS


static const long default_flags = CUI_HIDE_ON_EXIT + CUI_BORDER;


// resources to load

static CUI_StringId resList[] =
{
    nullStringId
};


BaseWindow::BaseWindow(char* Name, Widget* Parent, CUI_Resource* Resources,
					   CUI_WidgetId id)
	: Shell(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	flags |= default_flags;
	loadResources(resList);
	setValues(Resources);
}


BaseWindow::~BaseWindow(void)
{
	// nothing to do
}


//
//	resource routines
//

int BaseWindow::setValue(CUI_Resource *resource)
{
	return(Shell::setValue(resource));
}

int BaseWindow::getValue(CUI_Resource *resource)
{
	return(Shell::getValue(resource));
}


//
//	method called from CUI_initialize() to tell us we're the main BaseWindow
//

// !!! resource logic doesn't handle mixed class/instance specs
// !!! (eg Myapp.mybutton.label), so for now our own name is
// !!! the same as the Application's.
// !!! (It really should have first char lower-cased.)

void BaseWindow::appInit(Application *app)
{
	application = app;

	// if we don't have a title, assign name as title

	if(!title)
	{
		char* appName = app->getName();
		MEMHINT();
        title = CUI_newString(appName);
		if(window)
			window->setTitle(title);
	}

    // set flags appropriately

	flags |= (CUI_MAPPED | CUI_SENSITIVE);
}

