#pragma ident "@(#)cuitypes.h   1.4     98/10/22 SMI"

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

/*===========================================================================
	CUI typedefs and enums

	$RCSfile: cuitypes.h $ $Revision: 1.28 $ $Date: 1992/09/13 04:42:58 $
=============================================================================*/

#ifndef _CUI_TYPES_H
#define _CUI_TYPES_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _STRINGID_H
#include "stringid.h"
#endif

#endif /* PRE_COMPILED_HEADERS */


/* some basic typedefs */

#include <curses.h>
typedef unsigned char byte;


/* 'handles' to private objects */

typedef void *CUI_Widget;
typedef void *CUI_ListToken;


/*
 *	widget IDs
 *	WARNING! for each addition to this list, add an entry to
 *			 the typeNames arrays in widget.cc and cuicomp.cc
 */

#define CUI_BUMP 30000
typedef enum
{
	CUI_NULL_ID 			= 0   + CUI_BUMP, /* nothing */
	CUI_WIDGET_ID			= 1   + CUI_BUMP, /* base widget */
	CUI_COMPOSITE_ID		= 2   + CUI_BUMP, /* composite */
	CUI_CONTROL_AREA_ID 	= 3   + CUI_BUMP, /* Composite with ETI form */
	CUI_CAPTION_ID			= 4   + CUI_BUMP, /* caption */
	CUI_CONTROL_ID			= 5   + CUI_BUMP, /* control (widget with ETI field) */
	CUI_BUTTON_ID			= 6   + CUI_BUMP, /* button */
	CUI_OBLONGBUTTON_ID 	= 7   + CUI_BUMP, /* oblong <command> button */
	CUI_RECTBUTTON_ID		= 8   + CUI_BUMP, /* rect <(non)exlusive> button */
	CUI_MENUBUTTON_ID		= 9   + CUI_BUMP, /* menu button */
	CUI_CHECKBOX_ID 		= 10  + CUI_BUMP, /* checkbox */
	CUI_TEXTEDIT_ID 		= 11  + CUI_BUMP, /* multi-line text edit */
	CUI_TEXTFIELD_ID		= 12  + CUI_BUMP, /* single-line text edit */
	CUI_STATIC_TEXT_ID		= 13  + CUI_BUMP, /* non-editable multi-line text */
	CUI_NUMFIELD_ID 		= 14  + CUI_BUMP, /* single-line numeric field */
	CUI_ITEM_ID 			= 15  + CUI_BUMP, /* abstract menu/list item */
	CUI_MENU_ID 			= 16  + CUI_BUMP, /* menu */
	CUI_MITEM_ID			= 17  + CUI_BUMP, /* menu item */
	CUI_WINDOW_ID			= 18  + CUI_BUMP, /* window */
	CUI_SCROLLBAR_ID		= 19  + CUI_BUMP, /* scroll-bar */
	CUI_SHELL_ID			= 20  + CUI_BUMP, /* shell */
	CUI_BASEWIN_ID			= 21  + CUI_BUMP, /* base window */
	CUI_VCONTROL_ID 		= 22  + CUI_BUMP, /* virtual control */
	CUI_EXCLUSIVES_ID		= 23  + CUI_BUMP, /* exclusive */
	CUI_NONEXCLUSIVES_ID	= 24  + CUI_BUMP, /* non-exclusive */
	CUI_LIST_ID 			= 25  + CUI_BUMP, /* scrolling-list */
	CUI_LITEM_ID			= 26  + CUI_BUMP, /* list item */
	CUI_NOTICE_ID			= 27  + CUI_BUMP, /* notice */
	CUI_POPWIN_ID			= 28  + CUI_BUMP, /* popup window */
	CUI_ABBREV_ID			= 29  + CUI_BUMP, /* abbreviated menu button */
	CUI_FOOTER_ID			= 30  + CUI_BUMP, /* footer panel */
	CUI_SEPARATOR_ID		= 31  + CUI_BUMP, /* separator */
	CUI_SLIDER_CONTROL_ID	= 32  + CUI_BUMP, /* slider control (private) */
	CUI_GAUGE_ID			= 33  + CUI_BUMP, /* gauge */
	CUI_SLIDER_ID			= 34  + CUI_BUMP, /* slider */
	CUI_TEXTPANEL_ID		= 35  + CUI_BUMP, /* text-panel */
	CUI_HYPERPANEL_ID		= 36  + CUI_BUMP, /* hypertext-panel */
	CUI_HELPWIN_ID			= 37  + CUI_BUMP, /* help window (hypertext) */
	CUI_KEYBD_ID			= 38  + CUI_BUMP, /* keyboard */
	CUI_COLOR_ID			= 39  + CUI_BUMP /* color */
} CUI_WidgetId;

