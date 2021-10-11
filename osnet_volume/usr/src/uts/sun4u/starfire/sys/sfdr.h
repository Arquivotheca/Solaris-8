/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef _SYS_SFDR_H
#define	_SYS_SFDR_H

#pragma ident	"@(#)sfdr.h	1.38	99/09/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/starfire.h>

/*
 * contains definitions used internally by the sfdr module (PSM-DR).
 */

typedef const char *const fn_t;

#define	MAX_BOARDS			plat_max_boards()
#define	MAX_CPU_UNITS_PER_BOARD		plat_max_cpu_units_per_board()
#define	MAX_MEM_UNITS_PER_BOARD		plat_max_mem_units_per_board()
#define	MAX_IO_UNITS_PER_BOARD		plat_max_io_units_per_board()
/*
 * This is kludgy.  We need a static allocation for the status structure
 * and yet we can't reference plat_max_xxx in the decl for the stat array.
 */
#define	MAX_DEV_UNITS_PER_BOARD		7

#if 0
/*
 * Ala FHC.  CPU offset will support ecache sizes up to 256MB.
 */
#define	_SFDR_BASE_NOMEM		(1ull << 40)
#define	_SFDR_CPU_NOMEM_OFFSET(c)	((uint64_t)(c) << 28)
#define	SFDR_ADDR_NOMEM(c)		(_SFDR_BASE_NOMEM | \
					_SFDR_CPU_NOMEM_OFFSET(c))
#endif /* 0 */

/*
 * Starfire specific error attributes
 */
#define	SFDR_MAX_ERR_INT	((MAXPATHLEN / sizeof (int32_t)) - 1)

/*
 * Starfire per-board error structure used solely by sfdr module.
 */
typedef struct {
	dr_error_t	se_err;
	uint32_t	se_merr;
	char		*se_mstr;
} sfdr_error_t;

/*
 * We need definition and justification for these errors.
 * Values for sfdr_ioctl_arg_t.i_error.
 * NOTE: If you add a new error, a errno translation
 *	 needs to be added to the table (_sfdr_err2errno_table)
 *	 in sfdr.c.
 */
typedef enum {
/*  0 */	SFDR_ERR_NOERROR = 0,
/*  1 */	SFDR_ERR_INTERNAL,
/*  2 */	SFDR_ERR_SUSPEND,
/*  3 */	SFDR_ERR_RESUME,
/*  4 */	SFDR_ERR_UNSAFE,
/*  5 */	SFDR_ERR_UTHREAD,
/*  6 */	SFDR_ERR_RTTHREAD,
/*  7 */	SFDR_ERR_KTHREAD,
/*  8 */	SFDR_ERR_OSFAILURE,
/* 19 */	SFDR_ERR_OUTSTANDING,
/* 11 */	SFDR_ERR_CONFIG,
/* 12 */	SFDR_ERR_NOMEM,
/* 13 */	SFDR_ERR_PROTO,
/* 14 */	SFDR_ERR_BUSY,
/* 15 */	SFDR_ERR_NODEV,
/* 16 */	SFDR_ERR_INVAL,
/* 17 */	SFDR_ERR_STATE,
/* 18 */	SFDR_ERR_PROBE,
/* 19 */	SFDR_ERR_DEPROBE,
/* 20 */	SFDR_ERR_HW_INTERCONNECT,
/* 21 */	SFDR_ERR_OFFLINE,
/* 22 */	SFDR_ERR_ONLINE,
/* 23 */	SFDR_ERR_CPUSTART,
/* 24 */	SFDR_ERR_CPUSTOP,
/* 25 */	SFDR_ERR_JUGGLE_BOOTPROC,
/* 26 */	SFDR_ERR_CANCEL,
/* 27 */	SFDR_ERR_MAX
} sfdr_err_t;

#define	SFDR_NUM_ERRORS		((int)SFDR_ERR_MAX)

#ifdef _KERNEL
#define	SFDR_ERR2ERRNO(e)	((((int)(e) >= 0) && \
					((int)(e) < SFDR_NUM_ERRORS)) ? \
				_sfdr_err2errno_table[(int)(e)] : ENXIO)
extern int	_sfdr_err2errno_table[];
#endif /* _KERNEL */

/*
 * ioctl arg for PIM layer:
 *	DR_CMD_CONNECT
 *	DR_CMD_DISCONNECT
 *	DR_CMD_CONFIGURE
 *	DR_CMD_UNCONFIGURE
 *	DR_CMD_RELEASE
 *	DR_CMD_CANCEL
 */
typedef struct {
	int32_t		ierr_num;
	char		ierr_str[MAXPATHLEN];
} sfdr_ioctl_err_t;

typedef struct {
	/*
	 * Input
	 */
	int		i_cpuid;	/* cpuid for probe */
	int		i_major;	/* safe check */
	uint_t		i_flags;
	/*
	 * Output
	 */
	int		i_isref;
	sfdr_ioctl_err_t	i_pim,	/* dr_error_t */
				i_psm;	/* sfdr_error_t */
} sfdr_ioctl_arg_t;

