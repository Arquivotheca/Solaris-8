/*
 *  Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 *  All rights reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)rmsc.h	1.21	99/11/25 SMI"
 */

#ifndef _RMSC_H_
#define	_RMSC_H_



/*
 * Some MS-DOS debuggers hide static items.  Use "Static" rather than
 * "static" when declaring items that might be referenced while debugging.
 * Typically this means any functions and top-level variables that would
 * normally be static.
 *
 * Warning: don't use "Static" for static variables within functions because
 * that would change their behavior.
 *
 * We normally leave "Static" items global but allow command line override.
 */
#ifndef Static
#define	Static
#endif



/*
 * Include all files that this file depends on here rather than
 * requiring it in the files that include this one.
 *
 * stdarg.h must be included before dostypes.h because both try
 * to define va_list and we want the stdarg.h version.  dostypes.h
 * is included by befext.h.
 */
#include <types.h>
#include <stdarg.h>
#include <bef.h>
#include <befext.h>
#include <dev_info.h>
#include <stdio.h>



/*
 * Definitions for adding debug messages.  Modules using this facility
 * must define MODULE_DEBUG_FLAG appropriately.  Each module can have
 * module-specific flags in addition to the ones defined here.  Module-
 * specific flags are generally assigned starting with 0x0001 to minimize
 * the chance of collision.
 */
#ifdef DEBUG

#define	DBG_CALL	0x8000	/* enable messages about function calls */
#define	DBG_ERRS	0x4000	/* enable messages for error conditions */
#define	DBG_GEN		0x2000	/* enable general messages */
#define	DBG_INIT	0x1000	/* enable initialization messages */
#define	DBG_CONFIG	0x0800	/* enable configuration messages */
#define	DBG_SERVICE	0x0400	/* enable messages from service requests */
#define	DBG_PROBE	0x0200	/* enable messages from probe requests */
#define	DBG_ALL		0xffff	/* enable all messages */
#define	DBG_NONE	0x0000	/* disable all messages */

#define	DBG_RET_STR(x)	(((x) == BEF_OK) ? "BEF_OK" : "BEF_FAIL")
#define	Dprintf(f, x)	if (MODULE_DEBUG_FLAG & (f)) printf x
#define	Dcall(f, x)	Dprintf((f) | DBG_CALL, \
		("Calling %s routine\n", (x)))
#define	Dback(f, x)	Dprintf((f) | DBG_CALL, \
		("Back from %s routine\n", (x)))
#define	Dsucceed(f, x)	Dprintf((f) | DBG_CALL, \
		("Back from %s routine: no errors\n", (x)))
#define	Dfail(f, x)	Dprintf((f) | DBG_CALL, \
		("Back from %s routine: error(s) detected\n", (x)))
#define	Dreturn(f, x, y) \
		Dprintf((f) | DBG_CALL, \
		("Back from %s routine: return code %s\n", (x), \
		DBG_RET_STR(y)))

#else

#define	Dprintf(f, x)
#define	Dcall(f, x)
#define	Dback(f, x)
#define	Dsucceed(f, x)
#define	Dfail(f, x)
#define	Dreturn(f, x, y)

#endif /* DEBUG */



/* Device handle definition */
typedef unsigned long rmsc_handle;



/* Arguments to splx() */
#define	RMSC_INTR_ENABLE	0x200
#define	RMSC_INTR_DISABLE	0x000



/* Return values from rmsc_checkpoint */
#define	RMSC_CHKP_GOOD		0
#define	RMSC_CHKP_STOP		1
#define	RMSC_CHKP_BAD		2



/*
 *	Generic layer initialization structure.
 *
 *	In the following structure definition the comment at the
 *	start of the each line containing structure members indicates
 *	whether the driver_init routine is required to initialize
 *	that member (REQ) or whether initialization is optional (OPT).
 */
typedef struct driver_init {
/* REQ */    char *driver_name;
/* OPT */    void (*legacy_probe)(void);
/* OPT */    int (*configure)(void);
/* OPT */    int (*init)(rmsc_handle, struct bdev_info *);
/* OPT */    int (*read)(rmsc_handle, struct bdev_info *,
			ulong, ushort, char far *, ushort);
/* OPT */    int (*extgetparms)(rmsc_handle, struct bdev_info *,
			struct ext_getparm_resbuf far *);
} rmsc_driver_init;



/* Prototype for generic layer initialization routines */
extern int driver_init(rmsc_driver_init *);



/* Start of PCI definitions */

/*
 * PCI "cookie" dissection
 */
#define	PCI_COOKIE_TO_BUS(x)	((unchar)(((x)>>8)&0xff))
#define	PCI_COOKIE_TO_DEV(x)	((unchar)(((x)>>3)&0x1f))
#define	PCI_COOKIE_TO_FUNC(x)	((unchar)((x)&0x0007))



/*
 * PCI Configuration Header offsets (copied from Solaris pci.h)
 */
