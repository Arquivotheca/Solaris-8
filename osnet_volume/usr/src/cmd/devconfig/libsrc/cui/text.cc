#pragma ident "@(#)text.cc   1.9     99/02/19 SMI"

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
//	TextEdit, TextField, StaticText, and NumericField class implementations
//
//	$RCSfile: text.cc $ $Revision: 1.2 $ $Date: 1992/12/29 21:44:27 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_PROTO_H
#include "cuiproto.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_TEXT_H
#include "text.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_KEYBD_H
#include "keybd.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
 
#endif	// PRE_COMPILED_HEADERS


// flag to tell us we're processing help

extern bool CUI_doingHelp;

// our user-defined field types

extern FIELDTYPE *CUI_STRING;
extern FIELDTYPE *CUI_INTEGER;


// resources to load

static CUI_StringId textResList[] =
{
	stringId,
	verificationId,
    hScrollId,
	vScrollId,
	nullStringId
};

static CUI_StringId numResList[] =
{
	minId,
	maxId,
	nullStringId
};


//=============================================================================
//	TextEdit class implementation
//=============================================================================

//
//	constructor
//

TextEdit::TextEdit(char *Name, Widget *Parent, CUI_Resource *resources,
				   CUI_WidgetId id)
	: Control(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	flags |= CUI_VSCROLL;
    MEMHINT();
    initialValue = CUI_newString("");
	normalColor = CUI_NORMAL_COLOR;
//	  activeColor = CUI_UNDERLINE_COLOR;
    loadResources(textResList);
    setValues(resources);
}


//
//	realize
//

int TextEdit::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	rows = height;

    // invoke Control's realize method (will create ETI field)

	Control::realize();

	// set our field type to ensure validation

	if(field)
		set_field_type(field, CUI_STRING);

	// store our initial value 

	set_field_buffer(field, CUI_INIT_BUFF, initialValue);

    return(0);
}


//
//	destructor
//

TextEdit::~TextEdit(void)
{
	MEMHINT();
    CUI_free(initialValue);
}


//
//	manage our geometry
//

int TextEdit::manageGeometry(void)
{
	// if we haven't already done so...

	if(flags & CUI_INITIALIZED)
		return(0);

	// wrap our initialValue string to specified width
	// (returns newly-allocated buffer)

	char *tmp = wrapText(initialValue, width);
	if(tmp)
	{
		// reset initialValue to wrapped string and store in FIELD buffer

		MEMHINT();
        CUI_free(initialValue);
		initialValue = tmp;
		set_field_buffer(field, CUI_INIT_BUFF, initialValue);
	}

	// make sure height = at least number of lines

	int len   = strlen(initialValue);
	int lines = len / width;
	if(len % width)
		lines++;
	if(!lines)
		lines = 1;
	if(height < lines)
		height = lines;

	flags |= CUI_INITIALIZED;
    return(0);
}


//
//	resource routines
//

int TextEdit::setValue(CUI_Resource *resource)
{
	CUI_StringId value = (CUI_StringId)resource->value;
    switch(resource->id)
	{
		case stringId:
		{
			char *stringValue = CUI_lookupString(value);

			// if we haven't set our width...

            if(width == 1)
			{
				// if text contains newline(s), width = width of longest line
				// else width = length of string

				short len = 0;
				if(strchr(stringValue, '\n'))
				{
					for(char *lptr = stringValue; *lptr; lptr++)
					{
						if(*lptr == '\n')
						{
							if(len > width)
								width = len;
							len = 0;
						}
						else
                            len++;
					}
				}
				else
					width = strlen(stringValue);

				// don't exceed screen width!

				if(width > 78)
					width = 78;
			}

			// if we're realized...

			if(flags & CUI_REALIZED)
			{
				// ensure wrap will do something...

				flags &= ~CUI_INITIALIZED;

                // wrap the string to the required width
				// (returns newly-allocated buffer - don't reallocate again!)
				// reset the ETI field's buffer and refresh the ETI form

				MEMHINT();
                CUI_free(initialValue);
				initialValue = wrapText(stringValue, width);
				short cursorRow, cursorCol;
				CUI_getCursor(&cursorRow, &cursorCol);
                set_field_buffer(field, CUI_INIT_BUFF, initialValue);
				if(getControlArea())
				{
					bool save = CUI_updateDisplay(FALSE);
					getControlArea()->refresh();
					CUI_updateDisplay(save);
				}
				CUI_setCursor(cursorRow, cursorCol);
            }
			else // not realized - just save string
			{
				MEMHINT();
                CUI_free(initialValue);
				MEMHINT();
				initialValue = CUI_newString(CUI_lookupString(value));
			}
            break;
		}
		case verificationId:
		{
			char *lname = CUI_lookupString(value);
			addCallback(lname, CUI_VERIFY_CALLBACK);
			break;
        }
		case hScrollId:
		{
			if(value == trueId)
				flags |= CUI_HSCROLL;
			else
				flags &= ~CUI_HSCROLL;
            break;
		}
		case vScrollId:
		{
			if(value == trueId)
				flags |= CUI_VSCROLL;
			else
				flags &= ~CUI_VSCROLL;
            break;
		}
        default:
			return(Control::setValue(resource));
	}
	return(0);
}

int TextEdit::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Control::getValue(resource));
	}
}


//
//	translate keystroke into a form-driver request
//

