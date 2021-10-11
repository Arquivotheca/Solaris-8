/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sfdr_mem.c	1.41	99/06/01 SMI"

/*
 * Starfire memory support routines for DR.
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/dditypes.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/sysmacros.h>
#include <sys/machsystm.h>
#include <sys/spitregs.h>
#include <sys/cpuvar.h>
#include <sys/promif.h>
#include <sys/starfire.h>	/* XXX - get rid of once hw stuff moved */
#include <sys/pda.h>
#include <sys/mem_cage.h>

#include <sys/dr.h>
#include <sys/sfdr.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>

extern struct memlist	*phys_install;
extern int		ecache_linesize;

#define	CPU_FLUSH_ECACHE_IL(addr, sz) \
{ \
	flush_ecache_il(addr, sz); \
}
/*
 * Copy-rename function must be leaf function, so we
 * define inline macro to be used when waiting for memory
 * controllers to drain.
 */
#define	SFHW_MEMCTRL_BUSYWAIT(iaddr, count, rv) \
{ \
	uint_t		_ivalue = 0; \
	register int	_i; \
	for (_i = 0; _i < (count); _i++) { \
		_ivalue = ldphysio_il(iaddr); \
		_ivalue &= STARFIRE_MC_IDLE_MASK; \
		if (_ivalue == STARFIRE_MC_IDLE_MASK) \
			break; \
	} \
	(rv) = (_ivalue == STARFIRE_MC_IDLE_MASK) ? 0 : -1; \
}

/*
 * NOTE: This code assumes only one memory unit per board in some places,
 *	 however it code be extended to multiple mem-units-per-board by
 *	 checking for an appropriate unit identifier and using it as the
 *	 index into sb_dev[..DR_NT_MEM...] in the sfdr_board_t structure.
 */
#define	SFDR_RENAME_MAXOP	(PAGESIZE / sizeof (sfdr_rename_script_t))

typedef struct {
	uint64_t	masr_addr;
	uint_t		masr;
	uint_t		_filler;
} sfdr_rename_script_t;

extern void		bcopy32_il(uint64_t, uint64_t);
extern void		flush_ecache_il(uint64_t physaddr, uint_t size);
extern uint_t		ldphysio_il(uint64_t physaddr);
extern void		stphysio_il(uint64_t physaddr, uint_t value);
extern void		sfdr_exec_script_il(sfdr_rename_script_t *rsp);

static int		sfdr_post_detach_mem_unit(dr_handle_t *hp,
					dnode_t nodeid);
static int		sfdr_reserve_mem_target(dr_handle_t *hp,
					int s_unit, dnode_t t_nodeid);
static int		sfdr_select_mem_target(dr_handle_t *hp,
					dnode_t s_nodeid);
static int		sfdr_prep_rename_script(dr_handle_t *hp,
					sfdr_rename_script_t rsbuffer[],
					uint64_t memregaddr[]);
static int		sfdr_copy_rename(dr_handle_t *hp,
					struct memlist *mlist,
					sfdr_rename_script_t *rsp,
					uint64_t memregaddr[]);
static void		_sfdr_copy_rename_end(void);
static void		sfdr_init_mem_unit_data(sfdr_board_t *sbp, int unit);
static struct memlist	*memlist_add_span(struct memlist *mlist,
					uint64_t base, uint64_t len);
static struct memlist	*memlist_del_span(struct memlist *mlist,
					uint64_t base, uint64_t len);
static void		memlist_coalesce(struct memlist *mlist);
static int		memlist_canfit(struct memlist *s_mlist,
					struct memlist *t_mlist);

/*ARGSUSED*/
struct memlist *
sfdr_get_memlist(dnode_t nodeid)
{
	int		rlen, rblks;
	int		unit, bd;
	struct memlist	*mlist;
	uint64_t	basepa, mlen;
	sfdr_board_t	*sbp;
	sfdr_mem_unit_t	*mp;
	struct sf_memunit_regspec	*rlist;
	static fn_t	f = "sfdr_get_memlist";

	PR_MEM("%s...\n", f);

	if (sfdr_get_devtype(NULL, nodeid) != DR_NT_MEM) {
		cmn_err(CE_WARN,
			"sfdr:%s: nodeid (0x%x) is not memory node",
			f, (uint_t)nodeid);
		return (NULL);
	}

	/*
	 * IMPORTANT: The given nodeid may NOT necessarily be a
	 *	member of the board represented by the given
	 *	handle.  This can occur in cases where we're
	 *	searching for the target of a copy-rename,
	 *	i.e. the source will be represented by the
	 *	handle and the nodeid may be a potential target.
	 */
	if ((bd = sfdr_get_board(nodeid)) < 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: no board number for nodeid (0x%x)",
			f, (uint_t)nodeid);
		return (NULL);
	}
	sbp = BSLOT2MACHBD(bd);
	ASSERT(sbp->sb_num == bd);
	unit = sfdr_get_unit(nodeid, DR_NT_MEM);
	ASSERT(sbp->sb_devlist[NIX(DR_NT_MEM)][unit] == nodeid);

	mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);

	if (mp->sbm_mlist != NULL) {
		PR_MEM("%s: DUP memlist (board %d)\n", f, sbp->sb_num);
		mlist = memlist_dup(mp->sbm_mlist);
		MEMLIST_DUMP(mlist);
		return (mlist);
	}

	/*
	 * Get the base physical address to which this
	 * memory controller responds.
	 */
	if (sfhw_get_base_physaddr(nodeid, &basepa) < 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to get base physaddr for "
			"nodeid (0x%x)\n", f, (uint_t)nodeid);
		return (NULL);
	}

	/*
	 * Check if board already represented in phys_install.
	 */
	mlen = mc_get_mem_alignment();
	/*
	 * It's possible the basepa may not be on a alignment
	 * boundary.  Need to make it so.
	 */
	basepa &= ~(mlen - 1);

	/*
	 * We see if the board has a phys_install
	 * representation by dup'ing phys_install and
	 * then subtracting out all, but the range
	 * representing the board.  If anything is
	 * leftover, the board's memory is already
	 * installed.
	 */
	memlist_read_lock();
	mlist = memlist_dup(phys_install);
	if ((mlist = memlist_del_span(mlist, 0ull, basepa)) != NULL) {
		uint64_t	endpa;

		basepa += mlen;
		endpa = (uint64_t)(physmax + 1) << PAGESHIFT;
		if (endpa > basepa) {
			mlen = endpa - basepa;
			mlist = memlist_del_span(mlist, basepa, mlen);
		}
	}
	memlist_read_unlock();

	if (mlist) {
		/*
		 * We found the representative memlist.
		 */
		PR_MEM("%s: PHYS memlist (board %d)\n",
			f, sbp->sb_num);
		MEMLIST_DUMP(mlist);
		return (mlist);
	}

	/*
	 * Board not in phys_install, so he must be newly
	 * attached and thus better have a memory available
	 * property.
	 */
	rlen = prom_getproplen(nodeid, SFDR_PROMPROP_MEMAVAIL);
	if (rlen <= 0) {
		PR_MEM("%s: no %s property length for node 0x%x\n",
			f, SFDR_PROMPROP_MEMAVAIL, (uint_t)nodeid);
		return (NULL);
	}
	rblks = rlen / sizeof (struct sf_memunit_regspec);
	rlist = GETSTRUCT(struct sf_memunit_regspec, rblks);

	if (prom_getprop(nodeid, SFDR_PROMPROP_MEMAVAIL, (caddr_t)rlist) < 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: no %s property for node (0x%x)",
			f, SFDR_PROMPROP_MEMAVAIL, (uint_t)nodeid);
		FREESTRUCT(rlist, struct sf_memunit_regspec, rblks);
		return (NULL);
	}
	mlist = sf_reg_to_memlist(rlist, rblks);

	FREESTRUCT(rlist, struct sf_memunit_regspec, rblks);

	/*
	 * Make sure the incoming memlist doesn't already
	 * intersect with what's present in the system (phys_install).
	 */
	memlist_read_lock();
	if (memlist_intersect(phys_install, mlist)) {

		PR_MEM("%s: memlist phys_install...\n", f);
		MEMLIST_DUMP(phys_install);
		PR_MEM("%s: board %d memlist...\n", f, sbp->sb_num);
		MEMLIST_DUMP(mlist);

		cmn_err(CE_WARN,
			"sfdr:%s: board %d memlist already present "
			"in phys_install", f, sbp->sb_num);
		memlist_delete(mlist);
		mlist = NULL;
	}
	memlist_read_unlock();

	if (mlist) {
		PR_MEM("%s: PROP memlist (board %d)\n",
			f, sbp->sb_num);
		MEMLIST_DUMP(mlist);
	}

	return (mlist);
}

/*ARGSUSED*/
int
sfdr_pre_attach_mem(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		bd;
	int		err = 0;
	char		errstr[80];
	register int	d;
	sfdr_board_t	*sbp;
	static fn_t	f = "sfdr_pre_attach_mem";

	errstr[0] = 0;
	PR_MEM("%s...\n", f);

	sbp = BD2MACHBD(hp->h_bd);
	bd = sbp->sb_num;

	for (d = 0; d < devnum; d++) {
		dnode_t		nodeid;
		int		unit;
		sfdr_mem_unit_t	*mp;
		sfdr_state_t	state;

		if ((nodeid = devlist[d].dv_nodeid) == (dnode_t)0)
			break;

		unit = sfdr_get_unit(nodeid, DR_NT_MEM);
		state = SFDR_DEVICE_STATE(sbp, DR_NT_MEM, unit);

		cmn_err(CE_CONT,
			"DR: OS attach mem-unit (%d.%d)\n",
			sbp->sb_num, unit);

		switch (state) {
		case SFDR_STATE_UNCONFIGURED:
			PR_MEM("%s: recovering from UNCONFIG "
				"(mem-unit %d.%d)\n",
				f, bd, unit);

			mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);
			PR_MEM("%s: re-configuring memlist (%d.%d):\n",
				f, sbp->sb_num, unit);
			MEMLIST_DUMP(mp->sbm_mlist);
#ifdef DEBUG
			{
				int jj = kphysm_del_release(mp->sbm_memhandle);
				PR_MEM("%s: kphysm_del_release() = %d\n",
					f, jj);
			}
#else
			(void) kphysm_del_release(mp->sbm_memhandle);
#endif /* DEBUG */
			PR_MEM("%s: reprogramming mem hardware (board %d)\n",
				f, sbp->sb_num);

			/*FALLTHROUGH*/

		case SFDR_STATE_CONNECTED:
			if (sfhw_program_memctrl(nodeid, bd) < 0) {
				sprintf(errstr,
					"%s: failed to program mem ctrlr on "
					"board %d",
					f, bd);
				cmn_err(CE_WARN, "sfdr:%s", errstr);
				err = SFDR_ERR_HW_INTERCONNECT;
			}
			break;

		default:
			sprintf(errstr,
				"%s: unexpected state (%d) for mem-unit "
				"(%d.%d)",
				f, (int)state, bd, unit);
			cmn_err(CE_WARN, "sfdr:%s", errstr);
			err = SFDR_ERR_STATE;
			break;
		}
		if (err)
			break;
	}

	if (err) {
		SFDR_SET_ERR_STR(HD2MACHERR(hp), err, errstr);
		return (-1);
	} else {
		return (0);
	}
}

