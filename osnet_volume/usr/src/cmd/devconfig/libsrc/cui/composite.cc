#pragma ident "@(#)composite.cc   1.5     92/12/07 SMI"

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
//	Composite class implementation
//
//	$RCSfile: composit.cc $ $Revision: 1.23 $ $Date: 1992/09/29 20:23:33 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_COMPOSITE_H
#include "composite.h"
#endif
#ifndef  _CUI_CAPTION_H
#include "caption.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// Resources added by Composite:

static CUI_StringId resList[] =
{
	hSpaceId,
	vSpaceId,
	hPadId,
	vPadId,
	nullStringId
};


Composite::Composite(char *Name, Widget *Parent, CUI_Resource *Resources,
					 CUI_WidgetId id)
	: Widget(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	// initialize child array

    childArraySize = 10;
	numChildren = 0;
	MEMHINT();
	children = (Widget**)CUI_malloc(childArraySize * sizeof(Widget *));

 	hSpace = 2;
 	vSpace = 1;
 	hPad = vPad = 0;

	loadResources(resList);
	setValues(Resources);
}


int Composite::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// manage our geometry
	// (will first tell all our children to manage theirs)

	manageGeometry();

    // now that we and all children are managed, realize our children

	for(int i = 0; i < numChildren; i++)
		children[i]->realize();

	flags |= CUI_REALIZED;

	return 0;
}


Composite::~Composite(void)
{
	// delete our children and then the array

	for(int i = 0; i < numChildren; i++)
	{
		MEMHINT();
        delete(children[i]);
	}
	MEMHINT();
    CUI_free(children);
}


//
//	resource routines
//

int Composite::setValue(CUI_Resource *resource)
{
	byte value = (int)resource->value;
    switch(resource->id)
	{
		case hSpaceId:
			hSpace = value;
			break;
        case vSpaceId:
			vSpace = value;
            break;
		case hPadId:
			hPad = value;
            break;
		case vPadId:
			vPad = value;
            break;
        default:
			return(Widget::setValue(resource));
	}
	return(0);
}

int Composite::getValue(CUI_Resource *resource)
{
	return(Widget::getValue(resource));
}


//
//	add child to this Widget
//

int Composite::addChild(Widget *child)
{
	// grow child array if necessary, then add child to end of array

	if(numChildren == childArraySize - 1)  // keep final NULL!
	{
		children = (Widget **)CUI_arrayGrow((char **)children, 10);
		childArraySize += 10;
	}
	children[numChildren++] = child;
	return(0);
}


//
//	get containing ControlArea (exclude Captions!)
//

ControlArea *Composite::getControlArea(void)
{
	Widget *area = parent;
	while(area)
	{
		if(!area->isKindOf(CUI_CONTROL_AREA_ID) ||
		   area->getId() == CUI_CAPTION_ID)
		{
			area = area->getParent();
		}
		else
			break;
	}
	if(area->isKindOf(CUI_CONTROL_AREA_ID))
		return((ControlArea *)area);
	else
		return(NULL);
}


//=============================================================================
//	geometry-management routines
//=============================================================================


//
//	qsort routine to compare two Widgets
//

static int compareWidgets(const void *one, const void *two)
{
	Widget *first  = *(Widget **)one;
	Widget *second = *(Widget **)two;
	return(Widget::compare(first, second));
}


//
//	manage geometry
//

int Composite::manageGeometry(void)
{
	// first tell our children to manage their geometry

	for(int i = 0; i < numChildren; i++)
		children[i]->manageGeometry();

    // adjust location/size of all our children

	adjustChildren();

	// align captions

	if(flags & CUI_ALIGN_CAPTIONS)
		alignCaptions();

	// set our height and width

	setDimensions();

	// if we have any ControlArea children that are delineated
	// by Separators, reset their width to ours (previously
	// they will have calculated width based on their childrens'
	// sizes) and tell them to re-adjust Center and Right

	int adjustWidth = 2 + (2 * hPad); // allow for border & hPad
	for(i = 0; i < numChildren; i++)
	{
		if(children[i]->getId() == CUI_CONTROL_AREA_ID &&
		   (!(children[i]->flagValues() & CUI_BORDER)) &&
		   children[i]->wWidth() < width - adjustWidth)
		{
			children[i]->wWidth() = width - adjustWidth;
			((Composite *)children[i])->adjustCenter();
			((Composite *)children[i])->adjustRight();
		}
	}

	// now do our own center- and right-adjustment

    adjustCenter();
	adjustRight();

    flags |= CUI_MANAGED;
    return(0);
}


//
//	adjust location/size of children
//

