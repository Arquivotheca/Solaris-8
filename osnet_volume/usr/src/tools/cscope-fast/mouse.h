/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mouse.h	1.1	99/01/11 SMI"

typedef	struct {
	int	button;
	int	percent;
	int	x1;
	int	y1;
	int	x2;
	int	y2;
} MOUSEEVENT;

typedef	struct {
	char	*text;
	char	*value;
} MOUSEMENU;

typedef	enum {
	NONE,		/* must be first value */
	EMACSTERM,
	MYX,
	PC7300
} MOUSETYPE;

extern	MOUSETYPE mouse;

extern int mouseselection(MOUSEEVENT *p, int offset, int maxselection);
extern void cleanupmouse(void);
extern void drawscrollbar(int top, int bot, int total);
extern int getcoordinate(void);
extern MOUSEEVENT *getmouseevent(void);
extern int getpercent(void);
extern void initmouse(void);
extern int labelarea(char *s);
extern void reinitmouse(void);
extern void downloadmenu(MOUSEMENU *menu);