/*
 * sfdr_ioctl_arg_t.i_flags
 */
#define	SFDR_FLAG_FORCE		0x01
#define	SFDR_FLAG_AUTO_CPU	0x02

#ifdef _KERNEL
/*
 * 32bit support for ioctl arg.
 */
typedef struct {
	int32_t		i_cpuid;	/* cpuid for probe */
	int32_t		i_major;
	uint32_t	i_flags;
	int32_t		i_isref;
	sfdr_ioctl_err_t	i_pim,
				i_psm;
} sfdr_ioctl_arg32_t;
#endif /* _KERNEL */

#define	SFDR_ALLOC_ERR(sep) \
	((sep)->se_merr = 0, (sep)->se_mstr = GETSTRUCT(char, MAXPATHLEN))
#define	SFDR_FREE_ERR(sep) \
	(FREESTRUCT((sep)->se_mstr, char, MAXPATHLEN), (sep)->se_mstr = NULL)

#define	SFDR_GET_ERR(sep)		((sep)->se_merr)
#define	SFDR_SET_ERR(sep, en)		((sep)->se_merr = (en))
#define	SFDR_GET_ERRSTR(sep)		((sep)->se_mstr)
#define	SFDR_SET_ERRSTR(sep, es)	(strncpy((sep)->se_mstr, (es), \
						MAXPATHLEN))
#define	_SFDR_ERR_INTARR(sep, i)	(((int32_t *)SFDR_GET_ERRSTR(sep))[i])

#define	SFDR_GET_ERR_INT(sep, i) 	_SFDR_ERR_INTARR((sep), (i))
#define	SFDR_ERR_INT_IDX(sep)		_SFDR_ERR_INTARR((sep), 0)
/*
 * Set se_merr to "en", and set next integer in se_mstr[] array
 * to "n".
 */
#define	SFDR_SET_ERR_INT(sep, en, n) \
{ \
	register int32_t	_idx; \
	SFDR_SET_ERR((sep), (en)); \
	_idx = _SFDR_ERR_INTARR((sep), 0); \
	if (++_idx < SFDR_MAX_ERR_INT) { \
		_SFDR_ERR_INTARR((sep), _idx) = (int32_t)(n); \
		_SFDR_ERR_INTARR((sep), 0) = _idx; \
	} \
}
/*
 * Set se_merr to "en" and set se_mstr to "s".
 */
#define	SFDR_SET_ERR_STR(sep, en, s) \
{ \
	SFDR_SET_ERR((sep), (en)); \
	SFDR_SET_ERRSTR((sep), (s)); \
}
/*
 * Copy the sfdr error contents into the ioctl
 * error return.  Intended for both PIM and PSM usage.
 */
#define	SFDR_SET_IOCTL_ERR(iop, enum, estr) \
{ \
	(iop)->ierr_num = (int)(enum); \
	if ((estr) && (*(estr) != '\0')) \
		bcopy((caddr_t)(estr), \
			(caddr_t)(iop)->ierr_str, \
			sizeof ((iop)->ierr_str)); \
}

/*
 * Device states from Starfire's perspective.
 * PARTIAL state is really only relevant for board state.
 */
typedef enum {
	SFDR_STATE_EMPTY = 0,
	SFDR_STATE_OCCUPIED,
	SFDR_STATE_CONNECTED,
	SFDR_STATE_UNCONFIGURED,
	SFDR_STATE_PARTIAL,	/* part connected, part configured */
	SFDR_STATE_CONFIGURED,
	SFDR_STATE_RELEASE,
	SFDR_STATE_UNREFERENCED,
	SFDR_STATE_FATAL,
	SFDR_STATE_MAX
} sfdr_state_t;

#define	SFDR_NUM_STATES		((int)SFDR_STATE_MAX)

#define	SFDR_BOARD_IS_OS_RESIDENT(sb) \
		(((sb)->sb_state == SFDR_STATE_CONFIGURED) || \
		((sb)->sb_state == SFDR_STATE_RELEASE))
#ifdef _KERNEL
#define	SFDR_EXCL_LOCK_HELD()	(RW_WRITE_HELD(&sfdr_grwlock))
#define	SFDR_LOCK_HELD()	(RW_LOCK_HELD(&sfdr_grwlock))
#define	SFDR_LOCK_EXCL()	(rw_enter(&sfdr_grwlock, RW_WRITER))
#define	SFDR_LOCK_SHARED()	(rw_enter(&sfdr_grwlock, RW_READER))
#define	SFDR_LOCK_DOWNGRADE()	(rw_downgrade(&sfdr_grwlock))
#define	SFDR_UNLOCK()		(rw_exit(&sfdr_grwlock))

