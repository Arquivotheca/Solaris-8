/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)logindmux.c	1.1	99/08/11 SMI"

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

/*
 * Grrr ... move this to an _impl.h file:
 */
struct tmx {
	unsigned	state; 		/* Driver state value */
	queue_t		*rdq;		/* Upper read queue */
	struct tmxl	*tmxl; 		/* Lower link for mux */
	struct	tmx	*nexttp; 	/* Next queue pointer */
	struct	tmx	*pair; 		/* Q for clone mux used in logind */
	minor_t		dev0;		/* minor device number */
	minor_t		dev1;		/* minor device number for clone */
	int		flag;		/* set 1 for M_DATA/M_FLUSH from ptm */
};

struct tmxl {
	int		muxid;		/* id of link */
	unsigned	ltype;		/* persistent or non-persistent link */
	queue_t		*muxq;		/* link to lower write queue of mux */
	struct tmx	*tmx;   	/* upper tmx queue of mux */
	queue_t		*peerq; 	/* lower peerq of mux pair */
};

void
logdmux_uqinfo(const queue_t *q, char *buf, size_t nbytes)
{
	struct tmx tmx, tmxp;
	struct tmxl tmxl, tmxlp;
	uintptr_t peer, lower, lower_peer;
	queue_t lq;

	/*
	 * First, get the pointer to our lower write queue.
	 */
	(void) mdb_vread(&tmx, sizeof (tmx), (uintptr_t)q->q_ptr);
	(void) mdb_vread(&tmxl, sizeof (tmxl), (uintptr_t)tmx.tmxl);
	lower = (uintptr_t)tmxl.muxq;
	lower_peer = (uintptr_t)tmxl.peerq;

	/*
	 * Now read in the lower's peer, grab his tmxl, and follow that
	 * to up to our peer.
	 */
	(void) mdb_vread(&lq, sizeof (lq), lower_peer);
	(void) mdb_vread(&tmxlp, sizeof (tmxlp), (uintptr_t)lq.q_ptr);
	(void) mdb_vread(&tmxp, sizeof (tmxp), (uintptr_t)tmxlp.tmx);
	peer = (uintptr_t)tmxp.rdq;

	(void) mdb_snprintf(buf, nbytes,
	    "peer       : %p\nlower      : %p", peer, lower);
}

void
logdmux_lqinfo(const queue_t *q, char *buf, size_t nbytes)
{
	struct tmx tmx, tmxp;
	struct tmxl tmxl, tmxlp;
	uintptr_t peer, upper;
	queue_t lq;

	(void) mdb_vread(&tmxl, sizeof (tmxl), (uintptr_t)q->q_ptr);
	peer = (uintptr_t)tmxl.peerq;

	(void) mdb_vread(&tmx, sizeof (tmx), (uintptr_t)tmxl.tmx);
	upper = (uintptr_t)tmx.rdq;

	/*
	 * Now follow get our peer's tmxl, and follow that to the
	 * tmx and the rdq.
	 */
	(void) mdb_vread(&lq, sizeof (lq), peer);
	(void) mdb_vread(&tmxlp, sizeof (tmxlp), (uintptr_t)lq.q_ptr);
	(void) mdb_vread(&tmxp, sizeof (tmxp), (uintptr_t)tmxlp.tmx);

	(void) mdb_snprintf(buf, nbytes,
	    "peer       : %p\nupper      : %p", peer, upper);
}

uintptr_t
logdmux_lrnext(const queue_t *q)
{
	struct tmx tmx;
	struct tmxl tmxl;

	(void) mdb_vread(&tmxl, sizeof (tmxl), (uintptr_t)q->q_ptr);
	(void) mdb_vread(&tmx, sizeof (tmx), (uintptr_t)tmxl.tmx);

	return ((uintptr_t)tmx.rdq);
}

uintptr_t
logdmux_uwnext(const queue_t *q)
{
	struct tmx tmx;
	struct tmxl tmxl;

	(void) mdb_vread(&tmx, sizeof (tmx), (uintptr_t)q->q_ptr);
	(void) mdb_vread(&tmxl, sizeof (tmxl), (uintptr_t)tmx.tmxl);

	return ((uintptr_t)tmxl.muxq);
}

static const mdb_qops_t logdmux_uqops = {
	logdmux_uqinfo, mdb_qrnext_default, logdmux_uwnext
};

static const mdb_qops_t logdmux_lqops = {
	logdmux_lqinfo, logdmux_lrnext, mdb_qwnext_default
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION };

const mdb_modinfo_t *
_mdb_init(void)
{
	GElf_Sym sym;

	if (mdb_lookup_by_obj("logindmux", "logdmuxuwinit", &sym) == 0)
		mdb_qops_install(&logdmux_uqops, (uintptr_t)sym.st_value);
	if (mdb_lookup_by_obj("logindmux", "logdmuxlwinit", &sym) == 0)
		mdb_qops_install(&logdmux_lqops, (uintptr_t)sym.st_value);

	return (&modinfo);
}

void
_mdb_fini(void)
{
	GElf_Sym sym;

	if (mdb_lookup_by_obj("logindmux", "logdmuxuwinit", &sym) == 0)
		mdb_qops_remove(&logdmux_uqops, (uintptr_t)sym.st_value);
	if (mdb_lookup_by_obj("logindmux", "logdmuxlwinit", &sym) == 0)
		mdb_qops_remove(&logdmux_lqops, (uintptr_t)sym.st_value);
}