#define CUI_FIRST_WIDGET_ID (CUI_BUMP)
#define CUI_LAST_WIDGET_ID	(CUI_BUMP + 39)


/* types of callback */

typedef enum
{
	CUI_SELECT_CALLBACK,
	CUI_FOCUS_CALLBACK,
	CUI_UNFOCUS_CALLBACK,
	CUI_DESTROY_CALLBACK,
	CUI_UNSELECT_CALLBACK,
	CUI_VERIFY_CALLBACK,
	CUI_MOVED_CALLBACK,
	CUI_HELP_CALLBACK,
	CUI_TEXTFUNC_CALLBACK
} CUI_CallbackType;


/* callback procedure */

typedef int (*CUI_CallbackProc)(CUI_Widget, void *, void *);


/* Callback definition (allows callbacks to be defined by name) */

typedef struct
{
	CUI_CallbackProc proc;
	char			 *name;
} CUI_CallbackDef;


/* Resource-definition structure */

typedef struct
{
	CUI_StringId id;
	void		 *value;
} CUI_Resource;


/* macro to cast resource values */

#define CUI_RESOURCE(x) ((void *)(x))
#define CUI_STR_RESOURCE(x) (CUI_RESOURCE(CUI_compileString(x)))


/* macro to asssign a resource-value pair to element in resource array */

#define CUI_setArg(resource, Id, Value) \
    resource.id = (Id); resource.value = (void *)(Value)	


/* popup grab-modes */

typedef enum
{
	CUI_GRAB_EXCLUSIVE,
	CUI_GRAB_NONEXLUSIVE
} CUI_GrabMode;


/* TextField verification structure */

typedef struct
{
	char *string;
	bool ok;
} CUI_TextFieldVerify;


/*
 *	 List item for ScrollingList widget
 *	 (this differs from OLIT in that we have no label_type or glyph field)
 */

typedef struct
{
	char			*label;
	long			attr;
	void			*user_data;
	unsigned char	mnemonic;
} CUI_ListItem;


/* keyfilter procedure */

typedef bool (*CUI_KeyFilter)(CUI_Widget, int);

/* operation codes for textFunc */

typedef enum
{
	CUI_TEXTFUNC_INIT,		/* initialize (and seek to home) */
	CUI_TEXTFUNC_SIZE,		/* return #lines (-1 if we don't know) */
	CUI_TEXTFUNC_HOME,		/* seek to home */
	CUI_TEXTFUNC_NEXT,		/* seek to next line */
	CUI_TEXTFUNC_PREV,		/* seek to previous line */
	CUI_TEXTFUNC_READ,		/* return pointer to current line */
	CUI_TEXTFUNC_EXIT,		/* cleanup and exit */

	/* extensions for hypertext */

	CUI_TEXTFUNC_NEW_TOPIC  /* load topic */

} CUI_TextFuncOp;

/* textFunc routine */

typedef int (*CUI_TextFunc)(CUI_TextFuncOp, void *, void *);


/* exec codes for CUI_execCommand */

typedef enum
{
	CUI_EXEC_QUIET,  /* just do it											*/
	CUI_EXEC_WAIT,	 /* save/restore keyboard/screen - wait for key 		*/
	CUI_EXEC_NOWAIT, /* save/restore keyboard/screen - don't wait for key   */
	CUI_EXEC_CUI	 /* exec'd program is a CUI program                     */

} CUI_ExecFlag;


#endif /* _CUI_TYPES_H */

