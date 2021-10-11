#pragma ident "@(#)area.cc   1.10     99/02/19 SMI" 

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
//	ControlArea class implementation
//
//	$RCSfile: area.cc $ $Revision: 1.5 $ $Date: 1992/12/31 02:40:52 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdlib.h>
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_VCONTROL_H
#include "vcontrol.h"
#endif
#ifndef  _CUI_XBUTTON_H
#include "xbutton.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif
#ifndef  _CUI_CAPTION_H
#include "caption.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// refresh-control flag

extern bool CUI_doRefresh;

// dummy FIELDS array so we can create initially empty ETI form

static FIELD *dummyFields[2];

// default flags

static const long default_flags = CUI_ALIGN_CAPTIONS + CUI_LAYOUT;

// resources to load

static CUI_StringId resList[] =
{
	titleId,
	alignCaptionsId,
	exclusiveId,
	noneSetId,
	borderColorId,
	titleColorId,
    interiorColorId,
	arrowsId,
    nullStringId
};


//
//	initialization/cleanup routines
//

static bool initialized = FALSE;

int CUI_initControlArea(void)
{
	extern int CUI_initControl(void);
	
	if(initialized)
		return(0);
	
	// initialize dummy FIELDs array
	
	dummyFields[0] = new_field(1, 1, 0, 0, 0, 0);
	dummyFields[1] = NULL;
	
	// set default FORM options
	
	set_form_opts((FORM *)0, 0);
	
	// we must also initialize Control (FIELD) logic,
	// since some of the defaults must be set before we create our FORM
	
	CUI_initControl();
	
	initialized = TRUE;
	return(0);
}

int CUI_cleanupControlArea()
{
	MEMHINT();
	free_field(dummyFields[0]);
    return(0);
}


//
//	constructor
//

ControlArea::ControlArea(char* Name, Widget* Parent, CUI_Resource* Resources,
						 CUI_WidgetId id)
	: Composite(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	title = NULL;

    // initialize ControlArea (FORM) logic if necessary
	
	if(!initialized)
		CUI_initControlArea();
	
	// initialize array of FIELDs
	
	fieldArraySize = 10;
	numFields = 0;
	MEMHINT();
	fields = (FIELD **) CUI_malloc(fieldArraySize * sizeof(FIELD *));
	
	// create ETI form with dummy fields array, then disconnect for re-use
	
	form = new_form(dummyFields);
	if(!form)
		CUI_fatal(dgettext(CUI_MESSAGES, 
			"Can't create ControlArea '%s'"), Name);
	set_form_fields(form, NULL);
	
	// store back-pointer to self in ETI form's userptr

	set_form_userptr(form, (char*)this);

	flags |= default_flags;
	control = NULL;

	// default mode is NonExclusive

	flags |= CUI_MULTI;

	// assign default colors

	borderColor   = CUI_NORMAL_COLOR;
	titleColor	  = CUI_REVERSE_COLOR;
	interiorColor = CUI_NORMAL_COLOR;

	// assign resources

	loadResources(resList);
	setValues(Resources);
}


//
//	realize
//

int ControlArea::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// if we're nested inside another ControlArea, we must behave is if we're
	// a Control to our parent, as well as a ControlArea to our children;
	// in order to do so, we create, locate, and realize our VirtualControl
	// (this is simply an invisible Control that points back to us; it handles
	// inter-Control navigation events itself, and passes the rest to us)

	if(parent && parent->getControlArea())
	{
		sprintf(CUI_buffer, "%s@Control", name);
		MEMHINT();
        control = new VirtualControl(CUI_buffer, NULL);
		control->widgetPtr() = this;

        short Row = row;
		short Col = col;
		if(flags & CUI_BORDER)
		{
			Row++;
			Col++;
		}
		if(flags & CUI_HLINE)
			Row++;
        control->locate(Row, Col);

        control->realize();

		// if we have no children, or none of them are sensitive/active
		// make our control inactive
		// (special-case logic for TextPanels, which have no children - UGH!)
		// make sure our sensitive flag value reflects this

		bool active = FALSE;
		if(isKindOf(CUI_TEXTPANEL_ID))
		{
			if(flags & CUI_SENSITIVE)
				active = TRUE;
		}
		else
		{
			for(int i = 0; i < numChildren; i++)
			{
				long hflags = children[i]->flagValues();
				if(hflags & CUI_SENSITIVE)
				{
					active = TRUE;
					break;
				}
			}
		}

		if(!active)
		{
            field_opts_off(control->ETIfield(), O_ACTIVE);
			flags &= ~CUI_SENSITIVE;
		}
		else
		{
			field_opts_on(control->ETIfield(), O_ACTIVE);
            flags |= CUI_SENSITIVE;
		}

		// if we're a FooterPanel, move our control down one row
		// (we have a 'top border', but our border flag isn't set)

		if(ID == CUI_FOOTER_ID)
			control->wRow()++;
    }

	// tell Composite to do its job

	return(Composite::realize());
}


