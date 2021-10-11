#pragma ident "@(#)gauge.cc   1.4     99/02/19 SMI"

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
//	SliderControl, Gauge, and Slider class implementations
//
//			[######--------------]
//			0				   100
//
//	$RCSfile: gauge.cc $ $Revision: 1.11 $ $Date: 1992/09/12 15:24:09 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_GAUGE_H
#include "gauge.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_TEXT_H
#include "text.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
	ticksId,
	sliderMovedId,
    sliderValueId,
	sliderMinId,
	minLabelId,
	sliderMaxId,
    maxLabelId,
	normalColorId,
	activeColorId,
    nullStringId
};


//=============================================================================
//	SliderControl class implementation
//=============================================================================


//
//	constructor
//

SliderControl::SliderControl(char* Name, Widget* Parent,
							 CUI_Resource* Resources, CUI_WidgetId id)
	: Control(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
	ticks	 = 10;
	value	 = 0;
	minValue = 0;
	maxValue = 100;
    setValues(Resources);
}


//
//	realize
//

int SliderControl::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// make sure our width and height are correct

	height = rows = 1;
	width = ticks + 2;

	// constrain value between min and max

	setSliderValue(value);

	// invoke Control's realize method (will create ETI field)
	// update Slider

	Control::realize();
	updateSlider();
	return(0);
}


//
//	resource routines
//

int SliderControl::setValue(CUI_Resource *resource)
{
	int  intValue  = (int)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case ticksId:
		{
			ticks = (short)intValue;
			break;
        }
		case sliderValueId:
		{
			value = (short)intValue;
			if(flags & CUI_REALIZED)
				updateSlider();
			break;
        }
		case sliderMinId:
		{
			minValue = (short)intValue;
			break;
        }
		case sliderMaxId:
		{
			maxValue = (short)intValue;
			break;
        }
		case normalColorId:
		{
			return(setColorValue(strValue, normalColor));
		}
		case activeColorId:
		{
			return(setColorValue(strValue, activeColor));
		}
        default:
			return(Control::setValue(resource));
	}
	return(0);
}

int SliderControl::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		case sliderValueId:
			resource->value = (void *)value;
			break;
        default:
			return(Control::getValue(resource));
	}
	return(0);
}


//
//	handle key
//

int SliderControl::doKey(int key, Widget *from)
{
	// if edit key (8 bit char but not control char, INS, DEL), swallow it

    if((key < 256 && !iscntrl(key)) || key == KEY_INS || key == KEY_DEL)
		return(0);

    switch(key)
	{
		case KEY_LEFT:
		{
			short save = value;
			setSliderValue(value - valuesPerTick());
			if(value != save)
			{
				doCallback(CUI_MOVED_CALLBACK);
				updateSlider();
			}
			break;
		}
		case KEY_RIGHT:
		{
			short save = value;
			setSliderValue(value + valuesPerTick());
			if(value != save)
			{
				doCallback(CUI_MOVED_CALLBACK);
				updateSlider();
			}
			break;
		}
		default:
        {
			// else let Control handle it

			return(Control::doKey(key, from));
		}
	}
    return(0);
}


//
//	set slider value
//

void SliderControl::setSliderValue(int newValue)
{
	// sanity check min/max

	if(minValue >= maxValue)
		minValue = maxValue - 1;
	if(maxValue <= minValue)
		maxValue = minValue + 1;

	// constrain new value between min and max

    if(newValue < minValue)
		newValue = minValue;
	if(newValue > maxValue)
		newValue = maxValue;
	value = (short)newValue;
}


//
//	update slider value
//

int SliderControl::updateSlider(void)
{
	// construct the representation of the slider
	// (as "[#####------------]")

	char buffer[120];

	// first construct an 'empty' slider string

	buffer[0] = '[';
	buffer[width - 1] = ']';
	memset(buffer + 1, '-', width - 2);

	// calculate the number of hashes to write

	int numHashes = (value - minValue) / valuesPerTick();

	// write the hashes

    char *lptr = buffer + 1;
	while(numHashes--)
		*lptr++ = '#';

	// set field_buffer to this string

	set_field_buffer(field, CUI_INIT_BUFF, buffer);

	// refresh our ControlArea

	if(flags & CUI_REALIZED)
	{
		ControlArea *area = parent->getControlArea();
		if(area)
			area->refresh();
	}

	return(0);
}


//
//	calculate number of value units per tick
//

