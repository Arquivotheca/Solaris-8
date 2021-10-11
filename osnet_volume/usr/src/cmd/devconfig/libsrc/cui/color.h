#pragma ident "@(#)color.h   1.3     92/11/25 SMI"

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
//	Color class definition
//	(this is derived from Widget only so that we can load resources)
//
//	$RCSfile: color.h $ $Revision: 1.12 $ $Date: 1992/09/12 15:16:31 $
//=============================================================================

#ifndef _CUI_COLOR_H
#define _CUI_COLOR_H

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_WIDGET_H
#include "widget.h"
#endif

#endif	// PRE_COMPILED_HEADERS


class Color : public Widget
{
	private:

		static bool  haveColor;
		static Color **colors;
		static int	 index;
		static int	 arraySize;

		// We must save all the color values, rather than calculating
		// on-the-fly, since if we transition from, eg, WHITE-on-BLACK
		// to BLACK-ON-WHITE, we may go through an illegal color-pair
		// (WHITE-ON-WHITE or BLACK-ON-BLACK) - for some reason curses'
		// color logic treats the first and last pair as errors.
		// On an error condition, we'll lose our colorAttrib value.

		short  foreground;
		short  background;
		bool   colorBlink;
		bool   colorBold;
		chtype monoAttrib;

    protected:

		virtual int  setValue(CUI_Resource *);
		static	void initColorPairs(void);
				void addToArray(Color *color);

		static bool isBold(chtype attrib)
		{
			attrib = attrib & A_BOLD;
			return(attrib != 0);
		}
		static bool isBlink(chtype attrib)
		{
			attrib = attrib & A_BLINK;
			return(attrib != 0);
		}

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Color(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				 CUI_WidgetId = CUI_COLOR_ID);
		virtual ~Color(void);

		// class-specific routines

		static bool   colorMode(void)		{ return(haveColor);			}
        static void   initialize(void);
		static chtype invert(chtype attrib);
        static Color  *lookup(int index);
        static short  lookup(char *name);
		static chtype lookupValue(int index);
			   chtype value(void);
};

#endif // _CUI_COLOR_H


