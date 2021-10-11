#pragma ident "@(#)control.cc   1.9     99/02/19 SMI" 

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
//	Control class implementation
//
//	$RCSfile: control.cc $ $Revision: 1.4 $ $Date: 1992/12/29 23:06:12 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_CONTROL_H
#include "control.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif
#ifndef  _CUI_VCONTROL_H
#include "vcontrol.h"
#endif

#endif // PRE_COMPILED_HEADERS


//
//	arrow-keys will navigate between Controls in a ControlArea
//	if the ControlArea has the CUI_USE_ARROWS flag set (this
//	defaults to false, except for Exclusives where it is always
//	true; programmer may set it for other, matrix-like ControlAreas)
//

#define ARROWS_ACTIVE ((getControlArea()->flagValues()) & CUI_USE_ARROWS)


//
//	internal ETI routine to sync buffer of current field
//

extern "C"
{
	void _sync_buffer(FORM *f);
};


// static for insert-mode

int Control::insertMode = FALSE;

// flag to tell us when to refresh

bool CUI_doRefresh = TRUE;

// external flag to say we're in the help system

extern bool CUI_doingHelp;


extern "C"
{
	void CUI_controlInitHook(FORM *form);
	void CUI_controlExitHook(FORM *form);
};
extern int initUserfields(void);
extern int CUI_gobackCallback(CUI_Widget, void *, void *);


// resources to load

static CUI_StringId resList[] =
{
	labelId,
	sensitiveId,
	defaultId,
	normalColorId,
	activeColorId,
	visibleId,
    nullStringId
};


//
//	initialization/cleanup routines
//

static bool initialized;

int CUI_initControl(void)
{
	if(initialized)
		return(0);

	set_field_init((FORM *)0, (PTF_void)CUI_controlInitHook);
	set_field_term((FORM *)0, (PTF_void)CUI_controlExitHook);

    set_field_opts((FIELD *)0, O_VISIBLE | O_ACTIVE | O_PUBLIC |
				   O_EDIT | O_PASSOK | O_WRAP);
	initUserfields();

	initialized = TRUE;
	return(0);
}

int CUI_cleanupControl(void)
{
	return(0);
}


//
//	constructor (don't create ETI field till we realize)
//

Control::Control(char *Name, Widget *Parent, CUI_Resource *resources,
				 CUI_WidgetId id)
	: Widget(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	// initialize Control logic if necessary

	if(!initialized)
		CUI_initControl();

	rows   = 1;
	field  = NULL;
	flags  |= CUI_SENSITIVE;

	// assign default colors

	normalColor   = CUI_NORMAL_COLOR;
	activeColor   = CUI_REVERSE_COLOR;
	disabledColor = CUI_NORMAL_COLOR;

    loadResources(resList);
	setValues(resources);

	if(parent)
		parent->adjustLocation(row, col);
}


//
//	realize the control
//

int Control::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// create ETI field

    if(!field)
	{
		int extraRows = rows - height;
		if(extraRows < 0)
			extraRows = 0;
		int extraBuffers = 1;
		field = new_field(height, width, row, col, extraRows, extraBuffers);
		if(!field)
			CUI_fatal(dgettext( CUI_MESSAGES,"Can't realize Control"));

		// store back-pointer to our ditem in field's userptr

		set_field_userptr(field, (char *)this);

		// set field options based on flag values

		syncOptions();

		// set our default color

		set_field_back(field, Color::lookupValue(normalColor));
    }

	// add our ETI FIELD to parent's ETI FORM

	if(parent)
	{
		ControlArea *area = parent->getControlArea();
		if(area)
			area->addField(field);
    }

    flags |= CUI_REALIZED;
    return(0);
}


//
//	destructor
//

Control::~Control(void)
{
	// delete label and ETI field

	MEMHINT();
    CUI_free(label);
	if(field)
        free_field(field);
}


//
//	manage geometry
//

int Control::manageGeometry(void)
{
	// since setLabel() isn't virtualized when it's called from
	// the constructor; call it again here to ensure we build
	// the label string correctly

	if(label)
		setLabel(label);
	return(0);
}


//
//	 return the ETI FORM that this control is attached to
//	 WARNING! knows about FIELD internals
//

FORM *Control::ETIform(void)
{
	if(!field)
		return(NULL);
	return(field->form);
}


