#pragma ident "@(#)abbrev.cc   1.5     93/07/22 SMI"

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

#pragma ident "@(#)abbrev.cc   1.3     92/11/12 SMI"

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

//============================== -*-Mode: c++;-*- =============================
//	AbbrevMenu class implementation
//
//	$RCSfile: abbrev.cc $ $Revision: 1.21 $ $Date: 1992/09/26 15:41:22 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_ABBREV_h
#include "abbrev.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_MENU_H
#include "menu.h"
#endif
#ifndef  _CUI_MITEM_H
#include "mitem.h"
#endif
#ifndef  _CUI_MBUTTON_H
#include "mbutton.h"
#endif
#ifndef  _CUI_TEXT_H
#include "text.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_APPLIATION_H
#include "app.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// external flag to say cursor is correct - don't adjust after callback

extern bool CUI_cursorOK;


// resources added by AbbrevMenuButton

static CUI_StringId resList[] =
{
	menuId, 		// the menu we invoke
	normalColorId,
    activeColorId,
    nullStringId
};


// save compiled name of Menu added via resources

static int saveMenu = -1;


//
//	callback that's invoked whenever one of our Menu's items is selected
//

static int selectCallback(CUI_Widget widget,
						  void *clientData, void * /*callData*/)
{
	// set text for our StaticText to current item's label

	AbbrevMenuButton *abbrev = (AbbrevMenuButton *)clientData;
	abbrev->setText();

	// cancel the Menu (must also send cancel to MenuButton)

	MenuItem *item = (MenuItem *)widget;
	Menu *menu = item->getMenu();
	menu->cancel();
	menu->getParent()->cancel();

	// say cursor is correct

	CUI_cursorOK = TRUE;

	return(0);
}


//
//	constructor
//

AbbrevMenuButton::AbbrevMenuButton(char *Name, Widget *Parent, CUI_Resource *resources)
	: Composite(Name, Parent)
{
	ID = CUI_ABBREV_ID;

	// set saved menuPtr to -1 and load resources

	saveMenu = -1;
    loadResources(resList);
	setValues(resources);

	// set hSpace to ensure spacing between MenuButton and StaticText

	hSpace = 2;

	// assign default colors

	normalColor = CUI_NORMAL_COLOR;
	activeColor = CUI_REVERSE_COLOR;

    // create a MenuButton as our first child
	// (pass menu name on if it has already been set via resources)

    int i;
	CUI_Resource childResources[10];

	i = 0;
	CUI_setArg(childResources[i], rowId, row);				i++;
	int buttonLabel = CUI_compileString("");
	CUI_setArg(childResources[i], labelId, buttonLabel);	i++;
	CUI_setArg(childResources[i], colId, col);				i++;
    if(saveMenu != -1)
	{
		CUI_setArg(childResources[i], menuId, saveMenu); ++i;
	}
	CUI_setArg(childResources[i], nullStringId, 0);
	sprintf(CUI_buffer, "%s@Button", name);
	MEMHINT();
	MenuButton *button = new MenuButton(CUI_buffer, this, childResources);
	button->normalAttrib() = normalColor;
	button->activeAttrib() = activeColor;
}


int AbbrevMenuButton::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// pass colors on to StaticText child

	if(numChildren == 2)
	{
		children[1]->normalAttrib() = normalColor;
		children[1]->activeAttrib() = activeColor;
	}

    // invoke Composite's realize method to realize all our children

	Composite::realize();

	// set callback for each of Menu's items

	Menu *menu = ((MenuButton *)children[0])->menu();
	if(menu)
	{
		for(int i = 0; i < menu->numChildren; i++)
			menu->children[i]->addCallback(selectCallback,
										   CUI_SELECT_CALLBACK, this);
	}

	// if one of our Menu's items is specified as default,
	// it won't yet have been made current (since it hasn't been shown),
	// so do it now (must realize Menu first)

	if(menu)
	{
		menu->flagValues() &= ~CUI_MAPPED; // so Menu isn't shown now...
		menu->realize();
		menu->setDefaultItem();
	}
    return(0);
}


AbbrevMenuButton::~AbbrevMenuButton(void)
{
	// nothing to do
}


//
//	resource routines
//

int AbbrevMenuButton::setValue(CUI_Resource *resource)
{
	int intValue = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case menuId:
		{
			// save compiled menuName (we'll pass on to StaticText)

			saveMenu = intValue;
			break;
		}
		case normalColorId:
		{
			return(setColorValue(strValue, normalColor));
		}
		case activeColorId:
		{
			return(setColorValue(strValue, activeColor));
		}
        default:
			return(Composite::setValue(resource));
	}
	return(0);
}

