/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_REFSTR_H
#define	_SYS_REFSTR_H

#pragma ident	"@(#)refstr.h	1.1	99/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Strings with reference counts.
 * The 'struct refstr' definition is private to the refstr.c module.
 */

typedef struct refstr refstr_t;

#if	defined(_KERNEL)

refstr_t	*refstr_alloc(const char *);
const char	*refstr_value(refstr_t *);
void		refstr_hold(refstr_t *);
void		refstr_rele(refstr_t *);

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REFSTR_H */