//
//	return the ControlArea that this control is attached to
//

ControlArea *Control::getControlArea(void)
{
	FORM *etiForm = ETIform();
	if(!etiForm)
		return(NULL);
	else
		return((ControlArea *)form_userptr(etiForm));
}


//
//	make this Control the default in its ETI form
//

int Control::setDefault(void)
{
	FORM *form = ETIform();
	if(form && field)
		set_current_field(form, field);
   return(0);
}


//
//	resource routines
//

int Control::setValue(CUI_Resource *resource)
{
	int intValue = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case labelId:
		{
			return(setLabel(strValue));
		}
        case sensitiveId:
        {
			Widget::setValue(resource);
			syncOptions();
			return(0);
        }
		case visibleId:
		{
			if(intValue == trueId)
				flags &= ~CUI_NODISPLAY;
			else
				flags |= CUI_NODISPLAY;
            syncOptions();
			return(0);
		}
        case heightId:
		{
			height = rows = intValue;
			return(0);
        }
		case normalColorId:
		{
			return(setColorValue(strValue, normalColor));
		}
		case activeColorId:
		{
			return(setColorValue(strValue, activeColor));
		}
		case disabledColorId:
		{
			return(setColorValue(strValue, disabledColor));
		}
        default:
			return(Widget::setValue(resource));
	}
}

int Control::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
		return(Widget::getValue(resource));
	}
}


//
//	set/return length of label
//

int Control::setLabel(char *Label)
{
	MEMHINT();
    CUI_free(label);
	MEMHINT();
	label = CUI_newString(Label);
	width = strlen(label);

	// if we have a field, and we're refreshing, update it

	if(field)
	{
		set_field_buffer(field, CUI_INIT_BUFF, label);
		ControlArea *area = getControlArea();
		if(area && CUI_doRefresh)
		{
			// make sure we don't 'show through' in the event
			// that the field's window is obscured

            bool save = CUI_updateDisplay(FALSE);
			area->refresh();
			CUI_updateDisplay(save);
		}
	}
	return(0);
}

int Control::labelLength(void)
{
	if(!label)
		return(-1);
	setLabel(label);			// UGH! ensure label is composed
    return(strlen(label));
}


//
//	 handle key
//

