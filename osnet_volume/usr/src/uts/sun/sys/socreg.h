/*
 * Copyright (c) 1995,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SOCREG_H
#define	_SYS_SOCREG_H

#pragma ident	"@(#)socreg.h	1.7	98/01/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * socreg.h:
 *
 * 	SOC Register Definitions, Interface Adaptor to Fiber Channel
 */

#define	N_SOC_NPORTS	2

/*
 * Define the SOC configuration register bits.
 */
typedef union soc_cr_register {
	struct cr {
		uint_t	aaa:5;
		uint_t	ramsel:3;	/* Ram bank select. */
		uint_t	bbb:6;
		uint_t	eepromsel:2;	/* Eeprom bank select. */
		uint_t	ccc:5;
		uint_t	burst64:3;	/* Sbus Burst size, 64 bit mode. */
		uint_t	ddd:2;
		uint_t	parenable:1;	/* Partity test enable. */
		uint_t	parsbus:1;	/* Sbus Parity checking. */
		uint_t	sbusmode:1;	/* Enhanced Sbus mode. */
		uint_t	sbusburst:3;	/* Sbus burst size. */
	} reg;
	uint_t	w;
} soc_cr_reg_t;

/*
 * Define Configuration register bits.
 */

#define	SOC_CR_SBUS_BURST_SIZE_MASK		0x007
#define	SOC_CR_SBUS_BURST_SIZE_64BIT_MASK	0x700
#define	SOC_CR_SBUS_BURST_SIZE_64BIT(a)	\
	(((a) & SOC_CR_SBUS_BURST_SIZE_64BIT_MASK) >> 8)

#define	SOC_CR_BURST_4		0x0
#define	SOC_CR_BURST_16		0x4
#define	SOC_CR_BURST_32		0x5
#define	SOC_CR_BURST_64		0x6

#define	SOC_CR_SBUS_ENHANCED	0x08
#define	SOC_CR_SBUS_PARITY_CHK	0x10
#define	SOC_CR_SBUS_PARITY_TEST	0x20

#define	SOC_CR_EEPROM_BANK_MASK	0x30000
#define	SOC_CR_EEPROM_BANK(a)	(((a) & SOC_CR_EEPROM_BANK_MASK) >> 16)

#define	SOC_CR_EXTERNAL_RAM_BANK_MASK	0x7000000
#define	SOC_CR_EXTERNAL_RAM_BANK(a) \
	(((a) & SOC_CR_EXTERNAL_RAM_BANK_MASK) >> 24)

/*
 * Define SOC Slave Access Register.
 */
typedef union soc_sae_register {
	struct sae {
		uint_t	aaa:29;			/* Reserved. */
		uint_t	alignment_err:1;	/* Soc Alignment Error. */
		uint_t	bad_size_err:1;		/* Bad Size error. */
		uint_t	parity_err:1;		/* Parity Error. */
	} reg;
	uint_t	w;
} soc_sae_reg_t;

/*
 * Define the Slave Access Regsiter Bits.
 */

#define	SOC_SAE_PARITY_ERROR		0x01
#define	SOC_SAE_UNSUPPORTED_TRANSFER	0x02
#define	SOC_SAE_ALIGNMENT_ERROR		0x04

/*
 * Define SOC Command and Status Register.
 */
typedef union soc_csr_register {
	struct csr {
		uint_t	comm_param:8;	/* Communication Parameters. */
		uint_t	aaa:4;
		uint_t	soc_to_host:4;	/* Soc to host attention. */
		uint_t	bbb:4;
		uint_t	host_to_soc:4;	/* Host to soc attention. */
		uint_t	sae:1;		/* Slave access error indicator. */
		uint_t	ccc:3;
		uint_t	int_pending:1;	/* Interrupt Pending. */
		uint_t	nqcmd:1;	/* Non queued command */
		uint_t	idle:1;		/* SOC idle indicator. */
		uint_t	reset:1;	/* Software Reset. */
	} reg;
	uint_t	w;
} soc_csr_reg_t;


/*
 * Define SOC CSR Register Macros.
 */
#define	SOC_CSR_ZEROS		0x00000070
#define	SOC_CSR_SOC_TO_HOST	0x000f0000
#define	SOC_CSR_HOST_TO_SOC	0x00000f00
#define	SOC_CSR_SLV_ACC_ERR	0x00000080
#define	SOC_CSR_INT_PENDING	0x00000008
#define	SOC_CSR_NON_Q_CMD	0x00000004
#define	SOC_CSR_IDLE		0x00000002
#define	SOC_CSR_SOFT_RESET	0x00000001

#define	SOC_CSR_1ST_S_TO_H	0x00010000
#define	SOC_CSR_1ST_H_TO_S	0x00000100

#define	SOC_CSR_RSP_QUE_0	SOC_CSR_1ST_S_TO_H
#define	SOC_CSR_RSP_QUE_1	0x00020000

#define	SOC_CSR_REQ_QUE_0	SOC_CSR_1ST_H_TO_S
#define	SOC_CSR_REQ_QUE_1	0x00000200
#define	SOC_CSR_REQ_QUE_2	0x00000400
#define	SOC_CSR_REQ_QUE_3	0x00000800

/*
 * Define SOC Interrupt Mask Register Bits.
 */

#define	SOC_IMR_NON_QUEUED_STATE	0x04
#define	SOC_IMR_SLAVE_ACCESS_ERROR	0x80

#define	SOC_IMR_REQUEST_QUEUE_0		0x100
#define	SOC_IMR_REQUEST_QUEUE_1		0x200
#define	SOC_IMR_REQUEST_QUEUE_2		0x400
#define	SOC_IMR_REQUEST_QUEUE_3		0x800

#define	SOC_IMR_RESPONSE_QUEUE_0	0x10000
#define	SOC_IMR_RESPONSE_QUEUE_1	0x20000
#define	SOC_IMR_RESPONSE_QUEUE_2	0x40000
#define	SOC_IMR_RESPONSE_QUEUE_3	0x80000

typedef struct _socreg_ {
	soc_cr_reg_t	soc_cr;		/* Configuration Register. */
	soc_sae_reg_t	soc_sae;	/* Slave access error Register. */
	soc_csr_reg_t	soc_csr;	/* Command Status register. */
	unsigned	soc_imr;	/* Interrupt Mask register. */
} soc_reg_t;

/*
 * Device Address Space Offsets.
 */

#define	SOC_XRAM_OFFSET	0x10000
#define	SOC_XRAM_SIZE	0x10000

#define	SOC_REG_OFFSET	(SOC_XRAM_OFFSET + SOC_XRAM_SIZE)

#define	SOC_CQ_REQUEST_OFFSET (SOC_XRAM_OFFSET + 0x200)
#define	SOC_CQ_RESPONSE_OFFSET (SOC_XRAM_OFFSET + 0x220)


#define	SOC_INTR_CAUSE(socp, csr) \
	(((csr) & SOC_CSR_SOC_TO_HOST) | \
	((~csr) & (SOC_CSR_HOST_TO_SOC))) & socp->k_soc_imr

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_SOCREG_H */
