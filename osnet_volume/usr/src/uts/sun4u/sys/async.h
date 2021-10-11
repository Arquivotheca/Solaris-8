/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ASYNC_H
#define	_SYS_ASYNC_H

#pragma ident	"@(#)async.h	1.23	99/04/27 SMI"

#include <sys/privregs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

struct proc;				/* forward, in lieu of thread.h */

typedef	uint_t	(*afunc)();

struct async_flt {
	uint64_t	flt_stat;	/* async. fault stat. reg. */
	uint64_t	flt_addr;	/* async. fault addr. reg. */
	uint_t		flt_status;	/* error information */
	uint32_t	flt_bus_id;	/* bus id# of cpu/sbus/pci */
	uint32_t	flt_inst;	/* instance of cpu/sbus/pci */
	afunc		flt_func;	/* logging func for fault */
	struct proc 	*flt_proc;	/* curproc */
	caddr_t		flt_pc;		/* pc where curthread got ecc error */
	ushort_t	flt_in_use;	/* fault entry being used */
	ushort_t	flt_synd;	/* ECC syndrome */
	ushort_t	flt_size;	/* size of failed transfer */
	ushort_t	flt_offset;	/* offset of fault failed transfer */
	ushort_t	flt_in_memory;	/* fault occured in memory */
};

struct bus_func {
	ushort_t	ftype;		/* function type */
	afunc		func;		/* function to run */
	caddr_t		farg;		/* argument (pointer to soft state) */
};

extern void error_init(void);
extern void error_disable(void);
extern void register_bus_func(short type, afunc func, caddr_t arg);
extern void unregister_bus_func(caddr_t arg);

extern void ce_error(struct async_flt *ecc);
extern void ue_error(struct async_flt *ecc);
extern void bto_error(struct async_flt *bto);
extern int ue_check_buses(void);
extern void ce_count_unum(int status, int len, char *unum);
extern void ce_log_status(int status, char *unum);

extern	int	ce_verbose;
extern	int	ce_enable_verbose;
extern	int	ce_show_data;
extern	int	ce_debug;
extern	int	ue_debug;

#endif	/* !_ASM */

/*
 * Uncorrectable error logging return values.
 */
#define	UE_USER_FATAL	0x0		/* NonPriv. UnCorrectable ECC Error */
#define	UE_FATAL	0x1		/* Priv. UnCorrectable ECC Error */
#define	UE_DEBUG	0x2		/* Debugging loophole */

/*
 * ECC Error status.
 */
#define	ECC_C_TRAP	0x1		/* trap 0x63 ECC Error */
#define	ECC_ID_TRAP	0x2		/* I,D trap ECC Error */
#define	ECC_ECACHE	0x4		/* ecache fast ECC Error */
#define	ECC_IOBUS	0x8		/* pci or sysio ECC Error */
#define	ECC_INTERMITTENT 0x10		/* Intermittent ECC Error */
#define	ECC_PERSISTENT	0x20		/* Persistent ECC Error */
#define	ECC_STICKY	0x40		/* Sticky ECC Error */

/*
 * Types of error functions (for register_bus_func type field)
 */
#define	UE_ECC_FTYPE	0x0001		/* UnCorrectable ECC Error */
#define	DIS_ERR_FTYPE	0x0004		/* Disable errors */

/*
 * Maximum length of unum string returned from the prom.
 */
#define	UNUM_NAMLEN	60

/*
 * Alignment macros
 */
#define	ALIGN_64(i)	((i) & ~0x3F)
#define	ALIGN_32(i)	((i) & ~0x1F)
#define	ALIGN_16(i)	((i) & ~0xF)
#define	ALIGN_8(i)	((i) & ~0x7)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASYNC_H */
