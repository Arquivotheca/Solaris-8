/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _INET_ND_H
#define	_INET_ND_H

#pragma ident	"@(#)nd.h	1.12	98/12/16 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	ND_BASE		('N' << 8)	/* base */
#define	ND_GET		(ND_BASE + 0)	/* Get a value */
#define	ND_SET		(ND_BASE + 1)	/* Set a value */

#if defined(_KERNEL) && defined(__STDC__)
/* Named dispatch table entry */
typedef struct  nde_s {
	char    *nde_name;
	pfi_t   nde_get_pfi;
	pfi_t   nde_set_pfi;
	caddr_t nde_data;
} NDE;

/* Name dispatch table */
typedef struct  nd_s {
	int	nd_free_count;	/* number of unused nd table entries */
	int	nd_size;	/* size (in bytes) of current table */
	NDE	*nd_tbl;	/* pointer to table in heap */
} ND;

#define	NDE_ALLOC_COUNT 4
#define	NDE_ALLOC_SIZE  (sizeof (NDE) * NDE_ALLOC_COUNT)

extern void		nd_free(caddr_t *);
extern int		nd_getset(queue_t *, caddr_t, MBLKP);
extern int		nd_get_default(queue_t *, MBLKP, caddr_t);
extern int		nd_get_long(queue_t *, MBLKP, caddr_t);
extern int		nd_get_names(queue_t *, MBLKP, caddr_t);
extern boolean_t 	nd_load(caddr_t *, char *, pfi_t, pfi_t, caddr_t);
extern void		nd_unload(caddr_t *, char *);
extern int		nd_set_default(queue_t *, MBLKP, char *, caddr_t);
extern int		nd_set_long(queue_t *, MBLKP, char *, caddr_t);
extern void		nd_free(caddr_t *);
extern int		nd_getset(queue_t *, caddr_t, MBLKP);
/*
 * This routine may be used as the get dispatch routine in nd tables
 * for long variables.  To use this routine instead of a module
 * specific routine, call nd_load as
 *	nd_load(&nd_ptr, "name", nd_get_long, set_pfi, &long_variable)
 * The name of the variable followed by a space and the value of the
 * variable will be printed in response to a get_status call.
 */
extern int		nd_get_long(queue_t *, MBLKP, caddr_t);
/*
 * Load 'name' into the named dispatch table pointed to by 'ndp'.
 * 'ndp' should be the address of a char pointer cell.  If the table
 * does not exist (*ndp == 0), a new table is allocated and 'ndp'
 * is stuffed.  If there is not enough space in the table for a new
 * entry, more space is allocated.
 */
extern boolean_t	nd_load(caddr_t *, char *, pfi_t, pfi_t, caddr_t);
extern int		nd_set_long(queue_t *, MBLKP, char *, caddr_t);
#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_ND_H */
