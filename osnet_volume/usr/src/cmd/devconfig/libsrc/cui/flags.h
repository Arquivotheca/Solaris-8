#pragma ident "@(#)flags.h   1.4     93/01/08 SMI"

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

/*
 *	 flag definitions
 */

#ifndef _FLAGS_H
#define _FLAGS_H


#define CUI_REALIZED			   0x00000001L	// is realized
#define CUI_BORDER				   0x00000002L	// has border
#define CUI_VSCROLL 			   0x00000004L	// scrolls vertically
#define CUI_HSCROLL 			   0x00000008L	// scrolls horizontally
#define CUI_SYSMENU 			   0x00000010L	// has system menu
#define CUI_MOVEABLE			   0x00000020L	// is moveable
#define CUI_RESIZEABLE			   0x00000040L	// is resizeable
#define CUI_USE_ARROWS			   0x00000080L	// arrow-keys are active
#define CUI_UNUSED1 			   0x00000100L	// unused
#define CUI_SUSPENDED              0x00000200L  // is suspended
#define CUI_ADJUST_RIGHT		   0x00000400L	// adjust from right
#define CUI_ADJUST_CENTER		   0x00000800L	// adjust from center
#define CUI_MULTI                  0x00001000L  // multi-select
#define CUI_TOGGLE				   0x00002000L	// toggle-type
#define CUI_CONSTRAINED 		   0x00004000L	// (used by TextPanel)
#define CUI_NODISPLAY			   0x00008000L	// is not displayable
#define CUI_HIDE_ON_EXIT		   0x00010000L	// hide on exit
#define CUI_NONE_SET			   0x00020000L	// OK for no exclusive to be set
#define CUI_HLINE				   0x00040000L	// delineated by hlines
#define CUI_ADJUSTED_COL		   0x00080000L	// col has been adjusted
#define CUI_SENSITIVE			   0x00100000L	// accepting input?
#define CUI_DEFAULT 			   0x00200000L	// is default
#define CUI_MAPPED				   0x00400000L	// map when realized
#define CUI_LAYOUT				   0x00800000L	// layout policy
#define CUI_ENTERED_TEXT		   CUI_LAYOUT	// entered text field (overload)
#define CUI_ALIGN_CAPTIONS		   0x01000000L	// align captions
#define CUI_CENTERED			   0x02001000L	// centered
#define CUI_VISIBLE 			   0x04000000L	// are we visible?
#define CUI_REFRESH 			   0x08000000L	// are we refreshing this?
#define CUI_POPUP				   0x10000000L	// is a popup
#define CUI_MANAGED 			   0x20000000L	// geometry managed?
#define CUI_INITIALIZED 		   0x40000000L	// generic initialized flag
#define CUI_UNUSED4 			   0x80000000L	// unused

// definitions for some of the more obscure flag values

#define CUI_FIXEDCOLS_FLAG	(flags & CUI_LAYOUT)
#define CUI_FIXEDROWS_FLAG	(!(flags & CUI_LAYOUT))
#define CUI_BORDERED		(CUI_BORDER | CUI_HLINE)


#endif /* _FLAGS_H */