int Control::doKey(int key, Widget *from)
{
	int saveUpdate = CUI_updateDisplay(FALSE);

    // remember the ControlArea that dispatched to us and the current FORM

    ControlArea *area = (ControlArea *)from;
	FORM *form = area->ETIform();

	// We need to know whether that ControlArea is itself contained
	// within another ControlArea (for processing overloaded keys).

	ControlArea *parentArea  = getContainingControlArea();
	ControlArea *topmostArea = getTopmostControlArea();

    // translate the key into a form-driver request

	int request = translateKey(key);

	// process the request

	switch(request)
	{
		case REQ_KILL_LINE:
		{
			form_driver(form, REQ_BEG_LINE);
			form_driver(form, REQ_CLR_EOL);
			break;
		}
        case REQ_CANCEL:
		{
			// post CUI_CANCEL message to the ControlArea

			CUI_postCancelMessage(this, area);
			break;
		}
		case REQ_HELP:
		{
			// set flag to say we don't want to refresh at the end of
			// this block (if we popup a help window, refreshing the
			// (previous) ControlArea will foul-up the cursor)

			CUI_doRefresh = FALSE;
			doHelp();
			break;
		}
		case REQ_GOBACK:
		{
			CUI_doRefresh = FALSE;
			CUI_gobackCallback(this, NULL, NULL);
			break;
		}

        // next we handle the overloaded keys;
		//	 these may be processed by the form-driver of this form,
		//	 or we may process them as 'meta-keys', passing two
		//	 requests to the containing ControlArea

		// mea culpa!
		//	 the gotos seem to be necessary, since we want the default action
		//	 (the alternative would be a bunch of fall-throughs, which is
		//	 even uglier)

		case REQ_DOWN_FIELD:
        {

#ifdef ARROW_META_KEYS // disabled this - too confusing

            // if we're on the last row, process as meta-key

			if(area->onLastRow(this) && parentArea)
			{
				doMetaKey(parentArea, REQ_NEXT_FIELD, REQ_LOCAL_HOME);
				break;
			}

#endif // ARROW_META_KEYS

			goto doDefault;
		}
		case REQ_UP_FIELD:
        {

#ifdef ARROW_META_KEYS // disabled this - too confusing

            // if we're on the first row, process as meta-key

			if(area->onFirstRow(this) && parentArea)
			{
				doMetaKey(parentArea, REQ_PREV_FIELD, REQ_LOCAL_END);
				break;
			}

#endif // ARROW_META_KEYS

            goto doDefault;
		}
        case REQ_NEXT_FIELD:
        {
			// if arrows are active, this is always a meta-key,
			// else it behaves this way only on last field

			if(ARROWS_ACTIVE || area->isLast(this) && parentArea)
			{
				doMetaKey(parentArea, REQ_NEXT_FIELD, REQ_LOCAL_HOME);
				break;
			}
			goto doDefault;
		}
		case REQ_PREV_FIELD:
        {
			// if arrows are active, this is always a meta-key,
			// else it behaves this way only on first field

			if(ARROWS_ACTIVE || area->isFirst(this) && parentArea)
			{
				doMetaKey(parentArea, REQ_PREV_FIELD, REQ_LOCAL_END);
				break;
			}
            goto doDefault;
		}
		case REQ_SCR_FPAGE:
		{
			if(rows == 1 && topmostArea)
			{
				doMetaKey(topmostArea, REQ_NEXT_FIELD, REQ_LOCAL_HOME);
				break;
			}
            goto doDefault;
        }
        case REQ_SCR_BPAGE:
        {
			if(rows == 1 && topmostArea)
			{
				doMetaKey(topmostArea, REQ_PREV_FIELD, REQ_LOCAL_HOME);
				break;
			}
            goto doDefault;
        }

#ifdef HOME_END_META_KEYS // disabled these - too confusing

        case REQ_FIRST_FIELD:
        {
            // if we're the first field process as meta-key
 
			if(area->isFirst(this) && topmostArea)
			{
				doMetaKey(topmostArea, request, REQ_LOCAL_HOME);
				break;
			}
            goto doDefault;
        }
		case REQ_LAST_FIELD:
        {
            // if we're the last field process as meta-key

			if(area->isLast(this) && topmostArea)
			{
				doMetaKey(topmostArea, request, REQ_LOCAL_END);
				break;
			}
            goto doDefault;
        }

#endif // HOME_END_META_KEYS

		// finally, do the regular stuff

doDefault:
        default:
		{
			// ask the form driver to process the request
			// (special-case BSPACE logic here...)

			int result = E_REQUEST_DENIED;
            if(request == KEY_BSPACE)
			{
				if(form_driver(form, REQ_PREV_CHAR) == E_OK)
				{
					result = form_driver(form, REQ_DEL_CHAR);
					if(form_driver(form, REQ_PREV_CHAR) == E_OK)
						result = form_driver(form, REQ_NEXT_CHAR);
				}
				else
					result = form_driver(form, REQ_DEL_CHAR);
			}

			// NULL request always succeeds...

			else if(request == REQ_NULL)
				result = E_OK;
			else
                result = form_driver(form, request);

			// get current widget (which the request might have changed)

            Widget *currentWidget = area->currentWidget();
			switch(result)
			{
				case E_OK:
				{
					// if we've recently entered a TextEdit field...
					// (TextEdit focus method sets the flag)

					if(currentWidget->flagValues() & CUI_ENTERED_TEXT)
					{
						switch(request)
						{
							// we entered the field with this request;
							// go to end of line

							case REQ_NEXT_FIELD:
							case REQ_PREV_FIELD:
							case REQ_FIRST_FIELD:
							case REQ_LAST_FIELD:
							case REQ_LEFT_FIELD:
							case REQ_RIGHT_FIELD:
							case REQ_UP_FIELD:
							case REQ_DOWN_FIELD:
							case REQ_NULL:
								form_driver(form, REQ_END_LINE);
                                break;

							default:
							{
								// if we processed a BS or a printable char,
								// clear the field (and re-enter char)

								if(request == KEY_BSPACE || request < KEY_MIN)
								{
									form_driver(form, REQ_BEG_LINE);
									form_driver(form, REQ_CLR_EOL);
									if(request != REQ_DEL_PREV)
										form_driver(form, request);
								}

								// clear the ENTERED_TEXT flag, and reset attribute

								set_field_back(currentWidget->ETIfield(),
											   Color::lookupValue(normalColor));
								currentWidget->flagValues() &= ~CUI_ENTERED_TEXT;
                            }
						}
					}
					break;
				}
				default:
				{
					// if we tried to backspace and failed,
					// try instead to delete current char
					// (we're probably at beginning of field)

					if(request == REQ_DEL_PREV)
					{
						if(form_driver(form, REQ_DEL_CHAR) != E_OK)
							beep();
					}
					else
						beep();
					break;
                }
			}
		}
	}

	// if we've moved 'backwards', we might need to ensure
	// that we focus the last item in a List or Exclusives;
	// fixupLast() takes care of this...

	area->fixupLast(request);

    // refresh our ControlArea to reflect any changes

	CUI_updateDisplay(saveUpdate);
    if(CUI_doRefresh)
		area->refresh();
	CUI_doRefresh = TRUE;
    return(0);
}


