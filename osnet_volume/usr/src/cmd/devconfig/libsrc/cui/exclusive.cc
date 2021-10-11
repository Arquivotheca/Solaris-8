#pragma ident "@(#)exclusive.cc   1.6     93/07/23 SMI"

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
//	Exclusive and derived class implementations
//
//	$RCSfile: exclusive.cc $ $Revision: 1.2 $ $Date: 1992/12/29 22:06:35 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_EXCLUSIVE_H
#include "exclusive.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_XBUTTON_H
#include "xbutton.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
	layoutTypeId,
	measureId,
	sensitiveId,
    nullStringId
};


//=============================================================================
//	Exclusive
//=============================================================================


Exclusive::Exclusive(char* Name, Widget* Parent, CUI_Resource* Resources,
					 CUI_WidgetId id)
	: ControlArea(Name, Parent, NULL, id)
{
	measure = 1;
	layout	= fixedRowsId;
//	  flags  |= CUI_BORDER;
	flags |= CUI_SENSITIVE;
	flags |= CUI_USE_ARROWS;

	loadResources(resList);
	setValues(Resources);
}


int Exclusive::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// make sure we don't try to align captions
	// then tell ControlArea to finish the job

	flags &= ~CUI_ALIGN_CAPTIONS;
	return(ControlArea::realize());
}


//
//	resource routines
//

int Exclusive::setValue(CUI_Resource *resource)
{
	CUI_StringId value = (CUI_StringId)resource->value;
    switch(resource->id)
	{
		case layoutTypeId:
		{
			switch(value)
			{
				case fixedRowsId:
				case fixedColsId:
					layout = value;
					break;
				default:
					CUI_fatal(dgettext( CUI_MESSAGES, "Bad value for layoutType resource in %s '%s'"),
							  typeName(), name);
			 }
		}

		case measureId:
		{
			int tmpMeasure = (int)value;
			measure = (short)tmpMeasure;
			break;
		}

        default:
			return(ControlArea::setValue(resource));
	}
	return(0);
}


//
//	geometry-management
//

int Exclusive::manageGeometry(void)
{
	// tell children to manage their geometry
	// find length of children's longest label

	int maxLen = -1;
	for(int i = 0; i < numChildren; i++)
	{
		children[i]->manageGeometry();
		int len = children[i]->labelLength();
		if(len > maxLen)
			maxLen = len;
	}

	// calculate number of rows and columns from layout and measure resources

	int numCols;
	int numRows;
    if(layout == fixedRowsId)
	{
		numRows = measure;
		numCols = numChildren / numRows;
		if(numCols * numRows != numChildren)
			numCols++;
    }
	else // fixedCols
	{
		numCols = measure;
		numRows = numChildren / numCols;
		if(numCols * numRows != numChildren)
			numRows++;
    }

	// locate and resize all our children, and set their alignment
	// (if we have more than one row, we make all children same width)

	short childRow = 0;
	short childCol = 0;
	short tmpCol   = 0;
    for(i = 0; i < numChildren; i++)
	{
        if(numRows > 1)
		{
			tmpCol = childCol * maxLen;
			children[i]->resize(1, maxLen);
		}

		children[i]->locate(childRow, tmpCol);
		if(++childCol == numCols)
		{
			childCol = 0;
			childRow++;
		}
		if(numRows == 1)
			tmpCol += children[i]->wWidth();
	}

	// call Composite's manageGeometry and setDimensions methods
	// in order to adjust children according to hSpace, vSpace, etc,
	// and to set our height and width

	Composite::manageGeometry();
	Composite::setDimensions();
    flags |= CUI_MANAGED;
    return(0);
}


//=============================================================================
//	derived Exclusives and NonExclusives classes
//=============================================================================


//
//	constructors
//

Exclusives::Exclusives(char *Name, Widget *Parent, CUI_Resource *resources,
					   CUI_WidgetId id)
	: Exclusive(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	setValues(resources);
	flags &= ~CUI_MULTI;
}


NonExclusives::NonExclusives(char *Name, Widget *Parent,
							 CUI_Resource *resources, CUI_WidgetId id)
	: Exclusive(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	flags |= CUI_MULTI;
	setValues(resources);
}

