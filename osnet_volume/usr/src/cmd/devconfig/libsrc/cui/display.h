#pragma ident "@(#)display.h   1.3     92/11/25 SMI"

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
//	Display class definition
//
//	$RCSfile: display.h $ $Revision: 1.9 $ $Date: 1992/09/13 03:09:31 $
//=============================================================================

#ifndef _CUI_DISPLAY_H
#define _CUI_DISPLAY_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_WIDGET_H
#include "widget.h"
#endif
#ifndef _CUI_SYSCURSES_H
#include "syscurses.h"
#endif
#ifndef _CUI_COLOR_H
#include "color.h"
#endif

#endif // PRE_COMPILED_HEADERS


// forward-declare Window class

class Window;


class Display
{
	protected:

		static	char *dumpFile;

		Widget	*parent;		// parent (the Application)
		int 	infoRow;
		bool	updating;
		int 	haveMessage;
        Window  *screenWin;
		Window	*messageWin;

    public:

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		Display(Widget *Parent);
		~Display(void);

		// access methods

		Window	*screen(void)				{ return(screenWin);			}
		Window	*message(void)				{ return(messageWin);			}
		short	rows(void)					{ return((short)LINES); 		}
		short	cols(void)					{ return((short)COLS);			}
		int 	messageDisplayed(void)		{ return(haveMessage);			}

		// other methods

		bool	update(bool mode);
		bool	updateMode(void)	{ return(updating); }
		void	refresh(bool redraw = FALSE);
		void	setcur(short row, short col);
		void	getcur(short &row, short &col);
		void	infoMessage(char *format,...);
		void	clearMessage(void);
		bool	hasColor(void)				{ return(Color::colorMode());	}
		int 	save(char *file = NULL);
		int 	restore(char *file = NULL);
	};


// global pointer to the display

extern Display *CUI_display;

#endif // _CUI_DISPLAY_H

