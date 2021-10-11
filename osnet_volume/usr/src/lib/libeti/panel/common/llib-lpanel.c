/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)llib-lpanel.c	1.4	97/07/11 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "panel.h"

int
bottom_panel(PANEL *panel)
{
	return OK;
}

int
hide_panel(PANEL *panel)
{
	return OK;
}

int
del_panel(PANEL *panel)
{
	return OK;
}

WINDOW
*panel_window(PANEL *panel)
{
	return (WINDOW *) 0;
}

char
*panel_userptr(PANEL *panel)
{
	return (char *)0;
}

int
set_panel_userptr(PANEL *panel, char *ptr)
{
	return OK;
}

PANEL
*panel_above(PANEL *panel)
{
	return (PANEL *) 0;
}

PANEL
*panel_below(PANEL *panel)
{
	return (PANEL *) 0;
}

int
panel_hidden(PANEL *panel)
{
	return TRUE;
}

int
move_panel(PANEL *panel, int starty, int startx)
{
	return OK;
}

PANEL
*new_panel(WINDOW *window)
{
	return (PANEL *) 0;
}

int
show_panel(PANEL *panel)
{
	return OK;
}

int
replace_panel(PANEL *panel, WINDOW *window)
{
	return OK;
}

int
top_panel(PANEL *panel)
{
	return OK;
}

void
update_panels(void)
{}