//
//	 handle 'meta-key'
//

int Control::doMetaKey(ControlArea *area, int request1, int request2)
{
    // save update mode and turn off updating for speed

	bool saveMode = CUI_updateDisplay(FALSE);

	// remember the current widget

	FORM *form		   = area->ETIform();
	int  previousField = field_index(current_field(form));

	// process the first request

	form_driver(form, request1);

	// if current item didn't change, it's the only one in area

	// if we're doing NEXT and current item is < previous current,
	// or we're doing PREV and current item is > previous current,
	// item was the last/first in area, and we wrapped...

	// in each of these cases, we must process the request again
	// in the topmost ControlArea, to make sure that we jump to
	// the next/previous ControlArea (but check to make sure that
	// we're not already processing the topmost ControlArea)

	int nextField = field_index(current_field(form));
	if(nextField == previousField ||
	   (request1 == REQ_NEXT_FIELD && nextField < previousField) ||
	   (request1 == REQ_PREV_FIELD && nextField > previousField))
	{
		ControlArea *next = getTopmostControlArea();
		if(next != area)
		{
			area = next;
			form_driver(area->ETIform(), request1);
		}
	}

	// we only process the 2nd request if area's current widget is virtual

	int id = area->currentWidget()->getId();
	if(id == CUI_VCONTROL_ID)
		area->doKey(request2, NULL);

	// restore saved update mode and refresh (if CUI_doRefresh is TRUE)
	// (then set CUI_doRefresh to FALSE so we don't do it again...)

	CUI_updateDisplay(saveMode);
	if(CUI_doRefresh)
		area->refresh();
	CUI_doRefresh = FALSE;
	return(0);
}


//
//	translate keystroke into a form-driver request
//	(default action for all controls - derived classes may modify)
//

int Control::translateKey(int key)
{
	// first handle the movement keys

    switch(key)
	{
		case KEY_HELP:
			return(REQ_HELP);
		case KEY_UP:
			if(rows > 1)
				return(REQ_PREV_LINE);
			else
			{
				if(ARROWS_ACTIVE)
					return(validate(REQ_UP_FIELD));
				else
					return(-1);
			}
        case KEY_DOWN:
			if(rows > 1)
				return(REQ_NEXT_LINE);
			else
			{
				if(ARROWS_ACTIVE)
					return(validate(REQ_DOWN_FIELD));
				else
					return(-1);
			}
        case KEY_LEFT:
			if(ARROWS_ACTIVE)
				return(validate(REQ_LEFT_FIELD));
			else
				return(-1);
		case KEY_RIGHT:
			if(ARROWS_ACTIVE)
				return(validate(REQ_RIGHT_FIELD));
			else
				return(-1);
        case KEY_HOME:
			if(ARROWS_ACTIVE)
				return(validate(REQ_FIRST_FIELD));
			else
				return(-1);
        case KEY_END:
			if(ARROWS_ACTIVE)
                return(validate(REQ_LAST_FIELD));
			else
				return(-1);
        case KEY_PGUP:
			return(REQ_SCR_BPAGE);
		case KEY_PGDN:
			return(REQ_SCR_FPAGE);
		case KEY_TAB:
			return(validate(REQ_NEXT_FIELD));
		case KEY_BTAB:
			return(validate(REQ_PREV_FIELD));
		case KEY_CANCEL:
			return(-1);
	}

	// now handle the editing keys

	switch(key)
	{
		// ignore BSPACE unless we're doing help (TextEdit overrides)

        case KEY_BSPACE:
		{
			if(CUI_doingHelp)
				return(REQ_GOBACK);
			else
				return(-1);
		}
        case KEY_INS:
		{
			if(insertMode)
			{
				insertMode = FALSE;
				return(REQ_OVL_MODE);
			}
			else
			{
				insertMode = TRUE;
				return(REQ_INS_MODE);
			}
		}
		case KEY_DEL:
		    return(REQ_DEL_CHAR);
		default:
			return(key);
	}
}


