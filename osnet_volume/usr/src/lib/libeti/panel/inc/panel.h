/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_PANEL_H
#define	_PANEL_H

#pragma ident	"@(#)panel.h	1.4	93/03/04 SMI"	/* SVr4.0 1.4	*/

#include <curses.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct _obscured_list
	{
		struct PANEL	*panel_p;
		int	start, end;
		struct _obscured_list	*next;
	} _obscured_list;

typedef struct PANEL
	{
		WINDOW	*win;
		int	wstarty;
		int	wendy;
		int	wstartx;
		int	wendx;
		struct _obscured_list	*obscured;
		struct PANEL	*below, *above;
		char	*user;
	} PANEL;

#ifdef __STDC__

extern PANEL *new_panel(WINDOW *);
extern int del_panel(PANEL *);
extern int hide_panel(PANEL *);
extern int show_panel(PANEL *);
extern int panel_hidden(PANEL *);
extern int move_panel(PANEL *, int, int);
extern int replace_panel(PANEL *, WINDOW *);
extern int top_panel(PANEL *);
extern int bottom_panel(PANEL *);
extern void update_panels(void);
extern WINDOW *panel_window(PANEL *);
extern int set_panel_userptr(PANEL *, char *);
extern char *panel_userptr(PANEL *);
extern PANEL *panel_above(PANEL *);
extern PANEL *panel_below(PANEL *);

#else	/* old style extern's */

extern PANEL *new_panel();
extern int del_panel();
extern int hide_panel();
extern int show_panel();
extern int panel_hidden();
extern int move_panel();
extern int replace_panel();
extern int top_panel();
extern int bottom_panel();
extern void update_panels();
extern WINDOW *panel_window();
extern int set_panel_userptr();
extern char *panel_userptr();
extern PANEL *panel_above();
extern PANEL *panel_below();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _PANEL_H */