int
sfdr_post_attach_mem(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		d;
	char		errstr[80];
	struct memlist	*mlist, *ml;
	pda_handle_t	ph;
	sfdr_board_t	*sbp;
	sfdr_error_t	*sep;
	static fn_t	f = "sfdr_post_attach_mem";

	PR_MEM("%s...\n", f);

	ASSERT(devnum == 1);
	sbp = BD2MACHBD(hp->h_bd);
	sep = HD2MACHERR(hp);

	if ((ph = pda_open()) == NULL)
		cmn_err(CE_WARN, "sfdr:%s: failed to open pda", f);

	for (d = 0; d < devnum; d++) {
		int	unit;
		dnode_t	nodeid;

		nodeid = devlist[d].dv_nodeid;
		unit = sfdr_get_unit(nodeid, DR_NT_MEM);

		if ((mlist = sfdr_get_memlist(nodeid)) == NULL) {
			sprintf(errstr,
				"%s: no memlist for mem-unit (%d.%d)",
				f, sbp->sb_num, unit);
			cmn_err(CE_WARN, "sfdr:%s", errstr);
			SFDR_SET_ERR_STR(sep, SFDR_ERR_NODEV, errstr);
			continue;
		}
		/*
		 * Verify the memory really did successfully attach
		 * by checking for its existence in phys_install.
		 */
		memlist_read_lock();
		if (memlist_intersect(phys_install, mlist) == 0) {
			memlist_read_unlock();
			sprintf(errstr,
				"%s: mem-unit (%d.%d) memlist not in "
				"phys_install",
				f, sbp->sb_num, unit);
			PR_MEM("%s", errstr);
			if (SFDR_GET_ERR(sep) == 0) {
				SFDR_SET_ERR_STR(sep, SFDR_ERR_OSFAILURE,
						errstr);
			}
			memlist_delete(mlist);
			continue;
		}
		memlist_read_unlock();
		/*
		 * Update the PDA (post2obp) structure with the
		 * range of the newly added memory.
		 */
		if (ph != NULL)
			for (ml = mlist; ml; ml = ml->next)
				pda_mem_add_span(ph, ml->address, ml->size);

		memlist_delete(mlist);

		sfdr_init_mem_unit_data(sbp, unit);
	}

	if (ph != NULL)
		pda_close(ph);

	return (0);
}

/*ARGSUSED*/
int
sfdr_pre_detach_mem(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		d;
	int		unit;
	sfdr_board_t	*sbp;
	dnode_t		nodeid;

	sbp = BD2MACHBD(hp->h_bd);

	for (d = 0; d < devnum; d++) {
		if ((nodeid = devlist[d].dv_nodeid) == (dnode_t)0)
			continue;

		if ((unit = sfdr_get_unit(nodeid, DR_NT_MEM)) < 0)
			continue;

		cmn_err(CE_CONT,
			"DR: OS detach mem-unit (%d.%d)\n",
			sbp->sb_num, unit);
	}

	return (0);
}

/*ARGSUSED0*/
int
sfdr_post_detach_mem(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		d, rv;
	static fn_t	f = "sfdr_post_detach_mem";

	PR_MEM("%s...\n", f);

	rv = 0;
	for (d = 0; d < devnum; d++) {
		dr_error_t	*ep;

		ep = &devlist[d].dv_error;

		if (DR_GET_ERRNO(ep) == 0) {
			if (sfdr_post_detach_mem_unit(hp,
						devlist[d].dv_nodeid)) {
				int	err;

				err = SFDR_GET_ERR(HD2MACHERR(hp));
				DR_SET_ERRNO(ep, SFDR_ERR2ERRNO(err));

				PR_MEM("%s: sfdr ERROR = %d\n",
					f, DR_GET_ERRNO(ep));
				rv = -1;
			}
		} else {
			rv = -1;
		}
	}

	return (rv);
}

static int
sfdr_post_detach_mem_unit(dr_handle_t *hp, dnode_t nodeid)
{
	int		s_unit, t_unit;
	sfdr_mem_unit_t	*s_mp, *t_mp;
	sfdr_board_t	*s_sbp, *t_sbp;
	sfdr_error_t	*sep;
	struct memlist	*ml, *del_ml = NULL;
	static fn_t	f = "sfdr_post_detach_mem_unit";

	PR_MEM("%s...\n", f);

	t_sbp = NULL;
	s_sbp = BD2MACHBD(hp->h_bd);
	sep = HD2MACHERR(hp);
	s_unit = sfdr_get_unit(nodeid, DR_NT_MEM);
	ASSERT(s_unit != -1);
	s_mp = SFDR_GET_BOARD_MEMUNIT(s_sbp, s_unit);

	if (SFDR_GET_ERR(sep) != 0) {
		/*
		 * We cannot continue so long as there is
		 * an outstanding error.
		 */
		PR_MEM("%s: cannot continue w/error (%d)\n",
			f, SFDR_GET_ERR(sep));
		return (-1);
	}

	/*
	 * This is also where we kphysm_add the remaining
	 * portion of the target board in the scenario
	 * where we moved from a small board to a big board.
	 * The memory to attach should be results of
	 * (target - source) with target's base address
	 * reset to sources.
	 */
	nodeid = s_mp->sbm_target_nodeid;

	if (nodeid != (dnode_t)0) {
		pda_handle_t	ph;
		uint64_t	s_basepa, t_basepa;
		uint64_t	s_nbytes;

		t_unit = sfdr_get_unit(nodeid, DR_NT_MEM);
		ASSERT(t_unit != -1);
		/*
		 * If there was a target memory node then this
		 * is the chunk of memory that was actually detached
		 * from system.  So, it's with respect to the target's
		 * memlist that we update the PDA structure.
		 */
		t_sbp = s_sbp + (s_mp->sbm_target_board - s_sbp->sb_num);
		ASSERT(t_sbp->sb_num == s_mp->sbm_target_board);
		t_mp = SFDR_GET_BOARD_MEMUNIT(t_sbp, t_unit);

		ml = t_mp->sbm_mlist;
		ASSERT(ml);
		PR_MEM("%s: deleted memlist:\n", f);
		MEMLIST_DUMP(ml);

		/*
		 * Verify the memory really did successfully detach
		 * by checking for its non-existence in phys_install.
		 */
		memlist_read_lock();
		if (memlist_intersect(phys_install, ml)) {
			memlist_read_unlock();
			PR_MEM("%s: memlist for board %d, "
				"unexpectedly in phys_install\n",
				f, s_sbp->sb_num);
			if (SFDR_GET_ERR(sep) == 0) {
				char	errstr[80];

				sprintf(errstr,
					"%s: mem-unit (%d.%d) memlist "
					"still in phys_install",
					f, t_sbp->sb_num, t_unit);
				SFDR_SET_ERR_STR(sep, SFDR_ERR_OSFAILURE,
						errstr);
			}
			return (-1);
		}
		memlist_read_unlock();

		s_basepa = (uint64_t)s_mp->sbm_basepfn << PAGESHIFT;
		s_nbytes = (uint64_t)s_mp->sbm_npages << PAGESHIFT;
		t_basepa = (uint64_t)t_mp->sbm_basepfn << PAGESHIFT;

		if (s_mp->sbm_flags & SFDR_MFLAG_MEMMOVE) {
			/*
			 * We had to swap mem-units, so update
			 * memlists accordingly with new base
			 * addresses.
			 */
			for (ml = t_mp->sbm_mlist; ml; ml = ml->next) {
				ml->address -= t_basepa;
				ml->address += s_basepa;
			}
			for (ml = s_mp->sbm_mlist; ml; ml = ml->next) {
				ml->address -= s_basepa;
				ml->address += t_basepa;
			}
		}

		if ((ph = pda_open()) == NULL)
			cmn_err(CE_WARN, "sfdr:%s: failed to open pda", f);

		if (s_mp->sbm_flags & SFDR_MFLAG_MEMRESIZE) {
			struct memlist	*nl;
			/*
			 * We had to perform a copy-rename from a
			 * small memory node to a big memory node.
			 * Need to add back the remaining memory on
			 * the big board that wasn't covered by that
			 * from the small board during the copy.
			 * Subtract out the portion of the target memory
			 * node that was taken over by the source memory
			 * node.
			 */
			nl = memlist_dup(t_mp->sbm_mlist);
			nl = memlist_del_span(nl, s_basepa, s_nbytes);
			/*
			 * Any remaining portion can now be kphysm_add'd
			 */
			PR_MEM("%s: adding back leftovers on board %d..%s\n",
				f, t_sbp->sb_num, nl ? "." : ".NONE");
			MEMLIST_DUMP(nl);

			for (; nl; nl = nl->next) {
				int	err;
				pfn_t	base;
				pgcnt_t	npgs;

				base = (pfn_t)(nl->address >> PAGESHIFT);
				npgs = (pgcnt_t)(nl->size >> PAGESHIFT);
				err = kphysm_add_memory_dynamic(base, npgs);
				if (err) {
					/*
					 * Not fatal, but a bummer.
					 * XXX - lost pages accounted?
					 */
					cmn_err(CE_WARN,
						"sfdr:%s: failed to add back "
						"(base=0x%x, npgs=0x%x) = %d",
						f, base, npgs, err);
				} else {
					/*
					 * Add range to cage growth list. It
					 * is not a fatal error if this add
					 * fails. Worst case is the cage has
					 * less room to grow into.
					 * NOTE: if this board were consider
					 * a "floater board" (to steal a term
					 * from Serengeti) then we would not
					 * add its memory to the cage growth
					 * list.
					 */
					kcage_range_lock();
					err = kcage_range_add(base, npgs, 1);
					kcage_range_unlock();
					if (err) {
						/*
						 * Bummer.
						 */
						cmn_err(CE_WARN, "sfdr:%s: "
						"failed to add back to cage "
						"(base=0x%x, npgs=0x%x) = %d",
							f, base, npgs, err);
					}

					if (ph != NULL) {
						/*
						 * Need to update PDA with
						 * newly added memory.
						 */
						pda_mem_add_span(ph,
							nl->address, nl->size);
					}
				}
			}
			memlist_delete(nl);
		}

		t_mp->sbm_flags &= ~(SFDR_MFLAG_TARGET | SFDR_MFLAG_RELDONE);

		if (ph != NULL) {
			/*
			 * Delete the entire span possible for the
			 * memory that was detached.
			 */
			pda_mem_del_span(ph, t_basepa, mc_get_mem_alignment());
			/*
			 * Update ADRs managed by PDA with reality.
			 */
			pda_mem_sync(ph, t_sbp->sb_num, t_unit);
			/*
			 * There is only one mem-unit per board
			 * in Starfire, however we'll code this
			 * as though there were more (for the future!).
			 */
			if ((s_sbp != t_sbp) || (s_unit != t_unit))
				pda_mem_sync(ph, s_sbp->sb_num, s_unit);

			pda_close(ph);
		}

		s_mp->sbm_flags &= ~(SFDR_MFLAG_MEMMOVE |
					SFDR_MFLAG_MEMRESIZE);
		/*
		 * Re-set mem-unit data portion to reflect
		 * new reality.  Save a copy of the memlist
		 * for possible re-configure later.  The
		 * memlist that was actually deleted is
		 * represented in the "source" mem-unit data.
		 */
		del_ml = memlist_dup(s_mp->sbm_mlist);
		if (t_mp->sbm_mlist) {
			memlist_delete(t_mp->sbm_mlist);
			t_mp->sbm_mlist = NULL;
		}
		sfdr_init_mem_unit_data(t_sbp, t_unit);
	}

	if (s_mp->sbm_mlist) {
		memlist_delete(s_mp->sbm_mlist);
		s_mp->sbm_mlist = NULL;
	}
	if (s_sbp != t_sbp)
		sfdr_init_mem_unit_data(s_sbp, s_unit);
	/*
	 * Save off memlist that was deleted.  May need
	 * it to re-configure memlist.  Will get deleted
	 * once mem-unit is "disconnected".
	 */
	s_mp->sbm_mlist = del_ml;

#ifdef DEBUG
	{
		int	rv = kphysm_del_release(s_mp->sbm_memhandle);
		PR_MEM("%s: kphysm_del_release() = %d\n", f, rv);
	}
#else
	(void) kphysm_del_release(s_mp->sbm_memhandle);
#endif /* DEBUG */

	return (0);
}