int SliderControl::valuesPerTick(void)
{
	int range = maxValue - minValue;
	return(range/ticks);
}


//=============================================================================
//	Gauge class implementation
//=============================================================================


//
//	constructor
//

Gauge::Gauge(char* Name, Widget* Parent, CUI_Resource* Resources,
			 CUI_WidgetId id)
	: Composite(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	Widget *tmp;
    char buffer[80];

	// set defaults

	hSpace = vSpace = hPad = vPad = 0;
	MEMHINT();
    minLabel = CUI_newString("0");
	MEMHINT();
    maxLabel = CUI_newString("100");

	// load our resources

    loadResources(resList);
    setValues(Resources);

    // make SliderControl and StaticText children

	CUI_Resource resources[10];
	int i = 0;
	CUI_setArg(resources[i], heightId,		 1);			i++;
	CUI_setArg(resources[i], nullStringId,	 0);			i++;
	sprintf(buffer, "%s@%s", name, "Slider");
	MEMHINT();
    tmp = new SliderControl(buffer, this, resources);
	tmp->normalAttrib() = normalColor;
	tmp->activeAttrib() = activeColor;

	i = 0;
	CUI_setArg(resources[i], heightId,		 1);			i++;
	CUI_setArg(resources[i], nullStringId,	 0);			i++;
	sprintf(buffer, "%s@%s", name, "Text");
	MEMHINT();
    tmp = new StaticText(buffer, this, resources);
	tmp->normalAttrib() = normalColor;
}


//
//	manage geometry
//

int Gauge::manageGeometry(void)
{
	height = 2;
	width = ((SliderControl *)children[0])->ticks + 2;
	return(0);
}


//
//	realize
//

int Gauge::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// construct string value for StaticText

	char buffer[120];
	int spaces = width - strlen(minLabel) - strlen(maxLabel);
	strcpy(buffer, minLabel);
	char *lptr = buffer + strlen(minLabel);
	for(int i = 0; i < spaces; i++)
		*lptr++ = ' ';
	strcpy(lptr, maxLabel);

	// set StaticText's string resource to this value

	CUI_Resource resources[2];
	i = 0;
	int value = CUI_compileString(buffer);
	CUI_setArg(resources[i], stringId,		 value);	i++;
    CUI_setArg(resources[i], nullStringId,   0);        i++;
	CUI_setValues(children[1], resources);

	// locate our children

	children[0]->wRow() = row;
	children[0]->wCol() = col;
	children[1]->wRow() = row + 1;
	children[1]->wCol() = col;

	// invoke Composite's realize method and make SliderControl un-editable

	Composite::realize();
	field_opts_off(children[0]->ETIfield(), O_ACTIVE);

    // that's it...

	return(0);
}


//
//	resource routines
//

int Gauge::setValue(CUI_Resource *resource)
{
	char *strValue = CUI_lookupString((int)resource->value);
    switch(resource->id)
	{
		// pass these on to SliderControl

		case ticksId:
		case sliderMinId:
		case sliderMaxId:
		case sliderValueId:
		case normalColorId:
		case activeColorId:
		{
			CUI_Resource resources[2];
			resources[0] = *resource;
			resources[1].id = nullStringId;
			return(children[0]->setValues(resources));
		}

		// handle these here

        case minLabelId:
		{
            MEMHINT();
            CUI_free(minLabel);
			MEMHINT();
            minLabel = CUI_newString(strValue);
			break;
        }
		case maxLabelId:
		{
			MEMHINT();
            CUI_free(maxLabel);
			MEMHINT();
            maxLabel = CUI_newString(strValue);
			break;
        }

		// pass all others on to Composite

        default:
			return(Composite::setValue(resource));
	}
	return(0);
}

int Gauge::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		case sliderValueId:
		{
			resource->value = (void *)((SliderControl *)children[0])->value;
			break;
		}
        default:
			return(Composite::getValue(resource));
	}
	return(0);
}


//=============================================================================
//	Slider classs implementation
//=============================================================================


//
//	constructor
//

Slider::Slider(char* Name, Widget* Parent, CUI_Resource* Resources,
			   CUI_WidgetId id)
	: Gauge(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
    setValues(Resources);
}


//
//	realize
//

int Slider::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// invoke Gauge's realize method then make SliderControl's field editable

	Gauge::realize();
	field_opts_on(children[0]->ETIfield(), O_ACTIVE);

	return(0);
}