#ifdef DEBUG
#define	SFDR_DEVICE_TRANSITION(sb, nt, un, st) \
{ \
	int	_ostate, _nstate; \
	_ostate = (int)((sb)->sb_dev[NIX(nt)][un].u_common.sbdev_state); \
	_nstate = (int)(st); \
	PR_STATE("BOARD %d (%s.%d) STATE: %s(%d) -> %s(%d)\n", \
		(sb)->sb_num, \
		nt_str[nt], (un), \
		state_str[_ostate], _ostate, \
		state_str[_nstate], _nstate); \
	(sb)->sb_dev[NIX(nt)][un].u_common.sbdev_state = (st); \
}
#define	SFDR_BOARD_TRANSITION(sb, st) \
{ \
	PR_STATE("BOARD %d STATE: %s(%d) -> %s(%d)\n", \
		(sb)->sb_num, \
		state_str[(int)(sb)->sb_state], (int)(sb)->sb_state, \
		state_str[(int)(st)], (int)(st)); \
	(sb)->sb_pstate = (sb)->sb_state; \
	(sb)->sb_state = (st); \
}
#else /* DEBUG */
#define	SFDR_DEVICE_TRANSITION(sb, nt, un, st) \
		((sb)->sb_dev[NIX(nt)][un].u_common.sbdev_state = (st))
#define	SFDR_BOARD_TRANSITION(sb, st) \
		((sb)->sb_pstate = (sb)->sb_state, (sb)->sb_state = (st))
#endif /* DEBUG */

#define	SFDR_DEVICE_STATE(sb, nt, un) \
		((sb)->sb_dev[NIX(nt)][un].u_common.sbdev_state)
#define	SFDR_BOARD_STATE(sb) \
		((sb)->sb_state)
#define	SFDR_BOARD_PSTATE(sb) \
		((sb)->sb_pstate)
#define	SFDR_BOARD_ERROR_PENDING(sb)	((sb)->sb_lastop.l_dev != (dev_t)0)

#endif /* _KERNEL */

/*
 * The sfdr_lastop_t is a history buffer intended for use with
 * asynchronous operations.  It is only used if a RELEASE operation
 * for a particular device encountered an error.  The error information
 * is stored here until a subsequent DR operation is attempted which
 * will cause it to be automatically retrieved.  The error is saved
 * on a per-board basis.  Thus, the policy is that subsequent operations
 * on a particular board with an outstanding error cannot occur until
 * that error is retrieved.
 */
typedef struct sfdr_lastop {
	int		l_cmd;
	dev_t		l_dev;
	sfdr_ioctl_err_t	l_pimerr, l_psmerr;
} sfdr_lastop_t;

/*
 * Device status info.
 * These structures encapsulate the information previously
 * retrieved via:
 *	DR_SYS_BRDINFO, DR_CPU_COSTINFO, DR_CPU_CONFIG,
 *	DR_MEM_COSTINFO, DR_MEM_CONFIG, DR_MEM_STATE.
 * NOTE: If you change any of these, remember to change
 *	 the 32-bit support versions below.
 */
typedef struct {
	dr_nodetype_t	cs_type;	/* DR_NT_CPU */
	sfdr_state_t	cs_dstate;
	int		cs_isbootproc;
	processorid_t	cs_cpuid;
} sfdr_cpu_stat_t;

typedef struct {
	dr_nodetype_t	ms_type;	/* DR_NT_MEM */
	sfdr_state_t	ms_dstate;
	uint_t		ms_basepfn;
	uint_t		ms_totpages;
	uint_t		ms_detpages;
	int		ms_pageslost;
	uint_t		ms_mananged_pages;
	uint_t		ms_noreloc_pages;
	uint_t		ms_noreloc_first;
	uint_t		ms_noreloc_last;
	int		ms_cage_enabled;
} sfdr_mem_stat_t;

#define	SFDR_MAX_UNSAFE		16

typedef struct {
	dr_nodetype_t	is_type;	/* DR_NT_IO */
	sfdr_state_t	is_dstate;
	int		is_referenced;
	int		is_unsafe_count;
	int		is_unsafe_list[SFDR_MAX_UNSAFE];
} sfdr_io_stat_t;

typedef struct {
	dr_nodetype_t	c_type;
	sfdr_state_t	c_dstate;
} sfdr_common_stat_t;

typedef union {
	sfdr_common_stat_t	d_common;
	sfdr_cpu_stat_t		d_cpu;
	sfdr_mem_stat_t		d_mem;
	sfdr_io_stat_t		d_io;
} sfdr_dev_stat_t;

/*
 * The s_pbstate is the board state just previous to the
 * current board state as reported by s_bstate.  This information
 * is provided to the caller in order to determine sequence
 * of operations.  Specifically, it provides a method for the
 * daemon, upon recovery, to determine the sequence of operations
 * a board was headed.  This is necessary in situations where
 * the daemon has restarted and needs to determine what global
 * DR operation had been in-progress.
 */
typedef struct {
	int		s_board;	/* board containing device */
	sfdr_state_t	s_bstate;	/* board DR state */
	sfdr_state_t	s_pbstate;	/* previous board DR state */
	sfdr_lastop_t	s_lastop;
	int		s_nstat;
	sfdr_dev_stat_t	s_stat[MAX_DEV_UNITS_PER_BOARD];
} sfdr_stat_t;

