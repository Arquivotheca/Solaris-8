#pragma ident "@(#)color.cc   1.5     99/02/19 SMI"

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

//============================================================================
//	 color routines
//
//	 $RCSfile: color.cc $ $Revision: 1.15 $ $Date: 1992/09/12 15:21:34 $
//============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_SYSCURSES_H
#include "curses.h"
#endif
#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// static to specify whether display supports color

bool Color::haveColor = FALSE;


//
//	in order to keep widgets as small as possible, we don't store pointers
//	to Color objects in them; we store only an index into this array
//

Color **Color::colors	 = NULL;
int 	Color::index	 = 0;
int 	Color::arraySize = 0;

#define BUMP 16  // grow array by this number of elements


//
//	color pairs (arranged so that color-pair number = (FORE * 8) + BACK)
//

#define  BLACK_BLUE 			1
#define  BLACK_GREEN			2
#define  BLACK_CYAN 			3
#define  BLACK_RED				4
#define  BLACK_MAGENTA			5
#define  BLACK_YELLOW			6
#define  BLACK_WHITE			7

#define  BLUE_BLACK 			8
#define  BLUE_GREEN 			10
#define  BLUE_CYAN				11
#define  BLUE_RED				12
#define  BLUE_MAGENTA			13
#define  BLUE_YELLOW			14
#define  BLUE_WHITE 			15

#define  GREEN_BLACK			16
#define  GREEN_BLUE 			17
#define  GREEN_CYAN 			19
#define  GREEN_RED				20
#define  GREEN_MAGENTA			21
#define  GREEN_YELLOW			22
#define  GREEN_WHITE			23

#define  CYAN_BLACK 			24
#define  CYAN_BLUE				25
#define  CYAN_GREEN 			26
#define  CYAN_RED				28
#define  CYAN_MAGENTA			29
#define  CYAN_YELLOW			30
#define  CYAN_WHITE 			31

#define  RED_BLACK				32
#define  RED_BLUE				33
#define  RED_GREEN				34
#define  RED_CYAN				35
#define  RED_MAGENTA			37
#define  RED_YELLOW 			38
#define  RED_WHITE				39

#define  MAGENTA_BLACK			40
#define  MAGENTA_BLUE			41
#define  MAGENTA_GREEN			42
#define  MAGENTA_CYAN			43
#define  MAGENTA_RED			44
#define  MAGENTA_YELLOW 		46
#define  MAGENTA_WHITE			47

#define  YELLOW_BLACK			48
#define  YELLOW_BLUE			49
#define  YELLOW_GREEN			50
#define  YELLOW_CYAN			51
#define  YELLOW_RED 			52
#define  YELLOW_MAGENTA 		53
#define  YELLOW_WHITE			55

#define  WHITE_BLACK			56
#define  WHITE_BLUE 			57
#define  WHITE_GREEN			58
#define  WHITE_CYAN 			59
#define  WHITE_RED				60
#define  WHITE_MAGENTA			61
#define  WHITE_YELLOW			62


// resources to load

static CUI_StringId resList[] =
{
	colorForegroundId,
	colorBackgroundId,
	colorBlinkId,
	colorBoldId,
    monoAttribId,
	monoBlinkId,
	monoBoldId,
    nullStringId
};


//
//	static initialization routine
//