/*
 * Successful return from this function will have the memory
 * handle in sbp->sb_dev[..mem-unit...].sbm_memhandle allocated
 * and waiting.  This routine's job is to select the memory that
 * actually has to be released (detached) which may not necessarily
 * be the same memory node that came in in devlist[],
 * i.e. a copy-rename is needed.
 */
/*ARGSUSED*/
int
sfdr_pre_release_mem(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	dnode_t		nodeid;
	int		d, err = 0;
	char		errstr[80];
	sfdr_board_t	*sbp;
	sfdr_error_t	*sep;
	pfn_t		basepfn;
	pgcnt_t		npages;
	memquery_t	mq;
	sfdr_mem_unit_t	*mp;
	extern int	kcage_on;
	static fn_t	f = "sfdr_pre_release_mem";

	errstr[0] = 0;

	PR_MEM("%s...\n", f);

	sbp = BD2MACHBD(hp->h_bd);
	sep = HD2MACHERR(hp);

	if (kcage_on == 0) {
		/*
		 * Can't Detach memory if Cage is OFF.
		 */
		err = SFDR_ERR_CONFIG;
		sprintf(errstr, "%s: kernel cage is disabled", f);
		SFDR_SET_ERR_STR(sep, err, errstr);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		return (-1);
	}

	for (d = 0; d < devnum; d++) {
		struct memlist	*ml;
		int		unit;

		if ((nodeid = devlist[d].dv_nodeid) == (dnode_t)0) {
			cmn_err(CE_WARN,
				"sfdr:%s: devlist[%d] empty (expected %d)",
				f, d, devnum);
			sprintf(errstr,
				"%s: internal error: devlist empty", f);
			err = SFDR_ERR_INTERNAL;
			break;
		}
		unit = sfdr_get_unit(nodeid, DR_NT_MEM);

		mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);

		if (SFDR_DEV_IS_ATTACHED(sbp, DR_NT_MEM, unit) == 0) {
			PR_MEM("%s: board %d memory not attached\n",
				f, sbp->sb_num);
			continue;
		}

		if (mp->sbm_flags & SFDR_MFLAG_TARGET) {
			/*
			 * Board is currently a target for another
			 * memory detach.  Can't detach this guy
			 * until the other detach completes which
			 * is targeting this board.
			 */
			sprintf(errstr,
				"%s: ineligible mem-unit (%d.%d) for detach",
				f, sbp->sb_num, unit);
			cmn_err(CE_WARN, "sfdr:%s", errstr);
			err = SFDR_ERR_INVAL;
			break;
		}
		if (mp->sbm_target_nodeid != (dnode_t)0) {
			/*
			 * Board already has a target selected!
			 * Must have already been through this step.
			 */
			PR_MEM("%s: board %d memory already has "
				"target (bd=%d)\n",
				f, sbp->sb_num, mp->sbm_target_board);
			continue;
		}
		/*
		 * Should have not reach here with a board already
		 * marked for copy-rename, i.e. in the release state.
		 */
		ASSERT(!(mp->sbm_flags &
			(SFDR_MFLAG_MEMMOVE | SFDR_MFLAG_MEMRESIZE)));
		ASSERT(mp->sbm_mlist == NULL);

		if (mp->sbm_mlist != NULL) {
			memlist_delete(mp->sbm_mlist);
			mp->sbm_mlist = NULL;
		}

		mp->sbm_flags &= ~(SFDR_MFLAG_TARGET |
					SFDR_MFLAG_MEMMOVE |
					SFDR_MFLAG_MEMRESIZE |
					SFDR_MFLAG_RELDONE);
		/*
		 * Check whether the detaching memory requires a
		 * copy-rename.
		 */
		basepfn = mp->sbm_basepfn;
		npages = mp->sbm_npages;
		ASSERT(npages != 0);

		if (kphysm_del_span_query(basepfn, npages, &mq) != KPHYSM_OK) {
			err = SFDR_ERR_PROTO;
			sprintf(errstr,
				"%s: protocol error: kphysm_del_span_query "
				"[bd=%d, bp=0x%lx, n=%ld]",
				f, sbp->sb_num, basepfn, npages);
			cmn_err(CE_WARN, "sfdr:%s", errstr);
			break;
		}

		if ((ml = sfdr_get_memlist(nodeid)) == NULL) {
			PR_MEM("%s: no memlist found for board %d\n",
				f, sbp->sb_num);
			continue;
		}

		if ((mq.nonrelocatable != 0) ||
				sfdr_reserve_mem_target(hp, unit, nodeid)) {
			/*
			 * Either the detaching memory node contains
			 * non-reloc memory or we failed to reserve the
			 * detaching memory node (which did _not_ have
			 * any non-reloc memory, i.e. some non-reloc mem
			 * got onboard).
			 */
			mp->sbm_flags |= SFDR_MFLAG_MEMMOVE;
			mp->sbm_mlist = ml;

			if (sfdr_select_mem_target(hp, nodeid)) {
				/*
				 * We had no luck locating a target
				 * memory node to be the recipient of
				 * the non-reloc memory on the node
				 * we're trying to detach.
				 */
				sprintf(errstr,
					"%s: no available target for "
					"mem-unit (%d.%d)",
					f, sbp->sb_num, unit);
				cmn_err(CE_WARN, "sfdr:%s", errstr);
				err = SFDR_ERR_CONFIG;
				mp->sbm_flags &= ~SFDR_MFLAG_MEMMOVE;
				memlist_delete(ml);
				mp->sbm_mlist = NULL;
				break;
			}
		} else {
			/*
			 * We've selected ourself, no non-reloc mem on board.
			 */
			mp->sbm_flags |= SFDR_MFLAG_TARGET;
			mp->sbm_target_nodeid = nodeid;
			mp->sbm_target_board = sbp->sb_num;
			mp->sbm_mlist = ml;
		}

		ASSERT(mp->sbm_target_nodeid != (dnode_t)0);
		ASSERT(mp->sbm_mlist != NULL);

		PR_MEM("%s: requested board %d, selected board %d\n",
			f, sbp->sb_num, mp->sbm_target_board);
	}

	if (err) {
		SFDR_SET_ERR_STR(sep, err, errstr);
		return (-1);
	} else {
		return (0);
	}
}

void
sfdr_release_mem_done(dr_handle_t *hp, int unit)
{
	static fn_t	f = "sfdr_release_mem_done";

	if (sfdr_release_dev_done(hp, DR_NT_MEM, unit) == 0) {
		sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
		sfdr_mem_unit_t	*mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);

		ASSERT(DR_BOARD_LOCK_HELD(hp->h_bd));

		PR_MEM("%s: marking mem-unit (%d.%d) release DONE\n",
			f, sbp->sb_num, unit);

		mp->sbm_flags |= SFDR_MFLAG_RELDONE;
	}
}

/*ARGSUSED*/
int
sfdr_disconnect_mem(dr_handle_t *hp, int unit)
{
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	sfdr_mem_unit_t	*mp;
	struct memlist	*ml;
	static fn_t	f = "sfdr_disconnect_mem";

	ASSERT((SFDR_DEVICE_STATE(sbp, DR_NT_MEM, unit) ==
						SFDR_STATE_CONNECTED) ||
		(SFDR_DEVICE_STATE(sbp, DR_NT_MEM, unit) ==
						SFDR_STATE_UNCONFIGURED));

	mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);
	if (mp && ((ml = mp->sbm_mlist) != NULL)) {
		PR_MEM("%s: deleting target memlist (board %d):\n",
			f, sbp->sb_num);
		MEMLIST_DUMP(ml);
		memlist_delete(ml);
		mp->sbm_mlist = NULL;
	} else {
		PR_MEM("%s: NO MEMLIST to disconnect (board %d)\n",
			f, sbp->sb_num);
	}
	return (0);
}

