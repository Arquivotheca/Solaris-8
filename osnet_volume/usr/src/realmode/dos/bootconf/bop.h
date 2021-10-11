/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * bop.h -- public definitions for bootops routines
 */

#ifndef	_BOP_H
#define	_BOP_H

#ident "@(#)bop.h   1.8   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public bop function prototypes
 */

void init_bop();
void out_bop(char *buf);
char *in_bop(char *buf, u_short len);
void file_bop(char *fname);

#ifdef	__cplusplus
}
#endif

#endif	/* _BOP_H */
