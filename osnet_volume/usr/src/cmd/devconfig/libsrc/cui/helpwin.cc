#pragma ident "@(#)helpwin.cc   1.6     93/07/23 SMI" 

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
//	HelpWindow class implementation
//
//	$RCSfile: helpwin.cc $ $Revision: 1.2 $ $Date: 1992/12/31 00:27:20 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_HELPWIN_H
#include "helpwin.h"
#endif
#ifndef  _CUI_HYPER_H
#include "hyper.h"
#endif
#ifndef  _CUI_SPOTHELP_H
#include "spothelp.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// global SpotHelp object
// (CUI_helpWindow is defined in cui.h, and is a CUI_Widget, so we must cast)

extern SpotHelp   *CUI_spotHelp;

// callbacks

int CUI_helpIndexCallback(CUI_Widget, void *, void *);
int CUI_gobackCallback(CUI_Widget, void *, void *);
static int exitCallback(CUI_Widget, void *, void *);


// global flag to say we're processing help

bool CUI_doingHelp = FALSE;


// resources to load

static CUI_StringId resList[] =
{
	normalColorId,
	activeColorId,
	setColorId,
    nullStringId
};


//
//	constructor
//

HelpWindow::HelpWindow(char *Name, Widget *Parent, CUI_Resource *Resources,
					   CUI_WidgetId id)
	: PopupWindow(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	// assign default colors

	normalColor = CUI_NORMAL_COLOR;
	activeColor = CUI_REVERSE_COLOR;
	setColor    = CUI_UNDERLINE_COLOR;

	// load resources, and set absolutes & (defaults if not set by resources)

    loadResources(resList);
	setValues(Resources);
	row = col = -1;
	vPad = vSpace = 0;
	hPad = hSpace = 1;
	if(width < 2)
#ifdef THIS_NOT_DEFINED
		width = CUI_screenCols() - 4;
#endif
		width = 76;
	if(height == 1)
#ifdef THIS_NOT_DEFINED
		height = CUI_screenRows() - 5;
#endif
		height = 20;
}


//
//	realize
//

int HelpWindow::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// now we have loaded all our resources, create our children
	// (a HypertextPanel, a Separator, and 1 or 3 buttons depending
	// on whether we're in hypertext mode), passing resources on
	// where appropriate


    // if we have a global SpotHelp object (we found a help file)

    if(CUI_spotHelp)
	{
		Widget *child;

        char buffer[80];
        char buffer1[32];

	sprintf(buffer, "%s@%s", name, "panel");
	child = (Widget *)CUI_vaCreateWidget(buffer, CUI_HYPERPANEL_ID, this,
		heightId,	CUI_RESOURCE(height),
		textFuncId, CUI_STR_RESOURCE("hypertextfunc"),
		nullStringId);
	child->interiorAttrib() = interiorColor;
	child->borderAttrib()	= borderColor;

	sprintf(buffer, "%s@%s", name, "Separator");
	child = (Widget *)CUI_vaCreateWidget(buffer, CUI_SEPARATOR_ID, this,
		rowId,	  CUI_RESOURCE(1),
		nullStringId);

        if(CUI_spotHelp->hypertext())
	{
		sprintf(buffer1, "%-.20s", dgettext( CUI_MESSAGES, "Index"));
		sprintf(buffer, "%s@%s", name, "Index");

		child = (Widget *)CUI_vaCreateWidget(buffer,
	   		CUI_OBLONGBUTTON_ID, this,
		   rowId,	 CUI_RESOURCE(2),
		   colId,	 CUI_RESOURCE(0),
		   adjustId, CUI_RESOURCE(centerId),
		   labelId, CUI_STR_RESOURCE(buffer1),
		   nullStringId);
		child->normalAttrib() = normalColor;
		child->activeAttrib() = activeColor;
            CUI_addCallback(child, CUI_SELECT_CALLBACK,
	    	CUI_helpIndexCallback, NULL);
	    CUI_addCallback(child, CUI_HELP_CALLBACK,
		CUI_helpIndexCallback, NULL);

		sprintf(buffer1, "%-.20s", dgettext( CUI_MESSAGES, "Go Back"));
		sprintf(buffer, "%s@%s", name, "GoBack");

		child = (Widget *)CUI_vaCreateWidget(buffer,   CUI_OBLONGBUTTON_ID, this,
			rowId,	  CUI_RESOURCE(2),
			colId,	  CUI_RESOURCE(1),
			adjustId, CUI_RESOURCE(centerId),
			labelId,  CUI_STR_RESOURCE(buffer1),
		nullStringId);
		child->normalAttrib() = normalColor;
		child->activeAttrib() = activeColor;
		CUI_addCallback(child, CUI_SELECT_CALLBACK,
			CUI_gobackCallback, NULL);
		CUI_addCallback(child, CUI_HELP_CALLBACK,
			CUI_helpIndexCallback, NULL);
        }

	sprintf(buffer1, "%-.20s", dgettext( CUI_MESSAGES, "Dismiss"));

	sprintf(buffer, "%s@%s", name, "Dismiss");
	child = (Widget *)CUI_vaCreateWidget(buffer,   CUI_OBLONGBUTTON_ID, this,
		rowId,	  CUI_RESOURCE(2),
		colId,	  CUI_RESOURCE(2),
		adjustId, CUI_RESOURCE(centerId),
		labelId,  CUI_STR_RESOURCE(buffer1),
	nullStringId);

	child->normalAttrib() = normalColor;
	child->activeAttrib() = activeColor;
        CUI_addCallback(child, CUI_SELECT_CALLBACK,
			   exitCallback, NULL);
	CUI_addCallback(child, CUI_HELP_CALLBACK,
			CUI_helpIndexCallback, NULL);

    } // end if(CUI_spotHelp)

	return(Shell::realize());
}