int
sfdr_cancel_mem(dr_handle_t *hp, int unit)
{
	int		rv = 0;
	pfn_t		base;
	pgcnt_t		npgs;
	dnode_t		nodeid;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd), *t_sbp;
	sfdr_state_t	state;
	sfdr_mem_unit_t	*mp;
	struct memlist	*ml;
	static fn_t	f = "sfdr_cancel_mem";

	state = SFDR_DEVICE_STATE(sbp, DR_NT_MEM, unit);
	mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);

	switch (state) {
	case SFDR_STATE_UNREFERENCED:
	case SFDR_STATE_RELEASE:
		PR_MEM("%s: (state = %s) canceling possible release "
			"(board %d)\n",
			f, state_str[(int)state], sbp->sb_num);

#ifdef DEBUG
		{
			int	ii = 1;
			int	kk = kphysm_del_cancel(mp->sbm_memhandle);
			PR_MEM("%s: kphysm_del_cancel() = %d\n", f, kk);
			do {
				kk = kphysm_del_release(mp->sbm_memhandle);
				PR_MEM("%s: (ii=%d) kphysm_del_release() "
					"= %d\n", f, ii, kk);
				if (kk == KPHYSM_ENOTFINISHED)
					delay(hz);
				ii++;
			} while (kk == KPHYSM_ENOTFINISHED);
		}
#else
		(void) kphysm_del_cancel(mp->sbm_memhandle);
		do {
			rv = kphysm_del_release(mp->sbm_memhandle);
			if (rv == KPHYSM_ENOTFINISHED)
				delay(hz);
		} while (rv == KPHYSM_ENOTFINISHED);
#endif /* DEBUG */
		/*
		 * Need to add back the "target" memory that
		 * was being released, which is not necessarily
		 * the memory on the board represented in the handle,
		 * i.e. it might be a copy-rename.
		 */
		if (mp->sbm_target_board < 0) {
			PR_MEM("%s: no target board to reattach (board %d)\n",
				f, sbp->sb_num);
			break;
		}
		t_sbp = sbp + (mp->sbm_target_board - sbp->sb_num);
		nodeid = mp->sbm_target_nodeid;

		ASSERT(t_sbp->sb_num == mp->sbm_target_board);
		ASSERT(nodeid != (dnode_t)0);

		mp->sbm_flags &= ~(SFDR_MFLAG_TARGET |
					SFDR_MFLAG_MEMMOVE |
					SFDR_MFLAG_MEMRESIZE);
		if (t_sbp != sbp) {
			if (mp->sbm_mlist) {
				memlist_delete(mp->sbm_mlist);
				mp->sbm_mlist = NULL;
			}
			sfdr_init_mem_unit_data(sbp, unit);
		}

		/*
		 * Determine unit of actual memory being
		 * released.
		 */
		unit = sfdr_get_unit(nodeid, DR_NT_MEM);
		/*
		 * Find mem-unit of board whose memory is actually
		 * being released.  It's his memlist that we must
		 * add back.
		 */
		mp = SFDR_GET_BOARD_MEMUNIT(t_sbp, unit);

		if ((ml = mp->sbm_mlist) != NULL) {
			pda_handle_t	ph;

			PR_MEM("%s: attempting mem reattach (board %d)\n",
				f, t_sbp->sb_num);
			MEMLIST_DUMP(ml);

			if ((ph = pda_open()) == NULL)
				cmn_err(CE_WARN,
					"sfdr:%s: failed to open pda", f);

			rv = 0;
			for (; ml; ml = ml->next) {
				int	err;

				base = (pfn_t)(ml->address >> PAGESHIFT);
				npgs = (pgcnt_t)(ml->size >> PAGESHIFT);

				err = kphysm_add_memory_dynamic(base, npgs);
				if (err == KPHYSM_OK) {
					/*
					 * Add range to cage growth list. It
					 * is not a fatal error if this add
					 * fails. Worst case is the cage has
					 * less room to grow into.
					 * NOTE: if this board were consider
					 * a "floater board" (to steal a term
					 * from Serengeti) then we would not
					 * add its memory to the cage growth
					 * list.
					 */
					kcage_range_lock();
					err = kcage_range_add(base, npgs, 1);
					kcage_range_unlock();
					if (err != 0) {
						PR_MEM("%s: kcage_range_add"
						"(base=0x%x, npgs=%d) = %d\n",
						f, (uint_t)base, (uint_t)npgs,
						err);
					}
					err = 0;
				} else {
					PR_MEM("%s: kphysm_add_memory_dynamic"
						"(base=0x%x, npgs=%d) = %d\n",
						f, (uint_t)base, (uint_t)npgs,
						err);
					if (err == KPHYSM_ESPAN)
						err = 0;
				}

				if (ph != NULL)
					pda_mem_add_span(ph, ml->address,
							ml->size);
				if (!rv && err)
					rv = err;
			}
			if (ph != NULL)
				pda_close(ph);

			memlist_delete(mp->sbm_mlist);
		}
		mp->sbm_flags &= ~(SFDR_MFLAG_TARGET |
					SFDR_MFLAG_MEMMOVE |
					SFDR_MFLAG_MEMRESIZE);
		sfdr_init_mem_unit_data(t_sbp, unit);
		break;

	default:
		PR_MEM("%s: WARNING unexpected state (%d) for "
			"mem-unit %d.%d\n",
			f, (int)state, sbp->sb_num, unit);
		break;
	}

	return (rv);
}

void
sfdr_init_mem_unit(sfdr_board_t *sbp, int unit)
{
	dnode_t		nodeid;
	sfdr_state_t	new_state;

	nodeid = sbp->sb_devlist[NIX(DR_NT_MEM)][unit];

	if (SFDR_DEV_IS_ATTACHED(sbp, DR_NT_MEM, unit)) {
		new_state = SFDR_STATE_CONFIGURED;
	} else if (SFDR_DEV_IS_PRESENT(sbp, DR_NT_MEM, unit)) {
		new_state = SFDR_STATE_CONNECTED;
	} else if (nodeid != (dnode_t)0) {
		new_state = SFDR_STATE_OCCUPIED;
	} else {
		new_state = SFDR_STATE_EMPTY;
	}
	SFDR_DEVICE_TRANSITION(sbp, DR_NT_MEM, unit, new_state);

	sfdr_init_mem_unit_data(sbp, unit);
}

static void
sfdr_init_mem_unit_data(sfdr_board_t *sbp, int unit)
{
	dnode_t		nodeid;
	uint64_t	basepa;
	pda_handle_t	ph;
	sfdr_mem_unit_t	*mp;
	static fn_t	f = "sfdr_init_mem_unit_data";

	mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);
	nodeid = sbp->sb_devlist[NIX(DR_NT_MEM)][unit];

	if (sfhw_get_base_physaddr(nodeid, &basepa) != 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to get physaddr for mem-unit (%d.%d)",
			f, sbp->sb_num, unit);
		return;
	}
	/*
	 * Might not be on memory alignment boundary, make it so.
	 */
	basepa &= ~(mc_get_mem_alignment() - 1);

	mp->sbm_basepfn = (pfn_t)(basepa >> PAGESHIFT);
	if ((ph = pda_open()) == NULL) {
		cmn_err(CE_WARN, "sfdr:%s: failed to open pda", f);
		mp->sbm_npages = 0;
	} else {
		mp->sbm_npages = pda_get_mem_size(ph, sbp->sb_num);
		pda_close(ph);
	}
	if (mp->sbm_npages == 0) {
		struct memlist	*ml, *mlist;
		/*
		 * Either we couldn't open the PDA or our
		 * PDA has garbage in it.  We must have the
		 * page count consistent and whatever the
		 * OS states has precedence over the PDA
		 * so let's check the kernel.
		 */
		PR_MEM("%s: PDA query failed for npages. "
			"Check memlist (%d.%d)\n",
			f, sbp->sb_num, unit);

		mlist = sfdr_get_memlist(nodeid);
		for (ml = mlist; ml; ml = ml->next)
			mp->sbm_npages += btop(ml->size);
		memlist_delete(mlist);
	}
	mp->sbm_dimmsize = mc_get_dimm_size(nodeid);
	mp->sbm_target_nodeid = (dnode_t)0;
	mp->sbm_target_board = -1;
	mp->sbm_mlist = NULL;

	PR_MEM("%s: board %d (basepfn = 0x%x, npgs = %d)\n",
		f, sbp->sb_num, mp->sbm_basepfn, mp->sbm_npages);
}

static int
sfdr_reserve_mem_target(dr_handle_t *hp, int s_unit, dnode_t t_nodeid)
{
	int		err;
	pfn_t		base;
	pgcnt_t		npgs;
	struct memlist	*ml, *mc;
	sfdr_board_t	*sbp;
	sfdr_mem_unit_t	*mp;
	memhandle_t	*mhp;
	static fn_t	f = "sfdr_reserve_mem_target";

	PR_MEM("%s...\n", f);

	sbp = BD2MACHBD(hp->h_bd);
	mp = SFDR_GET_BOARD_MEMUNIT(sbp, s_unit);
	mhp = &mp->sbm_memhandle;

	err = kphysm_del_gethandle(mhp);
	if (err != KPHYSM_OK) {
		/*
		 * Unable to allocate a memhandle for deleting the
		 * memory on this node.
		 */
		cmn_err(CE_WARN,
			"sfdr:%s: unable to allocate memhandle for mem-unit "
			"(%d.%d)", f, sbp->sb_num, s_unit);
		return (-1);
	}

	ml = sfdr_get_memlist(t_nodeid);

	/*
	 * Go through the memory spans associated with this
	 * memory node (handle), and add them to the memhandle which
	 * will subsequently be used in the memory delete process.
	 * Note that this procedure directly removes the target
	 * memory from cage candidacy, thus once reserved we're
	 * free from the cage.
	 *
	 * OBSOLETE COMMENT:
	 * We could requery the span after each del_span addition,
	 * but doing them all at once will reduce some risk of a
	 * span becoming non-reloc.
	 */
	for (mc = ml; mc; mc = mc->next) {
		int cleanup = 0;

		base = (pfn_t)(mc->address >> PAGESHIFT);
		npgs = (pgcnt_t)(mc->size >> PAGESHIFT);

		/*
		 * Remove the range from the cage growth list.
		 * If successful, we are guaranteed the cage will not
		 * grow in to this range.
		 * NOTE: This effectively negates the need to check for
		 * non-reloc pages with kphysm_del_span_query().
		 *
		 * If the return code is non-zero, that most likely
		 * means the cage already occupies at least a portion
		 * of the range and therefore the range can not be
		 * removed from the cage growth list. Which also means
		 * the range can not be kphysm_del_span detached.
		 */
		kcage_range_lock();
		err = kcage_range_delete(base, npgs);
		kcage_range_unlock();
		if (err != 0) {
			if (err != EBUSY) {
				cmn_err(CE_WARN,
					"sfdr:%s: kcage_range_delete"
					"(0x%x, 0x%x) = %d, failed",
					f, (uint_t)base, (uint_t)npgs, err);
			}

			cleanup = 1;
		} else {
			err = kphysm_del_span(*mhp, base, npgs);
			if (err != KPHYSM_OK) {
				cmn_err(CE_WARN,
					"sfdr:%s: kphysm_del_span"
					"(0x%x, 0x%x) = %d, failed",
					f, (uint_t)base, (uint_t)npgs, err);

				cleanup = 1;
			}
		}

		if (cleanup) {
			struct memlist *mt;

			/*
			 * Put back ranges delete thus far.
			 */
			kcage_range_lock();
			for (mt = ml; mt != mc; mt = mt->next) {
				err = kcage_range_add(mt->address >> PAGESHIFT,
					mt->size >> PAGESHIFT, 1);
				/* TODO: deal with error */
				err = err;
			}
			kcage_range_unlock();

			/*
			 * Need to free up remaining memlist
			 * entries.
			 */
			memlist_delete(ml);
			/*
			 * Release use of the memhandle before returning.
			 */
			(void) kphysm_del_release(*mhp);

			return (-1);
		}
	}
	/*
	 * Now that we've reserved span go through and verify
	 * it's still non-reloc.
	 * Now that we call kcage_range_delete, mq.nonrelocatable
	 * should never be non-zero.
	 */
	for (mc = ml; mc; mc = mc->next) {
		memquery_t	mq;

		base = (pfn_t)(mc->address >> PAGESHIFT);
		npgs = (pgcnt_t)(mc->size >> PAGESHIFT);

		err = kphysm_del_span_query(base, npgs, &mq);

		if ((err != KPHYSM_OK) || (mq.nonrelocatable != 0)) {
			/*
			 * Failed to query a span for some reason
			 * or some non-reloc memory snuck in.
			 */
			memlist_delete(ml);
			/*
			 * Release handle before returning.
			 */
			(void) kphysm_del_release(*mhp);

			cmn_err(CE_WARN,
				"sfdr:%s: kphysm_del_span_query(0x%x, 0x%x) "
				"failed", f, (uint_t)base, (uint_t)npgs);

			return (-1);

		}
	}

	memlist_delete(ml);

	return (0);
}

