#pragma ident "@(#)cui.h   1.9     93/07/23 SMI"

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

/*
 * =========================================================================
 * 
 * $RCSfile: cui.h $ $Revision: 1.2 $ $Date: 1992/12/29 22:18:33 $
 * 
 * master include file for CUI Toolkit API
 * 
 * ===========================================================================
 */

#ifndef _CUI_H
#define _CUI_H

/* standard system header files */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include "memcheck.h"

#ifdef putc
#undef putc
#endif

#ifndef MSDOS
#include <unistd.h>
#else
#include <alloc.h>
#endif

#include "stringid.h"		/* string IDs   */
#include "cuitypes.h"		/* typedefs     */


#ifdef __cplusplus
extern          "C"
{
#endif


    /*
	 * ===================================================================
	 * API routines
	 * ===================================================================
     */

    void            CUI_version(int *major, int *minor);
    CUI_Widget      CUI_initialize(char *appName, CUI_Resource * resources,
				                   int *, char ** );
    CUI_Widget      CUI_vaInitialize(char *name, int *argc, char *argv[],...);
    CUI_Widget      CUI_createWidget(char *name, CUI_WidgetId type,
	                       CUI_Widget parent, CUI_Resource * resources);
    int             CUI_deleteWidget(char *name);
    int             CUI_deleteWidgetByHandle(CUI_Widget handle);
    CUI_Widget      CUI_vaCreateWidget(char *name, CUI_WidgetId type,
			                       CUI_Widget parentHandle,...);
    int             CUI_realizeWidget(CUI_Widget widget);
    void            CUI_mainLoop(void);
    int             CUI_setValues(CUI_Widget widget, CUI_Resource * resources);
    int             CUI_vaSetValues(CUI_Widget handle,...);
    int             CUI_getValues(CUI_Widget widget, CUI_Resource * resources);
    void            CUI_vaFreeResources(CUI_Resource * res);
    CUI_Resource *  CUI_vaGetValues(CUI_Widget handle, ...);
    int             CUI_defineCallbacks(CUI_CallbackDef table[]);
    int             CUI_addCallback(CUI_Widget handle, CUI_CallbackType type,
		                     CUI_CallbackProc callback, void *data);
    void            CUI_exit(CUI_Widget widget);
    int             CUI_popup(CUI_Widget widget, CUI_GrabMode mode);
    int             CUI_popdown(CUI_Widget widget);
    int             CUI_compileString(char *string);
    char           *CUI_lookupString(int id);
    int             CUI_textFieldCopyString(CUI_Widget widget, char *string);
    char           *CUI_textFieldGetString(CUI_Widget widget, int *size);
    CUI_Widget      CUI_lookupWidget(char *name);
    char           *CUI_widgetName(CUI_Widget handle);
    CUI_Widget      CUI_parent(CUI_Widget handle);
    int             CUI_addItem(CUI_Widget menuOrList, CUI_Widget menuOrListItem, int index);
    int             CUI_deleteItem(CUI_Widget menuOrList, int index);
    CUI_Widget      CUI_lookupItem(CUI_Widget menuOrList, int index);
    int             CUI_setCurrentItem(CUI_Widget menuOrList, int index);
    int             CUI_getCurrentItem(CUI_Widget menuOrList);

#define CUI_nListItems CUI_countListItems
    int             CUI_countListItems(CUI_Widget menuOrList);
    int             CUI_refreshItems(CUI_Widget menuOrList);
    CUI_KeyFilter   CUI_setKeyFilter(CUI_KeyFilter filter);
    int             CUI_screenRows(void);
    int             CUI_screenCols(void);
    bool            CUI_hasColor(void);
    void            CUI_textPanelSetcur(CUI_Widget handle, int row, int col);
    void            CUI_textPanelClear(CUI_Widget handle);
    void            CUI_textPanelClearRow(CUI_Widget handle, int row);
    void            CUI_textPanelScroll(CUI_Widget handle, int count);
    void            CUI_textPanelPrint(CUI_Widget handle, char *text);
    void            CUI_textPanelPrintLine(CUI_Widget handle, int row, char *text);
    void            CUI_refreshDisplay(bool redraw);
    bool            CUI_updateDisplay(bool mode);
    bool            CUI_updateMode(void);
    void            CUI_getCursor(short *row, short *col);
    void            CUI_setCursor(short row, short col);
    int             CUI_execCommand(char *command, CUI_ExecFlag flag);
    int             CUI_saveDisplay(void);
    int             CUI_restoreDisplay(void);
    CUI_CallbackProc CUI_lookupCallback(char *name);
    int             CUI_showHelp(char *topic);
    int             CUI_showHelpIndex(void);


    /*
	 * ==================================================================
	 * utility routines
	 * ==================================================================
     */

    /* message routines */

    void            CUI_fatal(char *format,...);
    void            CUI_warning(char *format,...);
    void            CUI_infoMessage(char *format,...);

    /* keyboard routines (should NOT be used in event-driven programs!) */

    int             CUI_keyReady(void);
    int             CUI_getKey(void);

    /* memcheck.h defines CUI_malloc, CUI_free, and CUI_newString */

#if 0
    /* memory-management routines */

    void           *CUI_malloc(size_t size);
    void            CUI_free(void *ptr);
    char           *CUI_newString(char *string);
#endif

    /* string routines */

    char           *CUI_uppercase(char *string);
    char           *CUI_lowercase(char *string);
    char           *CUI_substr(char *string, char *sub);
    int             CUI_strempty(char *string);
    char           *CUI_ltrim(char *string);
    char           *CUI_rtrim(char *string);
    char           *CUI_lrtrim(char *string);

    /* dynamic array routines */

    char          **CUI_fileToArray(char *file);
    char          **CUI_helpToArray(FILE * file);
    char          **CUI_buffToArray(char *buffer);
    void            CUI_deleteArray(char **array);
    char          **CUI_arrayGrow(char **array, int bump);
    int             CUI_arrayCount(char **array);
    int             CUI_arrayWidth(char **array);

#ifdef NO_GETTEXT
#define LC_ALL 0
    char           *gettext(char *text);
	char			*setlocale(int category, const char *locale);
#else
#include <libintl.h>
#endif

    /*
	 * ==================================================================
	 * builtin callbacks (mostly for testing)
	 * ==================================================================
     */

    int             CUI_reportCallback(CUI_Widget, void *, void *);
    int             CUI_verifyCallback(CUI_Widget, void *, void *);
    int             CUI_exitCallback(CUI_Widget, void *, void *);
    int             CUI_popupCallback(CUI_Widget, void *, void *);
    int             CUI_popdownCallback(CUI_Widget, void *, void *);
    int             CUI_helpIndexCallback(CUI_Widget, void *, void *);
    bool            CUI_testKeyFilter(CUI_Widget handle, int key);

#ifdef __cplusplus
}
#endif