void Color::initialize(void)
{
	// figure out whether display supports color, and if so init color-pairs

	if(has_colors())
    {
		haveColor = TRUE;
		Color::initColorPairs();
    }
	else
	{
		haveColor = FALSE;
	}

	// initialize our colors array

	arraySize = BUMP;
	index  = 0;
	MEMHINT();
	colors = (Color **)CUI_malloc(arraySize * sizeof(Color *));

	// create some default Colors...

	// CUI_NORMAL_COLOR:

	CUI_Widget CUI_normalColor =
		CUI_vaCreateWidget("CUI_normalColor",  CUI_COLOR_ID, NULL,
						   colorForegroundId,  CUI_RESOURCE(whiteId),
						   colorBackgroundId,  CUI_RESOURCE(blackId),
						   monoAttribId,	   CUI_RESOURCE(normalId),
						   nullStringId);

	// CUI_REVERSE_COLOR:

	CUI_Widget CUI_reverseColor =
		CUI_vaCreateWidget("CUI_reverseColor", CUI_COLOR_ID, NULL,
						   colorForegroundId,  CUI_RESOURCE(blackId),
						   colorBackgroundId,  CUI_RESOURCE(whiteId),
						   monoAttribId,	   CUI_RESOURCE(reverseId),
						   nullStringId);

	// CUI_BOLD_COLOR:

	CUI_Widget CUI_boldColor =
		CUI_vaCreateWidget("CUI_boldColor",    CUI_COLOR_ID, NULL,
						   colorForegroundId,  CUI_RESOURCE(whiteId),
						   colorBackgroundId,  CUI_RESOURCE(blackId),
						   colorBoldId, 	   CUI_RESOURCE(trueId),
						   monoAttribId,	   CUI_RESOURCE(normalId),
						   monoBoldId,		   CUI_RESOURCE(trueId),
						   nullStringId);

	// CUI_UNDERLINE_COLOR:

	CUI_Widget CUI_underlineColor =
		CUI_vaCreateWidget("CUI_underlineColor", CUI_COLOR_ID, NULL,
						   colorForegroundId,	 CUI_RESOURCE(whiteId),
						   colorBackgroundId,	 CUI_RESOURCE(blackId),
						   colorBoldId, 		 CUI_RESOURCE(trueId),
						   monoAttribId,		 CUI_RESOURCE(underlineId),
						   nullStringId);

	// CUI_BLINK_COLOR:

	CUI_Widget CUI_blinkColor =
		CUI_vaCreateWidget("CUI_blinkColor",   CUI_COLOR_ID, NULL,
						   colorForegroundId,  CUI_RESOURCE(whiteId),
						   colorBackgroundId,  CUI_RESOURCE(blackId),
						   colorBlinkId,	   CUI_RESOURCE(trueId),
						   monoAttribId,	   CUI_RESOURCE(normalId),
						   monoBlinkId, 	   CUI_RESOURCE(trueId),
						   nullStringId);

	// CUI_MONO_COLOR:

	CUI_Widget CUI_monoColor =
		CUI_vaCreateWidget("CUI_monoColor",   CUI_COLOR_ID, NULL,
						   colorForegroundId, CUI_RESOURCE(whiteId),
						   colorBackgroundId, CUI_RESOURCE(blackId),
						   monoAttribId,	  CUI_RESOURCE(normalId),
						   nullStringId);
}


//
//	constructor
//

Color::Color(char *Name, Widget *Parent, CUI_Resource *resources,
				   CUI_WidgetId id)
	: Widget(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	// set default values

	foreground	= COLOR_WHITE;
	background = COLOR_BLACK;
	colorBlink = FALSE;
	colorBold  = FALSE;
	monoAttrib = A_NORMAL;

	// We can't load resources from resource file at this point,
	// since Colors are treated specially, and are created (from
	// resource files) before resources are loaded.  Postpone
	// loading resources till we're first accessed.  This means
	// that resource-file definitions over-ride programmatic
	// assignments, but so be it...

	// we must, however, assign the passed resources

	setValues(resources);

	// save pointer to this in colors array

	addToArray(this);
}


//
//	destructor
//

Color::~Color(void)
{
}


//
//	return attribute value
//

chtype Color::value(void)
{
	// if we haven't yet loaded our resources, do so

	if(!(flags & CUI_INITIALIZED))
	{
		loadResources(resList);
		flags |= CUI_INITIALIZED;
	}

	if(!haveColor)
		return(monoAttrib);
	else
	{
		// calculate color-pair index

		chtype cpindex = (foreground << 3) | background;

		// calculate and return the color attribute

		chtype attrib = COLOR_PAIR(cpindex);

		// OR blink and bold bits in as appropriate

		if(colorBlink)
			attrib |= A_BLINK;
		if(colorBold)
			attrib |= A_BOLD;

		return(attrib);
	}
}


//
//	set resource values
//

