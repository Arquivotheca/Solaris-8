/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rd_elf.c	1.9	99/11/08 SMI"

#include	<stdlib.h>
#include	<stdio.h>
#include	<proc_service.h>
#include	<link.h>
#include	<rtld_db.h>
#include	"rtld.h"
#include	"_rtld_db.h"
#include	"msg.h"


/*
 * 64-bit builds are going to compile this module twice, the
 * second time with _ELF64 defined.  These defines should make
 * all the necessary adjustments to the code.
 */
#ifdef _LP64
#ifdef _ELF64
#define	_rd_reset32		_rd_reset64
#define	_rd_event_enable32	_rd_event_enable64
#define	_rd_event_getmsg32	_rd_event_getmsg64
#define	_rd_objpad_enable32	_rd_objpad_enable64
#define	_rd_loadobj_iter32	_rd_loadobj_iter64
#define	TList			List
#define	TListnode		Listnode
#else	/* ELF32 */
#define	Rt_map			Rt_map32
#define	Rtld_db_priv		Rtld_db_priv32
#define	r_debug			r_debug32
#define	TList			List32
#define	TListnode		Listnode32
#define	Lm_list			Lm_list32
#endif	/* _ELF64 */
#else	/* _LP64 */
#define	TList			List
#define	TListnode		Listnode
#endif	/* _LP64 */


rd_err_e
_rd_reset32(struct rd_agent *rap)
{
	psaddr_t			symaddr;
	struct ps_prochandle *		php = rap->rd_psp;
	Rtld_db_priv			db_priv;

	/*
	 * Load in location of private symbols
	 */
	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_RTLDDBPV),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LOOKFAIL),
			MSG_ORIG(MSG_SYM_RTLDDBPV)));
		return (RD_DBERR);
	}

	rap->rd_rtlddbpriv = symaddr;

	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_DEBUG),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LOOKFAIL),
			MSG_ORIG(MSG_SYM_DEBUG)));
		return (RD_DBERR);
	}

	rap->rd_rdebug = symaddr;

	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_PREINIT),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LOOKFAIL),
		    MSG_ORIG(MSG_SYM_PREINIT)));
		return (RD_DBERR);
	}

	rap->rd_preinit = symaddr;

	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_POSTINIT),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_NOFINDRTLD),
		    MSG_ORIG(MSG_SYM_POSTINIT)));
		return (RD_DBERR);
	}
	rap->rd_postinit = symaddr;

	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_DLACT),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_NOFINDRTLD),
		    MSG_ORIG(MSG_SYM_DLACT)));
		return (RD_DBERR);
	}
	rap->rd_dlact = symaddr;
	rap->rd_tbinder = 0;

	/*
	 * Verify that librtld_db & rtld are at the proper revision
	 * levels.
	 */

	if (ps_pread(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READPRIVFAIL_1),
			EC_ADDR(rap->rd_rtlddbpriv)));
		return (RD_DBERR);
	}

	if (db_priv.rtd_version != R_RTLDDB_VERSION) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_BADPVERS),
			db_priv.rtd_version, R_RTLDDB_VERSION));
		return (RD_NOCAPAB);
	}

	/*
	 * Is the image being examined from a core file or not.
	 * If it is a core file then the following write will fail.
	 */
	if (ps_pwrite(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK)
		rap->rd_flags |= RDF_FL_COREFILE;

	return (RD_OK);
}


rd_err_e
_rd_event_enable32(rd_agent_t * rap, int onoff)
{
	struct ps_prochandle *		php = rap->rd_psp;
	struct r_debug			rdb;

	/*
	 * Tell the debugged process that debugging is occuring
	 * This will enable the storing of event messages so that
	 * the can be gathered by the debugger.
	 */
	if (ps_pread(php, rap->rd_rdebug, (char *)&rdb,
	    sizeof (struct r_debug)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_1), EC_ADDR(&rdb)));
		return (RD_DBERR);
	}

	if (onoff)
		rdb.r_flags |= RD_FL_DBG;
	else
		rdb.r_flags &= ~RD_FL_DBG;

	if (ps_pwrite(php, rap->rd_rdebug, (char *)&rdb,
	    sizeof (struct r_debug)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_WRITEFAIL_1), EC_ADDR(&rdb)));
		return (RD_DBERR);
	}

	return (RD_OK);
}


rd_err_e
_rd_event_getmsg32(rd_agent_t * rap, rd_event_msg_t * emsg)
{
	struct r_debug	rdb;

	if (ps_pread(rap->rd_psp, rap->rd_rdebug, (char *)&rdb,
	    sizeof (struct r_debug)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READDBGFAIL_2),
		    EC_ADDR(rap->rd_rdebug)));
		return (RD_DBERR);
	}
	emsg->type = rdb.r_rdevent;
	if (emsg->type == RD_DLACTIVITY) {
		switch (rdb.r_state) {
			case RT_CONSISTENT:
				emsg->u.state = RD_CONSISTENT;
				break;
			case RT_ADD:
				emsg->u.state = RD_ADD;
				break;
			case RT_DELETE:
				emsg->u.state = RD_DELETE;
				break;
		}
	} else
		emsg->u.state = RD_NOSTATE;

	return (RD_OK);
}


rd_err_e
_rd_objpad_enable32(struct rd_agent * rap, size_t padsize)
{
	Rtld_db_priv			db_priv;
	struct ps_prochandle *		php = rap->rd_psp;

	if (ps_pread(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_3),
		    EC_ADDR(rap->rd_rtlddbpriv)));
		return (RD_DBERR);
	}
#if	defined(_LP64) && !defined(_ELF64)
	/*LINTED*/
	db_priv.rtd_objpad = (uint32_t)padsize;
#else
	db_priv.rtd_objpad = padsize;