//
//	destructor
//

ControlArea::~ControlArea(void)
{
	MEMHINT();
    CUI_free(title);
	MEMHINT();
    CUI_free(fields);
	if(form)
        free_form(form);
	if(window)
	{
		MEMHINT();
        delete(window);
	}
	if(control)
	{
		MEMHINT();
        delete(control);
	}
}


//
//	can't inline isCurrent() since we don't know all about ControlArea
//	by the time we process our header file
//

bool ControlArea::isCurrent(void)
{
	return(control ? control->isCurrent() : TRUE );
}


//
//	locate the cursor
//

void ControlArea::locateCursor(void)
{
	Widget *current = currentWidget();
	if(current)
	{
        current->locateCursor();
		if(CUI_doRefresh && window)
		{
			bool updating = CUI_updateMode();
			if(updating)
				CUI_refreshDisplay(FALSE);
			if(updating)
				window->refresh();
		}
	}
}


//
//	resource routines
//

int ControlArea::setValue(CUI_Resource *resource)
{
	int  intValue  = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case titleId:
		{
			MEMHINT();
			CUI_free(title);
			MEMHINT();
			title = CUI_newString(strValue);
			if(window)
				window->setTitle(title);
			break;
        }
		case alignCaptionsId:
		{
			setFlagValue(intValue, CUI_ALIGN_CAPTIONS);
            break;
		}
		case exclusiveId:
		{
			if(intValue == trueId)
				flags &= ~CUI_MULTI;
			else
				flags |= CUI_MULTI;
			break;
		}
		case noneSetId:
		{
			setFlagValue(intValue, CUI_NONE_SET);
			break;
		}
		case arrowsId:
		{
			setFlagValue(intValue, CUI_USE_ARROWS);
			break;
		}
        case borderColorId:
		{
			return(setColorValue(strValue, borderColor));
		}
		case titleColorId:
		{
			return(setColorValue(strValue, titleColor));
		}
        case interiorColorId:
		{
			return(setColorValue(strValue, interiorColor));
		}
        default:
			return(Composite::setValue(resource));
	}
	return(0);
}

int ControlArea::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Composite::getValue(resource));
	}
}


//
//	filter/handle key
//

bool ControlArea::filterKey(CUI_KeyFilter filter, int key)
{
	Widget *current = currentWidget();
	if(current)
		return(current->filterKey(filter, key));
	else
		return(FALSE);
}

int ControlArea::doKey(int key, Widget *)
{
	// LOCAL_HOME and LOCAL_END requests are special;
	// they're used for 'meta navigation', and they mean that we must
	// ask the form associated with the current VirtualControl to
	// process a FIRST_FIELD or LAST_FIELD request - these requests
	// must *not* be passed on to any lower-level ControlAreas (such
	// as an Exclusives)

	Widget *current = currentWidget();
	int retcode = -1;
	if(current)
	{
		if(key == REQ_LOCAL_HOME || key == REQ_LOCAL_END)
		{
			if(current->getId() == CUI_VCONTROL_ID)
			{
				VirtualControl *lcontrol = (VirtualControl *)current;
				current = lcontrol->widgetPtr();
				if(current->getId() == CUI_CONTROL_AREA_ID)
				{
            		FORM *lform = current->ETIform();
					int request = REQ_FIRST_FIELD;
					if(key == REQ_LOCAL_END)
						request = REQ_LAST_FIELD;
					retcode = form_driver(lform, request);

					// fixup backwards moves...

					if(key == REQ_LOCAL_END)
						((ControlArea *)current)->fixupLast(REQ_PREV_FIELD);

					// and fixup TextField...

					current = ((ControlArea *)current)->currentWidget();
					if(current && (current->flagValues() & CUI_ENTERED_TEXT))
						form_driver(lform, REQ_END_LINE);
				}
			}
		}
		else
			retcode  = current->doKey(key, this);
	}
	else
	{
		CUI_infoMessage(dgettext(CUI_MESSAGES, 
			"No current widget to receive key %d"), key);
	}

	return(retcode);
}


