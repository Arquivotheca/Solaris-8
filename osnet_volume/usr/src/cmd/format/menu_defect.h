
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_MENU_DEFECT_H
#define	_MENU_DEFECT_H

#pragma ident	"@(#)menu_defect.h	1.4	93/03/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI C compilers
 */
int	d_restore(void);
int	d_original(void);
int	d_extract(void);
int	d_add(void);
int	d_delete(void);
int	d_print(void);
int	d_dump(void);
int	d_load(void);
int	d_commit(void);
int	do_commit(void);
int	commit_list(void);
int	d_create(void);
int	d_primary(void);
int	d_grown(void);
int	d_both(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_DEFECT_H */
