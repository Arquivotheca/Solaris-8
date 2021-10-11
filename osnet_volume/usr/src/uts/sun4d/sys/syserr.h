/*
 * Copyright (c) 1988,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SYSERR_H
#define	_SYS_SYSERR_H

#pragma ident	"@(#)syserr.h	1.19	98/01/06 SMI"	/* SunOS-4.1 1.9 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * various external error handling routines
 */
extern int bb_ctl_get_ecsr(uint_t cpu_id);
extern void bb_ctl_set_ecsr(uint_t cpu_id, uint_t value);
extern int intr_bb_status1(void);
extern unsigned int jtag_ctl_get_ecsr(int);
extern int jtag_cmd_get_ecsr(unsigned int);
extern void jtag_cmd_set_ecsr(unsigned int, int);


extern u_longlong_t intr_mxcc_error_get(void);
extern void intr_mxcc_error_set(u_longlong_t);
extern int intr_get_pend_local(void);
extern int intr_clear_pend_local(int level);
extern int intr_get_table(int sbus_level);
extern void intr_clear_mask_bits(int mask);
extern void intr_clear_mask_bits_ecsr(int mask, uint_t cpu);
extern int intr_clear_table(int sbus_level, int mask);
extern int intr_get_table_bwb(int sbus_level);
extern int intr_clear_table_bwb(int sbus_level, int mask);

extern int bw_compid_get(int cpuid, int busid);

extern uint_t xdb_bb_status2_get(void);

extern void xmit_cpu_intr(uint_t cpu_id, uint_t pri);
extern uint_t syserr_handler(caddr_t);

/*
 * External Data
 */
extern uint_t n_xdbus;		/* Number of XDBus's in system */
extern int good_xdbus;		/* Number of working XDBus's */
extern uint_t n_bootbus;	/* Number of working bootbus's */
extern uint_t bootbusses;	/* bit mask for CPU's which own bootbus */

extern uint_t memerr_ready;

extern uint_t disable_acfail;		/* override for /etc/system */
extern uint_t disable_tempfail;		/* override for /etc/system */
extern uint_t disable_fanfail;		/* override for /etc/system */
extern uint_t disable_powerfail;	/* override for /etc/system */
extern uint_t disable_sbifail;		/* override for /etc/system */

/*
 * The model defines are for sun4d platforms. They are set early on
 * during boot by counting the number of xdbus's and also looking
 * at the OBP banner if necessary. They are used to setup the
 * memory error handlers and dual power supply interrupt handlers.
 */
#define	MODEL_SC2000	0x2000
#define	MODEL_SC1000	0x1000
#define	MODEL_UNKNOWN	-1
extern int sun4d_model;

/*
 * The STATUS defines all refer to bits in the boot bus status registers.
 */
#define	STATUS1_YL_LED	(1 << 1)
#define	STATUS2_AC_INT	(1 << 2)
#define	STATUS2_TMP_INT	(1 << 4)
#define	STATUS2_FAN_INT	(1 << 5)
#define	STATUS2_PWR_INT	(1 << 6)

/*
 * The following defines refer to the number of seconds the callback
 * delay for the specific handler is set for. Every time the
 * corresponding handler is called, it sets itself up to be called
 * again n seconds later.
 */
#define	SOFTINT_FAN_TIMEOUT_SEC	8
#define	SOFTINT_PWR_TIMEOUT_SEC	8
#define	SOFTINT_AC_TIMEOUT_SEC	1
#define	OVEN_TIMEOUT_SEC	8

/*
 * In order to indicate that we are in an environmental chamber, or
 * oven, the test people will set the 'testarea' property in the
 * options node to 192 decimal. Therefore we have the following define.
 */
#define	CHAMBER_VALUE	"192"

#define	INTTABLE_MQH_CE		(1 << 2)
#define	INTTABLE_MQH_UE		(1 << 3)
#define	INTTABLE_MQH_UE_CE	(1 << 4)
#define	INTTABLE_SBI_XPT	(1 << 5)

#define	INTTABLE_ECC (INTTABLE_MQH_CE | INTTABLE_MQH_UE | INTTABLE_MQH_UE_CE)

/*
 * Component ID of Bus Watcher model with timer design bug
 */
#define	BW_BAD		0x10ADB07D

/*
 * MXCC.ERR.CCOP.DCMD
 * MXCC ERRor register Cache Controller Operation Data CoMmanD field
 */
#define	DCMD_BC_MR	0x5
#define	DCMD_BC_MW	0xF
#define	DCMD_IOW	0x13
#define	DCMD_BC_IOR	0x15
#define	DCMD_BC_IOW	0x17
#define	DCMD_SBUS_SR	0x4
#define	DCMD_WB		0x6
#define	DCMD_CFR	0xc
#define	DCMD_FECSRR	0x10

/*
 * Boot bus control bits for enabling and disabling environmental
 * interrupts. Power is for the dual power supply failure interrupts.
 * The rest should be self-explanatory.
 */
#define	BBUS_CTL_POWER_BIT	(1 << 4)
#define	BBUS_CTL_TEMP_BIT	(1 << 5)
#define	BBUS_CTL_FAN_BIT	(1 << 6)

#define	BITSET 1
#define	BITCLR 0
#define	UNMASK_15 0
#define	MASK_15 1

#ifdef DEBUG
#define	DEBUG_INC(x) (++(x))
#else
#define	DEBUG_INC(x)
#endif	/* DEBUG */

/*
 * syserr_handler() is compiled directly into vectorlist[]
 * at these levels.  Note that zs output above IPL6 is not immediate.
 */
#define	SOFTERR_IPL_HIGH 8
#define	SOFTERR_IPL_MED 4
#define	SOFTERR_IPL_LOW 1

#define	IPL_AC		SOFTERR_IPL_MED
#define	IPL_TEMP	SOFTERR_IPL_MED
#define	IPL_FAN		SOFTERR_IPL_LOW
#define	IPL_POWER	SOFTERR_IPL_LOW
#define	IPL_MXCC	SOFTERR_IPL_MED
#define	IPL_SBI		SOFTERR_IPL_LOW
#define	IPL_ECC_UE	SOFTERR_IPL_MED
#define	IPL_ECC_CE	SOFTERR_IPL_LOW
#define	IPL_ECC		IPL_ECC_UE

extern lock_t syserr_req[];	/* set this before requesting softint */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSERR_H */