#ifdef _KERNEL
/*
 * Suspend states used internally by sfdr_suspend and
 * sfdr_resume
 */
typedef enum sfdr_suspend_state {
	SFDR_SRSTATE_BEGIN = 0,
	SFDR_SRSTATE_USER,
	SFDR_SRSTATE_DAEMON,
	SFDR_SRSTATE_DRIVER,
	SFDR_SRSTATE_FULL
} suspend_state_t;

/*
 * 32bit support for sfdr_stat_t.
 */
typedef struct {
	int32_t		l_cmd;
	uint32_t	l_dev;
	sfdr_ioctl_err_t	l_pimerr, l_psmerr;
} sfdr_lastop32_t;

typedef struct {
	int32_t		cs_type;	/* DR_NT_CPU */
	int32_t		cs_dstate;
	int32_t		cs_isbootproc;
	int32_t		cs_cpuid;
} sfdr_cpu_stat32_t;

typedef struct {
	int32_t		ms_type;	/* DR_NT_MEM */
	int32_t		ms_dstate;
	uint32_t	ms_basepfn;
	uint32_t	ms_totpages;
	uint32_t	ms_detpages;
	int32_t		ms_pageslost;
	uint32_t	ms_mananged_pages;
	uint32_t	ms_noreloc_pages;
	uint32_t	ms_noreloc_first;
	uint32_t	ms_noreloc_last;
	int32_t		ms_cage_enabled;
} sfdr_mem_stat32_t;

typedef struct {
	int32_t		is_type;	/* DR_NT_IO */
	int32_t		is_dstate;
	int32_t		is_referenced;
	int32_t		is_unsafe_count;
	int32_t		is_unsafe_list[SFDR_MAX_UNSAFE];
} sfdr_io_stat32_t;

typedef struct {
	int32_t		c_type;
	int32_t		c_dstate;
} sfdr_common_stat32_t;

typedef union {
	sfdr_common_stat32_t	d_common;
	sfdr_cpu_stat32_t	d_cpu;
	sfdr_mem_stat32_t	d_mem;
	sfdr_io_stat32_t	d_io;
} sfdr_dev_stat32_t;

typedef struct {
	int32_t			s_board;	/* board containing device */
	int32_t			s_bstate;	/* board DR state */
	int32_t			s_pbstate;	/* previous board DR state */
	sfdr_lastop32_t		s_lastop;
	int32_t			s_nstat;
	sfdr_dev_stat32_t	s_stat[MAX_DEV_UNITS_PER_BOARD];
} sfdr_stat32_t;

typedef ushort_t	sfdr_devset_t;

/*
 * Starfire specific PIM/PSM interface handle.
 * dr_handle_t MUST appear first.
 */
typedef struct sfdr_handle {
	dr_handle_t		sh_handle;
	void			*sh_arg;	/* ioctl */
	sfdr_devset_t		sh_devset;	/* based on h_dev */
	uint_t			sh_flags;
	sfdr_error_t		sh_err;
	sfdr_ioctl_arg_t	*sh_iap;
	int			sh_ref;
	uint_t			sh_ndi;
	struct sfdr_handle	*sh_next;
} sfdr_handle_t;

#define	SFDR_HANDLE_REF(shp)		((shp)->sh_ref)
#define	SFDR_HANDLE_REF_ADD(shp, n)	((shp)->sh_ref += (n))
#define	SFDR_HANDLE_REF_DECR(shp)	((shp)->sh_ref--)

/*
 * Starfire specific suspend/resume interface handle
 */
typedef struct {
	dr_handle_t		*sr_dr_handlep;
	dev_info_t		*sr_failed_dip;
	suspend_state_t		sr_suspend_state;
	uint_t			sr_flags;
} sfdr_sr_handle_t;

/*
 * Unsafe devices based on dr.conf prop "unsupported-io-drivers"
 */
typedef struct {
	char		**devnames;
	uint_t		ndevs;
} sfdr_unsafe_devs_t;

#define	SFDR_MAXNUM_NT		3
#define	NIX(t)			(((t) == DR_NT_CPU) ? 0 : \
				((t) == DR_NT_MEM) ? 1 : \
				((t) == DR_NT_IO) ? 2 : SFDR_MAXNUM_NT)

/*
 * Starfire specific board structure.
 * board_t MUST appear first.
 * NOTE: There is only mem-unit possible per board in Starfire,
 *	 however we'll still declare sb_mem[] as an array for
 *	 possible extendibility to other platforms (Serengeti?).
 */
typedef struct sfdr_mem_unit {
	sfdr_state_t	sbm_state;		/* mem-unit state */
	uint_t		sbm_flags;
	pfn_t		sbm_basepfn;
	pgcnt_t		sbm_npages;
	int		sbm_dimmsize;
	/*
	 * The following fields are used during
	 * the memory detach process only.
	 */
	dnode_t		sbm_target_nodeid;
	int		sbm_target_board;
	struct memlist	*sbm_mlist;
	memhandle_t	sbm_memhandle;
} sfdr_mem_unit_t;