int Color::setValue(CUI_Resource *resource)
{
	int value = (int)resource->value;
    switch(resource->id)
	{
		case colorForegroundId:
		case colorBackgroundId:
		{
			// color resource IDs are defined as curses color values + 1

			if(value <= 8)
			{
                if(resource->id == colorForegroundId)
					foreground = (short)(value - 1);
                else
					background = (short)(value - 1);
            }
			else
			{
				CUI_fatal(dgettext(CUI_MESSAGES, 
					"Bad value for fore/background resource in %s '%s'"),
						  typeName(), name);
			}
            break;
        }
        case monoAttribId:
		{
			// save current state of blink and bold bits

			bool haveBlink = isBlink(monoAttrib);
			bool haveBold  = isBold(monoAttrib);

			switch(value)
			{
				case underlineId:
					monoAttrib = A_UNDERLINE;
					break;
				case normalId:
					monoAttrib = A_NORMAL;
					break;
                case reverseId:
					monoAttrib = A_REVERSE;
					break;
                default:
				{
					CUI_fatal(dgettext(CUI_MESSAGES, 
						"Bad value for monoAttrib resource in %s '%s'"),
							  typeName(), name);
				}
            }

			// OR the blink and bold bits back in as appropriate

			if(haveBlink)
				monoAttrib |= A_BLINK;
			if(haveBold)
				monoAttrib |= A_BOLD;

            break;
		}
		case monoBlinkId:
		{
			if(value == trueId)
				monoAttrib |= A_BLINK;
			else
				monoAttrib &= ~A_BLINK;
			break;
		}
		case monoBoldId:
		{
			if(value == trueId)
				monoAttrib |= A_BOLD;
			else
				monoAttrib &= ~A_BOLD;
			break;
		}
		case colorBlinkId:
        {
			colorBlink = (value == trueId);
			break;
		}
        case colorBoldId:
		{
			colorBold = (value == trueId);
			break;
		}
        default:
			return(-1);
	}
	return(0);
}


//
//	lookup a Color by index (return pointer to Color object)
//

Color *Color::lookup(int i)
{
	if(i < 0 || i >= index)
		return(NULL);
	else
		return(colors[i]);
}


//
//	lookup a Color by index (return chtype attrib value)
//

chtype Color::lookupValue(int i)
{
	Color *color = Color::lookup(i);
	if(!color)
		return(A_NORMAL);
	else
	{
		chtype attrib = color->value();
		return(attrib);
	}
}


//
//	lookup a Color by name (return index)
//

short Color::lookup(char *name)
{
	Widget *color = Widget::lookup(name);
	if(color)
	{
		for(short i = 0; i < index; i++)
		{
			if(colors[i] == color)
				return(i);
		}
	}
	return((short)-1);
}


//
//	invert an attribute
//

#define ATTRIB_MASK A_ATTRIBUTES & ~A_ALTCHARSET

chtype Color::invert(chtype attrib)
{
	attrib = attrib & ATTRIB_MASK;

	// save current state of blink and bold bits

	bool haveBlink = isBlink(attrib);
	bool haveBold  = isBold(attrib);

    if(attrib & A_COLOR)
    {
		// extract colors

		short fore, back;
		int pair_number = (int)PAIR_NUMBER(attrib);
		pair_content(pair_number, &fore, &back);

		// calculate new color-index by reversing the normal operation

		int cpindex = (back << 3) | fore;
		attrib = COLOR_PAIR(cpindex);
    }
	else
	{
		// in mono mode, make normal if reverse/standout, else make reverse

		if( (attrib & A_REVERSE) || (attrib & A_STANDOUT) )
			attrib = A_NORMAL;
		else
			attrib = A_REVERSE;
    }

	// OR the blink and bold bits back in as appropriate

	if(haveBlink)
		attrib |= A_BLINK;
	if(haveBold)
		attrib |= A_BOLD;

	return(attrib);
}


//============================================================================
//	private routines
//============================================================================


//
//	define all possible color-pairs
//

