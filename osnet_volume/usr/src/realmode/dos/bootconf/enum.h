/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * enum.h -- public definitions for enumerator routines
 */

#ifndef	_ENUM_H
#define	_ENUM_H

#ident "@(#)enum.h   1.19   98/04/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public enumerator function prototypes
 */

void run_enum(int enum_option);
/* definitions for enum_option */
#define	ENUM_ALL	0	/* do all parts of run_enum() */
#define	ENUM_TOP	1	/* do top half without device_probe() */
#define	ENUM_BOT	2	/* do bottom half without device_probe() */

void program_enum(Board *bp);
void unprogram_enum(Board *bp);
void init_enum();
void print_board_enum(Board *bp);

extern unsigned char Main_bus;
extern Board *Head_board;
extern Board *Head_prog;

#ifdef	__cplusplus
}
#endif

#endif	/* _ENUM_H */
