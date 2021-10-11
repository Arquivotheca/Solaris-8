#pragma ident "@(#)txtpanel.cc   1.4     99/02/19 SMI"

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
//	TextPanel class implementation
//
//	$RCSfile: txtpanel.cc $ $Revision: 1.18 $ $Date: 1992/09/12 15:26:38 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_TEXTPANEL_H
#include "txtpanel.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_STRINGID_H
#include "stringid.h"
#endif
#ifndef  _CUI_APP_H
#include "app.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
	fileId,
	textFuncId,
	vScrollId,
	nullStringId
};


//
//	constructor
//

TextPanel::TextPanel(char *Name, Widget *Parent, CUI_Resource *Resources,
					 CUI_WidgetId id)
	: ControlArea(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	file	   = NULL;
	line	   = 0;
	column	   = 0;
	index	   = 0;
    firstLine  = 0;
	hadNewline = FALSE;
	func	   = NULL;
    flags      |= CUI_VSCROLL;
	flags	   |= CUI_SENSITIVE;
//	  flags 	 |= CUI_CONSTRAINED;
    loadResources(resList);
	setValues(Resources);
}


//
//	realize
//

int TextPanel::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// lookup and save TextFunc pointer

	func = (CUI_TextFunc)lookupCallback(CUI_TEXTFUNC_CALLBACK);

    // tell our textFunc to initialize itself (pass file, if any)
	// determine size of the file

	if(func)
	{
		func(CUI_TEXTFUNC_INIT, file, NULL);
		numLines = func(CUI_TEXTFUNC_SIZE, NULL, NULL);
    }

	// if our width is 1 (the default), use parent's width

	if(width == 1)
		width = parent->wWidth();

	// sanity-check border type (we only allow HLINE or none)

	if(flags & CUI_BORDER)
	{
		flags |= CUI_HLINE;
		flags &= ~CUI_BORDER;
	}

	// figure out the number of visible lines (depends on border type)

	if(flags & CUI_HLINE)
		visibleLines = height - 2;
	else
		visibleLines = height;

	// tell ControlArea to complete the job

	return(ControlArea::realize());
}


//
//	resource routines
//

int TextPanel::setValue(CUI_Resource *resource)
{
	int value = (int)resource->value;
	char *strValue = CUI_lookupString(value);
    switch(resource->id)
	{
		case vScrollId:
		{
			setFlagValue(value, CUI_VSCROLL);
			break;
		}
		case fileId:
		{
			// save the file name, and add the builtin textfunc callback

			MEMHINT();
            file = CUI_newString(strValue);
			addCallback("textfunc", CUI_TEXTFUNC_CALLBACK);

			// we don't want to scroll...

			flags &= ~CUI_VSCROLL;
            return(0);
        }
		case textFuncId:
		{
			addCallback(strValue, CUI_TEXTFUNC_CALLBACK);
			return(0);
		}
        default:
			return(ControlArea::setValue(resource));
	}
	return(0);
}

int TextPanel::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(ControlArea::getValue(resource));
	}
}


//
//	show window
//

int TextPanel::show(void)
{
	ControlArea::show();
	refreshContents();
    return(0);
}


//
//	print text at current cursor position
//	('delay' printing newlines so that we can fill last line
//	without scrolling)
//

void TextPanel::print(char *text)
{
	// if we're in scrolling mode, and last line had a trailing newline,
	// print it now

	if((flags & CUI_VSCROLL) && hadNewline)
		window->print("\n");

	// zap trailing newline (remember if we had one)

	int len = strlen(text);
	if(text[len - 1] == '\n')
	{
		hadNewline = TRUE;
		text[len - 1] = 0;
	}
	else
		hadNewline = FALSE;

	// print the text

	window->print(text);
}


//
//	print line of text to window at specified row
//	(if text is not specified, read it)
//

int TextPanel::printLine(int win_row, char *text)
{
    // read the text if not passed to us

	if(!text)
	{
		char buffer[256];
		char *txtptr;
		if(!func)
			return(-1);
		if(func(CUI_TEXTFUNC_READ, (void *)&txtptr, NULL) != 0)
			return(-1);

		// in this mode, we always strip newline

		int len = strlen(txtptr);
		if(txtptr[len - 1] == '\n')
		{
			strcpy(buffer, txtptr);
			buffer[len - 1] = 0;
			txtptr = buffer;
		}
		text = txtptr;
	}

	// clear the row and print the text

	clearRow(win_row);
	print(text);
	return(0);
}


//
//	draw scroll bar if we need it
//

void TextPanel::drawScrollBar(void)
{
	if(numLines > visibleLines)
	{
		int borderWidth = 0;

		if(flags & CUI_BORDER)
			borderWidth = 2;
		else if(flags & CUI_HLINE)
			borderWidth = 1;
		window->drawScrollBar(firstLine, numLines, borderWidth);
	}
	setcur(0, 0);
}


//
//	handle keystroke
//

