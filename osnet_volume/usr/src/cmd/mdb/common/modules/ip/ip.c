/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip.c	1.2	99/11/19 SMI"

#include <sys/types.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <inet/mib2.h>
#include <inet/common.h>
#include <inet/ip.h>
#include <inet/ip_ire.h>
#include <inet/ip6.h>

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

int
ire_walk_init(mdb_walk_state_t *wsp)
{
	if (mdb_layered_walk("ire_cache", wsp) == -1) {
		mdb_warn("can't walk 'ire_cache'");
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

int
ire_walk_step(mdb_walk_state_t *wsp)
{
	ire_t ire;

	if (mdb_vread(&ire, sizeof (ire), wsp->walk_addr) == -1) {
		mdb_warn("can't read ire at %p", wsp->walk_addr);
		return (WALK_ERR);
	}

	return (wsp->walk_callback(wsp->walk_addr, &ire, wsp->walk_cbdata));
}

static int
ire_format(uintptr_t addr, const ire_t *irep, uint_t *qfmt)
{
	if (*qfmt != 0) {
		mdb_printf("%?p %?p %?p\n",
		    addr, irep->ire_rfq, irep->ire_stq);
	} else {
		mdb_printf("%?p %-15I %-15I\n",
		    addr, irep->ire_src_addr, irep->ire_addr);
	}

	return (WALK_NEXT);
}

int
ire(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t qfmt = FALSE;
	ire_t ire;

	if (mdb_getopts(argc, argv,
	    'q', MDB_OPT_SETBITS, TRUE, &qfmt, NULL) != argc)
		return (DCMD_USAGE);

	if ((flags & DCMD_LOOPFIRST) || !(flags & DCMD_LOOP)) {
		if (qfmt) {
			mdb_printf("%<u>%?s %?s %?s%</u>\n",
			    "ADDR", "RECVQ", "SENDQ");
		} else {
			mdb_printf("%<u>%?s %15s %15s%</u>\n",
			    "ADDR", "SRC", "DST");
		}
	}

	if (flags & DCMD_ADDRSPEC) {
		(void) mdb_vread(&ire, sizeof (ire_t), addr);
		(void) ire_format(addr, &ire, &qfmt);
	} else if (mdb_walk("ire", (mdb_walk_cb_t)ire_format, &qfmt) == -1) {
		mdb_warn("failed to walk ire table");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

static size_t
mi_osize(const queue_t *q)
{
	/*
	 * Grrr ...
	 */
	struct mi_swill {
		size_t mi_nbytes;
		uintptr_t mi_o_next;
		uintptr_t mi_o_prev;
		dev_t mi_o_dev;
	} m;

	if (mdb_vread(&m, sizeof (m), (uintptr_t)q->q_ptr -
	    sizeof (m)) == sizeof (m))
		return (m.mi_nbytes - sizeof (m));

	return (0);
}

static int
ipc_ire_match(uintptr_t addr, const ire_t *ire, uintptr_t *ire_addr_p)
{
	ipaddr_t dst = (ipaddr_t)(*ire_addr_p);

	if ((ire->ire_addr & ire->ire_mask) == dst) {
		*ire_addr_p = addr;
		return (1);
	}

	return (0);
}

static int
ipc_to_ill(uintptr_t ipc_addr, ill_t *illp)
{
	uintptr_t ireaddr;	/* Address of Internet Route Entry */
	ipc_t ipc;		/* IP per-client structure */
	ire_t ire;		/* IP route entry structure */
	ipif_t ipif;		/* IP interface structure */

	if (mdb_vread(&ipc, sizeof (ipc), ipc_addr) != sizeof (ipc))
		return (-1); /* Failed to read ipc_t */

	if (ipc.ipc_faddr == 0)
		return (-1); /* Not connected */

	ireaddr = (uintptr_t)ipc.ipc_faddr;
	(void) mdb_walk("ire", (mdb_walk_cb_t)ipc_ire_match, &ireaddr);

	if (ireaddr == ipc.ipc_faddr)
		return (-1); /* No route to destination */

	if (mdb_vread(&ire, sizeof (ire), ireaddr) == sizeof (ire) &&
	    mdb_vread(&ipif, sizeof (ipif), (uintptr_t)ire.ire_ipif) ==
	    sizeof (ipif) && mdb_vread(illp, sizeof (ill_t), (uintptr_t)
	    ipif.ipif_ill) == sizeof (ill_t))
		return (0); /* Success */

	return (-1); /* Read failed along pointer chain */
}

static void
ip_ipc_qinfo(const queue_t *q, char *buf, size_t nbytes)
{
	ipc_t ipc;

	if (mdb_vread(&ipc, sizeof (ipc),
	    (uintptr_t)q->q_ptr) == sizeof (ipc)) {
		(void) mdb_snprintf(buf, nbytes, "laddr: %I\nraddr: %I",
		    ipc.ipc_laddr, ipc.ipc_faddr);
	}
}

static void
ip_ill_qinfo(const queue_t *q, char *buf, size_t nbytes)
{
	char name[32];
	ill_t ill;

	if (mdb_vread(&ill, sizeof (ill),
	    (uintptr_t)q->q_ptr) == sizeof (ill) &&
	    mdb_readstr(name, sizeof (name), (uintptr_t)ill.ill_name) > 0)
		(void) mdb_snprintf(buf, nbytes, "if: %s", name);
}

void
ip_qinfo(const queue_t *q, char *buf, size_t nbytes)
{
	size_t size = mi_osize(q);

	if (size == sizeof (ill_t))
		ip_ill_qinfo(q, buf, nbytes);
	else if (size == sizeof (ipc_t))
		ip_ipc_qinfo(q, buf, nbytes);
}

uintptr_t
ip_rnext(const queue_t *q)
{
	size_t size = mi_osize(q);
	ill_t ill;

	if (size == sizeof (ill_t) && mdb_vread(&ill, sizeof (ill),
	    (uintptr_t)q->q_ptr) == sizeof (ill))
		return ((uintptr_t)ill.ill_rq);

	if (size == sizeof (ipc_t) &&
	    ipc_to_ill((uintptr_t)q->q_ptr, &ill) == 0)
		return ((uintptr_t)ill.ill_rq);

	return (NULL);
}

uintptr_t
ip_wnext(const queue_t *q)
{
	size_t size = mi_osize(q);
	ill_t ill;

	if (size == sizeof (ill_t) && mdb_vread(&ill, sizeof (ill),
	    (uintptr_t)q->q_ptr) == sizeof (ill))
		return ((uintptr_t)ill.ill_wq);

	if (size == sizeof (ipc_t) &&
	    ipc_to_ill((uintptr_t)q->q_ptr, &ill) == 0)
		return ((uintptr_t)ill.ill_wq);

	return (NULL);
}

static const mdb_dcmd_t dcmds[] = {
	{ "ire", "[-q]", "display Internet Route Entry structures", ire },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "ire", "walk active ire_t structures",
		ire_walk_init, ire_walk_step, NULL },
	{ NULL }
};

static const mdb_qops_t ip_qops = { ip_qinfo, ip_rnext, ip_wnext };
static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	GElf_Sym sym;

	if (mdb_lookup_by_obj("ip", "winit", &sym) == 0)
		mdb_qops_install(&ip_qops, (uintptr_t)sym.st_value);

	return (&modinfo);
}

void
_mdb_fini(void)
{
	GElf_Sym sym;

	if (mdb_lookup_by_obj("ip", "winit", &sym) == 0)
		mdb_qops_remove(&ip_qops, (uintptr_t)sym.st_value);
}