int TextEdit::translateKey(int key)
{
    switch(key)
	{
		// BSPACE requires special action for scrolling fields...

        case KEY_BSPACE:
		{
			if(CUI_doingHelp)
				return(REQ_GOBACK);
			else
				return(KEY_BSPACE);
		}
        case KEY_UP:
			if(rows > 1)
				return(REQ_UP_CHAR);
			else
				return(validate(REQ_UP_FIELD));
        case KEY_DOWN:
			if(rows > 1)
				return(REQ_DOWN_CHAR);
			else
				return(validate(REQ_DOWN_FIELD));
        case KEY_LEFT:
			return(REQ_PREV_CHAR);
		case KEY_RIGHT:
			return(REQ_NEXT_CHAR);
		case KEY_HOME:
			return(REQ_BEG_FIELD);
        case KEY_END:
			return(REQ_END_FIELD);
		case KEY_RETURN:
			if(rows > 1)
				return(REQ_NEXT_LINE);
			else
				return(validate(REQ_NEXT_FIELD));
        default:
		{
			if(key == CUI_keyboard->killLineKey())
				return(REQ_KILL_LINE);
			else
				return(Control::translateKey(key));
		}
	}
}


//
//	focus (default action is to focus in normalColor;
//	TextField overrides to focus in activeColor)
//

int TextEdit::focus(bool)
{
	doCallback(CUI_FOCUS_CALLBACK);
	ControlArea *area = getControlArea();
	if(area)
	{
		set_field_back(field, Color::lookupValue(normalColor));
        area->refresh();
		return(0);
    }
	else
		return(-1); 	// can't do it...
}


//=============================================================================
//	TextField class implementation
//=============================================================================


//
//	constructor
//

TextField::TextField(char *Name, Widget *Parent, CUI_Resource *resources,
					 CUI_WidgetId id)
	: TextEdit(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	height = 1;
	rows   = 1;
	setValues(resources);
}


//
//	translate key into form-driver request
//

int TextField::translateKey(int key)
{
    switch(key)
	{
		case KEY_UP:
		case KEY_DOWN:
			return(Control::translateKey(key));
		default:
			return(TextEdit::translateKey(key));
	}
}


//
//	focus/unfocus
//

int TextField::focus(bool reset)
{
	// set flag to say we've entered field, then let Control finish the job

	flags |= CUI_ENTERED_TEXT;
	return(Control::focus(reset));
}

int TextField::unfocus(void)
{
	// Strip trailing spaces from field contents (so we can correctly
	// position to end of field next time we enter).  Note that we
	// are guaranteed that the field's contents are flushed to the
	// buffer, since we've already validated the field.

	char *data = CUI_rtrim(field_buffer(field, CUI_INIT_BUFF));
	set_field_buffer(field, CUI_INIT_BUFF, data);

	// clear 'entered field' flag, then let Control handle it

	flags &= ~CUI_ENTERED_TEXT;
	return(Control::unfocus());
}


//=============================================================================
//	NumericField class implementation
//=============================================================================


//
//	constructor
//

NumericField::NumericField(char *Name, Widget *Parent, CUI_Resource *resources,
						   CUI_WidgetId id)
	: TextField(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	height	 = 1;
	rows	 = 1;
	minValue = LONG_MIN;
	maxValue = LONG_MAX;

	loadResources(numResList);
	setValues(resources);
}


//
//	realize
//

int NumericField::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	TextEdit::realize();

	// set field type

	if(field)
		set_field_type(field, CUI_INTEGER, 1, minValue, maxValue);

    return(0);
}


//
//	resource routines
//

int NumericField::setValue(CUI_Resource *resource)
{
	CUI_StringId value = (CUI_StringId)resource->value;
    switch(resource->id)
	{
		case minId:
			minValue = value;
			break;
		case maxId:
			maxValue = value;
			break;
        default:
			return(TextField::setValue(resource));
	}
	return(0);
}

int NumericField::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(TextEdit::getValue(resource));
	}
}


//=============================================================================
//	StaticText class implementation
//=============================================================================


//
//	constructor
//

StaticText::StaticText(char *Name, Widget *Parent, CUI_Resource *resources,
					   CUI_WidgetId id)
	: TextEdit(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	height = 1;
	rows   = 1;
	flags  &= ~CUI_SENSITIVE;
	setValues(resources);
}

int StaticText::setValue(CUI_Resource *resource)
{
    switch(resource->id)
	{
        default:
			return(TextEdit::setValue(resource));
	}
}


//
//	Word-wrap text to specified line width into newly-allocated buffer.
//	(doesn't really wrap - pads each line with spaces to specified width)
//

char *TextEdit::wrapText(char *text, int width)
{
	int	line = 1;

	if(!text)
	{
		MEMHINT();
		return(CUI_newString(""));
	}

	char *from	 = text;
	MEMHINT();
    char *buffer = (char *)CUI_malloc(1);
	char *to;
    while(*from)
	{
	MEMHINT();
	buffer = (char *)realloc(buffer, (line * width + 1));
	to = buffer + ((line - 1) * width);

        for(int i = 0; i < width; i++)
		{
			if(!*from)
				break;
			if(*from == '\n')
			{
				// put spaces to end of line

				while(i < width)
				{
					*to++ = ' ';
					i++;
				}
				i = 0;
				break;
			}
			*to++ = *from++;
		}

		// done?

		if(!*from)
		{
			*to = 0;
			break;
		}

		// is next char a space or newline?

		if(*from == ' ' || *from == '\n')
		{
			// yes - bump from ptr and continue

//			  *to++ = '\n';
			from++;
			line++;
			continue;
		}

        // work backwards to look for a space

		char *tmpFrom = from;
		char *tmpTo   = to - 1;
        for( i--; i > 0; i--)
		{
			if(*tmpTo == ' ')
			{
				// found a space - pad with spaces and put newline

				tmpTo++;
				int count = width - i;
				for(int j = 0; j < count - 1; j++)
					*tmpTo++ = ' ';
				from   = tmpFrom;
				break;
			}
			tmpTo--;
			tmpFrom--;
		}

		// we didn't find a space or newline; start next line
		line++;
    }
	return(buffer);
}