#endif
	if (ps_pwrite(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_WRITEFAIL_2),
		    EC_ADDR(rap->rd_rtlddbpriv)));
		return (RD_DBERR);
	}
	return (RD_OK);
}




static rd_err_e
iter_map(rd_agent_t * rap, unsigned long ident, psaddr_t lmaddr,
	rl_iter_f * cb, void * client_data)
{
	while (lmaddr) {
		Rt_map		rmap;
		rd_loadobj_t	lobj;
		int		i;
		ulong_t		off;
		Ehdr		ehdr;
		Phdr		phdr;

		if (ps_pread(rap->rd_psp, lmaddr, (char *)&rmap,
		    sizeof (Rt_map)) != PS_OK) {
			LOG(ps_plog(MSG_ORIG(MSG_DB_LKMAPFAIL)));
			return (RD_DBERR);
		}

		lobj.rl_nameaddr = (psaddr_t)PATHNAME(&rmap);
		lobj.rl_refnameaddr = (psaddr_t)REFNAME(&rmap);
		lobj.rl_flags = 0;
		lobj.rl_base = (psaddr_t)ADDR(&rmap);
		lobj.rl_lmident = ident;
		lobj.rl_bend = ADDR(&rmap) + MSIZE(&rmap);
		lobj.rl_padstart = PADSTART(&rmap);
		lobj.rl_padend = PADSTART(&rmap) + PADIMLEN(&rmap);

		/*
		 * Look for beginning of data segment.
		 *
		 * NOTE: the data segment can only be found for full
		 *	processes and not from core images.
		 */
		lobj.rl_data_base = 0;
		if (rap->rd_flags & RDF_FL_COREFILE)
			lobj.rl_data_base = 0;
		else {
			off = ADDR(&rmap);
			if (ps_pread(rap->rd_psp, off, (char *)&ehdr,
			    sizeof (Ehdr)) != PS_OK) {
				LOG(ps_plog(MSG_ORIG(MSG_DB_LKMAPFAIL)));
				return (RD_DBERR);
			}
			off += sizeof (Ehdr);
			for (i = 0; i < ehdr.e_phnum; i++) {
				if (ps_pread(rap->rd_psp, off, (char *)&phdr,
				    sizeof (Phdr)) != PS_OK) {
					LOG(ps_plog(MSG_ORIG(
					    MSG_DB_LKMAPFAIL)));
					return (RD_DBERR);
				}
				if ((phdr.p_type == PT_LOAD) &&
				    (phdr.p_flags & PF_W)) {
					lobj.rl_data_base = phdr.p_vaddr;
					if (ehdr.e_type == ET_DYN)
						lobj.rl_data_base +=
							ADDR(&rmap);
					break;
				}
				off += ehdr.e_phentsize;
			}
		}


		/*
		 * When we transfer control to the client we free the
		 * lock and re-atain it after we've returned from the
		 * client.  This is to avoid any deadlock situations.
		 */
		RDAGUNLOCK(rap);
		if ((*cb)(&lobj, client_data) == 0) {
			RDAGLOCK(rap);
			break;
		}
		RDAGLOCK(rap);
		lmaddr = (psaddr_t)NEXT(&rmap);
	}
	return (RD_OK);
}


rd_err_e
_rd_loadobj_iter32(rd_agent_t * rap, rl_iter_f * cb, void * client_data)
{
	Rtld_db_priv	db_priv;
	TList		list;
	TListnode	lnode;
	Addr		lnp;
	unsigned long	ident;
	rd_err_e	rc;

	if (ps_pread(rap->rd_psp, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READDBGFAIL_1),
		    EC_ADDR(rap->rd_rtlddbpriv)));
		return (RD_DBERR);
	}

	if (db_priv.rtd_dynlmlst == 0) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LKMAPNOINIT),
			EC_ADDR(db_priv.rtd_dynlmlst)));
		return (RD_NOMAPS);
	}

	if (ps_pread(rap->rd_psp, (psaddr_t)db_priv.rtd_dynlmlst, (char *)&list,
	    sizeof (TList)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READDBGFAIL_3),
			EC_ADDR(db_priv.rtd_dynlmlst)));
		return (RD_DBERR);
	}

	if (list.head == 0) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LKMAPNOINIT_1),
			EC_ADDR(list.head)));
		return (RD_NOMAPS);
	}


	if (cb == 0) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_NULLITER)));
		return (RD_ERR);
	}

	for (lnp = (Addr)list.head; lnp; lnp = (Addr)lnode.next) {
		Lm_list	lml;

		/*
		 * Iterate through the List of Lm_list's.
		 */
		if (ps_pread(rap->rd_psp, (psaddr_t)lnp, (char *)&lnode,
		    sizeof (TListnode)) != PS_OK) {
			LOG(ps_plog(MSG_ORIG(MSG_DB_READDBGFAIL_4),
				EC_ADDR(lnp)));
			return (RD_DBERR);
		}

		if (ps_pread(rap->rd_psp, (psaddr_t)lnode.data, (char *)&lml,
		    sizeof (Lm_list)) != PS_OK) {
			LOG(ps_plog(MSG_ORIG(MSG_DB_READDBGFAIL_5),
				EC_ADDR(lnode.data)));
			return (RD_DBERR);
		}

		/*
		 * Determine IDENT of current LM_LIST
		 */
		if (lml.lm_flags & LML_FLG_BASELM)
			ident = LM_ID_BASE;
		else if (lml.lm_flags & LML_FLG_RTLDLM)
			ident = LM_ID_LDSO;
		else
			ident = (unsigned long)lnode.data;

		if ((rc = iter_map(rap, ident, (psaddr_t)lml.lm_head,
		    cb, client_data)) != RD_OK) {
			return (rc);
		}
	}
	return (rc);
}