#define	PCI_CONF_VENID		0x0	/* vendor id, 2 bytes */
#define	PCI_CONF_DEVID		0x2	/* device id, 2 bytes */
#define	PCI_CONF_COMM		0x4	/* command register, 2 bytes */
#define	PCI_CONF_STAT		0x6	/* status register, 2 bytes */
#define	PCI_CONF_REVID		0x8	/* revision id, 1 byte */
#define	PCI_CONF_PROGCLASS	0x9	/* programming class code, 1 byte */
#define	PCI_CONF_SUBCLASS	0xA	/* sub-class code, 1 byte */
#define	PCI_CONF_BASCLASS	0xB	/* basic class code, 1 byte */
#define	PCI_CONF_CACHE_LINESZ	0xC	/* cache line size, 1 byte */
#define	PCI_CONF_LATENCY_TIMER	0xD	/* latency timer, 1 byte */
#define	PCI_CONF_HEADER		0xE	/* header type, 1 byte */
#define	PCI_CONF_BIST		0xF	/* builtin self test, 1 byte */
#define	PCI_CONF_BASE0		0x10	/* base register 0, 4 bytes */
#define	PCI_CONF_BASE1		0x14	/* base register 1, 4 bytes */
#define	PCI_CONF_BASE2		0x18	/* base register 2, 4 bytes */
#define	PCI_CONF_BASE3		0x1c	/* base register 3, 4 bytes */
#define	PCI_CONF_BASE4		0x20	/* base register 4, 4 bytes */
#define	PCI_CONF_BASE5		0x24	/* base register 5, 4 bytes */
#define	PCI_CONF_CIS		0x28	/* Cardbus CIS Pointer */
#define	PCI_CONF_SUBVENID	0x2c	/* Subsystem Vendor ID */
#define	PCI_CONF_SUBSYSID	0x2e	/* Subsystem ID */
#define	PCI_CONF_ROM		0x30	/* ROM base register, 4 bytes */
#define	PCI_CONF_ILINE		0x3c	/* interrupt line, 1 byte */
#define	PCI_CONF_IPIN		0x3d	/* interrupt pin, 1 byte */
#define	PCI_CONF_MIN_G		0x3e	/* minimum grant, 1 byte */
#define	PCI_CONF_MAX_L		0x3f	/* maximum grant, 1 byte */



/*
 * PCI command register bits (copied from Solaris pci.h)
 */
#define	PCI_COMM_IO		0x0001   /* I/O access enable */
#define	PCI_COMM_MAE		0x0002   /* memory access enable */
#define	PCI_COMM_ME		0x0004   /* master enable */
#define	PCI_COMM_SPEC_CYC	0x0008
#define	PCI_COMM_MEMWR_INVAL	0x0010
#define	PCI_COMM_PALETTE_SNOOP	0x0020
#define	PCI_COMM_PARITY_DETECT	0x0040
#define	PCI_COMM_WAIT_CYC_ENAB	0x0080
#define	PCI_COMM_SERR_ENABLE	0x0100
#define	PCI_COMM_BACK2BACK_ENAB	0x0200



/* Definitions for programmable interrupt controllers */
#define	MPIC_CMD	0x20	/* Master PIC command register */
#define	MPIC_IMR	0x21	/* Master PIC command register */
#define	SPIC_CMD	0xA0    /* Slave PIC command register */
#define	SPIC_IMR	0xA1    /* Slave PIC command register */
#define	PIC_EOI		0x20    /* PIC end-of-interrupt command */
#define	PIC_IRR_READ	0xA	/* PCI read request register command */



/*
 * Prototypes for available library functions.
 */

/* Subset of C library routines */
extern char *calloc(ushort, ushort);
extern int fprintf(FILE *, const char *, ...);
extern int memcmp(const void far *, const void far *, unsigned);
extern void memcpy(void far *, const void far *, unsigned);
extern void memset(void far *, int, unsigned);
extern int printf(const char *, ...);
extern void putchar(char);
extern int sprintf(char *, const char *, ...);
extern char far *strchr(const char far *, int);
extern int strcmp(const char far *, const char far *);
extern void strcpy(char far *, const char far *);
extern int strlen(const char far *);
extern void strncpy(char far *, const char far *, unsigned);

/*
 * DevConf device node routines. (Defined in befext.h.)
 *
 *	extern int node_op(int);
 *	extern int set_prop(char far *, char far * far *, int far *, int);
 *	extern int get_prop(char far *, char far * far *, int far *);
 *	extern int get_res(char far *, DWORD far *, DWORD far *);
 *	extern int rel_res(char far *, DWORD far *, DWORD far *);
 *	extern int set_res(char far *, DWORD far *, DWORD far *, int);
 */

/* PCI configuration routines */
extern int is_pci(void);
extern int pci_find_device(ushort, ushort, ushort, ushort *);
extern int pci_read_config_byte(ushort, ushort, unchar *);
extern int pci_read_config_word(ushort, ushort, ushort *);
extern int pci_read_config_dword(ushort, ushort, ulong *);
extern int pci_write_config_byte(ushort, ushort, unchar);
extern int pci_write_config_word(ushort, ushort, ushort);
extern int pci_write_config_dword(ushort, ushort, ulong);

/* Routines for executing x86 I/O instructions */
extern unchar inb(ushort);
extern ushort inw(ushort);
extern ulong inl(ushort);
extern void outb(ushort, unchar);
extern void outw(ushort, ushort);
extern void outl(ushort, ulong);

/* Interrupt control routines */
extern ushort splhi(void);
extern ushort splx(ushort);
extern ushort irq_mask(ushort, ushort);

/* Miscellaneous routines */
extern void drv_msecwait(ulong);
extern void drv_usecwait(ulong);
extern ushort get_code_selector(void);
extern ushort get_data_selector(void);
extern void get_vector(unsigned short, long *);
extern ushort _int86(ushort, union _REGS *, union _REGS *);
extern int rmsc_checkpoint(int);
extern int rmsc_register(rmsc_handle, struct bdev_info *);
extern void set_vector(unsigned short, char far *);

#endif	/* _RMSC_H_ */
