#pragma ident "@(#)stringid.cc   1.7     93/01/08 SMI"

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
//	compile common strings to integer IDs
//
//	$RCSfile: stringid.cc $ $Revision: 1.2 $ $Date: 1992/12/29 22:03:01 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdlib.h>
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif

#endif	// PRE_COMPILED_HEADERS


extern StringTable *CUI_Stringtab;	// system StringTable


//
//	these strings must match the order of the enums in stringid.h!
//

static char *strings[] =
{
	"",     // must put something in the array for nullStringId

	//	color values
	//	WARNING! these must be first, and in this order,
	//			 (they map directly to curses color-values + 1)

    "black",
	"blue",
    "green",
    "cyan",
	"red",
	"magenta",
	"yellow",
    "white",

	// resource names & values

	"activeColor",
	"adjust",
	"alignCaptions",
	"alignment",
	"ancestorSensitive",
	"borderColor",
	"borderWidth",
	"bottom",
	"bright",                   // remove this!
	"cancelKey",
	"center",
	"col",
	"colorBackground",
	"colorBlink",
	"colorForeground",
	"default",
	"delKey",
	"destroyCallback",
	"disabledColor",
    "downKey",
	"endKey",
	"exclusive",
	"exitCallback",
	"false",
	"file",
	"fixedCols",
	"fixedRows",
	"focusCallback",
	"granularity",
	"hPad",
	"hScroll",
	"hSpace",
	"height",
	"help",
	"helpCallback",
	"helpKey",
	"homeKey",
	"indexCallback",
	"insKey",
	"interiorColor",
	"label",
	"layoutType",
	"left",
	"leftJustify",
	"leftKey",
	"mappedWhenManaged",
	"max",
	"maxLabel",
	"measure",
	"menu",
	"min",
	"minLabel",
	"monoAttrib",
	"monoBlink",
	"no",
	"noneSet",
	"normal",
	"normalColor",
	"off",
	"on",
	"parentWindow",             // internal use only!
	"pgDownKey",
	"pgUpKey",
	"popdownCallback",
	"popdownOnSelect",
	"popupCallback",
	"popupOnSelect",
	"position",
	"refreshKey",
	"reverse",
	"right",
	"rightJustify",
	"rightKey",
	"row",
	"select",
	"selectCallback",
	"sensitive",
	"set",
	"setColor",
	"sliderMax",
	"sliderMin",
	"sliderMoved",
	"sliderValue",
	"space",
	"stabKey",
	"string",
	"tabKey",
	"textFunc",
	"ticks",
	"title",
	"titleColor",
	"toggle",
	"top",
	"true",
	"underline",
	"unfocusCallback",
	"unselect",
	"unselectCallback",
	"upKey",
	"userPointer",
	"vPad",
	"vScroll",
	"vSpace",
	"verification",
	"verifyCallback",
	"width",
	"x",
	"y",
	"yes",

	// we add new stuff at the end rather than re-sorting
	// to avoid recompiling the world when we add a new string;
	// clean this up later

	"colorBold",
	"monoBold",
	"CUI_normalColor",
	"CUI_reverseColor",
	"CUI_boldColor",
	"CUI_underlineColor",
	"CUI_blinkColor",
	"CUI_monoColor",
	"visible",
	"footerMessage",
	"killLineKey",
	"arrows",

    // that's all folks

    0
};


//
//	compile our known strings (tell compiler these are unique,
//	so we can save a little time by not doing a lookup)
//

int CUI_compileStrings(void)
{
	for(int i = 0; strings[i]; i++)
		CUI_Stringtab->compile(strings[i], TRUE);
	return(0);
}