/*
 * Algorithm is to first try and find a big enough memory node (board)
 * that has the same DIMM size.  If no luck there, then we search
 * for a memory node whose DIMM size is at least as big as the
 * source memory node.  This is currently the only criteria.
 */
static int
sfdr_select_mem_target(dr_handle_t *hp, dnode_t nodeid)
{
	int		s_bd, t_bd;
	int		s_unit, t_unit;
	int		rv, mindex, pass = 0;
	boardset_t	next_candidates = 0;
	sfdr_board_t	*s_sbp, *t_sbp;
	sfdr_mem_unit_t	*s_mp, *t_mp;
	struct memlist	*s_ml, *t_ml;
	pfn_t		basepfn;
	pgcnt_t		npages;
	memquery_t	mq;
	dnode_t		*devlist;
	static fn_t	f = "sfdr_select_mem_target";

	PR_MEM("%s...\n", f);

	mindex = NIX(DR_NT_MEM);
	s_sbp = BD2MACHBD(hp->h_bd);
	s_bd = s_sbp->sb_num;
	s_unit = sfdr_get_unit(nodeid, DR_NT_MEM);
	s_mp = SFDR_GET_BOARD_MEMUNIT(s_sbp, s_unit);

	if ((s_ml = s_mp->sbm_mlist) == NULL) {
		/*
		 * We'll save this new memlist only if we're
		 * able to successfully select a target.
		 */
		if ((s_ml = sfdr_get_memlist(nodeid)) == NULL) {
			cmn_err(CE_WARN,
				"sfdr:%s: no memlist for mem-unit (%d.%d)",
				f, s_bd, s_unit);
			return (-1);
		}
	}

	/*
	 * We make two passes looking for candidates.
	 * On the first pass we only look for boards that have
	 * the same DIMM sizes.  If none found then we continue
	 * into the second pass.
	 * On the second pass we consider memory nodes that
	 * are _larger_.  If still none found, then fail.
	 */
	for (pass = 0; pass < 2; pass++) {
		for (t_bd = 0; t_bd < MAX_BOARDS; t_bd++) {
			/*
			 * On the 2nd pass we only look at
			 * boards captured on first pass as
			 * candidates.
			 */
			if (pass && !BOARD_IN_SET(next_candidates, t_bd))
				continue;
			/*
			 * The board structs are a contiguous array
			 * so we take advantage of that to find the
			 * correct board struct pointer for a given
			 * board number.
			 */
			t_sbp = s_sbp + (t_bd - s_bd);
			ASSERT(t_sbp->sb_num == t_bd);
			devlist = t_sbp->sb_devlist[mindex];

			for (t_unit = 0; t_unit < MAX_MEM_UNITS_PER_BOARD;
							t_unit++) {
				t_mp = SFDR_GET_BOARD_MEMUNIT(t_sbp, t_unit);
				/*
				 * If we're in this routine, the given
				 * memory node we're detaching can
				 * never be a candidate.
				 */
				if (s_mp == t_mp)
					continue;

				ASSERT((s_bd != t_bd) || (s_unit != t_unit));

				if (!SFDR_DEV_IS_ATTACHED(t_sbp, DR_NT_MEM,
							t_unit) ||
					(t_mp->sbm_flags &
						(SFDR_MFLAG_TARGET |
							SFDR_MFLAG_MEMMOVE)))
					continue;

				if (pass == 0) {
					/*
					 * Only need to check for simm
					 * sizes on first pass.
					 */
					if (s_mp->sbm_dimmsize >
							t_mp->sbm_dimmsize)
						continue;

					if (s_mp->sbm_dimmsize <
							t_mp->sbm_dimmsize) {
						/*
						 * Target board at least as
						 * big as detaching one.
						 * Mark him as a secondary
						 * candidate.
						 */
						BOARD_ADD(next_candidates,
								t_bd);
						continue;
					}
				}
				/*
				 * Has to have a memory node if passed
				 * check above.
				 */
				ASSERT(devlist);
				nodeid = devlist[t_unit];
				if (nodeid == (dnode_t)0) {
					PR_MEM("%s: empty memory node "
						"for board %d\n", f, t_bd);
					continue;
				}

				if ((t_ml = t_mp->sbm_mlist) == NULL) {
					t_ml = sfdr_get_memlist(nodeid);
					/*
					 * We'll save this new memlist
					 * only if this guy is chosen
					 * as the target.
					 */
				}
				if (t_ml == NULL) {
					cmn_err(CE_WARN,
						"sfdr:%s: no memlist for "
						"mem-unit (%d.%d)",
						f, t_bd, t_unit);
					continue;
				}

				if (memlist_canfit(s_ml, t_ml) == 0) {
					PR_MEM("%s: source memlist won't "
						"fit in target memlist\n", f);
					PR_MEM("%s: source memlist:\n", f);
					MEMLIST_DUMP(s_ml);
					PR_MEM("%s: target memlist:\n", f);
					MEMLIST_DUMP(t_ml);

					if (t_mp->sbm_mlist == NULL)
						memlist_delete(t_ml);
					continue;
				}
				/*
				 * Check for no-reloc.
				 */
				basepfn = t_mp->sbm_basepfn;
				npages = t_mp->sbm_npages;

				PR_MEM("%s: checking no-reloc in "
					"(basepfn=0x%x, npgs=%d)\n",
					f, (uint_t)basepfn, (uint_t)npages);

				rv = kphysm_del_span_query(basepfn,
							npages, &mq);
				if (rv != KPHYSM_OK) {
					PR_MEM("%s: failed to query span of "
						"board %d\n", f, t_bd);
					if (t_mp->sbm_mlist == NULL)
						memlist_delete(t_ml);
					continue;
				}

				if ((mq.nonrelocatable != 0) ||
						sfdr_reserve_mem_target(hp,
								s_unit,
								nodeid)) {

					PR_MEM("%s: candidate board %d "
						"has noreloc\n", f, t_bd);
					if (t_mp->sbm_mlist == NULL)
						memlist_delete(t_ml);
					continue;
				}

				/*
				 * Found a candidate!
				 * We save off memlists in mem-unit
				 * board structs.  Note that sbm_mlist
				 * entries will have been NULL if we're
				 * inserting a newly created memlist,
				 * otherwise we're just shoving in
				 * the same list that's already there.
				 */
				PR_MEM("%s: FOUND target for board %d "
					"-> board %d\n", f, s_bd, t_bd);

				s_mp->sbm_target_nodeid = nodeid;
				s_mp->sbm_target_board = t_bd;
				s_mp->sbm_mlist = s_ml;
				t_mp->sbm_flags &= ~SFDR_MFLAG_RELDONE;
				t_mp->sbm_flags |= SFDR_MFLAG_TARGET;
				t_mp->sbm_mlist = t_ml;
				if (s_mp->sbm_npages < t_mp->sbm_npages) {
					s_mp->sbm_flags |=
							SFDR_MFLAG_MEMRESIZE;
					PR_MEM("%s: resize detected "
						"(%d < %d)\n",
						f, s_mp->sbm_npages,
						t_mp->sbm_npages);
				}

				return (0);
			}
		}
	}

	PR_MEM("%s: NO target candidates found for board %d\n", f, s_bd);

	if (s_mp->sbm_mlist == NULL)
		memlist_delete(s_ml);

	return (-1);
}

#define	SFDR_SCRUB_VALUE	0x0d0e0a0d0b0e0e0fULL

void
sfdr_memscrub(struct memlist *mlist)
{
	struct memlist	*ml;
	uint64_t	scrub_value = SFDR_SCRUB_VALUE;
#ifdef DEBUG
	clock_t		stime;
#endif /* DEBUG */
	static fn_t	f = "sfdr_memscrub";

	PR_MEM("%s: scrubbing memlist...\n", f);
	MEMLIST_DUMP(mlist);
#ifdef DEBUG
	stime = lbolt;
#endif /* DEBUG */

	for (ml = mlist; ml; ml = ml->next) {
		uint64_t	dst_pa;
		uint64_t	nbytes;

		/* calculate the destination physical address */
		dst_pa = ml->address;
		if (ml->address & PAGEOFFSET)
			cmn_err(CE_WARN,
				"sfdr:%s: address (0x%llx) not on "
				"page boundary", f, ml->address);

		nbytes = ml->size;
		if (ml->size & PAGEOFFSET)
			cmn_err(CE_WARN,
				"sfdr:%s: size (0x%llx) not on "
				"page boundary", f, ml->size);

		/*LINTED*/
		while (nbytes > 0) {
			/* write 64 bits to dst_pa */
			stdphys(dst_pa, scrub_value);

			/* increment/decrement by cacheline sizes */
			dst_pa += ecache_linesize;
			nbytes -= ecache_linesize;
		}
	}
#ifdef DEBUG
	stime = lbolt - stime;
	PR_MEM("%s: scrub ticks = %ld (%ld secs)\n", f, stime, stime / hz);
#endif /* DEBUG */
}