//
//	show/hide form
//

int ControlArea::show(void)
{
	// make sure we're realized and we have a window

	realize();
	initWindow();

	// disable screen updating for speed

	bool saveMode = CUI_updateDisplay(FALSE);

    // show our window and post the FORM (if we have any fields)

	window->show();
	if(numFields)
	{
		int err = post_form(form);
		if(err && err != E_POSTED)
			CUI_fatal(dgettext(CUI_MESSAGES, 
				"Fatal error posting FORM (errcode = %d)"), err);

		// default to overlay mode

		form_driver(form, REQ_OVL_MODE);

		// unfocus all children - if any are ScrollingLists, they will
		// show focus on their own current item(s) - then refocus current

		Widget *current = currentWidget();
		for(int i = 0; i < numChildren; i++)
			children[i]->unfocus();
		current->focus();

		// check flags and set default item if specified

		for(i = 0; i < numChildren; i++)
		{
			if(children[i]->flagValues() & CUI_DEFAULT)
				children[i]->setDefault();
		}
	}

    // tell children to show themselves

	for(int i = 0; i < numChildren; i++)
		children[i]->show();

	// re-focus our current widget to ensure that any footer-message
	// is redisplayed (tell that widget not to reset the focus)

	Widget *current = currentWidget();
	if(current)
		current->focus(FALSE);

	// post NULL request to self so we correctly initialize TextFields

	doKey(REQ_NULL, this);

    // restore screen update mode and refresh

	CUI_updateDisplay(saveMode);

	return(refresh());
}

int ControlArea::hide(void)
{
	// tell children to hide themselves

	for(int i = 0; i < numChildren; i++)
		children[i]->hide();

    // unpost the FORM (ignore any error)

	int err = unpost_form(form);
	NO_REF(err);

	// if we have a window, hide it

	if(window)
		window->hide();

	return(0);
}


//
//	add/remove field
//

int ControlArea::addField(FIELD *field)
{
	// hide self, and if we'll change size, delete our window
	// disconnect fields

	hide();
	if(window && (flags & CUI_RESIZEABLE))
		freeWindow();
	set_form_fields(form, NULL);

	// grow fields array if necessary

	if(numFields == fieldArraySize - 1)  // keep final NULL!
	{
		fields = (FIELD **)CUI_arrayGrow((char **)fields, 10);
		fieldArraySize += 10;
	}

	//	add field at the end of the array and reconnect

	fields[numFields++] = field;
	return(connectFields());
}

int ControlArea::removeField(FIELD *field)
{
	// error if not in the array

	int index = lookupField(field);
	if(index < 0)
		return(-1);

	// hide self, and if we'll change size, delete our window
	// disconnect fields

	hide();
	if(window && (flags & CUI_RESIZEABLE))
		freeWindow();
	set_form_fields(form, NULL);

	// shuffle down to remove the item

	for( ; index < numFields - 1; index++)
		fields[index] = fields[index + 1];
	numFields--;

	// reconnect fields

	return(connectFields());
}


//
//	locate self
//

int ControlArea::locate(short Row, short Col)
{
	// save new location

	row = Row;
	col = Col;

	// if we've got a window; move it

	if(window)
		window->locate(row, col);

	// refresh and return

	return(refresh());
}


//
//	refresh
//

int ControlArea::refresh(void)
{
	if(window)
	{
		bool updating = CUI_updateMode();
		if(updating)
			CUI_refreshDisplay(FALSE);
		locateCursor();
		if(updating)
			window->refresh();
    }
    return(0);
}


//
//	focus/unfocus
//

int ControlArea::focus(bool reset)
{
	// invoke callback, if any

	doCallback(CUI_FOCUS_CALLBACK);

	// go to first control if we were asked to reset

	if(form)
	{
		if(reset)
			form_driver(form, REQ_FIRST_FIELD);
		else
		{
			// re-focus our current widget to ensure that any footer-message
			// is redisplayed (tell that widget not to reset the focus)

			Widget *current = currentWidget();
			if(current)
				current->focus(FALSE);
		}
        if(CUI_doRefresh)
			refresh();
		return(0);
	}
	return(-1);
}

