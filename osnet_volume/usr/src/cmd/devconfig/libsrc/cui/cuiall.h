#pragma ident "@(#)cuiall.h   1.3     92/11/25 SMI"

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

//============================== -*-Mode: c++;-*- =============================
//	master include file for CUI internals
//
//	$RCSfile: cuiall.h $ $Revision: 1.20 $ $Date: 1992/09/12 22:45:00 $
//=============================================================================

#ifndef _CUI_ALL_H
#define _CUI_ALL_H


#ifdef HCR_CPLUSPLUS
#define IOSTREAMH		// no iostreams, thanks very much!
#define __REGCMP_H		// regex stuff in ETI forms clashes with regcmp.h
#endif

class Widget;

#include "syscurses.h"      // curses
#include "cui.h"            // public definitions
#include "message.h"
#include "cuiproto.h"
#include <sys/stat.h>
#include <signal.h>

#ifdef MSDOS
#include <dos.h>	// for sleep
#else
#include <unistd.h>	// for sleep
#endif

#include "cursvmem.h"
#include "cuilib.h"
#include "flags.h"
#include "symtab.h"
#include "strtab.h"
#include "restab.h"
#include "widget.h"
#include "keybd.h"
#include "color.h"
#include "emanager.h"
#include "app.h"
#include "window.h"
#include "display.h"
#include "composite.h"
#include "control.h"
#include "area.h"
#include "caption.h"
#include "button.h"
#include "item.h"
#include "mitem.h"
#include "cuimenu.h"
#include "mbutton.h"
#include "xbutton.h"
#include "vcontrol.h"
#include "exclusive.h"
#include "text.h"
#include "shell.h"
#include "basewin.h"
#include "popwin.h"
#include "notice.h"
#include "litem.h"
#include "list.h"
#include "spothelp.h"
#include "abbrev.h"
#include "footer.h"
#include "separator.h"
#include "txtpanel.h"
#include "gauge.h"
#include "helpwin.h"
#include "topic.h"
#include "hyper.h"

#endif // _CUI_ALL_H

