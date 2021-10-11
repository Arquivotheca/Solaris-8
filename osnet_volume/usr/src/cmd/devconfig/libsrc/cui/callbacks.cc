#pragma ident "@(#)callbacks.cc   1.6     93/07/22 SMI"

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
//	some standard callbacks (for testing)
//
//	$RCSfile: callbacks.cc $ $Revision: 1.20 $ $Date: 1992/09/23 00:51:18 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_POPWIN_H
#include "popwin.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef _CUI_FOOTER_H
#include "footer.h"
#endif
#ifndef _CUI_SPOTHELP_H
#include "spothelp.h"
#endif
#ifndef  _CUI_PROTO_H
#include "cuiproto.h"
#endif

#endif	// PRE_COMPILED_HEADERS


//
//	display callback info
//

static int reportCallback(CUI_Widget handle, char *type)
{
	Widget *widget = (Widget *)handle;
	CUI_infoMessage("%s callback for %s '%s'", type,
							 widget->typeName(), widget->getName());
	return(0);
}


//
//	miscellaneous callbacks that simply report they've been invoked
//

int CUI_selectCallback(CUI_Widget handle, void *, void *)
{
	return(reportCallback(handle, "Select"));
}

int CUI_unselectCallback(CUI_Widget handle, void *, void *)
{
	return(reportCallback(handle, "Unselect"));
}

int CUI_focusCallback(CUI_Widget handle, void *, void *)
{
	return(reportCallback(handle, "Focus"));
}

int CUI_unfocusCallback(CUI_Widget handle, void *, void *)
{
	return(reportCallback(handle, "Unfocus"));
}


//
//	verification callback that reports the value to be verified
//

int CUI_verifyCallback(CUI_Widget handle, void *, void *calldata)
{
	CUI_TextFieldVerify *verify = (CUI_TextFieldVerify *)calldata;
	Widget *widget = (Widget *)handle;

	CUI_infoMessage("Verify %s '%s' (data: '%s')",
		widget->typeName(), widget->getName(), verify->string);
	return(0);
}


//
//	callback that exits the application
//

int CUI_exitCallback(CUI_Widget, void *, void *)
{
	CUI_postAppExitMessage();
	return(0);
}


//
//	popup/down callbacks
//

int CUI_popupCallback(CUI_Widget, void *clientdata, void *)
{
	return(CUI_popup((CUI_Widget)clientdata, CUI_GRAB_EXCLUSIVE));
}

int CUI_popdownCallback(CUI_Widget, void *clientdata, void *)
{
	return(CUI_popdown((CUI_Widget)clientdata));
}


//
//  callback to display footer message
//

int CUI_footerMsgCallback(CUI_Widget handle, void *, void *)
{
    Widget *widget = (Widget *)handle;

	// do nothing if we don't have SpotHelp

    extern SpotHelp *CUI_spotHelp; 
	if(!CUI_spotHelp)
		return(0);

	// do we have a footer message key? 
	// (we search the entire hierarchy and use the first we find)

	char *msgKey = NULL;
	while(widget)
	{
    	msgKey = widget->footerMessage();
    	if(msgKey)
			break;
		widget = widget->getParent();
	}

	if(!msgKey)
		return(0);
 
	// lookup the text

	char **messageArray = CUI_spotHelp->lookup(msgKey);
	if(!messageArray)
		return(0);

	// find ancestor window and get pointer to Footer's StaticText

	Widget *ancestor = widget->getParent();
	while(ancestor && ancestor->getParent())
		ancestor = ancestor->getParent();
	if(!ancestor || !ancestor->isKindOf(CUI_SHELL_ID))
		return(0);
	StaticText *text = ((Shell *)ancestor)->getFooterText();

	// if we have a StaticText, set its string resource to message

	if(text)
	{	
		// set StaticText's string resource
		
		CUI_vaSetValues(text,
						stringId, CUI_STR_RESOURCE(messageArray[0]),
						nullStringId);
 	}

	// free the looked-up message

	CUI_deleteArray(messageArray);
    return(0);
}
 

//
//	define these callbacks
//

extern int CUI_textFunc(CUI_TextFuncOp, void *, void *);
extern int CUI_hypertextFunc(CUI_TextFuncOp, void *, void *);

static CUI_CallbackDef callbacks[] =
{
    { CUI_selectCallback,             "selectCallback"    },
	{ CUI_unselectCallback, 		  "unselectCallback"  },
	{ CUI_focusCallback,			  "focusCallback"     },
	{ CUI_unfocusCallback,			  "unfocusCallback"   },
    { CUI_exitCallback,               "exitCallback"      },
	{ CUI_verifyCallback,			  "verifyCallback"    },
	{ CUI_popupCallback,			  "popupCallback"     },
	{ CUI_popdownCallback,			  "popdownCallback"   },
	{ CUI_helpIndexCallback,		  "indexCallback"     },
	{ CUI_footerMsgCallback,	      "footerMsgCallback" },

	// these signatures don't match CUI_CallbackProc, so we cast them...

	{ (CUI_CallbackProc)CUI_textFunc,	   "textfunc",         },
	{ (CUI_CallbackProc)CUI_hypertextFunc, "hypertextfunc",    },
    { NULL }
};

int CUI_initCallbacks(void)
{
	return(CUI_defineCallbacks(callbacks));
}


//
//	key-filter routine (for testing)
//

bool CUI_testKeyFilter(CUI_Widget handle, int key)
{
	Widget *widget = (Widget *)handle;
	CUI_infoMessage("%s '%s' received key %d",
		widget->typeName(), widget->getName(), key);
	return(FALSE);
}


//=============================================================================
//	utility routines
//=============================================================================


// callback ID for symtab lookup

#define CUI_CALLBACK_ID 1


//
//	add a callback to symbol table so it can be looked-up by name
//

static int CUI_defineCallback(CUI_CallbackProc callback, char *name)
{
	extern void CUI_createSymtab(void);

    // make sure we have a Symbol table

	if(!CUI_Symtab)
		CUI_createSymtab();

	switch(CUI_Symtab->insert(name, callback, CUI_CALLBACK_ID))
	{
		case 0:
			return(0);
		case E_NOT_UNIQUE:
			CUI_fatal(dgettext(CUI_MESSAGES, 
				"Callback name '%s' not unique"), name);
		default:
			CUI_fatal(dgettext(CUI_MESSAGES,
				"Can't define callback '%s'"), name);
	}
	return(0);	// keep the compiler happy
}

//
//	lookup a callback by name
//

CUI_CallbackProc CUI_lookupCallback(char *name)
{
	return((CUI_CallbackProc)CUI_Symtab->lookup(name, CUI_CALLBACK_ID));
}

//
//	define callbacks from array of CUI_CallbackDefs
//

int CUI_defineCallbacks(CUI_CallbackDef table[])
{
	for(int i = 0; table[i].proc; i++)
	{
		if(CUI_defineCallback(table[i].proc, table[i].name))
			return(-1);
	}
	return(0);
}

