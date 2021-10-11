/*
 * Copyright (c) 1995,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_JTAG_H
#define	_SYS_JTAG_H

#pragma ident	"@(#)jtag.h	1.9	99/03/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

enum board_type jtag_get_board_type(volatile uint_t *, sysc_cfga_stat_t *);
int jtag_powerdown_board(volatile uint_t *, int, enum board_type,
	uint_t *, uint_t *, int);
int jtag_get_board_info(volatile uint_t *, struct sysc_cfga_stat *);
int jtag_init_disk_board(volatile uint_t *, int, uint_t *, uint_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_JTAG_H */