void Color::initColorPairs(void)
{
	init_pair(BLACK_BLUE,		COLOR_BLACK,	COLOR_BLUE);
	init_pair(BLACK_GREEN,		COLOR_BLACK,	COLOR_GREEN);
	init_pair(BLACK_CYAN,		COLOR_BLACK,	COLOR_CYAN);
	init_pair(BLACK_RED,		COLOR_BLACK,	COLOR_RED);
	init_pair(BLACK_MAGENTA,	COLOR_BLACK,	COLOR_MAGENTA);
	init_pair(BLACK_YELLOW, 	COLOR_BLACK,	COLOR_YELLOW);
	init_pair(BLACK_WHITE,		COLOR_BLACK,	COLOR_WHITE);

	init_pair(BLUE_BLACK,		COLOR_BLUE, 	COLOR_BLACK);
	init_pair(BLUE_GREEN,		COLOR_BLUE, 	COLOR_GREEN);
	init_pair(BLUE_CYAN,		COLOR_BLUE, 	COLOR_CYAN);
	init_pair(BLUE_RED, 		COLOR_BLUE, 	COLOR_RED);
	init_pair(BLUE_MAGENTA, 	COLOR_BLUE, 	COLOR_MAGENTA);
	init_pair(BLUE_YELLOW,		COLOR_BLUE, 	COLOR_YELLOW);
	init_pair(BLUE_WHITE,		COLOR_BLUE, 	COLOR_WHITE);

	init_pair(GREEN_BLACK,		COLOR_GREEN,	COLOR_BLACK);
	init_pair(GREEN_BLUE,		COLOR_GREEN,	COLOR_BLUE);
	init_pair(GREEN_CYAN,		COLOR_GREEN,	COLOR_CYAN);
	init_pair(GREEN_RED,		COLOR_GREEN,	COLOR_RED);
	init_pair(GREEN_MAGENTA,	COLOR_GREEN,	COLOR_MAGENTA);
	init_pair(GREEN_YELLOW, 	COLOR_GREEN,	COLOR_YELLOW);
	init_pair(GREEN_WHITE,		COLOR_GREEN,	COLOR_WHITE);

	init_pair(CYAN_BLACK,		COLOR_CYAN, 	COLOR_BLACK);
	init_pair(CYAN_BLUE,		COLOR_CYAN, 	COLOR_BLUE);
	init_pair(CYAN_GREEN,		COLOR_CYAN, 	COLOR_GREEN);
	init_pair(CYAN_RED, 		COLOR_CYAN, 	COLOR_RED);
	init_pair(CYAN_MAGENTA, 	COLOR_CYAN, 	COLOR_MAGENTA);
	init_pair(CYAN_YELLOW,		COLOR_CYAN, 	COLOR_YELLOW);
	init_pair(CYAN_WHITE,		COLOR_CYAN, 	COLOR_WHITE);

	init_pair(RED_BLACK,		COLOR_RED,		COLOR_BLACK);
	init_pair(RED_BLUE, 		COLOR_RED,		COLOR_BLUE);
	init_pair(RED_GREEN,		COLOR_RED,		COLOR_GREEN);
	init_pair(RED_CYAN, 		COLOR_RED,		COLOR_CYAN);
	init_pair(RED_MAGENTA,		COLOR_RED,		COLOR_MAGENTA);
	init_pair(RED_YELLOW,		COLOR_RED,		COLOR_YELLOW);
	init_pair(RED_WHITE,		COLOR_RED,		COLOR_WHITE);

	init_pair(MAGENTA_BLACK,	COLOR_MAGENTA,	COLOR_BLACK);
	init_pair(MAGENTA_BLUE, 	COLOR_MAGENTA,	COLOR_BLUE);
	init_pair(MAGENTA_GREEN,	COLOR_MAGENTA,	COLOR_GREEN);
	init_pair(MAGENTA_CYAN, 	COLOR_MAGENTA,	COLOR_CYAN);
	init_pair(MAGENTA_RED,		COLOR_MAGENTA,	COLOR_RED);
	init_pair(MAGENTA_YELLOW,	COLOR_MAGENTA,	COLOR_YELLOW);
	init_pair(MAGENTA_WHITE,	COLOR_MAGENTA,	COLOR_WHITE);

	init_pair(YELLOW_BLACK, 	COLOR_YELLOW,	COLOR_BLACK);
	init_pair(YELLOW_BLUE,		COLOR_YELLOW,	COLOR_BLUE);
	init_pair(YELLOW_GREEN, 	COLOR_YELLOW,	COLOR_GREEN);
	init_pair(YELLOW_CYAN,		COLOR_YELLOW,	COLOR_CYAN);
	init_pair(YELLOW_RED,		COLOR_YELLOW,	COLOR_RED);
	init_pair(YELLOW_MAGENTA,	COLOR_YELLOW,	COLOR_MAGENTA);
	init_pair(YELLOW_WHITE, 	COLOR_YELLOW,	COLOR_WHITE);

	init_pair(WHITE_BLACK,		COLOR_WHITE,	COLOR_BLACK);
	init_pair(WHITE_BLUE,		COLOR_WHITE,	COLOR_BLUE);
	init_pair(WHITE_GREEN,		COLOR_WHITE,	COLOR_GREEN);
	init_pair(WHITE_CYAN,		COLOR_WHITE,	COLOR_CYAN);
	init_pair(WHITE_RED,		COLOR_WHITE,	COLOR_RED);
	init_pair(WHITE_MAGENTA,	COLOR_WHITE,	COLOR_MAGENTA);
	init_pair(WHITE_YELLOW, 	COLOR_WHITE,	COLOR_YELLOW);
}


//
//	add newly-created Color object to our array
//

void Color::addToArray(Color *color)
{
	if(index == arraySize - 1)	// keep final NULL!
	{
		colors = (Color **)CUI_arrayGrow((char **)colors, BUMP);
		arraySize += BUMP;
	}
	colors[index++] = color;
}

