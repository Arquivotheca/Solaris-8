/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident   "@(#)ghd_gcmd.c 1.4	97/07/08 SMI"

#include <sys/dada/adapters/ghd/ghd.h>

/*
 * Round up all allocations so that we can guarantee
 * long-long alignment.  This is the same alignment
 * provided by kmem_alloc().
 */
#define	ROUNDUP(x)	(((x) + 0x07) & ~0x07)

/*
 * Private wrapper for gcmd_t
 */
typedef struct gw_gcmd_and_length {
	gcmd_t	gcmd;		/* this must be first */
	int	glen;		/* length includes HBA private area */
}gw_t;

/*
 * round up the size so the HBA private area is on a 8 byte boundary
 */
#define	GW_PADDED_LENGTH	ROUNDUP(sizeof (gw_t))

typedef struct gcmd_padded_wrapper {
	union {
		gw_t	gw;
		char	gw_pad[GW_PADDED_LENGTH];

	} gwrap;
} gwrap_t;

/*
 * Allocate a gcmd_t wrapper and HBA private area
 */

gcmd_t *
ghd_gcmd_alloc(gtgt_t	*gtgtp,
		int	 ccblen,
		int	 sleep)
{
	gwrap_t	*gwp;
	gcmd_t	*gcmdp;
	int	 gwrap_len;

	ccblen = ROUNDUP(ccblen);
	gwrap_len = sizeof (gwrap_t) + ccblen;
	gwp = kmem_zalloc(gwrap_len, (sleep ? KM_SLEEP : KM_NOSLEEP));
	if (gwp == NULL) {
		ASSERT(sleep == FALSE);
		return (NULL);
	}

	/* save the total length for the free function */
	gwp->gwrap.gw.glen = gwrap_len;

	/*
	 * save the ptr to HBA private area and initialize all
	 * the gcmd_t members and save
	 */
	gcmdp = &gwp->gwrap.gw.gcmd;
	GHD_GCMD_INIT(gcmdp, (void *)(gwp + 1), gtgtp);
	return (gcmdp);
}



/*
 * Free the gcmd_t wrapper and HBA private area
 */

void
ghd_gcmd_free(gcmd_t *gcmdp)
{
	kmem_free(gcmdp, ((gw_t *)gcmdp)->glen);
}
