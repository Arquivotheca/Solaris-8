/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ifndef _SYS_NVSIMM_H
#define	_SYS_NVSIMM_H

#pragma ident	"@(#)nvsimm.h	1.2	93/03/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

/*
 * Struct used to record async fault handlers that drivers have registered
 * (and address ranges of nvsimm memory for bustype calculations)
 */
struct simmslot {
	struct simmslot *ss_next;	/* next chunk in list */
	pa_t ss_addr_lo;		/* lowest address this chunk */
	pa_t ss_addr_hi;		/* highest address this chunk */
	struct dev_info *ss_dip;	/* driver */
	void *ss_arg;			/* arg to pass to ss_func */
	int (*ss_func)(void *, void *);	/* fault handler */
};

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_NVSIMM_H */
