#pragma ident "@(#)shell.cc   1.4     92/11/25 SMI"

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
//	Shell class implementation
//
//	$RCSfile: shell.cc $ $Revision: 1.9 $ $Date: 1992/09/12 17:01:22 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_SHELL_H
#include "shell.h"
#endif
#ifndef  _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef _CUI_FOOTER_H
#include "footer.h"
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

Shell::Shell(char *Name, Widget *Parent, CUI_Resource *Resources,
			 CUI_WidgetId id)
	: ControlArea(Name, Parent, NULL, id)
{
	if(!ID)
		ID = CUI_SHELL_ID;
	loadResources(resList);
	setValues(Resources);
}


//
//	realize
//

int Shell::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// call parent's realize method

	ControlArea::realize();

	// if mappedWhenManaged is set, show self

	if(flags & CUI_MAPPED)
		show();

	// if we're sensitive, select (register for events)

	if(flags & CUI_SENSITIVE)
		select();

	return(0);
}


//
//	resource routines
//

int Shell::setValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(ControlArea::setValue(resource));
	}
}

int Shell::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(ControlArea::getValue(resource));
	}
}


//
//  select/unselect shell
//

int Shell::select(void)
{
	// register for events (callback to say we're getting the focus?)

	return(CUI_Emanager->activate(this, CUI_KEYS));
}

int Shell::unselect(void)
{
	// de-register for events (callback to say we're losing the focus?)

    return(CUI_Emanager->deactivate(this));
}


//
//	show/hide self
//

int Shell::show(void)
{
    // unhighlight title of current window then tell ControlArea to show self

	Window *currentWindow = CUI_Emanager->currentWindow();
	if(currentWindow)
		currentWindow->drawTitle(FALSE);
	return(ControlArea::show());
}

int Shell::hide(void)
{
    // tell ControlArea to show self, then highlight title of current window

	int retval = ControlArea::hide();
	Window *currentWindow = CUI_Emanager->currentWindow();
	if(currentWindow)
		currentWindow->drawTitle(TRUE);
	return(retval);
}


//
//	respond to CANCEL/DONE messages
//

int Shell::cancel(void)
{
	return(doExit());
}

int Shell::done(void)
{
	return(doExit());
}


//
//	clean up and exit
//

int Shell::doExit(void)
{
	// deregister for events, then hide self if we were instructed to

	unselect();
    if(flags & CUI_HIDE_ON_EXIT)
		hide();
	return(0);
}


//
//  get pointer to StaticText widget from FooterPanel (if any)
//
 
StaticText *Shell::getFooterText(void)
{
    for(int i = numChildren - 1; i >= 0; i--)
    {
        if(children[i]->getId() == CUI_FOOTER_ID)
        {
            FooterPanel *footer = (FooterPanel *)children[i];
        	return(footer->getStaticText());
        }
    }    
   	return(NULL);
}


