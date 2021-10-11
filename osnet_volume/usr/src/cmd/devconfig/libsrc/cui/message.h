#pragma ident "@(#)message.h   1.4     98/10/22 SMI"

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
//	Message class definition
//
//	$RCSfile: message.h $ $Revision: 1.3 $ $Date: 1992/09/12 15:17:52 $
//=============================================================================

#ifndef _CUI_MESSAGE_H
#define _CUI_MESSAGE_H

class Widget;


//
//   message IDs
//

typedef enum
{
	CUI_NOP,
	CUI_SHOW,					// show self
	CUI_HIDE,					// hide self
	CUI_SELECT, 				// select self (for key events)
	CUI_UNSELECT,				// unselect self (for key events)
	CUI_CANCEL, 				// user pressed CANCEL
	CUI_DONE,					// user pressed OK
	CUI_INTERPRET,				// interpret command string
	CUI_APP_EXIT				// stop processing events
} CUI_MessageId;


//
//	message structure
//

struct cui_message
{
	Widget		  *from;
	Widget		  *to;
	CUI_MessageId type;
	void		  **args;
};
typedef struct cui_message CUI_Message;


//
//	array of message names
//

extern char *CUI_MessageNames[];


//==========================================================================
//				   send standard messages to objects
//==========================================================================


// post messages to queue

#define CUI_postShowMessage(from, to) \
	CUI_Emanager->postMessage(from, to, CUI_SHOW, NULL)

#define CUI_postHideMessage(from, to) \
	CUI_Emanager->postMessage(from, to, CUI_HIDE, NULL)

#define CUI_postSelectMessage(from, to) \
	CUI_Emanager->postMessage(from, to, CUI_SELECT, NULL)

#define CUI_postUnselectMessage(from, to) \
	CUI_Emanager->postMessage(from, to, CUI_UNSELECT, NULL)

#define CUI_postCancelMessage(from, to) \
	CUI_Emanager->postMessage(from, to, CUI_CANCEL, NULL)

#define CUI_postDoneMessage(from, to) \
	CUI_Emanager->postMessage(from, to, CUI_DONE, NULL)

#define CUI_postInterpretMessage(from, to, args) \
	CUI_Emanager->postMessage(from, to, CUI_INTERPRET, (void **)args)

#define CUI_postAppExitMessage() \
	CUI_Emanager->postMessage(NULL, NULL, CUI_APP_EXIT, NULL)


#endif // _MESSAGE_H

