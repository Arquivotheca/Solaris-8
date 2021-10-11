/*
 * Copyright (c) 1996,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ZSDEV_H
#define	_SYS_ZSDEV_H

#pragma ident	"@(#)zsdev.h	1.33	98/01/06 SMI"

/*
 * Onboard serial ports.
 * Device dependent software definitions.
 */


/*
 * Chip, buffer, and register definitions for Z8530 SCC
 */

#include <sys/spl.h>
#include <sys/ksynch.h>
#include <sys/dditypes.h>
#include <sys/ser_zscc.h>

#ifdef _MACHDEP
#include <sys/zsmach.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _MACHDEP
#define	ZSDELAY()
#define	ZSFLUSH()
#define	ZSNEXTPOLL(zscurr)
#endif

/*
 * OUTLINE defines the high-order flag bit in the minor device number that
 * controls use of a tty line for dialin and dialout simultaneously.
 */
#define	OUTLINE		((minor_t)1 << (NBITSMINOR32 - 1))
#define	UNIT(x)		(getminor(x) & ~OUTLINE)

#define	ZSWR1_INIT	(ZSWR1_SIE|ZSWR1_TIE|ZSWR1_RIE)

extern int zs_usec_delay;


#define	ZS_REG_SIZE	(2 * sizeof (struct zscc_device))

#define	PCLK		(19660800/4)	/* basic clock rate for UARTs */

#define	SDLCFLAG	0x7E

#define	ZS_ON		(ZSWR5_DTR|ZSWR5_RTS)
#define	ZS_OFF		0

/*
 * Modem control commands.
 */
#define	DMSET   0
#define	DMBIS   1
#define	DMBIC   2
#define	DMGET   3

/*
 * Macros to access a port
 */
#define	SCC_WRITEA(reg, val) { \
	((struct zscc_device *) \
	((uintptr_t)zs->zs_addr | ZSOFF))->zscc_control = reg; \
	ZSDELAY(); \
	((struct zscc_device *) \
	((uintptr_t)zs->zs_addr | ZSOFF))->zscc_control = val; \
	ZSDELAY(); \
	zs->zs_wreg[reg] = val; \
}
#define	SCC_WRITEB(reg, val) { \
	((struct zscc_device *) \
	((uintptr_t)zs->zs_addr & ~ZSOFF))->zscc_control = reg; \
	ZSDELAY(); \
	((struct zscc_device *) \
	((uintptr_t)zs->zs_addr & ~ZSOFF))->zscc_control = val; \
	ZSDELAY(); \
	zs->zs_wreg[reg] = val; \
}
#define	SCC_WRITE(reg, val)  { \
	zs->zs_addr->zscc_control = reg; \
	ZSDELAY(); \
	zs->zs_addr->zscc_control = val; \
	ZSDELAY(); \
	zs->zs_wreg[reg] = val; \
}

#define	SCC_READA(reg, var) { \
	((struct zscc_device *) \
	((uintptr_t)zs->zs_addr | ZSOFF))->zscc_control = reg; \
	ZSDELAY(); \
	var = ((struct zscc_device *) \
	((uintptr_t)zs->zs_addr | ZSOFF))->zscc_control; \
	ZSDELAY(); \
}
#define	SCC_READB(reg, var) { \
	((struct zscc_device *) \
	((uintptr_t)zs->zs_addr & ~ZSOFF))->zscc_control = reg; \
	ZSDELAY(); \
	var = ((struct zscc_device *) \
	((uintptr_t)zs->zs_addr & ~ZSOFF))->zscc_control; \
	ZSDELAY(); \
}
#define	SCC_READ(reg, var) { \
	register struct zscc_device *tmp; \
	tmp = zs->zs_addr; \
	tmp->zscc_control = reg; \
	ZSDELAY(); \
	var = tmp->zscc_control; \
	ZSDELAY(); \
}

#define	SCC_BIS(reg, val) { \
	zs->zs_addr->zscc_control = reg; \
	ZSDELAY(); \
	zs->zs_addr->zscc_control =  zs->zs_wreg[reg] |= val; \
	ZSDELAY(); \
}

#define	SCC_BIC(reg, val) { \
	zs->zs_addr->zscc_control = reg; \
	ZSDELAY(); \
	zs->zs_addr->zscc_control =  zs->zs_wreg[reg] &= ~val; \
	ZSDELAY(); \
}


#define	SCC_WRITE0(val) { \
	zs->zs_addr->zscc_control = val; \
	ZSDELAY(); \
	ZSFLUSH(); \
}

#define	SCC_WRITEDATA(val) { \
	zs->zs_addr->zscc_data = val; \
	ZSDELAY(); \
	ZSFLUSH(); \
}
#define	SCC_READ0()	zs->zs_addr->zscc_control
#define	SCC_READDATA()	zs->zs_addr->zscc_data


/*
 * Protocol specific entry points for driver routines.
 */
struct zsops {
	void	(*zsop_txint)();	/* xmit buffer empty */
	void	(*zsop_xsint)();	/* external/status */
	void	(*zsop_rxint)();	/* receive char available */
	void	(*zsop_srint)();	/* special receive condition */
	int	(*zsop_softint)();	/* second stage interrupt handler */
	int	(*zsop_suspend)();	/* suspend driver */
	int	(*zsop_resume)();	/* resume driver */
};