int ControlArea::unfocus(void)
{
	// invoke callback, if any

	doCallback(CUI_UNFOCUS_CALLBACK);

    // unfocus our current widget

	Widget *current = currentWidget();
	if(current)
		current->unfocus();
	if(CUI_doRefresh)
		refresh();
    return(0);
}


//
//	return next/previous Widget (before/after specified widget)
//

Widget *ControlArea::nextWidget(Widget *widget)
{
	int index = lookupWidget(widget);
	if(index == -1)
		return(NULL);
	if(index == numChildren - 1)
		return(children[0]);
	return(children[index + 1]);
}

Widget *ControlArea::previousWidget(Widget *widget)
{
	int index = lookupWidget(widget);
	if(index == -1)
		return(NULL);
	if(index == 0)
		return(children[numChildren - 1]);
	return(children[index - 1]);
}


//
//	make specified Widget current
//

int ControlArea::setCurrent(Widget *widget)
{
	// is widget one of our children?

	if(lookupWidget(widget) == -1)
		return(-1);

	FIELD *field = widget->ETIfield();
	if(field && form)
		set_current_field(form, field);
	return(0);
}


//
//	unset whatever children are currently set
//

int ControlArea::unsetCurrent(void)
{
	for(int i = 0; i < numChildren; i++)
		children[i]->setOn(FALSE);
	return(0);
}


//
//	return our ETI FIELD (can't inline due to include-file sequencing)
//

FIELD *ControlArea::ETIfield(void)
{
	return(control ? control->ETIfield() : NULL);
}


//
//	make this default (can't inline due to include-file sequencing)
//

int ControlArea::setDefault(void)
{
		return(control && control->setDefault());
}


//===========================================================================
//					   internal helper routines
//===========================================================================


//
//	lookup widget and return its index
//

int ControlArea::lookupWidget(Widget *widget)
{
	if(widget)
	{
		for(int i = 0; i < numChildren; i++)
		{
			if(widget == children[i])
				return(i);
		}
	}
	return(-1);
}


//
//	lookup field and return its index
//

int ControlArea::lookupField(FIELD *field)
{
	for(int i = 0; i < numFields; i++)
	{
		if(field == fields[i])
			return(i);
	}
	return(-1);
}


//
//	initialize/free our window
//

int ControlArea::initWindow(void)
{
	// anything to do?

	if(window)
		return(0);

	// if row or col is -1, we must pop up...

	if(row == -1 || col == -1)
	{
		// if CENTERED, center on-screen, else use current row/col

		if(flags & CUI_CENTERED)
		{
			// subtract 1 from row for message window...

			row = (CUI_screenRows() - height - 1) / 2;
			col = (CUI_screenCols() - width) / 2;
        }
		else
			CUI_getCursor(&row, &col);
	}

	// create and realize our window

	int winTitle = CUI_compileString(title);
	short Row = row;
	short Col = col;
	makeAbsolute(Row, Col); // make coordinates absolute

    short Height = height;
	short Width  = width;
	CUI_Resource windowResources[15];

	int i = 0;
	if(flags & CUI_BORDER)
	{
		CUI_setArg(windowResources[i], borderWidthId,  2);	  i++;
	}
	else if(flags & CUI_HLINE)
	{
		CUI_setArg(windowResources[i], borderWidthId,  1);	  i++;
	}
	CUI_setArg(windowResources[i], rowId,			Row);			i++;
	CUI_setArg(windowResources[i], colId,			Col);			i++;
	CUI_setArg(windowResources[i], heightId,		Height);		i++;
	CUI_setArg(windowResources[i], widthId, 		Width); 		i++;
	CUI_setArg(windowResources[i], titleId, 		winTitle);		i++;
	bool scroll = (flags & CUI_VSCROLL) != 0;
	CUI_setArg(windowResources[i], vScrollId,	   scroll);   i++;

	// if we're contained within another ControlArea, we need to make
	// our window a subwindow of the ControlArea's window; we do this
	// by setting the 'parentWindow' resource to the ControlArea's name

	ControlArea *containingArea = NULL;
    if(parent)
		containingArea = parent->getControlArea();
	if(containingArea)
	{
		int bname = CUI_compileString(containingArea->getName());
		CUI_setArg(windowResources[i], parentWindowId, bname); i++;
	}

	// null-terminate resource list and create the window

	CUI_setArg(windowResources[i], nullStringId, 0); i++;

	MEMHINT();
    window = new Window(NULL, NULL, windowResources);

	// pass on our colors

	window->borderAttrib()	 = borderColor;
	window->titleAttrib()	 = titleColor;
	window->interiorAttrib() = interiorColor;

	// make sure that the window inherits our CUI_POPUP flag value
	// (this determines whether it's a sub-window of CUI_screenWindow,
	// or whether it's a standalone window)

	if(flags & CUI_POPUP)
		window->flagValues() |= CUI_POPUP;

	// finally, realize the window

    window->realize();

	// associate the curses windows with the form

	set_form_win(form, window->getOuter());
	set_form_sub(form, window->getInner());

    return(0);
}

