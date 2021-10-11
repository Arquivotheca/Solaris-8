#pragma ident "@(#)litem.cc   1.3     92/11/25 SMI"

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
//	ListItem class implementation
//
//	$RCSfile: litem.cc $ $Revision: 1.7 $ $Date: 1992/09/12 15:25:04 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#ifndef  _CUI_LITEM_H
#include "litem.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_STRINGID_H
#include "stringid.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
    nullStringId
};

ListItem::ListItem(char* Name, Widget* Parent, CUI_Resource* Resources,
				   CUI_WidgetId id)
	: Item(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
    loadResources(resList);
	setValues(Resources);
}


//
//	realize
//

int ListItem::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	Item::realize();
    flags |= CUI_REALIZED;
    return(0);
}


//
//	destructor
//

ListItem::~ListItem(void)
{
}


//
//	resource routines
//

int ListItem::setValue(CUI_Resource *resource)
{
//	int value = (int)resource->value;
//	char *stringValue  = CUI_lookupString(value);
    switch(resource->id)
	{
        default:
			return(Item::setValue(resource));
	}
//	return(0);
}

int ListItem::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
        default:
			return(Item::getValue(resource));
	}
}