/*
 * Hardware channel common data.  One structure per port.
 * Each of the fields in this structure is required to be protected by a
 * mutex lock at the highest priority at which it can be altered.
 * The zs_flags, zs_wreg and zs_next fields can be altered by interrupt
 * handling code that runs at ZS_PL_HI (IPL 12), so they must be protected
 * by the mutex whose handle is stored in zs_excl_hi.  All others can be
 * protected by the zs_excl mutex, which is lower priority and adaptive.
 */
#define	ZS_MAX_PRIV_STR 800 /* int */
struct zscom {
	void		(*zs_txint)();	/* SCC interrupt vector routines */
	unsigned char	*zs_wr_cur;
	unsigned char 	*zs_wr_lim;
	void		(*zs_rxint)();	/* SCC interrupt vector routines */
	unsigned char 	*zs_rd_cur;
	unsigned char 	*zs_rd_lim;
	struct zscc_device *zs_addr;	/* address of second half of chip */
	void		(*zs_xsint)();	/* SCC interrupt vector routines */
	void		(*zs_srint)();	/* SCC interrupt vector routines */
	int		(*zs_suspend)(); /* routine to suspend driver */
	int		(*zs_resume)();	/* routine to resume driver */
	uchar_t		zs_wreg[16];	/* shadow of write registers */
	caddr_t		zs_priv;	/* protocol private data */
	struct zsops	*zs_ops;	/* basic operations vectors */
	dev_info_t	*zs_dip;	/* dev_info */
	time_t		zs_dtrlow;	/* time dtr went low */
	short		zs_unit;	/* which channel (0:NZSLINE) */
	/*
	 * The zs_wreg, zs_next and zs_flags fields
	 * are protected by zs_excl_hi.
	 */
	uchar_t		zs_suspended;	/* True, if suspended */
	struct zs_prog	*zs_prog_save;	/* H/W state, saved for CPR */
	struct zscom	*zs_next; /* next in the circularly linked list */
	struct zscom	*zs_back; /* back in the circularly linked list */

	kmutex_t	*zs_excl_hi;	/* zs spinlock mutex */
	kmutex_t	*zs_excl;	/* zs adaptive mutex */
	kmutex_t	*zs_ocexcl;	/* zs adaptive mutex for open/close */
	kcondvar_t	zs_flags_cv;	/* condition variable for flags */
	ulong_t		zs_priv_str[ZS_MAX_PRIV_STR];
	uchar_t 	zs_flags_need;	/* random flags */
	uchar_t		zs_flags_doing;	/* random flags */
	kstat_t		*intrstats;	/* interrupt statistics */
};

/*
 * Definition for zs_flags field
 */
#define	ZS_NEEDSOFT	1

#ifdef	_KERNEL
#define	ZS_H_LOG_MAX	0x8000
/*
 * ZSSETSOFT macro to pend a level 3 interrupt if one isn't already pending.
 */

extern kmutex_t	zs_soft_lock;		/* ptr to lock for zssoftpend */
extern int zssoftpend;			/* secondary interrupt pending */

extern ddi_softintr_t zs_softintr_id;
#define	ZSSETSOFT(zs)	{		\
	zs->zs_flags_need = ZS_NEEDSOFT;	\
	if (!zssoftpend)  { 		\
		zssoftpend = 1;		\
	ddi_trigger_softintr(zs_softintr_id); \
	}				\
}

#endif	/* _KERNEL */

/*
 * Lock priority definitions.
 * XXX: These should be obtained from configuration data, eventually.
 */
#define	ZS_PL   ipltospl(SPL3)		/* translates to SPARC IPL 6  */
#define	ZS_PL_HI ipltospl(SPLTTY)	/* translates to SPARC IPL 12 */

/*
 * Definitions for generic SCC programming routine
 */
struct zs_prog {
	struct zscom	*zs;	/* common data for this channel */
	uchar_t		flags;	/* see definitions below */
	uchar_t		wr4;	/* misc parameters and modes */
	uchar_t		wr11;	/* clock mode control */
	uchar_t		wr12;	/* BRG time constant Lo byte */
	uchar_t		wr13;	/* BRG time constant Hi byte */
	uchar_t		wr3;	/* receiver parameters and control */
	uchar_t		wr5;	/* transmitter parameters and control */
	uchar_t		wr15;	/* external status interrupt control */
};

/*
 * Definitions for zs_prog flags field
 */
#define	ZSP_SYNC		01	/* 0 = async line; 1 = synchronous */
#define	ZSP_NRZI		02	/* 0 = NRZ encoding; 1 = NRZI */
#define	ZSP_PLL			04	/* request use of PLL clock source */
#define	ZSP_LOOP		010	/* request interal loopback mode */
#define	ZSP_PARITY_SPECIAL	020	/* parity error causes ext status int */
#define	ZSP_ECHO		040	/* request auto echo mode */

extern void	zsa_init(struct zscom *zs);
extern int	zsmctl(struct zscom *zs, int bits, int how);
extern void	zs_program(struct zs_prog *zspp);
extern void	zsopinit(struct zscom *zs, struct zsops *zso);
extern void	setzssoft(void);

#ifdef	__cplusplus
}
#endif

#endif	/* !_SYS_ZSDEV_H */