/*
 * Currently only maintain state information for individual
 * components.
 */
typedef struct sfdr_cpu_unit {
	sfdr_state_t	sbc_state;		/* cpu-unit state */
	processorid_t	sbc_cpu_id;
	int		sbc_cpu_status;
} sfdr_cpu_unit_t;

typedef struct sfdr_io_unit {
	sfdr_state_t	sbi_state;		/* io-unit state */
} sfdr_io_unit_t;

typedef struct sfdr_common_unit {
	sfdr_state_t	sbdev_state;
} sfdr_common_unit_t;

typedef union {
	sfdr_common_unit_t	u_common;
	sfdr_mem_unit_t		_mu;
	sfdr_cpu_unit_t		_cu;
	sfdr_io_unit_t		_iu;
} sfdr_dev_unit_t;

typedef struct {
/*  0 */	board_t		sb_bd;
/*  8 */	sfdr_handle_t	*sb_handle;
/*  c */	int		sb_ref;		/* # of handle references */

/* 10 */	int		sb_num;		/* board number */
/* 14 */	dev_info_t	*sb_dip;	/* dip for make-nodes */
/* 18 */	sfdr_state_t	sb_state;	/* (current) board state */
/* 1c */	sfdr_state_t	sb_pstate;	/* previous board state */
		/*
		 * 0=CPU, 1=MEM, 2=IO, 3=NULL
		 */
/* 20 */	dnode_t		*sb_devlist[SFDR_MAXNUM_NT + 1];

/* 30 */	sfdr_devset_t	sb_dev_present;		/* present mask */
/* 32 */	sfdr_devset_t	sb_dev_attached;	/* attached mask */
/* 34 */	sfdr_devset_t	sb_dev_released;	/* released mask */
/* 36 */	sfdr_devset_t	sb_dev_unreferenced;	/* unreferenced mask */
/* 38 */	sfdr_dev_unit_t	*sb_dev[SFDR_MAXNUM_NT];

/* 44 */	sfdr_lastop_t	sb_lastop;
} sfdr_board_t;

#define	SFDR_BOARD_REF(sbp)		((sbp)->sb_ref)
#define	SFDR_BOARD_REF_INCR(sbp)	((sbp)->sb_ref++)
#define	SFDR_BOARD_REF_DECR(sbp)	((sbp)->sb_ref--)

#define	SFDR_GET_BOARD_MEMUNIT(sb, un) \
			(&((sb)->sb_dev[NIX(DR_NT_MEM)][un]._mu))
#define	SFDR_GET_BOARD_CPUUNIT(sb, un) \
			(&((sb)->sb_dev[NIX(DR_NT_CPU)][un]._cu))
#define	SFDR_GET_BOARD_IOUNIT(sb, un) \
			(&((sb)->sb_dev[NIX(DR_NT_IO)][un]._iu))

typedef ushort_t	boardset_t;	/* assumes 16 boards max */

#define	BOARDSET(b)		((boardset_t)(1 << (b)))
#define	BOARD_IN_SET(bs, b)	(((bs) & BOARDSET(b)) != 0)
#define	BOARD_ADD(bs, b)	((bs) |= BOARDSET(b))
#define	BOARD_DEL(bs, b)	((bs) &= ~BOARDSET(b))

/*
 * Format of sfdr_devset_t bit masks:
 *	15       8    4    0
 *	|....|..II|...M|CCCC|
 * 1 = indicates respective component present/attached.
 * I = I/O, M = Memory, C = CPU.
 */
#define	DEVSET_ANYUNIT		(-1)
#define	_NT2DEVPOS(t, u)	((NIX(t) << 2) + (u))
#define	_DEVSET_MASK		0x031f
#define	DEVSET(t, u) \
	(((u) == DEVSET_ANYUNIT) ? \
			(sfdr_devset_t)((0xf << _NT2DEVPOS((t), 0)) & \
					_DEVSET_MASK) : \
			(sfdr_devset_t)(1 << _NT2DEVPOS((t), (u))))
#define	DEVSET_IN_SET(ds, t, u)	(((ds) & DEVSET((t), (u))) != 0)
#define	DEVSET_ADD(ds, t, u)	((ds) |= DEVSET((t), (u)))
#define	DEVSET_DEL(ds, t, u)	((ds) &= ~DEVSET((t), (u)))
#define	DEVSET_GET_UNITSET(ds, t) \
	(((ds) & DEVSET((t), DEVSET_ANYUNIT)) >> _NT2DEVPOS((t), 0))
/*
 * Ops for sfdr_board_t.sb_dev_present
 */
#define	SFDR_DEV_IS_PRESENT(bp, nt, un) \
			DEVSET_IN_SET((bp)->sb_dev_present, (nt), (un))
