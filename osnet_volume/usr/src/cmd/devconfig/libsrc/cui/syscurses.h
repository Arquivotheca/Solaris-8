#pragma ident "@(#)syscurses.h   1.3     92/11/25 SMI"

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

#ifndef _CUI_SYSCURSES_H
#define _CUI_SYSCURSES_H

#include <curses.h>
#include <panel.h>
#include <menu.h>
#include <form.h>

// undefine curses and stdio macros so we can use them as method names

#ifdef move
#undef move
#endif
#ifdef putc
#undef putc
#endif
#ifdef clear
#undef clear
#endif
#ifdef refresh
#undef refresh
#endif

#endif // _CUI_SYSCURSES_H