int Composite::adjustChildren(void)
{
	// anything to do?

	if(!numChildren)
		return(0);

	// sort the array of children

	qsort((void *)children, numChildren, sizeof(Widget *), compareWidgets);

	// adjust horizontal and vertical locations

	adjustHorizontal();
	adjustVertical();

    return(0);
}


//
//	ajust horizontal location of children
//

void Composite::adjustHorizontal(void)
{
	int i, j, k;

	// find first column position (we must scan the array)

	short firstCol = 9999;
	for(i = 0; i < numChildren; i++)
	{
		if(children[i]->wCol() < firstCol)
			firstCol = children[i]->wCol();
	}

	// adjust for hPad - make first col equal to hPad,
	// and adjust all others accordingly

	short adjust = firstCol - hPad;
	if(adjust != 0)
	{
		for(i = 0; i < numChildren; i++)
			children[i]->wCol() -= adjust;
	}

	// adjust for hSpace...

	for(i = 0; i < numChildren; i++)
	{
		short currentRow = children[i]->wRow();

		// figure out index of first and last child on current row

		int first	= i;
		int last	= first;
		for(j = first;
			j < numChildren && children[j]->wRow() == currentRow;
			j++)
		{
			last = j;
		}

        // for subsequent children in this row...

		for(j = first + 1; j <= last; j++)
		{
			Widget *previous = children[j - 1];
			Widget *next	 = children[j];

			// figure out where next widget on row would start,
			// assuming minimal spacing

			short saveCol = next->wCol();
			short nextCol = previous->wCol() + previous->wWidth() + hSpace;

			// nothing to do if next is already optimally positioned

			if(next->wCol() == nextCol)
				continue;

			// if spacing is greater than minimal, and we've already adjusted
			// next widget, don't change its location (it was located where
			// it is in order to maintain spacing in an earlier row)

			if(saveCol > nextCol && (next->flagValues() & CUI_ADJUSTED_COL))
				continue;

            // adjust start column of next to maintain spacing
			// (set flag to indicate that we've done so)

			next->wCol()  = nextCol;
			next->flagValues() |= CUI_ADJUSTED_COL;

			// if any other widgets started in same col as the one we've
			// just adjusted, adjust their start columns so they still line up
			// (set flag to indicate that we've done so)

			for(k = 0; k < numChildren; k++)
			{
				Widget *tmp = children[k];
				if(tmp->wCol() == saveCol)
				{
					tmp->wCol() = nextCol;
					tmp->flagValues() |= CUI_ADJUSTED_COL;
				}
            }
		}

		// skip to beginning of next row (remember i will be incremented!)

		i = last;
	}
}


//
//	ajust vertical location of children
//

void Composite::adjustVertical(void)
{
	int i, j, k;

	// find first row position (this is easy, since children are sorted)

	short firstRow = children[0]->wRow();

    // adjust for vPad - make first row equal to vPad,
	// and adjust all others accordingly

	short adjust = firstRow - vPad;
	if(adjust != 0)
	{
		for(i = 0; i < numChildren; i++)
			children[i]->wRow() -= adjust;
	}

	// adjust for vSpace (-1 means use actual resource values)

	if(vSpace == (byte)-1)	 // vSpace is a byte!
		return;

	for(i = 0; i < numChildren; i++)
	{
		short currentRow = children[i]->wRow();

		// figure out correct location for children on next row

		short optimalNext = currentRow + 1;
		for(j = i;
			j < numChildren && children[j]->wRow() == currentRow;
			j++)
		{
			short calcNext = currentRow + children[j]->wHeight() + vSpace;
			if(calcNext > optimalNext)
				optimalNext = calcNext;
		}

		// figure out the actual next row

		short actualNext = -1;
		if(j < numChildren)
			actualNext = children[j]->wRow();

		// if this is not equal to the optimal next row...

		if(actualNext >= 0 && actualNext != optimalNext)
		{
			// set all children on actualNext row to optimalNext row

			for(k = j;
				k < numChildren && children[k]->wRow() == actualNext;
				k++)
			{
				children[k]->wRow() = optimalNext;
			}

            // figure out the adjustment factor

			short adjust = optimalNext - actualNext;

			// adjust all children on lower rows accordingly

			for( ; k < numChildren; k++)
                children[k]->wRow() += adjust;
		}

		// reset i to index of first widget on next row, and continue
		// (subtract 1 since we'll bump at end of loop)

		i = j - 1;
	}
}


//
//	ajust any children that should be centered
//