int AbbrevMenuButton::getValue(CUI_Resource *resource)
{
	static char buffer[120];

	switch(resource->id)
	{
		case stringId:
		{
			resource->value = NULL;
            for(int i = 0; i < numChildren; i++)
			{
				if(children[i]->isKindOf(CUI_STATIC_TEXT_ID))
				{
					if(CUI_textFieldCopyString(children[i], buffer))
						resource->value = (void *)buffer;
					break;
				}
			}
            break;
		}
		default:
			return(Composite::getValue(resource));
	}
	return(0);
}


//
//	focus/unfocus
//

int AbbrevMenuButton::focus(bool)
{
	doCallback(CUI_FOCUS_CALLBACK);
	return(0);
}

int AbbrevMenuButton::unfocus(void)
{
	doCallback(CUI_UNFOCUS_CALLBACK);
	return(0);
}


//
//	add child to this Widget
//

int AbbrevMenuButton::addChild(Widget *child)
{
	switch(numChildren)
	{
		// first is our MenuButton (defined in constructor)

		case 0:
		{
			Composite::addChild(child);
			break;
		}

		// second should be Menu (defined by application)
		// or StaticText (defined in realize method)

		case 1:
		{
			// if child is our StaticText, add in the normal way

			sprintf(CUI_buffer, "%s@Text", name);
			if(strcmp(child->getName(), CUI_buffer) == 0)
			{
				Composite::addChild(child);
				break;
			}

			// else make it a child of our MenuButton (which will verify type)

			else
            {
				children[0]->addChild(child);
				break;
			}
		}

		// don't take any more children...

		default:
			CUI_fatal(gettext("AbbrevMenuButtons can take only a single Menu child"));
	}
	return(0);
}


//
//	manage geometry
//	(must create our StaticText now, so we know how wide we are)
//

int AbbrevMenuButton::manageGeometry(void)
{
	if(numChildren != 2)	// don't do this twice!
	{
		// get pointer to Menu and calculate width of its widest child
		// (we'll use this width for width of StaticText)

		short textWidth = 1;
		Menu *menu = ((MenuButton *)children[0])->menu();
		if(menu)
		{
			for(int i = 0; i < menu->numChildren; i++)
			{
				// note MenuItem widths add 1 for leading space...

				short len = menu->children[i]->wWidth() - 1;
				if(len > textWidth)
					textWidth = len;
			}
		}
		else
			CUI_fatal(gettext("AbbrevMenuButton '%s' has no child Menu"), name);

		// create a StaticText as our second child

		int i = 0;
		CUI_Resource childResources[10];

		CUI_setArg(childResources[i], rowId,   row);		 ++i;
		CUI_setArg(childResources[i], colId,   col + 2);	 ++i;
		CUI_setArg(childResources[i], widthId, textWidth);	 ++i;
		CUI_setArg(childResources[i], nullStringId, 0);
		sprintf(CUI_buffer, "%s@Text", name);
		MEMHINT();
        StaticText *text = new StaticText(CUI_buffer, this, childResources);
		NO_REF(text);
	}

	// locate our children

	children[0]->locate(row, col);
	children[1]->locate(row, col + 5);

	// adjust our height/width and return

	height = 1;
	width = 5 + children[1]->wWidth();
	return(0);
}


//
//	return Window associated with our ControlArea
//

Window *AbbrevMenuButton::getWindow(void)
{
	ControlArea *area = getControlArea();
	if(area)
		return(area->getWindow());
	else
		return(NULL);
}


//
//	show
//

int AbbrevMenuButton::show(void)
{
	children[0]->show();
	setText();
	return(0);
}


//
// set StaticText's string value to label of current MenuItem
//

int AbbrevMenuButton::setText(void)
{
	Menu *menu = ((MenuButton *)children[0])->menu();
	Widget *current = menu->currentWidget();
	if(!current)
		current = menu->children[0];
    if(current)
	{
		char *text = ((Item *)current)->getLabel();
		int stringVal = CUI_compileString(text);
        int i;
		CUI_Resource resources[10];

		i = 0;
		CUI_setArg(resources[i], stringId, stringVal);	++i;
		CUI_setArg(resources[i], nullStringId, 0);

		bool saveMode = CUI_updateDisplay(FALSE);
        CUI_setValues(children[1], resources);

        // must refresh menu if we're popped up,
        // otherwise it gets overwritten by parent form
 
        if(menu->isPoppedUp())
        {
            Window *window = menu->getWindow();
            window->select();
        }
 
		CUI_updateDisplay(saveMode);
		if(saveMode)
			CUI_refreshDisplay(FALSE);
    }
	return(0);
}

