#pragma ident "@(#)caption.cc   1.5     93/07/22 SMI" 

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
//	Caption class implementation
//
//	$RCSfile: caption.cc $ $Revision: 1.2 $ $Date: 1992/12/29 22:14:57 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_CAPTION_H
#include "caption.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_TEXT_H
#include "text.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
	labelId,
	positionId,
	alignmentId,
	exclusiveId,
    normalColorId,
    nullStringId
};


//
//	constructor
//

Caption::Caption(char *Name, Widget *Parent, CUI_Resource *resources,
				 CUI_WidgetId id)
	: Composite(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	label		= nullStringId;
	position	= leftId;
	alignment	= centerId;
	hSpace		= 1;
	space		= 2;
	normalColor = CUI_NORMAL_COLOR;

	// default mode is NonExclusive

	flags |= CUI_MULTI;

	loadResources(resList);
	setValues(resources);
}


int Caption::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);
	if(!numChildren)
		CUI_fatal(dgettext(CUI_MESSAGES, "Caption '%s' has no children"), name);

	// pass color on to our StaticText child

	children[0]->normalAttrib() = normalColor;

	// tell Composite to finish the job

	Composite::realize();

	// (re)-sync our flags

	syncFlags();
	return(0);
}


Caption::~Caption(void)
{
    // nothing to do (label is compiled, not a string)
}


//
//	resource routines
//

int Caption::setValue(CUI_Resource *resource)
{
	int intValue = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case labelId:
			return(addLabel(intValue));
		case positionId:
		{
			switch(intValue)
			{
				case leftId:
				case topId:
					position = (CUI_StringId)intValue;
					break;
				default:
					CUI_fatal(dgettext(CUI_MESSAGES,
						"Bad position resource value for %s '%s'"),
						typeName(), name);
			}
			break;
		}
		case exclusiveId:
		{
			if(intValue == trueId)
				flags &= ~CUI_MULTI;
			else
				flags |= CUI_MULTI;
			break;
		}
        case alignmentId:
		{
			switch(intValue)
			{
				case leftId:
				case centerId:
					alignment = (CUI_StringId)intValue;
					break;
				default:
					CUI_fatal(dgettext(CUI_MESSAGES,
						"Bad alignment resource value for %s '%s'"),
						typeName(), name);
			}
			break;
        }
		case normalColorId:
		{
			return(setColorValue(strValue, normalColor));
		}
        default:
			return(Composite::setValue(resource));
	}
	return(0);
}

int Caption::getValue(CUI_Resource *resource)
{
	return(Widget::getValue(resource));
}


//
//	add child
//

int Caption::addChild(Widget *child)
{
	// we allow only a StaticText (label) and another Widget as children

	switch(numChildren)
    {
		case 1:
		{
			if(children[0]->getId() != CUI_STATIC_TEXT_ID &&
			   child->getId() != CUI_STATIC_TEXT_ID)
			{
				goto oneChild;
			}
			break;
		}
		case 2:
		{
oneChild:
			CUI_fatal(dgettext(CUI_MESSAGES, 
				"Caption '%s' can have only one child"), name);
			break;
		}
	}

	Composite::addChild(child);

	// label must be first in list; switch if it isn't

	if(numChildren == 2 && children[0]->getId() != CUI_STATIC_TEXT_ID)
	{
		Widget *tmp = children[0];
		children[0] = children[1];
		children[1] = tmp;
	}
    return(0);
}


//
//	add label
//

int Caption::addLabel(int labelValue)
{
    // save the label

	label = labelValue;

    // do we already have a StaticText child?

	StaticText *staticChild = NULL;
	for(int i = 0; i < numChildren; i++)
	{
		if(children[i]->getId() == CUI_STATIC_TEXT_ID)
		{
			staticChild = (StaticText *)children[i];
			break;
		}
	}

	// if we don't already have a StaticText child, create one

	if(!staticChild)
	{
		// construct a name for StaticText widget

		char nameBuffer[256];
		sprintf(nameBuffer, "%s@Label", name);

		// create StaticText widget as child of this

		MEMHINT();
        staticChild = new StaticText(nameBuffer, NULL, NULL);

		// we created with NULL parent so we don't trip up on the
		// 'only one child' check in setParent; now it's safe to set

		staticChild->setParent(this);
	}

	// now we know we have a child, set its string resource to labelValue

	CUI_Resource resources[10];
	i = 0;
	CUI_setArg(resources[i], stringId,		 labelValue);	i++;
	CUI_setArg(resources[i], nullStringId,	 0);			i++;
    CUI_setValues(staticChild, resources);

	return(0);
}


