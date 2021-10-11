/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SXREG_H
#define	_SYS_SXREG_H

#pragma ident	"@(#)sxreg.h	1.6	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Sun Pixel Arithmetic Memory (SX) graphics accelerator register definitions,
 * some ioctl commands and pertinent data type definitions. SX is supported
 * on Campus-II platforms.
 */

#define	SX_NREGS		128	/* registers in the register file */
#define	SX_B0_REGS		0	/* Start of registers in bank one */
#define	SX_B1_REGS		32	/* Start of registers in bank two */
#define	SX_B2_REGS		64	/* Start of registers in bank three */
#define	SX_B3_REGS		96	/* Start of registers in bank four */

#define	SX_PGSIZE		0x1000
#define	SX_INIT_STATE		0x01
#define	SX_RESET_STATE	0x02
#define	SX_BUSTYPE		0x08	/* Bits 35:32 of the physical addr */

/*
 * Layout of the SX registers. User's map 1 page of the SX register set
 * with read/write access but cannnot write certain bits in the control/status
 * register and the page bounds registers (for security reasons).
 */

struct sx_register_address {

	uint_t	s_csr;		/* Control/Status register */
	uint_t	s_ser;		/* Spam Error Register */
	uint_t	s_pg_lo;	/* lower page bounds register	*/
	uint_t	s_pg_hi;	/* Upper page bounds regiser	*/
	uint_t	s_planereg;	/* Plane register		*/
	uint_t	s_ropreg;	/* Raster operations control register */
	uint_t	s_iqcounter;	/* instruction queue overflow counter */
	uint_t	s_diagreg;	/* diagnostics control register */
	uint_t	s_iq;		/* instruction queue		*/
	uint_t	s_pad1;		/* reserved			*/
	uint_t	s_idreg;	/* ID register			*/
	uint_t	s_r0_init;	/* Write to R0 to zero		*/
	uint_t	s_reset;	/* Write to reset SX		*/
	uint_t	s_sync;		/* SX hardware sync		*/
				/* A write of any value to this */
				/* location, causes SX to issue */
				/* Relinquish & Retry to the host */
				/* processor until SX IQ empties */
	uint_t	s_pad2[50];	/* Reserved			*/
	uint_t	s_regfile[SX_NREGS];	/* Direct read/write */
	uint_t	s_q_regfile[SX_NREGS];	/* Direct read; queued write */
};


/*
 * Flags for SX control/status register
 */

/*
 * EE1 through EE6 enable SX error reporting in the SX error register.
 * If EI (Enable Interrupt) is set any of the error conditions EE1-EE6 generate
 * a level 15 interrupt. On reset, the driver sets all of these.
 */

#define	SX_ERRMASK	0xff
#define	SX_EE1	0x01	/* Illegal or unimplemented instructions */
#define	SX_EE2	0x02	/* Out of page bounds access		*/
#define	SX_EE3	0x04	/* Access to addr space outside of D[V]RAM */
#define	SX_EE4	0x08	/* Access to SX register <0 or >127	*/
#define	SX_EE5	0x10	/* Misaligned address (SX memory reference */
				/* instruction */
#define	SX_EE6	0x20	/* Illegal write to the instruction queue */
#define	SX_EI		0x80	/* Enable interrupts from SX		*/
#define	SX_PB		0x400	/* Enable extended page bounds checking	*/
#define	SX_WO		0x800	/* Write Occurred. Set by SX. Cleared by */
				/* software */
#define	SX_GO		0x1000	/* Must be set for SX to fetch instructions */
				/* from instruction queue */
#define	SX_JB		0x2000	/* Jammed/Busy specifies the type of events */
				/* which increment the SX timer	*/
#define	SX_MT		0x4000	/* Set when instruction Q is empty.	*/
#define	SX_BZ		0x8000 	/* Busy bit. When set it indicates that SX */
				/* is processing an instruction or an */
				/* instruction is pending in the Q	*/

#define	SX_B0MOD	0x10000	/* When set by SX it indicates that a write */
				/* to bank zero of the SX registers (0-31) */
				/* occured */

#define	SX_B1MOD	0x20000	/* When set by SX it indicates that a write */
				/* to bank 1 of the SX registers (32-63) */
				/* occured */
#define	SX_B2MOD	0x40000	/* When set by SX it indicates that a write */
				/* to bank 2 of the SX registers (64-95) */
				/* occured */

#define	SX_B3MOD	0x80000	/* When set by SX it indicates that a write */
				/* to bank 3 of the SX registers (96-127) */
				/* occured */
/*
 * SE1 through SE6 describe error conditions in the SX Error Register
 */

#define	SX_SE1	0x01	/* Illegal or unimplemented instructions */
#define	SX_SE2	0x02	/* Out of page bounds access		*/
#define	SX_SE3	0x04	/* Access to address space outside of D[V]RAM */
#define	SX_SE4	0x08	/* Access to SX register <0 or >127	*/
#define	SX_SE5	0x10	/* Misaligned address (SX memory reference */
				/*  instruction) */
#define	SX_SE6	0x20	/* Illegal write to the instruction queue */
#define	SX_SI		0x80	/* Set only if EI is set in the */
				/* control/status register */

/*
 * Masks for the ID register.
 */

#define	SX_ARCH_MASK	0x03		/* SX architectural specification */
#define	SX_CHIPREV_MASK 0xf8		/* SX Chip Revision		*/

/*
 * Flags for the Diagnostic Register.
 */

#define	SX_DIAG_MASK	0x1f

#define	SX_DIAG_IQ_FIFO_ACCESS	0x01	/* When set, disables IQ */
						/* value checking. */
#define	SX_DIAG_SERIAL_INSTRUCTIONS	0x02	/* When set, forces instr */
						/* serialization. */
#define	SX_DIAG_RAM_PAGE_CROSS	0x04	/* When set, indicates a RAM */
						/* PAGE cross occured */
#define	SX_DIAG_ARRAY_CONSTRAINING	0x08	/* When set constrains VRAM */
						/* array offset effective */
						/* address calculation  */
#define	SX_UPG_MPG_DISABLE		0x10	/* When set, disables page */
						/* cross input into ld/st */
						/* state machines */
#define	SX_DIAG_INIT		0x4804	/* Setting of the diag reg upon reset */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SXREG_H */