/*
 * XXX - code assumes single mem-unit.
 */
int
sfdr_move_memory(dr_handle_t *hp, struct memlist *mlist)
{
	int		err;
	caddr_t		mempage;
	ulong_t		data_area, index_area;
	ulong_t		e_area, e_page;
	int		availlen, indexlen, funclen, scriptlen;
	int		*indexp;
	time_t		copytime;
	int		(*funcp)();
	uint64_t	memregaddr[2];
	uint64_t	neer;
	pda_handle_t	ph;
	sfdr_sr_handle_t	*srhp;
	sfdr_rename_script_t	*rsp;
	sfdr_rename_script_t	*rsbuffer = NULL;
	static fn_t	f = "sfdr_move_memory";

	PR_MEM("%s: (INLINE) moving memory from memory node %d to node %d\n",
		f, BD2MACHBD(hp->h_bd)->sb_num,
		SFDR_GET_BOARD_MEMUNIT(BD2MACHBD(hp->h_bd),
					0)->sbm_target_board);
	MEMLIST_DUMP(mlist);

	funclen = (int)((ulong_t)_sfdr_copy_rename_end -
			(ulong_t)sfdr_copy_rename);
	if (funclen > PAGESIZE) {
		cmn_err(CE_WARN,
			"sfdr:%s: copy-rename funclen (%d) > PAGESIZE (%d)",
			f, funclen, PAGESIZE);
		return (-1);
	}

	/*
	 * mempage will be page aligned, since we're calling
	 * kmem_alloc() with an exact multiple of PAGESIZE.
	 */
	mempage = kmem_alloc(PAGESIZE, KM_SLEEP);

	PR_MEM("%s: mempage = 0x%p\n", f, mempage);

	/*
	 * Copy the code for the copy-rename routine into
	 * a page aligned piece of memory.  We do this to guarantee
	 * that we're executing within the same page and thus reduce
	 * the possibility of cache collisions between different
	 * pages.
	 */
	bcopy((caddr_t)sfdr_copy_rename, mempage, funclen);

	funcp = (int (*)())mempage;

	PR_MEM("%s: copy-rename funcp = 0x%x (len = 0x%x)\n",
		f, (ulong_t)funcp, funclen);

	/*
	 * Prepare data page that will contain script of
	 * operations to perform during copy-rename.
	 * Allocate temporary buffer to hold script.
	 */
	rsbuffer = GETSTRUCT(sfdr_rename_script_t, SFDR_RENAME_MAXOP);

	scriptlen = sfdr_prep_rename_script(hp, rsbuffer, memregaddr);
	if (scriptlen <= 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to prep for copy-rename", f);
		FREESTRUCT(rsbuffer, sfdr_rename_script_t, SFDR_RENAME_MAXOP);
		kmem_free(mempage, PAGESIZE);
		return (-1);
	}
	PR_MEM("%s: copy-rename script length = 0x%x\n", f, scriptlen);

	indexlen = sizeof (*indexp) << 1;

	if ((funclen + scriptlen + indexlen) > PAGESIZE) {
		cmn_err(CE_WARN,
			"sfdr:%s: func len (%d) + script len (%d) "
			"+ index len (%d) > PAGESIZE (%d)",
			f, funclen, scriptlen, indexlen, PAGESIZE);
		FREESTRUCT(rsbuffer, sfdr_rename_script_t, SFDR_RENAME_MAXOP);
		kmem_free(mempage, PAGESIZE);
		return (-1);
	}

	/*
	 * Find aligned area within data page to maintain script.
	 */
	data_area = (ulong_t)mempage;
	data_area += (ulong_t)funclen + (ulong_t)(ecache_linesize - 1);
	data_area &= ~((ulong_t)(ecache_linesize - 1));

	availlen = PAGESIZE - indexlen;
	availlen -= (int)(data_area - (ulong_t)mempage);

	if (availlen < scriptlen) {
		cmn_err(CE_WARN,
			"sfdr:%s: available len (%d) < script len (%d)",
			f, availlen, scriptlen);
		FREESTRUCT(rsbuffer, sfdr_rename_script_t, SFDR_RENAME_MAXOP);
		kmem_free(mempage, PAGESIZE);
		return (-1);
	}

	PR_MEM("%s: copy-rename script data area = 0x%p\n",
		f, data_area);

	bcopy((caddr_t)rsbuffer, (caddr_t)data_area, scriptlen);
	rsp = (sfdr_rename_script_t *)data_area;

	FREESTRUCT(rsbuffer, sfdr_rename_script_t, SFDR_RENAME_MAXOP);

	index_area = data_area + (ulong_t)scriptlen +
			(ulong_t)(ecache_linesize - 1);
	index_area &= ~((ulong_t)(ecache_linesize - 1));
	indexp = (int *)index_area;
	/*
	 * XXX - no longer needed with inline script executor.
	 *
	 * First index used as index into script and
	 * flag to "cache" copy-rename section.
	 * Second index used to go through script-loop
	 * for real.  We set to 0 now so that when it's
	 * copied, we pick up the desired value when we
	 * "switch" boards.
	 */
	indexp[0] = 0;
	indexp[1] = 0;

	e_area = index_area + (ulong_t)indexlen;
	e_page = (ulong_t)mempage + PAGESIZE;
	if (e_area > e_page) {
		cmn_err(CE_WARN,
			"sfdr:%s: index area size (%d) > available (%d)\n",
			f, indexlen, (int)(e_page - index_area));
		kmem_free(mempage, PAGESIZE);
		return (-1);
	}

	PR_MEM("%s: copy-rename index area = 0x%p\n", f, indexp);

	srhp = sfdr_get_sr_handle(hp);
	ASSERT(srhp);

	if ((ph = pda_open()) == NULL) {
		cmn_err(CE_WARN, "sfdr:%s: failed to open pda", f);
		sfdr_release_sr_handle(srhp);
		kmem_free(mempage, PAGESIZE);
		return (-1);
	}

	copytime = lbolt;
	/*
	 * Quiesce the OS.
	 */
	if (sfdr_suspend(srhp)) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to quiesce OS for copy-rename", f);
		pda_close(ph);
		sfdr_release_sr_handle(srhp);
		kmem_free(mempage, PAGESIZE);
		return (-1);
	}

	/*
	 *==================================
	 * COPY-RENAME BEGIN.
	 *==================================
	 */
	sfhw_idle_interconnect((void *)ph);	/* quiet interconnect */

	neer = get_error_enable();
	neer &= ~EER_CEEN;
	set_error_enable(neer);			/* disable CE reporting */

	err = (*funcp)(hp, mlist, rsp, memregaddr);

	neer |= EER_CEEN;
	set_error_enable(neer);			/* enable CE reporting */

	sfhw_resume_interconnect((void *)ph);	/* resume interconnect */
	/*
	 *==================================
	 * COPY-RENAME END.
	 *==================================
	 */

	/*
	 * Resume the OS.
	 */
	sfdr_resume(srhp);

	copytime = lbolt - copytime;

	pda_close(ph);

	sfdr_release_sr_handle(srhp);

	PR_MEM("%s: copy-rename elapsed time = %ld ticks (%ld secs)\n",
		f, copytime, copytime / hz);

	kmem_free(mempage, PAGESIZE);

	return (err ? -1 : 0);
}

/*
 * XXX - code assumes single mem-unit.
 */
static int
sfdr_prep_rename_script(dr_handle_t *hp, sfdr_rename_script_t rsp[],
			uint64_t memregaddr[])
{
	dnode_t		s_nodeid, t_nodeid;
	int		b, m, s_bd, t_bd;
	uint_t		s_masr, t_masr, new_base;
	sfdr_board_t	*s_sbp, *t_sbp, *sbp;
	sfdr_mem_unit_t	*s_mp;
	static fn_t	f = "sfdr_prep_rename_script";

	PR_MEM("%s...\n", f);

	s_sbp = BD2MACHBD(hp->h_bd);
	s_bd = s_sbp->sb_num;
	s_mp = SFDR_GET_BOARD_MEMUNIT(s_sbp, 0);
	t_sbp = s_sbp + (s_mp->sbm_target_board - s_sbp->sb_num);
	t_bd = t_sbp->sb_num;
	ASSERT(t_bd == s_mp->sbm_target_board);

	s_nodeid = s_sbp->sb_devlist[NIX(DR_NT_MEM)][0];
	t_nodeid = s_mp->sbm_target_nodeid;

	memregaddr[0] = mc_get_idle_addr(s_nodeid);
	memregaddr[1] = mc_get_idle_addr(t_nodeid);

	if (mc_read_asr(s_nodeid, &s_masr)) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to read source MC ASR of board %d",
			f, s_sbp->sb_num);
		return (-1);
	}

	if (mc_read_asr(t_nodeid, &t_masr)) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to read target MC ASR of board %d",
			f, t_sbp->sb_num);
		return (-1);
	}

	/*
	 * Massage ASRs to desired end results.
	 */
	s_masr &= ~STARFIRE_MC_MEM_PRESENT_MASK;
	/*
	 * Keep base of source mem which will remain in system.
	 */
	new_base = s_masr & STARFIRE_MC_MEM_BASEADDR_MASK;
	/*
	 * Reassign the source base to the target's base (swap).
	 */
	s_masr &= ~STARFIRE_MC_MEM_BASEADDR_MASK;
	s_masr |= t_masr & STARFIRE_MC_MEM_BASEADDR_MASK;
	/*
	 * We care about everything _but_ the base address in the
	 * target masr.  We want to replace this portion with the
	 * base address from the source masr.
	 */
	t_masr &= ~STARFIRE_MC_MEM_BASEADDR_MASK;
	t_masr |= new_base;
	t_masr |= STARFIRE_MC_MEM_PRESENT_MASK;

	/*
	 * Step 0:	Mark source memory as not present.
	 */
	m = 0;
	rsp[m].masr_addr = mc_get_asr_addr(s_nodeid);
	rsp[m].masr = s_masr;
	m++;
	/*
	 * Step 1:	Write source base address to target MC
	 *		with present bit off.
	 */
	rsp[m].masr_addr = mc_get_asr_addr(t_nodeid);
	rsp[m].masr = t_masr & ~STARFIRE_MC_MEM_PRESENT_MASK;
	m++;
	/*
	 * Step 2:	Now rewrite target reg with present bit on.
	 */
	rsp[m].masr_addr = rsp[m-1].masr_addr;
	rsp[m].masr = t_masr;
	m++;

	/*
	 * Find pointer to board "0".
	 */
	sbp = s_sbp - s_sbp->sb_num;

	for (b = 0; b < MAX_BOARDS; sbp++, b++) {
		int	c, i;

		ASSERT(sbp->sb_num == b);
		if (SFDR_DEVS_PRESENT(sbp) == 0)
			continue;

		/*
		 * Step 3:	Update PC MADR tables for CPUs.
		 */
		for (c = 0; c < MAX_CPU_UNITS_PER_BOARD; c++) {
			if (SFDR_DEV_IS_PRESENT(sbp, DR_NT_CPU, c) == 0)
				continue;
			/*
			 * Disabled detaching mem node.
			 */
			rsp[m].masr_addr = STARFIRE_PC_MADR_ADDR(b, s_bd, c);
			rsp[m].masr = s_masr;
			m++;
			/*
			 * Always write masr with present bit
			 * off and then again with it on.
			 */
			rsp[m].masr_addr = STARFIRE_PC_MADR_ADDR(b, t_bd, c);
			rsp[m].masr = t_masr & ~STARFIRE_MC_MEM_PRESENT_MASK;
			m++;
			rsp[m].masr_addr = rsp[m-1].masr_addr;
			rsp[m].masr = t_masr;
			m++;
		}
		/*
		 * Step 4:	Update PC MADR tables for IOs.
		 */
		for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++) {
			if (SFDR_DEV_IS_PRESENT(sbp, DR_NT_IO, i) == 0)
				continue;
			/*
			 * Disabled detaching mem node.
			 */
			rsp[m].masr_addr = STARFIRE_PC_MADR_ADDR(b, s_bd, i+4);
			rsp[m].masr = s_masr;
			m++;
			/*
			 * Always write masr with present bit
			 * off and then again with it on.
			 */
			rsp[m].masr_addr = STARFIRE_PC_MADR_ADDR(b, t_bd, i+4);
			rsp[m].masr = t_masr & ~STARFIRE_MC_MEM_PRESENT_MASK;
			m++;
			rsp[m].masr_addr = rsp[m-1].masr_addr;
			rsp[m].masr = t_masr;
			m++;
		}
	}
	/*
	 * Zero masr_addr value indicates the END.
	 */
	rsp[m].masr_addr = 0ull;
	rsp[m].masr = 0;
	PR_MEM("%s: number of steps in rename script = %d\n", f, m);
	m++;