//
//	resource routines
//

int HelpWindow::setValue(CUI_Resource *resource)
{
	int  intValue  = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case normalColorId:
			return(setColorValue(strValue, normalColor));
		case activeColorId:
			return(setColorValue(strValue, activeColor));
		case setColorId:
			return(setColorValue(strValue, setColor));
        default:
			return(PopupWindow::setValue(resource));
	}
}

int HelpWindow::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(PopupWindow::getValue(resource));
	}
}


//
//	show help topic for 'key'
//

int HelpWindow::showHelp(char *key)
{
	if(CUI_spotHelp)
	{
		CUI_doingHelp = TRUE;

		// realize if necessary, and save pointer to our HypertextPanel

		realize();
		HypertextPanel *panel = (HypertextPanel *)children[0];

		// make sure we're popped-up

		popUp();

        // already there?

		char *currentKey = panel->topicKey();
		if(currentKey && strcmp(key, currentKey) == 0)
			return(0);

		// show the topic

		return(panel->showTopic(key));
	}
	else
	{
		CUI_infoMessage(dgettext( CUI_MESSAGES, "No help is available for '%s'"), key);
		return(-1);
	}
}


//
//	show previous help topic
//

int HelpWindow::showPrevHelp(void)
{
	return(((HypertextPanel *)children[0])->prevTopic());
}


//
//	focus our Panel, and show either first link, or current depending on flag
//

void HelpWindow::focusPanel(bool showFirst)
{
	HypertextPanel *panel = (HypertextPanel *)children[0];

	// when panel gets the focus, it will automatically select first link;
	// if this isn't what we want, we tell it to save and restore current link

	if(!showFirst)
	{
		CUI_updateDisplay(FALSE);
		panel->saveLink();
	}
	setCurrent(panel);
	if(!showFirst)
	{
		CUI_updateDisplay(TRUE);
		panel->restoreLink();
		panel->focusIfInLink(); 	// restore may have had no saved link...
	}
	CUI_cursorOK = TRUE;
}


//
//	exit help
//

int HelpWindow::exitHelp(void)
{
	if(CUI_spotHelp)
		((HypertextPanel *)children[0])->exitHelp();
	return(0);
}


//
//	callbacks for Index and Go Back buttons
//

int CUI_helpIndexCallback(CUI_Widget widget, void *, void *)
{
	if(CUI_helpWindow)
	{
		HelpWindow *helpWindow = (HelpWindow *)CUI_helpWindow;

		// show index topic

		if(helpWindow->showHelp(HELP_INDEX_KEY) != 0)
		{
			helpWindow->popDown();
			return(-1);
		}

		// if caller's parent is the HelpWindow,
		// tell it to focus the Panel and show first link

		Widget *caller = (Widget *)widget;
		if(caller->getParent() == helpWindow)
		{
			helpWindow->focusPanel(TRUE);
//			  caller->getControlArea()->refresh();
//			  helpWindow->locateCursor();
//			  CUI_cursorOK = TRUE;
		}
		return(0);
    }
	else
		return(-1);
}

int CUI_gobackCallback(CUI_Widget widget, void *, void *)
{
	int retcode = -1;
	if(CUI_helpWindow)
	{
		HelpWindow *helpWindow = (HelpWindow *)CUI_helpWindow;

		// turn off display updating and show previous topic

		CUI_updateDisplay(FALSE);
		retcode = helpWindow->showPrevHelp();
		short saveRow, saveCol;
		CUI_getCursor(&saveRow, &saveCol);

		// if caller's parent is the HelpWindow (it should be!),
		// focus its panel and highlight its current link

		Widget *caller = (Widget *)widget;
		if(caller->getParent() == helpWindow)
		{
            helpWindow->focusPanel(FALSE);
			caller->getControlArea()->refresh();
			CUI_cursorOK = TRUE;
		}

		// re-enable display updating

		CUI_updateDisplay(TRUE);
		CUI_setCursor(saveRow, saveCol);
    }
	return(retcode);
}

static int exitCallback(CUI_Widget, void *, void *)
{
	int retcode = -1;
	if(CUI_helpWindow)
	{
		HelpWindow *helpWindow = (HelpWindow *)CUI_helpWindow;
		retcode = helpWindow->exitHelp();
	}
	return(retcode);
}


