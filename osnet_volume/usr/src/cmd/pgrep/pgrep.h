/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PGREP_H
#define	_PGREP_H

#pragma ident	"@(#)pgrep.h	1.1	97/12/08 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	E_MATCH		0	/* Exit status for match */
#define	E_NOMATCH	1	/* Exit status for no match */
#define	E_USAGE		2	/* Exit status for usage error */
#define	E_ERROR		3	/* Exit status for other error */

typedef int (*opt_cb_t)(char, char *);

typedef struct optdesc {
	ushort_t o_opts;	/* Flags indicating how to process option */
	ushort_t o_bits;	/* Bits to set or clear in *o_ptr */
	opt_cb_t o_func;	/* Function to call */
	void *o_ptr;		/* Address of flags or string */
} optdesc_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _PGREP_H */