/*
 * ==================================================================
 * errno values
 * ==================================================================
 */

extern int		CUI_errno;

#define E_NULL            -1001
#define E_BAD_HANDLE      -1002
#define E_BAD_CODE        -1003
#define E_BAD_TYPE        -1004
#define E_BAD_ATTRIB      -1005
#define E_NO_CHANGE       -1006
#define E_INIT_FAIL       -1007
#define E_NOT_UNIQUE      -1008
#define E_NOT_FOUND       -1009
#define E_NO_FILE         -1010
#define E_READ            -1011
#define E_OPEN            -1012
#define E_TOO_MANY        -1013

/*
 * ==================================================================
 * miscellaneous globals and definitions
 * ==================================================================
 */

extern bool 	CUI_cursorOK;
extern bool 	CUI_doRefresh;

#define CUI_NORMAL_COLOR		0
#define CUI_REVERSE_COLOR		1
#define CUI_BOLD_COLOR			2
#define CUI_UNDERLINE_COLOR 	3
#define CUI_BLINK_COLOR 		4
#define CUI_MONO_COLOR			5

extern CUI_Widget CUI_normalColor;
extern CUI_Widget CUI_reverseColor;
extern CUI_Widget CUI_boldColor;
extern CUI_Widget CUI_underlineColor;
extern CUI_Widget CUI_blinkColor;
extern CUI_Widget CUI_monoColor;

extern CUI_Widget CUI_helpWindow;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif
#ifndef NO_REF
#define NO_REF(x) (x = x)
#endif

#ifndef NULL

#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*0)
#endif

#endif

/* default $CUIHOME value - !!!change this!!! */

#define CUI_DEFAULT_HOME "/cui"


#endif				/* _CUI_H */

