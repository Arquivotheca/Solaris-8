
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_MENU_ANALYZE_H
#define	_MENU_ANALYZE_H

#pragma ident	"@(#)menu_analyze.h	1.5	95/02/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI
 */

int	a_read(void);
int	a_refresh(void);
int	a_test(void);
int	a_write(void);
int	a_compare(void);
int	a_verify(void);
int	a_print(void);
int	a_setup(void);
int	a_config(void);
int	a_purge(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_ANALYZE_H */
