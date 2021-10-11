#pragma ident "@(#)window.h   1.4     92/11/25 SMI"

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
//	Window class definition
//
//	$RCSfile: window.h $ $Revision: 1.16 $ $Date: 1992/09/12 15:28:39 $
//=============================================================================


#ifndef _CUI_WINDOW_H
#define _CUI_WINDOW_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_WIDGET_H
#include "widget.h"
#endif
#ifndef _CUI_SYSCURSES_H
#include "syscurses.h"
#endif

#endif // PRE_COMPILED_HEADERS


// compensate for missing prototype in Solaris panel.h
 
extern "C" 
{ 
int panel_hidden(PANEL *panel); 
} 
 

class Window : public Widget
{
protected:
	
	WINDOW		*outer;
	WINDOW		*inner;
	PANEL		*panel;
	Window		*parentWindow;
	char		*title;
	short		borderColor;
	short		titleColor;
	short		interiorColor;

	virtual int setValue(CUI_Resource *);
	virtual int getValue(CUI_Resource *);
	
	void		adjust(short &Row, short &Col, short &Height, short &Width);
	void		forceCoords(int &Row, int &Col);
	void		getcurAbs(int &Row, int &Col);
	void		attribPrint(char *string);
	
public:
	
	// memory-management
	
	void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
	void operator delete(void *ptr) 	{ CUI_free(ptr);				}
	
	// constructor & destructor
	
	Window(char *Name, Widget * = NULL, CUI_Resource * = NULL,
		   CUI_WidgetId id = CUI_WINDOW_ID);
    virtual int realize(void);
	virtual ~Window(void);
	
	// access methods
	
	WINDOW *getInner(void)				{ return(inner);				}
	WINDOW *getOuter(void)				{ return(outer);				}
	char   *getTitle(void)				{ return(title);				}
	int 	setTitle(char *Title);
	virtual short  &borderAttrib()		{ return(borderColor);			}
	virtual short  &titleAttrib()		{ return(titleColor);			}
	virtual short  &interiorAttrib()	{ return(interiorColor);		}

	// utility routines
	
	virtual int   isKindOf(CUI_WidgetId type)
		{ return(type == CUI_WINDOW_ID); }
	static Window *current(void);
	int 		isVisible(void) 	{ return(panel && !panel_hidden(panel)); }
	int 		isTop(void) 		{ return(panel == panel_below(NULL));	 }
	bool		isObscured(void)	{ return(panel && panel->obscured); 	 }

	// class-specific methods

	void   scrolling(bool mode);
	int    drawBorder(void);
	int    drawTitle(bool focus = TRUE);
	int    drawHline(int row);
	int    drawBox(int r, int c, int h, int w, chtype attrib);
	int    drawScrollBar(int current, int count, int borderWidth = 2);
	
	void   setcur(int r, int c);
	void   setTextAttrib(short attrib);
	void   getcur(int &r, int &c);
	void   clear(void);
	void   clearRow(int row);
	int    printf(char *format,...);
	int    print(char *string);
	int    scroll(int count);
	void   putc(int ch);
	
	// message-handlers
	
	virtual int doKey(int = 0, Widget * = NULL) { return(0);			}
	virtual int show(void);
	virtual int hide(void);
	virtual int select(void);
	virtual int unselect(void);
	virtual int locate(short Row, short Col);
	virtual int refresh(void);
};


// compensate for missing prototype in Solaris panel.h

extern "C"
{
int panel_hidden(PANEL *panel);
}

#endif /* _CUI_WINDOW_H */