#define	SFDR_DEV_SET_PRESENT(bp, nt, un) \
			DEVSET_ADD((bp)->sb_dev_present, (nt), (un))
#define	SFDR_DEV_CLR_PRESENT(bp, nt, un) \
			DEVSET_DEL((bp)->sb_dev_present, (nt), (un))
/*
 * Ops for sfdr_board_t.sb_dev_attached
 */
#define	SFDR_DEV_IS_ATTACHED(bp, nt, un) \
			DEVSET_IN_SET((bp)->sb_dev_attached, (nt), (un))
#define	SFDR_DEV_SET_ATTACHED(bp, nt, un) \
			DEVSET_ADD((bp)->sb_dev_attached, (nt), (un))
#define	SFDR_DEV_CLR_ATTACHED(bp, nt, un) \
			DEVSET_DEL((bp)->sb_dev_attached, (nt), (un))
/*
 * Ops for sfdr_board_t.sb_dev_released
 */
#define	SFDR_DEV_IS_RELEASED(bp, nt, un) \
			DEVSET_IN_SET((bp)->sb_dev_released, (nt), (un))
#define	SFDR_DEV_SET_RELEASED(bp, nt, un) \
			DEVSET_ADD((bp)->sb_dev_released, (nt), (un))
#define	SFDR_DEV_CLR_RELEASED(bp, nt, un) \
			DEVSET_DEL((bp)->sb_dev_released, (nt), (un))
/*
 * Ops for sfdr_board_t.sb_dev_unreferenced
 */
#define	SFDR_DEV_IS_UNREFERENCED(bp, nt, un) \
			DEVSET_IN_SET((bp)->sb_dev_unreferenced, (nt), (un))
#define	SFDR_DEV_SET_UNREFERENCED(bp, nt, un) \
			DEVSET_ADD((bp)->sb_dev_unreferenced, (nt), (un))
#define	SFDR_DEV_CLR_UNREFERENCED(bp, nt, un) \
			DEVSET_DEL((bp)->sb_dev_unreferenced, (nt), (un))

#define	SFDR_DEVS_PRESENT(bp) \
			((bp)->sb_dev_present)
#define	SFDR_DEVS_ATTACHED(bp) \
			((bp)->sb_dev_attached)
#define	SFDR_DEVS_RELEASED(bp) \
			((bp)->sb_dev_released)
#define	SFDR_DEVS_UNREFERENCED(bp) \
			((bp)->sb_dev_unreferenced)
#define	SFDR_DEVS_UNATTACHED(bp) \
			((bp)->sb_dev_present & ~(bp)->sb_dev_attached)
#define	SFDR_DEVS_CONFIGURE(bp, devs) \
			((bp)->sb_dev_attached = (devs))
#define	SFDR_DEVS_DISCONNECT(bp, devs) \
			((bp)->sb_dev_present &= ~(devs))
#define	SFDR_DEVS_CANCEL(bp, devs) \
			((bp)->sb_dev_released &= ~(devs), \
			(bp)->sb_dev_unreferenced &= ~(devs))

/*
 * sfdr_board_t.sbmem[].sbm_flags
 */
#define	SFDR_MFLAG_TARGET	0x01	/* board selected as target */
#define	SFDR_MFLAG_MEMMOVE	0x02	/* copy-rename needed */
#define	SFDR_MFLAG_MEMRESIZE	0x04	/* move from small to big board */
#define	SFDR_MFLAG_RELDONE	0x08	/* memory release (delete) done */

#endif /* _KERNEL */

typedef struct {
	int	sfio_cmd;
	void	*sfio_arg;
} sfdr_ioctl_t;

#ifdef _KERNEL
/*
 * 32bit support for sfdr_ioctl_t.
 */
typedef struct {
	int32_t		sfio_cmd;
	uint32_t	sfio_arg;
} sfdr_ioctl32_t;

/*
 * PSM-DR layers are only allowed to use lower 16 bits of dev_t.
 * B    - bottom 4 bits are for the slot number.
 * D    - device type chosen (0 = indicates all devices in slot).
 * U	- unit number if specific device type chosen.
 * X    - not used.
 *
 * Upper      Lower
 * XXXXUUUUDDDDBBBB
 *
 * Note that this format only allows attachment points to
 * either represent all the units on a board or one particular
 * unit.  A more general specification would permit any combination
 * of specific units and types to be represented by individual
 * attachment points.
 */
#define	SFDR_DEV_SLOTMASK	0x000f
/*
 * These device level definitions are primarily for unit testing.
 */
#define	SFDR_DEV_UNITMASK	0x0f00
#define	SFDR_DEV_UNITSHIFT	8
#define	SFDR_DEV_CPU		0x0010
#define	SFDR_DEV_MEM		0x0020
#define	SFDR_DEV_IO		0x0040
#define	SFDR_DEV_TYPEMASK	(SFDR_DEV_CPU | SFDR_DEV_MEM | SFDR_DEV_IO)
#define	SFDR_DEV_TYPESHIFT	4