//
//	manage geometry
//

int Caption::manageGeometry(void)
{
	short labelCol;  // value set in align-caption logic
	short labelRow = row;
    short childCol = col;
	short childRow = row;

    // get pointers to label and child widgets

	Widget *labelWidget = NULL;
	Widget *childWidget = NULL;

	switch(numChildren)
	{
        case 2:
		{
			labelWidget = children[0];
			labelCol	= labelWidget->wCol();
            childWidget = children[1];
			break;
		}
		case 1:
		{
			if(label)
				CUI_fatal(dgettext(CUI_MESSAGES, 
					"Caption '%s' has no child widget"), name);
			else
				childWidget = children[0];
			break;
		}
		case 0:
			CUI_fatal(dgettext(CUI_MESSAGES, 
				"Caption '%s' has no label or child widget"), name);
	}

	// if we have a child widget, manage its geometry and sync our flags

	if(childWidget)
	{
		childWidget->manageGeometry();
		syncFlags();
	}

    // locate our label and child depending on value of 'position'
	// and 'alignment' resources

    if(position == leftId)
	{
		if(label)
		{
			// (if child has a border, align label with first inside row,
			// rather than with the top border)

			if(numChildren == 2 && (childWidget->flagValues() & CUI_BORDERED))
				labelRow = row + 1;
		}
		if(numChildren == 2)
			childCol = labelCol + labelWidget->wWidth() + hSpace;
	}
	else if (position == topId)
	{
		if(numChildren == 2)
		{
			if(alignment == centerId)
			{
                short lWidth = labelWidget->wWidth();
				short cWidth = childWidget->wWidth();
				if(lWidth < cWidth)
					labelCol = childCol + ((cWidth - lWidth) / 2);
				else
					labelCol = childCol;
            }
            else
                labelCol = childCol;
			childRow++;
        }
	}
	if(labelWidget)
		labelWidget->locate(labelRow, labelCol);
	if(childWidget)
		childWidget->locate(childRow, childCol);

	// calculate width

	width = hPad;

	if(position == topId)
	{
		int labelWidth = (labelWidget ? labelWidget->wWidth() : 0);
		int childWidth = (childWidget ? childWidget->wWidth() : 0);
		if(labelWidth > childWidth)
			width += labelWidth;
		else
			width += childWidth;
    }
	else
	{
		if(label)
			width += labelWidget->wWidth() + hSpace;
		if(childWidget)
			width += childWidget->wWidth();
	}

	// calculate height

    if(label)
		height = labelWidget->wHeight();
	if(childWidget && childWidget->wHeight() > height)
		height = childWidget->wHeight();

	if(label && childWidget && position == topId)
		height++;

    // say we're managed and return

    flags |= CUI_MANAGED;
    return(0);
}


//
//	return length of our label
//

int Caption::labelLength(void)
{
	if(!label)
		return(-1);
	return(strlen(CUI_lookupString(label)));
}


//
//	align our label at column Col
//

int Caption::alignLabel(short Col)
{
	if(label && position != topId)
    {
		int len = strlen(CUI_lookupString(label));
		short newCol = Col - len + 1;
		short diff = newCol - col;
		children[0]->locate(children[0]->wRow(), newCol);
		width += diff;
	}
	return(0);
}


//
//	sync our flags with our child's
//

void Caption::syncFlags(void)
{
	Widget *child = NULL;
	switch(numChildren)
	{
        case 2:
			child = children[1];
			break;
		case 1:
			child = children[0];
			break;
	}
	if(child)
	{
		long childFlags = child->flagValues();
		if(childFlags & CUI_SENSITIVE)
			flags |= CUI_SENSITIVE;
		else
			flags &= ~CUI_SENSITIVE;
	}
}


