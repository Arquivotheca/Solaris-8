#pragma ident "@(#)separator.h   1.3     92/11/25 SMI"

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
//	Separator class definitions
//
//	$RCSfile: separator.h $ $Revision: 1.3 $ $Date: 1992/09/12 15:31:41 $
//=============================================================================


#ifndef _CUI_SEPARATOR_H
#define _CUI_SEPARATOR_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_CONTROL_H
#include "control.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Separator : public Control
{
	protected:

    public:

		// constructor

		Separator(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				  CUI_WidgetId id = CUI_SEPARATOR_ID);

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
			 { return(type == CUI_SEPARATOR_ID || Control::isKindOf(type)); }

		// message-handlers

		virtual int show(void);
		virtual int hide(void)	  { /* should really 'undraw' */ return(0); }
};
#define NULL_SEPARATOR (Separator *)0

#endif // _CUI_SEPARATOR_H

