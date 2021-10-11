/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef	_KSTAT_H
#define	_KSTAT_H

#pragma ident	"@(#)kstat.h	1.4	93/06/30 SMI"

#include <sys/types.h>
#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * kstat_open() returns a pointer to a kstat_ctl_t.
 * This is used for subsequent libkstat operations.
 */
typedef struct kstat_ctl {
	kid_t	kc_chain_id;	/* current kstat chain ID	*/
	kstat_t	*kc_chain;	/* pointer to kstat chain	*/
	int	kc_kd;		/* /dev/kstat descriptor	*/
} kstat_ctl_t;

#ifdef	__STDC__
extern	kstat_ctl_t	*kstat_open(void);
extern	int		kstat_close(kstat_ctl_t *);
extern	kid_t		kstat_read(kstat_ctl_t *, kstat_t *, void *);
extern	kid_t		kstat_write(kstat_ctl_t *, kstat_t *, void *);
extern	kid_t		kstat_chain_update(kstat_ctl_t *);
extern	kstat_t		*kstat_lookup(kstat_ctl_t *, char *, int, char *);
extern	void		*kstat_data_lookup(kstat_t *, char *);
#else
extern	kstat_ctl_t	*kstat_open();
extern	int		kstat_close();
extern	kid_t		kstat_read();
extern	kid_t		kstat_write();
extern	kid_t		kstat_chain_update();
extern	kstat_t		*kstat_lookup();
extern	void		*kstat_data_lookup();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _KSTAT_H */
