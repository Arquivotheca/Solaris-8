/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#ifndef _PRIVATE_H
#define	_PRIVATE_H

#pragma ident	"@(#)private.h	1.5	97/09/17 SMI"	/* SVr4.0 1.1	*/

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef PANELS_H
#define	PANELS_H

#include "panel.h"

#define	_panels_intersect(p1, p2)	(!((p1)->wstarty > (p2)->wendy || \
					(p1)->wendy < (p2)->wstarty || \
					(p1)->wstartx > (p2)->wendx || \
					(p1)->wendx < (p2)->wstartx))

extern	PANEL	*_Bottom_panel;
extern	PANEL	*_Top_panel;

extern	int	_Panel_cnt;

extern void _intersect_panel(PANEL *);
extern void _remove_overlap(PANEL *);
extern int _alloc_overlap(int);
extern void _free_overlap(_obscured_list *);
extern _obscured_list *_unlink_obs(PANEL *, PANEL *);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _PRIVATE_H */