void Composite::adjustCenter(void)
{
	bool adjusted = FALSE;

	for(int i = 0; i < numChildren; i++)
	{
		short currentRow = children[i]->wRow();

		// check to see if all widgets in this row are to be
		// center-adjusted (saving index of first and last for later)

		int first	= i;
		int last	= first;
		bool center = TRUE;
		for(int j = first;
			j < numChildren && children[j]->wRow() == currentRow;
			j++)
		{
			last = j;
			if(!(children[j]->flagValues() & CUI_ADJUST_CENTER))
				center = FALSE;
		}

		// if we're centering, do it

		if(center)
		{
			adjusted = TRUE;

            // calculate how many spaces we have to divide up

			short spaces = width;
			for(j = first; j <= last; j++)
				spaces -= children[j]->wWidth();

			// and divide them

			if(first == last)
				spaces /= 2;
			else
				spaces /= (last - first + 2);

			// locate the children (to decrease asymmetry caused by
			// rounding errors, ensure we have same number of spaces
			// on right as we have on left)

			short childCol = spaces;
			for(j = first; j <= last; j++)
			{
				if(j == last)
				{
					children[j]->wCol() =
						width - spaces - children[last]->wWidth() - 2;
				}
				else
				{
					children[j]->wCol() = childCol;
					childCol += children[j]->wWidth() + spaces;
				}
				if(children[j]->wCol() < 0)
					children[j]->wCol() = 0;
			}
		}

		// skip to beginning of next row (remember i will be incremented!)

		i = last;
	}

	// if we did an adjustment, re-align captions

	if(adjusted && (flags & CUI_ALIGN_CAPTIONS))
		alignCaptions();
}


//
//	ajust any children that should be right-adjusted
//

void Composite::adjustRight(void)
{
	for(int i = 0; i < numChildren; i++)
	{
		short currentRow = children[i]->wRow();

		// figure out index of first and last child on current row

		int first	= i;
		int last	= first;
		for(int j = first;
			j < numChildren && children[j]->wRow() == currentRow;
			j++)
		{
			last = j;
		}

		// do we want to right-adjust any of the children in this row?

		short endCol = width - 1 - hPad;
		for(j = last; j >= first; j--)
		{
			if(children[j]->flagValues() & CUI_ADJUST_RIGHT)
			{
				children[j]->wCol() = endCol - children[j]->wWidth() - 1;
				endCol -= (children[j]->wWidth() + hSpace);
			}
			else
				break;
		}

		// skip to beginning of next row (remember i will be incremented!)

		i = last;
	}
}


//
//	set height and width based on locations and sizes of children
//

int Composite::setDimensions(void)
{
	// set our width  to furthest right child's (col + width) + hPad
	// and our height to lowest child's (row + height) + vPad
	// (bump as necessary to allow for borders)

	short vBump = 0;
    short hBump = 0;
	if(flags & CUI_BORDER)
	{
		hBump = 2;
		vBump = 2;
	}
	if(flags & CUI_HLINE)
		vBump = 2;

	for(int i = 0; i < numChildren; i++)
	{
		Widget *widget = children[i];
		short tmp;

		tmp = widget->wCol() + widget->wWidth() + hPad + hBump;
		if(tmp > width)
			width = tmp;
		tmp = widget->wRow() + widget->wHeight() + vPad + vBump;
		if(tmp > height)
			height = tmp;
    }

	// if our last child is a FooterPanel, or we are a FooterPanel,
	// don't add the final vPad

	if(numChildren)
	{
		if(children[numChildren - 1]->getId() == CUI_FOOTER_ID ||
			ID == CUI_FOOTER_ID)
		{
			height -= vPad;
		}
	}
	return(0);
}


//
//	align children's Captions
//

void Composite::alignCaptions(void)
{
	// keep track of which cols we've processed

	bool cols[200] = { 0 };

    // for each unique column position...

	for(int i = 0; i < numChildren; i++)
	{
		short thisCol = children[i]->wCol();

		// have we already processed this column?

		if(cols[thisCol])
			continue;
		else
			cols[thisCol] = TRUE; // now we have...

		// find longest left-aligned caption in this column

		int maxLen = -1;
		for(int j = i; j < numChildren; j++)
		{
			int len;
			if(children[j]->getId() == CUI_CAPTION_ID &&
			   ((Caption *)children[j])->getPosition() != topId &&
			   children[j]->wCol() == thisCol)
			{
				len = children[j]->labelLength();
				if(len > maxLen)
					maxLen = len;
			}
		}

		// tell children in this col to align their labels accordingly
		// (they'll do nothing if top-aligned)

		if(maxLen > 0)
		{
			for(j = i; j < numChildren; j++)
			{
				if(children[j]->getId() == CUI_CAPTION_ID &&
				   children[j]->wCol() == thisCol)
				{
					children[j]->alignLabel(thisCol + maxLen - 1);
				}
			}
		}
	}
}