/*
 * Slot, Instance, and Minor number Macro definitions
 */
#define	SLOT2DEV(s)		((s) & SFDR_DEV_SLOTMASK)
#define	GETSLOT(unit)		((unit) & SFDR_DEV_SLOTMASK)
/*
 * The following is primarily for unit testing.
 */
#define	ALLCPU2DEV(s)		(SFDR_DEV_CPU | SLOT2DEV(s))
#define	ALLMEM2DEV(s)		(SFDR_DEV_MEM | SLOT2DEV(s))
#define	ALLIO2DEV(s)		(SFDR_DEV_IO | SLOT2DEV(s))
#define	_UNIT2DEV(u)		(((u) << SFDR_DEV_UNITSHIFT) & \
					SFDR_DEV_UNITMASK)
#define	CPUUNIT2DEV(s, c)	(_UNIT2DEV(c) | ALLCPU2DEV(s))
#define	MEMUNIT2DEV(s, m)	(_UNIT2DEV(m) | ALLMEM2DEV(s))
#define	IOUNIT2DEV(s, i)	(_UNIT2DEV(i) | ALLIO2DEV(s))

#define	DEV_IS_ALLUNIT(d)	(((d) & SFDR_DEV_UNITMASK) == 0)
#define	_DEV_IS_ALLTYPE(d)	(((d) & SFDR_DEV_TYPEMASK) == 0)
#define	DEV_IS_ALLBOARD(d)	(DEV_IS_ALLUNIT(d) && _DEV_IS_ALLTYPE(d))
#define	DEV_IS_CPU(d)		((d) & SFDR_DEV_CPU)
#define	DEV_IS_MEM(d)		((d) & SFDR_DEV_MEM)
#define	DEV_IS_IO(d)		((d) & SFDR_DEV_IO)
#define	DEV_IS_ALLCPU(d)	(DEV_IS_ALLUNIT(d) && DEV_IS_CPU(d))
#define	DEV_IS_ALLMEM(d)	(DEV_IS_ALLUNIT(d) && DEV_IS_MEM(d))
#define	DEV_IS_ALLIO(d)		(DEV_IS_ALLUNIT(d) && DEV_IS_IO(d))
#define	DEV2UNIT(d) \
		((((d) & SFDR_DEV_UNITMASK) >> SFDR_DEV_UNITSHIFT) - 1)
#define	DEV2NT(d) \
		(DEV_IS_MEM(d) ? DR_NT_MEM : \
		DEV_IS_CPU(d) ? DR_NT_CPU : \
		DEV_IS_IO(d) ? DR_NT_IO : DR_NT_UNKNOWN)

/*
 * Macros to cast between PIM and PSM layers of the following
 * structures:
 *	board_t		<-> sfdr_board_t
 *	dr_handle_t	<-> sfdr_handle_t
 *	dr_error_t	<-> sfdr_error_t
 *	slot		-> board_t
 *	slot		-> sfdr_board_t
 *	sfdr_board_t	-> dr_handle_t
 *	sfdr_sr_handle_t -> dr_handle_t
 *	sfdr_sr_handle_t -> sfdr_handle_t
 *	dr_handle_t	-> sfdr_error_t
 */
#define	BD2MACHBD(bd)		((sfdr_board_t *)(bd))
#define	MACHBD2BD(mbd)		((board_t *)&((mbd)->sb_bd))

#define	HD2MACHHD(hd)		((sfdr_handle_t *)(hd))
#define	MACHHD2HD(mhd)		((dr_handle_t *)&((mhd)->sh_handle))

#define	ERR2MACHERR(err)	((sfdr_error_t *)(err))
#define	MACHERR2ERR(merr)	((dr_error_t *)&((merr)->se_err))

#define	BSLOT2MACHBD(b)		(&(sfdr_boardlist[b]))
#define	BSLOT2BD(slot)		MACHBD2BD(BSLOT2MACHBD(slot))

#define	MACHBD2HD(sbp)		MACHHD2HD((sbp)->sb_handle)

#define	HD2MACHERR(hd)		ERR2MACHERR(DR_HD2ERR(hd))

#define	MACHSRHD2HD(srh)	((srh)->sr_dr_handlep)

#endif /* _KERNEL */
/*
 * ioctl commands to PSM-DR layer.
 */
#define	_SFDR_IOC		(('X' << 16) | ('F' << 8))

#define	SFDR_JUGGLE_BOOTPROC	(_SFDR_IOC | 0x01)
#define	SFDR_TEST_SUSPEND	(_SFDR_IOC | 0x02)
#define	SFDR_DUMP_PDAINFO	(_SFDR_IOC | 0x03)
#define	SFDR_TEST_CAGE		(_SFDR_IOC | 0x04)

#ifdef _KERNEL

/*
 * Some stuff to assist in debug.
 */
#ifdef DEBUG
#define	SFDRDBG_STATE	0x00000001
#define	SFDRDBG_QR	0x00000002
#define	SFDRDBG_CPU	0x00000004
#define	SFDRDBG_MEM	0x00000008
#define	SFDRDBG_IO	0x00000010
#define	SFDRDBG_HW	0x00000020

