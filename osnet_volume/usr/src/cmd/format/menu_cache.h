
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 */

#ifndef	_MENU_CACHE_H
#define	_MENU_CACHE_H

#pragma ident	"@(#)menu_cache.h	1.1	99/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI
 */
int	c_cache(void);
int	ca_write_cache(void);
int	ca_read_cache(void);
int	ca_write_display(void);
int	ca_write_enable(void);
int	ca_write_disable(void);
int	ca_read_display(void);
int	ca_read_enable(void);
int	ca_read_disable(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_CACHE_H */