int TextPanel::doKey(int key, Widget *)
{
	if(!func)
		return(-1);

    switch(key)
	{
		case KEY_UP:
		{
			if(line > firstLine)
			{
				line--;
			}
			else if(firstLine)
			{
				firstLine--;
				line--;
				seek(firstLine);
				scroll(-1);
				printLine(0);
				drawScrollBar();
            }
			syncCursor();
            return(0);
		}
		case KEY_DOWN:
		{
			if(line < (firstLine + (visibleLines - 1)) &&
			   line < numLines - 1)
			{
				line++;
			}
			else
			{
				int last = numLines - visibleLines;
				if(numLines > 0 && firstLine < last)
				{
					seek(firstLine + visibleLines);
                    firstLine++;
					line++;
					scroll(1);
					printLine(visibleLines - 1);
					drawScrollBar();
				}
			}
			syncCursor();
            return(0);
		}
		case KEY_LEFT:
		{
			if(column > 0)
				column--;
			else
				column = width - 3;
			syncCursor();
			return(0);
        }
		case KEY_RIGHT:
		{
			if(column < (width - 2))
				column++;
			else
				column = 0;
			syncCursor();
            return(0);
        }
        case KEY_PGUP:
		{
			if(firstLine)
			{
				firstLine -= visibleLines;
				if(firstLine < 0)
					firstLine = 0;
				line -= visibleLines;
				if(line < 0)
					line = 0;
            }
			else
				return(0);
            break;
		}
        case KEY_PGDN:
		{
			int last;

			// CONSTRAINED flag tells us whether we can allow
			// trailing 'empty' lines at end of display, or
			// whether we should constrain so there are none

			if(flags & CUI_CONSTRAINED)
				last = numLines - visibleLines;
			else
				last = numLines - 1;
			if(numLines > 0 && firstLine < last)
			{
				firstLine += visibleLines;
				if(firstLine > last)
					firstLine = last;
				line += visibleLines;
				if(line >= numLines)
					line = numLines - 1;
            }
			else
				return(0);
			break;
		}
		case KEY_HOME:
		{
			column = 0;
			line   = 0;
            if(firstLine)
				firstLine = 0;
            else
			{
				syncCursor();
                return(0);
			}
			break;
		}
		case KEY_END:
		{
			column = 0;
			line = numLines - 1;
			int last = numLines - visibleLines;
			if(numLines > 0 && firstLine < last)
			{
				firstLine = last;
			}
			else
			{
				syncCursor();
                return(0);
			}
            break;
		}
		default:
		{
			return(0);
		}
	}
	seek(firstLine);
	refreshContents();
    return(0);
}


//
//	seek to specified line
//

void TextPanel::seek(int newLine)
{
	// don't seek to before start of file

	if(newLine < 0)
		newLine = 0;

	// nor beyond end of file (if we know where it is)

    if(numLines >= 0)
	{
		if(newLine >= numLines)
			newLine = numLines - 1;
	}

	// do it (special-case HOME)

	if(newLine == 0)
	{
		func(CUI_TEXTFUNC_HOME, NULL, NULL);
		index = firstLine = 0;
	}
	while(index < newLine)
	{
		if(func(CUI_TEXTFUNC_NEXT, NULL, NULL) == 0)
			index++;
		else
			break;
	}
	while(index > newLine)
	{
		if(func(CUI_TEXTFUNC_PREV, NULL, NULL) == 0)
			index--;
		else
			break;
    }
}


//
//	refresh window contents (assumes we're positioned for first line)
//

void TextPanel::refreshContents(void)
{
	if(func && window)
	{
		clear();
		int max = visibleLines - 1;
		for(int i = 0; i <= max; i++)
		{
			printLine(i);
			if(i != max)
			{
				if(func(CUI_TEXTFUNC_NEXT, NULL, NULL) != 0)
					break;
				index++;
			}
        }
		drawScrollBar();
		syncCursor();
    }
}


//=============================================================================
//	builtin textFunc routine that reads its data from a file which is
//	passed as arg1 on CUI_TEXTFUNC_INIT operation
//	(this is simple and stupid; it reads the entire file into an array)
//=============================================================================

int CUI_textFunc(CUI_TextFuncOp op, void *arg1, void *)
{
    static char **array = NULL;
	static int index;
	static int lines;

	switch(op)
	{
		case CUI_TEXTFUNC_INIT:
		{
			// cast args (1st is file name)

			char *file = (char *)arg1;

			// load the file

            array = CUI_fileToArray(file);
			if(array)
			{
				index = 0;
				lines =  CUI_arrayCount(array);
                return(0);
			}
			else
				return(-1);
        }
		case CUI_TEXTFUNC_SIZE:
		{
			if(array)
				return(lines);
			else
				return(-1);
        }
        case CUI_TEXTFUNC_HOME:
		{
			if(array)
			{
				index = 0;
				return(0);
			}
			else
				return(-1);
        }
		case CUI_TEXTFUNC_NEXT:
		{
			if(array && index < lines - 1)
			{
                index++;
				return(0);
			}
            else
				return(-1);
		}
		case CUI_TEXTFUNC_PREV:
		{
			if(array && index > 0)
			{
				index--;
				return(0);
			}
            else
				return(-1);
        }
		case CUI_TEXTFUNC_READ:
		{
			if(array)
			{
				// cast arg (1st is pointer to char *)

				char **ptr = (char **)arg1;

				// set caller's pointer to the current line

				*ptr = array[index];
				return(0);
			}
			else
				return(-1);
        }
        case CUI_TEXTFUNC_EXIT:
        {
			if(array)
			{
				CUI_deleteArray(array);
				array = NULL;
				return(0);
			}
			else
				return(-1);
        }
	}
	return(-1);
}

