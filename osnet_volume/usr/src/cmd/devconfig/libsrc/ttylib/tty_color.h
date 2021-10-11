#ifndef TTY_COLOR_H
#define	TTY_COLOR_H
/*
 * Copyright (c) 1993 Sun Microsystems, Inc.  All Rights Reserved. Sun
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

#pragma	ident	"@(#)tty_color.h 1.2 93/11/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Standard items to be hi-lited or shown in color:
 *
 * 	Screen Title
 * 	Screen Footer
 * 	Normal text
 * 	Focus/Cursor
 * 	Reverse Video Focus/Cursor (for status msg &c).
 *
 * foreground/background color pairings for each different item are
 * maintained.  Color pairings are accessed by specifying one of the
 * following hi-lite `indexes' to the wcolor_* functions.
 */

typedef enum hilite {
	TITLE = 0,
	FOOTER = 1,
	BODY = 2,
	CURSOR = 3,
	CURSOR_INV = 4,
	NORMAL = 5
} HiLite_t;

/*
 * Public functions for hi-lite or color attributes
 *
 * wcolor_set_bkgd(WINDOW *w, HiLite_t index)
 * 	sets fg/bg colors on window to indexed pair
 *
 *	any window created should have its fg/bg colors set.
 *
 * wcolor_on(WINDOW *w, HiLite_t index)
 *	sets current fg/bg drawing colors for window.
 *	any subsequent characters drawn on window will be drawn
 *	in fg/bg colors.
 *
 * wcolor_off(WINDOW *w, HiLite_t index)
 *	turns off fg/bg colors for window.
 *
 *	To draw text in color, bracket draw operations with
 * 	wcolor_on()/w_color_off()
 *
 * wfocus_on(WINDOW *w, int r, int c, char *s)
 *	draws `s' in window at position r, c in default
 *	cursor color.
 *
 * wfocus_off(WINDOW *w, int r, int c, char *s)
 *	draws `s' in window at position r, c in default
 *	window color.
 *
 *
 * 	To move `focus' around use wfocus_on()/wfocus_off()
 *	to draw item with current focus.
 */
extern void	wcolor_init(void);
extern void	wcolor_set_bkgd(WINDOW *, HiLite_t);
extern void	wcolor_on(WINDOW *, HiLite_t);
extern void	wcolor_off(WINDOW *, HiLite_t);
extern void	wfocus_on(WINDOW *, int, int, char *);
extern void	wfocus_off(WINDOW *, int, int, char *);

#ifdef __cplusplus
}
#endif

#endif	/* !TTY_COLORS_H */
