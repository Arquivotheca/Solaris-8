#pragma ident "@(#)trace.cc   1.3     92/11/25 SMI"

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
//	trace routines
//
//	$RCSfile: trace.cc $ $Revision: 1.3 $ $Date: 1992/09/12 15:25:21 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS
#include <stdio.h>

#ifndef  _CUI_WIDGET_H
#include "widget.h"
#endif

#endif	// PRE_COMPILED_HEADERS


int CUI_tracing = 0;
FILE *CUI_traceFile = NULL;


void CUI_traceMessage(Widget *to, CUI_Message *message)
{
	fprintf(CUI_traceFile, "Message %s to %s '%s'\n",
							CUI_MessageNames[message->type],
							to->typeName(),
							to->getName());
}


void CUI_traceKey(Widget *to, int key)
{
	fprintf(CUI_traceFile, "Key %d to %s '%s'\n",
							key,
							to->typeName(),
							to->getName());
}
 
