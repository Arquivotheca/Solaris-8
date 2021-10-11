#pragma ident "@(#)stringid.h   1.8     98/10/22 SMI"

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

/*===========================================================================*
 *	IDs for compiled strings
 *
 *	$RCSfile: stringid.h $ $Revision: 1.2 $ $Date: 1992/12/29 22:03:01 $
 *===========================================================================*/

#ifndef _STRINGID_H
#define _STRINGID_H


/*
 *	these enums must match the order in which strings are listed in stringid.cc
 */

typedef enum
{
	nullStringId,

	/*
	 *	color values
	 *	WARNING! these must be first, and in this order,
	 *			 (they map directly to curses color-values + 1)
	 */

    blackId,
	blueId,
    greenId,
    cyanId,
	redId,
	magentaId,
	yellowId,
	whiteId,

	/* resource names & values */

	activeColorId,
	adjustId,
	alignCaptionsId,
	alignmentId,
	ancestorSensitiveId,
	borderColorId,
	borderWidthId,
	bottomId,
	brightId,					/* remove this! */
	cancelKeyId,
	centerId,
	colId,
	colorBackgroundId,
    colorBlinkId,
	colorForegroundId,
    defaultId,
	delKeyId,
	destroyCallbackId,
	disabledColorId,
	downKeyId,
	endKeyId,
	exclusiveId,
	exitCallbackId,
	falseId,
	fileId,
	fixedColsId,
	fixedRowsId,
	focusCallbackId,
	granularityId,
	hPadId,
	hScrollId,
	hSpaceId,
	heightId,
	helpId,
    helpCallbackId,
	helpKeyId,
	homeKeyId,
	indexCallbackId,
	insKeyId,
	interiorColorId,
	labelId,
	layoutTypeId,
	leftId,
	leftJustifyId,
	leftKeyId,
	mappedWhenManagedId,
	maxId,
	maxLabelId,
	measureId,
	menuId,
	minId,
	minLabelId,
	monoAttribId,
	monoBlinkId,
	noId,
	noneSetId,
	normalId,
    normalColorId,
	offId,
	onId,
	parentWindowId, 			/* internal use only! */
	pgDownKeyId,
	pgUpKeyId,
	popdownCallbackId,
	popdownOnSelectId,
	popupCallbackId,
	popupOnSelectId,
	positionId,
	refreshKeyId,
	reverseId,
	rightId,
	rightJustifyId,
	rightKeyId,
	rowId,
	selectId,
    selectCallbackId,
	sensitiveId,
	setId,
    setColorId,
	sliderMaxId,
	sliderMinId,
	sliderMovedId,
	sliderValueId,
	spaceId,
	stabKeyId,
	stringId,
	tabKeyId,
	textFuncId,
	ticksId,
	titleId,
    titleColorId,
	toggleId,
	topId,
	trueId,
	underlineId,
	unfocusCallbackId,
	unselectId,
    unselectCallbackId,
	upKeyId,
	userPointerId,
	vPadId,
	vScrollId,
	vSpaceId,
	verificationId,
	verifyCallbackId,
	widthId,
	xId,
	yId,
	yesId,

	/*
	 *	we add new stuff at the end rather than re-sorting
	 *	to avoid recompiling the world when we add a new string;
	 *	clean this up later
	 */

	colorBoldId,
	monoBoldId,
	CUI_normalColorId,
	CUI_reverseColorId,
	CUI_boldColorId,
	CUI_underlineColorId,
	CUI_blinkColorId,
	CUI_monoColorId,
	visibleId,
	footerMessageId,
	killLineKeyId,
	arrowsId

    /* that's all folks */

}
CUI_StringId;

#endif /* _STRINGID_H */