#define	PR_ALL		if (dr_debug)			printf
#define	PR_STATE	if (dr_debug & SFDRDBG_STATE)	printf
#define	PR_QR		if (dr_debug & SFDRDBG_QR)	prom_printf
#define	PR_CPU		if (dr_debug & SFDRDBG_CPU)	printf
#define	PR_MEM		if (dr_debug & SFDRDBG_MEM)	printf
#define	PR_IO		if (dr_debug & SFDRDBG_IO)	printf
#define	PR_HW		if (dr_debug & SFDRDBG_HW)	printf

#define	MEMLIST_DUMP(ml)	memlist_dump(ml)

extern uint_t	dr_debug;
#else /* DEBUG */
#define	PR_ALL		if (0) printf
#define	PR_STATE	PR_ALL
#define	PR_QR		PR_ALL
#define	PR_CPU		PR_ALL
#define	PR_MEM		PR_ALL
#define	PR_IO		PR_ALL
#define	PR_HW		PR_ALL

#define	MEMLIST_DUMP(ml)
#endif /* DEBUG */
extern char	*state_str[];
extern char	*nt_str[];

/*
 * IMPORTANT:
 * The following two defines are also coded into OBP, so if they
 * need to change here, don't forget to change OBP also.
 */
#define	SFDR_OBP_PROBE_GOOD	0
#define	SFDR_OBP_PROBE_BAD	1

#define	SFDR_PROMPROP_MEMAVAIL	"dr-available"

extern sfdr_board_t	*sfdr_boardlist;

extern int		sfdr_disconnect_cpu(dr_handle_t *hp, int unit);
extern int		sfdr_disconnect_mem(dr_handle_t *hp, int unit);
extern int		sfdr_disconnect_io(dr_handle_t *hp, int unit);
extern int		sfdr_get_unit(dnode_t nodeid,
					dr_nodetype_t nodetype);
extern sfdr_sr_handle_t	*sfdr_get_sr_handle(dr_handle_t *handle);
extern void		sfdr_release_sr_handle(sfdr_sr_handle_t *srh);
extern int		sfdr_suspend(sfdr_sr_handle_t *srh);
extern void		sfdr_resume(sfdr_sr_handle_t *srh);
extern void		sfdr_check_devices(dev_info_t *dip, int *refcount,
					dr_handle_t *handle);
extern struct memlist	*sfdr_get_memlist(dnode_t nodeid);
extern int		sfdr_release_dev_done(dr_handle_t *hp,
					dr_nodetype_t nodetype, int unit);
extern void		sfdr_init_mem_unit(sfdr_board_t *sbp, int unit);
extern void		sfdr_release_mem_done(dr_handle_t *hp, int unit);
extern void		sfdr_release_cleanup(dr_handle_t *hp);
extern int		sfdr_cancel_cpu(dr_handle_t *hp, int unit);
extern int		sfdr_cancel_mem(dr_handle_t *hp, int unit);
extern dr_nodetype_t	sfdr_get_devtype(dr_handle_t *hp, dnode_t nodeid);
extern int		sfdr_get_board(dnode_t nodeid);
extern int		sfhw_get_base_physaddr(dnode_t nodeid,
					uint64_t *basepa);
extern int		sfhw_program_memctrl(dnode_t nodeid, int boardnum);
extern int		sfhw_deprogram_memctrl(dnode_t nodeid, int boardnum);
extern void		sfhw_idle_interconnect(void *ph);
extern void		sfhw_resume_interconnect(void *ph);
extern int		sfhw_cpu_pc_idle(int cpuid);
extern int		sfhw_cpu_reset_on(int cpuid);
extern int		sfhw_cpu_reset_off(int cpuid);
extern void		sfdr_memscrub(struct memlist *mlist);
extern int		sfdr_move_memory(dr_handle_t *hp,
					struct memlist *mlist);
extern void		memlist_delete(struct memlist *mlist);
extern struct memlist	*memlist_dup(struct memlist *mlist);
extern void		memlist_dump(struct memlist *mlist);
extern int		memlist_intersect(struct memlist *alist,
					struct memlist *blist);
extern uint64_t		mc_get_asr_addr(dnode_t nodeid);
extern uint64_t		mc_get_idle_addr(dnode_t nodeid);
extern int		mc_read_asr(dnode_t nodeid, uint_t *mcregp);
extern uint64_t		mc_asr_to_pa(uint_t mcreg);
extern int		mc_get_dimm_size(dnode_t nodeid);
extern uint64_t		mc_get_mem_alignment();
extern struct memlist	*sf_reg_to_memlist(struct sf_memunit_regspec rlist[],
					int rblks);
extern int		sfdr_juggle_bootproc(dr_handle_t *hp,
					processorid_t cpuid);

extern void		memlist_read_lock();
extern void		memlist_read_unlock();

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SFDR_H */
