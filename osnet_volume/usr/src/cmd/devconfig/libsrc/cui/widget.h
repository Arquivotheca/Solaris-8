#pragma ident "@(#)widget.h   1.6     93/01/08 SMI"

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
//	Widget class definition (base for all others)
//
//	$RCSfile: widget.h $ $Revision: 1.3 $ $Date: 1992/12/30 21:12:47 $
//=============================================================================

#ifndef _CUI_WIDGET_H
#define _CUI_WIDGET_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_H
#include "cui.h"
#endif
#ifndef _CUI_MESSAGE_H
#include "message.h"
#endif
#ifndef _CUI_SYSCURSES_H
#include "syscurses.h"
#endif

#endif // PRE_COMPILED_HEADERS


// forward-declare Window and ControlArea classes

class Window;
class ControlArea;


//
//	Callback-entry structure
//

struct cui_CallbackEntry
{
	CUI_CallbackProc		 callback;
	CUI_CallbackType		 type;
	void					 *clientData;
	struct cui_CallbackEntry *next;
};
typedef struct cui_CallbackEntry CUI_CallbackEntry;


//
//	the Widget class
//

class Widget
{
protected:
	
	static short		dummyColor; 	// for virtual references
	static char 		*typeNames[];
    CUI_WidgetId        ID;
	Widget				*parent;
	short				row;
	short				col;
	short				height;
	short				width;
	char				*name;
	char				*help;
	char				*footerMsg;
	void				*ptr;			// user-pointer
	long				flags;
	CUI_CallbackEntry	*callbacks;
	
	int 		loadResources(CUI_StringId[]);
    virtual int setValue(CUI_Resource *resource);
	virtual int getValue(CUI_Resource *resource);
    
	void		makeAbsolute(short &Row, short &Col);
	int 		constructClassName(char *buffer);
	int 		constructInstanceName(char *buffer);
	void		badMessage(CUI_MessageId message);
	void		badCommand(char *command);
	CUI_CallbackProc lookupCallback(CUI_CallbackType type);
    bool        hasCallback(CUI_CallbackType type);
	void		showHelp(char *text, char *title);

public:
	
	void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
	void operator delete(void *ptr) 	{ CUI_free(ptr);				}
	
	Widget(char *Name, Widget * = 0, CUI_Resource * = 0,
		   CUI_WidgetId id = CUI_WIDGET_ID);
    virtual int realize(void)           { return(0);                    }
	virtual ~Widget(void);
	
	// access methods
	
	short &wRow(void)					{ return(row);					}
	short &wCol(void)					{ return(col);					}
	short &wHeight(void)				{ return(height);				}
	short &wWidth(void) 				{ return(width);				}
	virtual Window *getWindow(void) 	{ return(0); 					}
	virtual ControlArea *getControlArea(void) { return(0);				}
	virtual char *typeName(void)	 { return typeNames[ID - CUI_BUMP]; }
	CUI_WidgetId getId(void)			{ return(ID);					}
	long   &flagValues(void)			{ return(flags);				}
	int    setName(char *Name);
	char   *getName(void)				{ return(name); 				}
	int    setHelp(char *Help);
	char   *&footerMessage(void)		{ return(footerMsg);			}
	char   *getHelp(void)				{ return(help); 				}
    int    setParent(Widget *Parent);
	Widget *getParent(void) 			{ return(parent);				}
	virtual FORM  *ETIform(void)		{ return(NULL); 				}
	virtual FIELD *ETIfield(void)		{ return(NULL); 				}
	virtual short &borderAttrib(void)	{ return(dummyColor);			}
	virtual short &titleAttrib(void)	{ return(dummyColor);			}
	virtual short &interiorAttrib(void) { return(dummyColor);			}
	virtual short &normalAttrib(void)	{ return(dummyColor);			}
	virtual short &activeAttrib(void)	{ return(dummyColor);			}
	virtual short &disabledAttrib(void) { return(dummyColor);			}
	virtual short &setAttrib(void)		{ return(dummyColor);			}

	// add Child, Callback
	
	virtual int addChild(Widget *);
    virtual int addCallback(CUI_CallbackProc, CUI_CallbackType, void * = 0);
			int addCallback(char *name, CUI_CallbackType type);

	// verification routines
	
	static Widget *verify(Widget *object);
	static Widget *verifyIsA(Widget *object, CUI_WidgetId type);
	static Widget *verifyIsKindOf(Widget *object, CUI_WidgetId type);
	virtual int isKindOf(CUI_WidgetId)	{ return FALSE; 			   }
	
	// utility routines
	
	static int			compare(Widget *one, Widget *two);
	static Widget		*lookup(char *name);
	static char 		*typeName(CUI_WidgetId id);
	static CUI_WidgetId lookupName(char *name);
	int 				setValues(CUI_Resource*);
	int 				getValues(CUI_Resource*);
	void				setFlagValue(int resource, long value);
	int 				setColorValue(char *colorName, short &valueToSet);
    virtual int         manageGeometry(void)        { return(0);        }
	virtual int 		labelLength(void)			{ return(0);		}
	virtual int 		alignLabel(short)			{ return(0);		}
	virtual void		adjustLocation(short &Row, short &col);
	int doCallback(CUI_CallbackType type, void *callData = 0);
	virtual void locateCursor(void) { /* nothing */ 	}
	virtual bool isCurrent(void)	{ return(FALSE);	}
	virtual int  setDefault(void)	{ return(0);		}
	virtual void setOn(bool)		{ /* nothing */ 	}

	// message-handlers
	
			int messageHandler(CUI_Message *message);
			int doHelp(void);
	virtual int  doKey(int = 0, Widget * = 0)			 { return(-1);	}
	virtual bool filterKey(CUI_KeyFilter, int)		   { return(FALSE); }
	virtual int  show(void) 		 { return(0);						}
	virtual int  hide(void) 		 { return(0);						}
	virtual int  select(void)		 { return(0);						}
	virtual int  unselect(void) 	 { return(0);						}
	virtual int  focus(bool = TRUE)	 { return(0);						}
	virtual int  unfocus(void)		 { return(0);						}
	virtual int  cancel(void)		 { return(0);						}
	virtual int  done(void) 		 { return(0);						}
	virtual int  locate(short Row, short Col);
	virtual int  resize(short Height, short Width);
	virtual int  refresh(void)		 { return(0);						}
	virtual int  interpret(char *c)  { badCommand(c); return(-1);		}
};

#endif /* _CUI_WIDGET_H */
