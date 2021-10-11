#pragma ident "@(#)resource.cc   1.6     93/07/23 SMI"

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
//	Widget resource methods
//
//	$RCSfile: resource.cc $ $Revision: 1.22 $ $Date: 1992/09/12 15:20:53 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_PROTO_H
#include "cuiproto.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WIDGET_H
#include "widget.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


//
//	set/get resource(s) under program control
//

int Widget::setValues(CUI_Resource *resources)
{
	if(resources)
	{
		for(int i = 0; resources[i].id; i++)
			setValue(&resources[i]);
	}
	return(0);
}

int Widget::setValue(CUI_Resource *resource)
{
	int value = (int)resource->value;
	char *strValue = CUI_lookupString(value);
    switch(resource->id)
	{
		case rowId:
			locate(value, col);
			break;
		case colId:
			locate(row, value);
            break;
		case heightId:
			resize(value, width);
			break;
		case widthId:
			resize(height, value);
			break;
		case adjustId:
		{
			switch(value)
			{
				case leftId:
					flags &= ~CUI_ADJUST_RIGHT;
					flags &= ~CUI_ADJUST_CENTER;
					break;
				case rightId:
					flags &= ~CUI_ADJUST_CENTER;
					flags |= CUI_ADJUST_RIGHT;
					break;
                case centerId:
					flags &= ~CUI_ADJUST_RIGHT;
					flags |= CUI_ADJUST_CENTER;
					break;
				default:
					CUI_fatal(dgettext( CUI_MESSAGES, "Bad value for adjust resource in %s %s"),
							  typeName(), name);
            }
			break;
		}
        case borderWidthId:
		{
			switch(value)
			{
				case 1:
					flags |= CUI_HLINE;
					flags &= ~CUI_BORDER;
                    break;
				case 2:
					flags |= CUI_BORDER;
					flags &= ~CUI_HLINE;
                    break;
				default:
					flags &= ~CUI_BORDER;
					flags &= ~CUI_HLINE;
			}
			break;
		}
		case sensitiveId:
		{
			setFlagValue(value, CUI_SENSITIVE);

			// propagate to parent if it's a Caption

			if(parent && parent->getId() == CUI_CAPTION_ID)
			{
				if(value == trueId)
					parent->flagValues() |= CUI_SENSITIVE;
				else
					parent->flagValues() &= ~CUI_SENSITIVE;
			}
            break;
		}
		case popupOnSelectId:
        case popdownOnSelectId:
		{
			Widget *widget = Widget::lookup(strValue);
			if(!widget)
				CUI_fatal(dgettext( CUI_MESSAGES, "Can't find widget %s to popup/down"),
						  strValue);
			CUI_CallbackProc callback;
			if(resource->id == popupOnSelectId)
				callback = CUI_popupCallback;
			else
				callback = CUI_popdownCallback;
			addCallback(callback, CUI_SELECT_CALLBACK, widget);
			break;
		}
        case defaultId:
		{
			setFlagValue(value, CUI_DEFAULT);

			// propagate to parent if it's a Caption

			if(parent && parent->getId() == CUI_CAPTION_ID)
			{
				if(value == trueId)
					parent->flagValues() |= CUI_DEFAULT;
				else
					parent->flagValues() &= ~CUI_DEFAULT;
			}
            break;
		}
		case mappedWhenManagedId:
		{
			setFlagValue(value, CUI_MAPPED);
			break;
		}
		case selectId:
		{
			addCallback(strValue, CUI_SELECT_CALLBACK);
			break;
        }
		case focusCallbackId:
		{
			addCallback(strValue, CUI_FOCUS_CALLBACK);
			break;
        }
		case unfocusCallbackId:
		{
			addCallback(strValue, CUI_UNFOCUS_CALLBACK);
			break;
        }
        case helpCallbackId:
		{
			addCallback(strValue, CUI_HELP_CALLBACK);
			break;
        }
        case helpId:
		{
			MEMHINT();
            CUI_free(help);
			MEMHINT();
			help = CUI_newString(strValue);
			break;
		}
		case footerMessageId:
		{
			MEMHINT();
			CUI_free(footerMsg);
			MEMHINT();
			footerMsg = CUI_newString(strValue); 
			break;
		}
		case userPointerId:
		{
			ptr = resource->value;
			break;
        }
        default:
			CUI_fatal(dgettext( CUI_MESSAGES, "Unrecognized resource id (%d)"), resource->id);
	}
	return(0);
}

void Widget::setFlagValue(int resource, long value)
{
	if(resource == trueId)
		flags |= value;
	else
		flags &= ~value;
}

int Widget::setColorValue(char *colorName, short &valueToSet)
{
	short color = Color::lookup(colorName);
	if(color != -1)
	{
		valueToSet = color;
		return(0);
	}
	return(-1);
}

int Widget::getValues(CUI_Resource *resources)
{
	if(resources)
	{
		for(int i = 0; resources[i].id; i++)
			getValue(&resources[i]);
	}
	return(0);
}

int Widget::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		case userPointerId:
			resource->value = ptr;
			break;
        default:
			CUI_fatal(dgettext( CUI_MESSAGES, "Unrecognized resource id (%d)"), resource->id);
	}
	return(0);
}


//
//	load resources from ResourceTable
//

int Widget::loadResources(CUI_StringId list[])
{
	// anything to do?

	if(!list[0])
		return(0);

	char className[1024];
	className[0] = 0;
	char instanceName[1024];
	instanceName[0] = 0;

	constructClassName(className);
	constructInstanceName(instanceName);

	for(int i = 0; list[i]; i++)
	{
		int value = 0;
		if(CUI_Restab->lookup(list[i], className, instanceName, value) == 0)
		{
			CUI_Resource resource;
			resource.id = list[i];
			resource.value = (void *)value;
			setValue(&resource);
		}
	}
    return(0);
}


//
//	construct fully-qualified class/instance name
//

int Widget::constructClassName(char *buffer)
{
	// tell parent to do its job first

	if(parent)
	{
		parent->constructClassName(buffer);
		int len = strlen(buffer);
		buffer[len] = '.';
		buffer[len + 1] = 0;
	}

	// add our typeName

    strcat(buffer, typeName());
	return(0);
}

int Widget::constructInstanceName(char *buffer)
{
	// tell parent to do its job first

    if(parent)
	{
		parent->constructInstanceName(buffer);
		int len = strlen(buffer);
		buffer[len] = '.';
		buffer[len + 1] = 0;
	}

	// if we have a name, add it, else add "!NONAME!" (so we won't match)

	if(name)
		strcat(buffer, name);
	else
		strcat(buffer, "!NONAME!");
	return(0);
}


