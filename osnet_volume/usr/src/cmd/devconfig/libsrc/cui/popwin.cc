#pragma ident "@(#)popwin.cc   1.4     92/11/25 SMI"

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
//	PopupWindow class implementation
//
//	$RCSfile: popwin.cc $ $Revision: 1.11 $ $Date: 1992/09/22 23:39:39 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_POPWIN_H
#include "popwin.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif

#endif	// PRE_COMPILED_HEADERS


static const long default_flags = CUI_BORDER + CUI_MAPPED + CUI_SENSITIVE
								  + CUI_HIDE_ON_EXIT;

// resources to load

static CUI_StringId resList[] =
{
	nullStringId
};


//
//	constructor
//

PopupWindow::PopupWindow(char *Name, Widget *Parent, CUI_Resource *Resources,
						 CUI_WidgetId id)
	: Shell(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	flags |= default_flags;
	poppedUp = FALSE;

    loadResources(resList);
	setValues(Resources);
	flags |= CUI_POPUP;
}


//
//	resource routines
//

int PopupWindow::setValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Shell::setValue(resource));
	}
}

int PopupWindow::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Shell::getValue(resource));
	}
}


//
//	popup/down
//

int PopupWindow::popUp(CUI_GrabMode)
{
	// we don't use the CUI_GrabMode parameter (default is CUI_GRAB_EXCLUSIVE)

    if(poppedUp)
		return(0);

	// unfocus and refresh the current ControlArea

    Widget *currentWidget = CUI_Emanager->currentWidget();
	if(currentWidget)
	{
		bool save = CUI_updateDisplay(FALSE);
		currentWidget->unfocus();
		ControlArea *area = currentWidget->getControlArea();
		CUI_updateDisplay(save);
		area->refresh();
	}
//	  CUI_cursorOK = TRUE;

    // realize, (will show, since we're MAPPED_WHEN_MANAGED)
	// show if we don't realize, and register for events

	if(!(flags & CUI_REALIZED))
		realize();
	else
		show();
	select();
	CUI_cursorOK = TRUE;			// cursor is correctly positioned...

	// save state

	poppedUp = TRUE;
    return(0);
}

void PopupWindow::popDown(void)
{
	if(poppedUp)
	{
		// done() will hide and de-register for events

		done();
		poppedUp = FALSE;

		CUI_Emanager->locateCursor();
        short cursorRow, cursorCol;
		CUI_getCursor(&row, &col);

        // refocus newly current widget

		Widget *current = CUI_Emanager->currentWidget();
		if(current)
			current->focus(FALSE);

		CUI_setCursor(row, col);
        CUI_cursorOK = TRUE;
    }
}