#ifdef DEBUGxxx
	{
		int	i;

		PR_MEM("%s: dumping copy-rename script:\n", f);
		for (i = 0; i < m; i++) {
			PR_MEM("%s: 0x%llx = 0x%x\n",
				f, rsp[i].masr_addr, rsp[i].masr);
		}
		DELAY(1000000);
	}
#endif /* DEBUGxxx */

	return (m * sizeof (sfdr_rename_script_t));
}

/*
 * The routine performs the necessary memory COPY and MC adr SWITCH.
 * Both operations MUST be at the same "level" so that the stack is
 * maintained correctly between the copy and switch.  The switch
 * portion implements a caching mechanism to guarantee the code text
 * is cached prior to execution.  This is to guard against possible
 * memory access while the MC adr's are being modified.
 *
 * IMPORTANT: The _sfdr_copy_rename_end() fucntion must immediately
 * follow sfdr_copy_rename() so that the correct "length' of the
 * sfdr_copy_rename can be calculated.  This routine MUST be a
 * LEAF function, i.e. it can make NO function calls, primarily
 * for two reasons:
 *	1. We must keep the stack consistent across the "switch".
 *	2. Function calls are compiled to relative offsets, and
 *	   we execute this function we'll be executing it from
 *	   a copied version in a different area of memory, thus
 *	   the relative offsets will be bogus.
 *
 * XXX - code assumes single mem-unit.
 */
static int
sfdr_copy_rename(dr_handle_t *hp, struct memlist *mlist,
		register sfdr_rename_script_t *rsp,
		uint64_t memregaddr[])
{
	int		err;
	uint_t		csize;
	uint64_t	caddr;
	int		s_bd, t_bd;
	uint64_t	s_base, t_base;
	uint64_t	s_idle_addr, t_idle_addr;
	sfdr_board_t	*s_sbp, *t_sbp;
	sfdr_mem_unit_t	*s_mp, *t_mp;
	struct memlist	*ml;

	caddr = ecache_flushaddr;
	csize = (uint_t)(cpunodes[CPU->cpu_id].ecache_size << 1);

	s_sbp = BD2MACHBD(hp->h_bd);
	s_bd = s_sbp->sb_num;
	s_mp = SFDR_GET_BOARD_MEMUNIT(s_sbp, 0);
	t_bd = s_mp->sbm_target_board;
	t_sbp = s_sbp + (t_bd - s_bd);
	t_mp = SFDR_GET_BOARD_MEMUNIT(t_sbp, 0);

	s_idle_addr = memregaddr[0];
	t_idle_addr = memregaddr[1];

	s_base = (uint64_t)s_mp->sbm_basepfn << PAGESHIFT;
	t_base = (uint64_t)t_mp->sbm_basepfn << PAGESHIFT;

	/*
	 * DO COPY.
	 */
	for (ml = mlist; ml; ml = ml->next) {
		uint64_t	s_pa, t_pa;
		uint64_t	nbytes;

		s_pa = ml->address;
		t_pa = t_base + (ml->address - s_base);
		nbytes = ml->size;

		while (nbytes != 0ull) {
			/*
			 * This copy does NOT use an ASI
			 * that avoids the Ecache, therefore
			 * the dst_pa addresses may remain
			 * in our Ecache after the dst_pa
			 * has been removed from the system.
			 * A subsequent write-back to memory
			 * will cause an ARB-stop because the
			 * physical address no longer exists
			 * in the system. Therefore we must
			 * flush out local Ecache after we
			 * finish the copy.
			 */

			/* copy 32 bytes at src_pa to dst_pa */
			bcopy32_il(s_pa, t_pa);

			/* increment by 32 bytes */
			s_pa += (4 * sizeof (uint64_t));
			t_pa += (4 * sizeof (uint64_t));

			/* decrement by 32 bytes */
			nbytes -= (4 * sizeof (uint64_t));
		}
	}
	/*
	 * Since bcopy32_il() does NOT use an ASI to bypass
	 * the Ecache, we nee dto flush our Ecache after
	 * the copy is complete.
	 */
	CPU_FLUSH_ECACHE_IL(caddr, csize);	/* inline version */

	/*
	 * Wait for MCs to go idle.
	 */
	SFHW_MEMCTRL_BUSYWAIT(s_idle_addr, 10, err);
	if (err)
		return (-1);

	SFHW_MEMCTRL_BUSYWAIT(t_idle_addr, 10, err);
	if (err)
		return (-1);

	/*
	 * The following inline assembly routine caches
	 * the rename script and then caches the code that
	 * will do the rename.  This is necessary
	 * so that we don't have any memory references during
	 * the reprogramming.  We accomplish this by first
	 * jumping through the code to guarantee it's cached
	 * before we actually execute it.
	 */
	sfdr_exec_script_il(rsp);

	return (0);
}
static void
_sfdr_copy_rename_end(void)
{
	/*
	 * IMPORTANT:	This function's location MUST be located
	 *		immediately following sfdr_copy_rename.
	 *		It is needed to estimate the sizeof
	 *		(sfdr_copy_rename).  Note that this assumes
	 *		the compiler keeps these functions in the
	 *		order in which they appear :-o
	 */
}

/*
 * Memlist support.
 */
void
memlist_delete(struct memlist *mlist)
{
	register struct memlist	*ml;

	for (ml = mlist; ml; ml = mlist) {
		mlist = ml->next;
		FREESTRUCT(ml, struct memlist, 1);
	}
}

struct memlist *
memlist_dup(struct memlist *mlist)
{
	struct memlist *hl = NULL, **mlp;

	if (mlist == NULL)
		return (NULL);

	mlp = &hl;
	for (; mlist; mlist = mlist->next) {
		*mlp = GETSTRUCT(struct memlist, 1);
		(*mlp)->address = mlist->address;
		(*mlp)->size = mlist->size;
		mlp = &((*mlp)->next);
	}
	*mlp = NULL;

	return (hl);
}

void
memlist_dump(struct memlist *mlist)
{
	register struct memlist *ml;

	if (mlist == NULL) {
		PR_MEM("memlist> EMPTY\n");
	} else {
		for (ml = mlist; ml; ml = ml->next)
			PR_MEM("memlist> 0x%llx, 0x%llx\n",
				ml->address, ml->size);
	}
}

/*ARGSUSED*/
static void
memlist_coalesce(struct memlist *mlist)
{
	uint64_t	end, nend;

	if ((mlist == NULL) || (mlist->next == NULL))
		return;

	while (mlist->next) {
		end = mlist->address + mlist->size;
		if (mlist->next->address <= end) {
			struct memlist 	*nl;

			nend = mlist->next->address + mlist->next->size;
			if (nend > end)
				mlist->size += (nend - end);
			nl = mlist->next;
			mlist->next = mlist->next->next;
			if (nl) {
				FREESTRUCT(nl, struct memlist, 1);
			}
			if (mlist->next)
				mlist->next->prev = mlist;
		} else {
			mlist = mlist->next;
		}
	}
}

/*ARGSUSED*/
static struct memlist *
memlist_add_span(struct memlist *mlist, uint64_t base, uint64_t len)
{
	struct memlist	*ml, *tl, *nl;

	if (len == 0ull)
		return (NULL);

	if (mlist == NULL) {
		mlist = GETSTRUCT(struct memlist, 1);
		mlist->address = base;
		mlist->size = len;
		mlist->next = mlist->prev = NULL;

		return (mlist);
	}

	for (tl = ml = mlist; ml; tl = ml, ml = ml->next) {
		if (base < ml->address) {
			if ((base + len) < ml->address) {
				nl = GETSTRUCT(struct memlist, 1);
				nl->address = base;
				nl->size = len;
				nl->next = ml;
				if ((nl->prev = ml->prev) != NULL)
					nl->prev->next = nl;
				if (mlist == ml)
					mlist = nl;
			} else {
				ml->size = MAX((base + len),
						(ml->address + ml->size)) -
						base;
				ml->address = base;
			}
			break;

		} else if (base <= (ml->address + ml->size)) {
			ml->size = MAX((base + len),
					(ml->address + ml->size)) -
					MIN(ml->address, base);
			ml->address = MIN(ml->address, base);
			break;
		}
	}
	if (ml == NULL) {
		nl = GETSTRUCT(struct memlist, 1);
		nl->address = base;
		nl->size = len;
		nl->next = NULL;
		nl->prev = tl;
		tl->next = nl;
	}

	memlist_coalesce(mlist);

	return (mlist);
}