//
//	validate; returns passed request if valid, else -1
//

int Control::validate(int request)
{
    FORM *form = ETIform();
	if(form_driver(form, REQ_VALIDATION) != E_OK)
		return(-1);
	else
		return(request);
}


//
//	focus/unfocus
//

int Control::focus(bool)
{
	doCallback(CUI_FOCUS_CALLBACK);

    // if our ControlArea is current, set field background to focus,
	// else set to unfocus

	ControlArea *area = getControlArea();
	if(area)
	{
		if(area->isCurrent())
			set_field_back(field, Color::lookupValue(activeColor));
		else
			set_field_back(field, Color::lookupValue(normalColor));
		area->refresh();
		return(0);
    }
	return(-1);  // tell caller we can't oblige
}

int Control::unfocus(void)
{
	doCallback(CUI_UNFOCUS_CALLBACK);
	set_field_back(field, Color::lookupValue(normalColor));
	return(0);
}


//
//	is this Control current?
//

bool Control::isCurrent(void)
{
	FORM *form = ETIform();
	return(form ? field == current_field(form) : FALSE);
}


//
//	if this ControlArea is contained within another ControlArea,
//	return pointer to the containing area
//

ControlArea *Control::getContainingControlArea(void)
{
	ControlArea *containingArea = getControlArea();
	if(!containingArea)
		return(NULL);		// shouldn't happen!

	Widget *grandParent = containingArea->getParent();
	if(!grandParent)
		return(NULL);		// no grandParent

	return(grandParent->getControlArea());
}


//
//	return the topmost containing ControlArea
//

ControlArea *Control::getTopmostControlArea(void)
{
	ControlArea *containingArea = getContainingControlArea();
	if(!containingArea)
		return(NULL);		// shouldn't happen!

	while(TRUE)
	{
		Widget *lparent = containingArea->getParent();
		if(!lparent)
			break;
		ControlArea *tmp = lparent->getControlArea();
		if(!tmp)
			break;
		containingArea = tmp;
		if(containingArea == lparent)
			break;
	}
	return(containingArea);
}


//
//	sync ETI options with our flag values
//

void Control::syncOptions(void)
{
	// do nothing if we don't yet have a FIELD

	if(!field)
		return;

	// set sensitivity/visibilty

	field_opts_on(field, O_ACTIVE | O_VISIBLE);

	if(flags & CUI_NODISPLAY)
	{
		field_opts_off(field, O_ACTIVE);
		field_opts_off(field, O_VISIBLE);
	}
	if(!(flags & CUI_SENSITIVE))
		field_opts_off(field, O_ACTIVE);

    // determine whether we should scroll (grow dynamically)
	// (check CUI_HSCROLL for single-row fields,
	// CUI_VSCROLL for multiple-row fields)

	if(rows <= 1)
	{
		if(flags & CUI_HSCROLL)
			field_opts_off(field, O_STATIC);
		else
			field_opts_on(field, O_STATIC);
	}
	else
	{
		if(flags & CUI_VSCROLL)
			field_opts_off(field, O_STATIC);
		else
			field_opts_on(field, O_STATIC);
	}
}


//===========================================================================
//						   init/exit hooks
//===========================================================================

#define CURRENT_CONTROL(f)	((Widget *)(field_userptr(current_field(f))))

void CUI_controlInitHook(FORM *form)
{
	Widget *current = CURRENT_CONTROL(form);
	if(current)
        current->focus();
}

void CUI_controlExitHook(FORM *form)
{
	Widget *current = CURRENT_CONTROL(form);
	if(current)
        current->unfocus();
}