int ControlArea::freeWindow(void)
{
	MEMHINT();
    delete(window);
	window = NULL;
	return(0);
}


//
//	(re)connect fields to group (if we have any)
//

int ControlArea::connectFields(void)
{
	if(numFields)
	{
		int errcode = set_form_fields(form, fields);
		if(errcode)
			CUI_fatal(dgettext(CUI_MESSAGES, 
				"Fatal error connecting items to FORM (errcode = %d)"),
				 errcode);
	}
    return(0);
}


//
//	is specified Widget the first/last sensitive Control in this ControlArea?
//	(if widget is a Vcontrol, we must dereference it)
//

bool ControlArea::isFirst(Widget *widget)
{
	if(widget->getId() == CUI_VCONTROL_ID)
		widget = ((VirtualControl *)widget)->widgetPtr();
	return(widget == getFirst());
}

bool ControlArea::isLast(Widget *widget)
{
	if(widget->getId() == CUI_VCONTROL_ID)
		widget = ((VirtualControl *)widget)->widgetPtr();
	return(widget == getLast());
}


//
//	is specified widget on first/last row of this ControlArea?
//	(if widget is a Vcontrol, we must dereference it)
//

bool ControlArea::onFirstRow(Widget *widget)
{
	if(widget->getId() == CUI_VCONTROL_ID)
		widget = ((VirtualControl *)widget)->widgetPtr();
	Widget *first = getFirst();
	return(widget->wRow() <= first->wRow());
}

bool ControlArea::onLastRow(Widget *widget)
{
	if(widget->getId() == CUI_VCONTROL_ID)
		widget = ((VirtualControl *)widget)->widgetPtr();
	Widget *last = getLast();
    return(widget->wRow() >= last->wRow());
}


//
//	get pointer to first/last sensitive widget in ControlArea
//

Widget *ControlArea::getFirst(void)
{
	for(int i = 0; i < numChildren; i++)
    {
		Widget *widget = children[i];
		long fflags = widget->flagValues();
		if(fflags & CUI_SENSITIVE)
		{
			if(widget->getId() == CUI_CAPTION_ID)
				widget = ((Caption *)widget)->getChild();
            return(widget);
		}
	}
	return(NULL);
}

Widget *ControlArea::getLast(void)
{
	for(int i = numChildren - 1; i >= 0; i--)
	{
		Widget *widget = children[i];
		long kflags = widget->flagValues();
		if(kflags & CUI_SENSITIVE)
		{
			if(widget->getId() == CUI_CAPTION_ID)
				widget = ((Caption *)widget)->getChild();
			return(widget);
		}
	}
	return(NULL);
}


//
//	check request to see if it's a 'backwards' move;
//	if so, check whether our current widget points to
//	an Exclusives or List, and if so, make the last
//	control in the group current
//

void ControlArea::fixupLast(int request)
{
	if(request != REQ_PREV_FIELD && request != REQ_UP_FIELD)
		return;
#ifdef META_NAVIGATION
	Widget *current = currentWidget();
	if(current->getId() == CUI_VCONTROL_ID)
	{
		Widget *control = ((VirtualControl *)current)->widgetPtr();
        if(control->isKindOf(CUI_EXCLUSIVES_ID))
			control->doKey(REQ_LAST_FIELD);
		else if(control->isKindOf(CUI_LIST_ID))
			control->doKey(REQ_LAST_ITEM);
	}
#endif // META_NAVIGATION
}