/*ARGSUSED*/
static struct memlist *
memlist_del_span(struct memlist *mlist, uint64_t base, uint64_t len)
{
	uint64_t	end;
	struct memlist	*ml, *tl, *nlp;

	if (mlist == NULL)
		return (NULL);

	end = base + len;
	if ((end <= mlist->address) || (base == end))
		return (mlist);

	for (tl = ml = mlist; ml; tl = ml, ml = nlp) {
		uint64_t	mend;

		nlp = ml->next;

		if (end <= ml->address)
			break;

		mend = ml->address + ml->size;
		if (base < mend) {
			if (base <= ml->address) {
				ml->address = end;
				if (end >= mend)
					ml->size = 0ull;
				else
					ml->size = mend - ml->address;
			} else {
				ml->size = base - ml->address;
				if (end < mend) {
					struct memlist	*nl;
					/*
					 * splitting an memlist entry.
					 */
					nl = GETSTRUCT(struct memlist, 1);
					nl->address = end;
					nl->size = mend - nl->address;
					if ((nl->next = nlp) != NULL)
						nlp->prev = nl;
					nl->prev = ml;
					ml->next = nl;
					nlp = nl;
				}
			}
			if (ml->size == 0ull) {
				if (ml == mlist) {
					if ((mlist = nlp) != NULL)
						nlp->prev = NULL;
					FREESTRUCT(ml, struct memlist, 1);
					if (mlist == NULL)
						break;
					ml = nlp;
				} else {
					if ((tl->next = nlp) != NULL)
						nlp->prev = tl;
					FREESTRUCT(ml, struct memlist, 1);
					ml = tl;
				}
			}
		}
	}

	return (mlist);
}

int
memlist_intersect(struct memlist *al, struct memlist *bl)
{
	uint64_t	astart, aend, bstart, bend;

	if ((al == NULL) || (bl == NULL))
		return (0);

	aend = al->address + al->size;
	bstart = bl->address;
	bend = bl->address + bl->size;

	while (al && bl) {
		while (al && (aend <= bstart))
			if ((al = al->next) != NULL)
				aend = al->address + al->size;
		if (al == NULL)
			return (0);

		if ((astart = al->address) <= bstart)
			return (1);

		while (bl && (bend <= astart))
			if ((bl = bl->next) != NULL)
				bend = bl->address + bl->size;
		if (bl == NULL)
			return (0);

		if ((bstart = bl->address) <= astart)
			return (1);
	}

	return (0);
}

/*
 * Determine whether the source memlist (s_mlist) will
 * fit into the target memlist (t_mlist) in terms of
 * size and holes (i.e. based on same relative base address).
 */
static int
memlist_canfit(struct memlist *s_mlist, struct memlist *t_mlist)
{
	int		rv = 0;
	uint64_t	s_basepa, t_basepa;
	struct memlist	*s_ml, *t_ml;

	if ((s_mlist == NULL) || (t_mlist == NULL))
		return (0);

	/*
	 * Base both memlists on common base address (0).
	 */
	s_basepa = s_mlist->address;
	t_basepa = t_mlist->address;

	for (s_ml = s_mlist; s_ml; s_ml = s_ml->next)
		s_ml->address -= s_basepa;

	for (t_ml = t_mlist; t_ml; t_ml = t_ml->next)
		t_ml->address -= t_basepa;

	s_ml = s_mlist;
	for (t_ml = t_mlist; t_ml && s_ml; t_ml = t_ml->next) {
		uint64_t	s_start, s_end;
		uint64_t	t_start, t_end;

		t_start = t_ml->address;
		t_end = t_start + t_ml->size;

		for (; s_ml; s_ml = s_ml->next) {
			s_start = s_ml->address;
			s_end = s_start + s_ml->size;

			if ((s_start < t_start) || (s_end > t_end))
				break;
		}
	}
	/*
	 * If we ran out of source memlist chunks that mean
	 * we found a home for all of them.
	 */
	if (s_ml == NULL)
		rv = 1;

	/*
	 * Need to add base addresses back since memlists
	 * are probably in use by caller.
	 */
	for (s_ml = s_mlist; s_ml; s_ml = s_ml->next)
		s_ml->address += s_basepa;

	for (t_ml = t_mlist; t_ml; t_ml = t_ml->next)
		t_ml->address += t_basepa;

	return (rv);
}

struct memlist *
sf_reg_to_memlist(struct sf_memunit_regspec rlist[], int rblks)
{
	struct memlist	*mlist = NULL;
	register int	i;

	for (i = 0; i < rblks; i++) {
		uint64_t	addr, size;

		addr = (uint64_t)rlist[i].regspec_addr_hi << 32;
		addr |= (uint64_t)rlist[i].regspec_addr_lo;
		size = (uint64_t)rlist[i].regspec_size_hi << 32;
		size |= (uint64_t)rlist[i].regspec_size_lo;

		mlist = memlist_add_span(mlist, addr, size);
	}

	return (mlist);
}

/*
 * Cage testing routines.
 * TODO: clean this up.
 */

#include <sys/vnode.h>
#include <sys/mem_cage.h>

/*
 * TODO: when driver unloads all pages on sfdr_test_cage_vnode will be lost,
 * pp->vnode will point into empty or reused memory, both very bad things.
 */
static vnode_t sfdr_test_cage_vnode;
static pgcnt_t sfdr_test_cage_offset;

static int
sfdr_test_cage_alloc(pgcnt_t npages, int canwait)
{
	page_t *pp;
	pgcnt_t n;
	struct seg kseg;

	kseg.s_as = &kas;
	for (n = 0; n < npages; n++) {
		pp = page_create_va(
			&sfdr_test_cage_vnode,
			(u_offset_t)sfdr_test_cage_offset,
			PAGESIZE,
			(canwait ? PG_WAIT : 0) | PG_EXCL | PG_NORELOC,
			&kseg,
			(caddr_t)sfdr_test_cage_offset);

		if (pp == NULL)
			break;
		else
			sfdr_test_cage_offset += PAGESIZE;
	}

	return (n == npages);
}

static int
sfdr_test_cage_threshold(volatile pgcnt_t *threshold, int canwait)
{
	int result = 1;

	while (kcage_freemem > *threshold) {
		pgcnt_t want;

		want = (kcage_freemem - *threshold) / 32;
		if (want == 0)
			want = 1;

		result = sfdr_test_cage_alloc(want, canwait);
		if (result == 0)
			break;
	}

	return (result);
}

static void
sfdr_test_cage_free(pgcnt_t npages)
{
	page_t *pp;

	while ((pp = sfdr_test_cage_vnode.v_pages) != NULL && npages-- > 0) {
		page_vpsub(&sfdr_test_cage_vnode.v_pages, pp);

		pp->p_hash = NULL;
		page_clr_all_props(pp);
		pp->p_vnode = NULL;
		pp->p_offset = (u_offset_t)-1;

		page_io_unlock(pp);
		page_free(pp, 1);

		sfdr_test_cage_offset -= PAGESIZE;
	}
}

/*
 * Pass-through ioctl inteface.
 * Would be nice if sfdr_ioctl did not assume what the ioctl arg looked
 * like. But, it does. Oh, well. So, we overload the meaning of i_cpuid
 * and i_major to mean sub-command and sub-command-arg, respectively.
 * TODO: move these macros to sfdr.h (and fix sfdr_ioctl_arg_t?).
 */

#define	i_subcmd i_cpuid
#define	i_subcmdarg i_major

#define	SFDR_TEST_CAGE_ALLOC 1
#define	SFDR_TEST_CAGE_FREE 2
#define	SFDR_TEST_CAGE_LOTSFREE 3
#define	SFDR_TEST_CAGE_DESFREE 4
#define	SFDR_TEST_CAGE_MINFREE 5
#define	SFDR_TEST_CAGE_THROTTLEFREE 6

void
sfdr_test_cage(dr_handle_t *hp, sfdr_ioctl_arg_t *iap)
{
	static fn_t f = "sfdr_test_cage";
	int force = iap->i_flags & SFDR_FLAG_FORCE;
	sfdr_error_t *sep;

	PR_MEM("%s...\n", f);

	sep = HD2MACHERR(hp);

	SFDR_SET_ERR(sep, 0);   /* handles get re-used.. */

	switch (iap->i_subcmd) {
		case SFDR_TEST_CAGE_ALLOC: {
			pgcnt_t want = (pgcnt_t)iap->i_subcmdarg;
			int success;

			success = sfdr_test_cage_alloc(want, force == 0);
			if (!success)
				SFDR_SET_ERR(sep, SFDR_ERR_NOMEM);
			break;
		}

		case SFDR_TEST_CAGE_FREE: {
			pgcnt_t free = (pgcnt_t)iap->i_subcmdarg;

			if (free == 0)
				free = sfdr_test_cage_offset;

			sfdr_test_cage_free(free);
			break;
		}

		case SFDR_TEST_CAGE_LOTSFREE: {
			int success;

			success = sfdr_test_cage_threshold(
				&kcage_lotsfree, force == 0);
			if (!success)
				SFDR_SET_ERR(sep, SFDR_ERR_NOMEM);
			break;
		}

		case SFDR_TEST_CAGE_DESFREE: {
			int success;

			success = sfdr_test_cage_threshold(
				&kcage_desfree, force == 0);
			if (!success)
				SFDR_SET_ERR(sep, SFDR_ERR_NOMEM);
			break;
		}

		case SFDR_TEST_CAGE_MINFREE: {
			int success;

			success = sfdr_test_cage_threshold(
				&kcage_minfree, force == 0);
			if (!success)
				SFDR_SET_ERR(sep, SFDR_ERR_NOMEM);
			break;
		}

		case SFDR_TEST_CAGE_THROTTLEFREE: {
			int success;

			success = sfdr_test_cage_threshold(
				&kcage_throttlefree, force == 0);
			if (!success)
				SFDR_SET_ERR(sep, SFDR_ERR_NOMEM);
			break;
		}

		default:
			SFDR_SET_ERR(sep, SFDR_ERR_INVAL);
			break;
	}
}
