/*
 * Copyright (c) 1991-1995,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DVMA_H
#define	_SYS_DVMA_H

#pragma ident	"@(#)dvma.h	1.3	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DVMAO_REV	1

struct dvma_ops  {
#ifdef __STDC__
	int dvmaops_rev;		/* rev of this structure */
	void (*dvma_kaddr_load)(ddi_dma_handle_t h, caddr_t a,
			    uint_t len, uint_t index, ddi_dma_cookie_t *cp);
	void (*dvma_unload)(ddi_dma_handle_t h, uint_t objindex,
			    uint_t view);
	void (*dvma_sync)(ddi_dma_handle_t h, uint_t objindex,
			    uint_t view);
#else /* __STDC__ */
	int dvmaops_rev;
	void (*dvma_kaddr_load)();
	void (*dvma_unload)();
	void (*dvma_sync)();
#endif /* __STDC__ */
};

struct	fast_dvma	{
	caddr_t softsp;
	uint_t *pagecnt;
	unsigned long long  *phys_sync_flag;
	int *sync_flag;
	struct dvma_ops *ops;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DVMA_H */
