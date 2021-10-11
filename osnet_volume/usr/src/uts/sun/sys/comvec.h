/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_COMVEC_H
#define	_SYS_COMVEC_H

#pragma ident	"@(#)comvec.h	1.10	98/01/06 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/obpdefs.h>
#include <sys/memlist.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Autoconfig operations
 */

struct config_ops {
#ifdef	_KERNEL
	dnode_t	(*devr_next)(/* dnode_t nodeid */);
	dnode_t	(*devr_child)(/* dnode_t nodeid */);
#else	/* _KERNEL */
	int	(*devr_next)(/* dnode_t nodeid */);
	int	(*devr_child)(/* dnode_t nodeid */);
#endif	/* _KERNEL */
	int	(*devr_getproplen)(/* dnode_t nodeid, char *name */);
	int	(*devr_getprop)(/* dnode_t nodeid, char *name, caddr_t buf */);
	int	(*devr_setprop)(/* dnode_t nodeid, char *name, caddr_t value,
	    uint_t size */);
	caddr_t	(*devr_nextprop)(/* dnode_t nodeid, char *previous */);
};



/*
 * Other than the first four fields of the sunromvec,
 * the fields in the V3 romvec are not aligned since
 * they don't have compatability with the old fields.
 *
 * KEY:
 *	op_XXX:		Interfaces common to *all* versions.
 *	v_XXX:		V0 Interface (included in V2 for compatability)
 *	op2_XXX:	V2 and later only
 *	op3_XXX:	V3 and later only
 *
 * It's a fatal error (results are completely undefined and range from
 * unknown behaviour to deadlock) to call a non-existant interface.
 * (I.e.: A V0 interface from a V3 PROM or an MP-only interface on a
 * UP.)  The structures line up for programmers convenience only!
 *
 * XXX	We would like to ANSIfy this structure with appropriate prototypes,
 *	though until genassym.c is compiled by an ANSI compiler, we can't.
 */

struct romvec_obp {
	uint_t	op_magic;		/* magic mushroom */
	uint_t	op_romvec_version;	/* Version number of "romvec" */
	uint_t	op_plugin_version;	/* Plugin Architecture version */
	uint_t	op_mon_id;		/* version # of monitor firmware */

	struct memlist **v_physmemory;	/* total physical memory list */
	struct memlist **v_virtmemory;	/* taken virtual memory list */
	struct memlist **v_availmemory;	/* available physical memory */
	struct config_ops *op_config_ops; /* dev_info configuration access */

	/*
	 * storage device access facilities
	 */
	char	**v_bootcmd;	/* expanded with PROM defaults */
	uint_t	(*v_open)(/* char *name */);
	uint_t	(*v_close)(/* ihandle_t fileid */);

	/*
	 * block-oriented device access
	 */
	uint_t	(*v_read_blocks)();
	uint_t	(*v_write_blocks)();

	/*
	 * network device access
	 */
	uint_t	(*v_xmit_packet)();
	uint_t	(*v_poll_packet)();

	/*
	 * byte-oriented device access
	 */
	uint_t	(*v_read_bytes)();
	uint_t	(*v_write_bytes)();

	/*
	 * 'File' access - i.e.,  Tapes for byte devices.
	 * TFTP for network devices
	 */
	uint_t	(*v_seek)();

	/*
	 * single character I/O
	 */
	uchar_t	*v_insource;	/* Current source of input */
	uchar_t	*v_outsink;	/* Currrent output sink */
	uchar_t	(*v_getchar)();	/* Get a character from input */
	void	(*v_putchar)();	/* Put a character to output sink. */
	int	(*v_mayget)();	/* Maybe get a character, or "-1". */
	int	(*v_mayput)();	/* Maybe put a character, or "-1". */

	/*
	 * Frame buffer
	 */
	void	(*v_fwritestr)();	/* write a string to framebuffer */

	/*
	 * Miscellaneous Goodies
	 */
	void	(*op_boot)(/* char *bootspec */);	/* reboot machine */
	int	(*v_printf)();		/* handles fmt string plus 5 args */
	void	(*op_enter)();		/* Entry for keyboard abort. */
	int	*op_milliseconds;	/* Counts in milliseconds. */
	void	(*op_exit)();		/* Exit from user program. */

	/*
	 * Note:  Different semantics for V0 versus other op_vector_cmd:
	 */
	void	(**op_vector_cmd)();	/* Handler for the vector */
	void	(*op_interpret)(/* char *string, ... */);
					/* interpret forth string */

	/* boot parameters and 'old' style device access */
	struct bootparam	**v_bootparam;

	uint_t	(*v_mac_address)(/* int fd, caddr_t buf */);
			/* Copyout ether address */

	/*
	 * new V2 openprom stuff
	 */

	char	**op2_bootpath;	/* Full path name of boot device */
	char	**op2_bootargs;	/* Boot command line after dev spec */

#ifdef	_KERNEL
	ihandle_t *op2_stdin;	/* Console input device */
	ihandle_t *op2_stdout;	/* Console output device */

	phandle_t (*op2_phandle)(/* ihandle_t */);
					/* Convert ihandle to phandle */
#else	/* _KERNEL */
	int	*op2_stdin;	/* Console input device */
	int	*op2_stdout;	/* Console output device */

	int	(*op2_phandle)(/* ihandle_t */);
					/* Convert ihandle to phandle */
#endif	/* _KERNEL */

	caddr_t (*op2_alloc)(/* caddr_t virthint, uint_t size */);
					/* Allocate physical memory */

	void    (*op2_free)(/* caddr_t virthint, uint_t size */);
					/* Deallocate physical memory */

	caddr_t (*op2_map)(/* caddr_t virthint, uint_t space, uint_t phys,
	    uint_t size */);		/* Create device mapping */

	void    (*op2_unmap)(/* caddr_t virt, uint_t size */);
					/* Destroy device mapping */

#ifdef	_KERNEL
	ihandle_t (*op2_open)(/* char *name */);
#else	/* _KERNEL */
	int	(*op2_open)(/* char *name */);
#endif	/* _KERNEL */
	uint_t	(*op2_close)(/* int ihandle */);

	int (*op2_read)(/* int ihandle, caddr_t buf, uint_t len */);
	int (*op2_write)(/* int ihandle, caddr_t buf, uint_t len */);
	int (*op2_seek)(/* int ihandle, uint_t offsh, uint_t offsl */);

	void    (*op2_chain)(/* caddr_t virt, uint_t size, caddr_t entry,
	    caddr_t argaddr, uint_t arglen */);

	void    (*op2_release)(/* caddr_t virt, uint_t size */);

	/*
	 * End V2 stuff
	 */

	caddr_t	(*op3_alloc)(/* caddr_t virthint, uint_t size, int align */);
					/* Allocate mem and align */

	int	*v_reserved[14];

	/*
	 * Sun4c specific romvec routines (From sys/sun4c/machine/romvec.h)
	 * Common to all PROM versions.
	 */

	void    (*op_setcxsegmap)(/* int ctx, caddr_t v, int pmgno */);
					/* Set segment in any context. */

	/*
	 * V3 MP only functions: It's a fatal error to call these from a UP.
	 */

	int (*op3_startcpu)(/* dnode_t moduleid, dev_reg_t contextable,
	    int whichcontext, caddr_t pc */);

	int (*op3_stopcpu)(/* dnode_t */);

	int (*op3_idlecpu)(/* dnode_t */);
	int (*op3_resumecpu)(/* dnode_t */);
};

/*
 * The remainder is only used in the sun4 architecture (sunmon).
 */

/*
 * This file defines the entire interface between the ROM
 * Monitor and the programs (or kernels) that run under it.
 *
 * The main Sun-4 interface consists of (1) the VECTOR TABLE and (2) the
 * TRAP VECTOR TABLE, near the front of the Boot PROM.  Beginning at address
 * "0x00000000", there is a 4K-byte TRAP TABLE containing 256 16-byte entries.
 * Each 16-byte TRAP TABLE entry contains the executable code associated with
 * that trap.  The initial 128 TRAP TABLE entries are dedicated to hardware
 * traps while, the final 128 TRAP TABLE entries are reserved for programmer-
 * initiated traps.  With a few exceptions, the VECTOR TABLE, which appeared
 * in Sun-2 and Sun-3 firmware, follows the TRAP TABLE.  Finally, the TRAP
 * VECTOR TABLE follows the VECTOR TABLE.  Each TRAP VECTOR TABLE entry
 * contains the address of the trap handler, which is eventually called to
 * handle the trap condition.
 *
 * These tables are the ONLY knowledge the outside world has of this rom.
 * They are referenced by hardware and software.  Once located, NO ENTRY CAN
 * BE ADDED, DELETED or RE-LOCATED UNLESS YOU CHANGE THE ENTIRE WORLD THAT
 * DEPENDS ON IT!  Notice that, for Sun-4, EACH ENTRY IN STRUCTURE "sunromvec"
 * MUST HAVE A CORRESPONDING ENTRY IN VECTOR TABLE "vector_table", which
 * resides in file "../sun4/traptable.s".
 *
 * The easiest way to reference elements of these TABLEs is to say:
 *	*romp->xxx
 * as in:
 *	(*romp->v_putchar)(c);
 *
 * Various entries have been added at various times.  As of the Rev N, the
 * VECTOR TABLE includes an entry "v_romvec_version" which is an integer
 * defining which entries in the table are valid.  The "V1:" type comments
 * on each entry state which version the entry first appeared in.  In order
 * to determine if the Monitor your program is running under contains the
 * entry, you can simply compare the value of "v_romvec_version" to the
 * constant in the comment field.  For example,
 *
 *	if (romp->v_romvec_version >= 1) {
 *		reference *romp->v_memorybitmap...
 *	} else {
 *		running under older version of the Monitor...
 *	}
 * Entries which do not contain a "Vn:" comment are in all versions.
 */

struct romvec_sunmon {
	char	*v_initsp;	/* Initial Stack Pointer for hardware */
	void	(*v_startmon)(); /* Initial PC for hardware */
	int	*v_diagberr;	/* Bus error handler for diagnostics. */

	/*
	 * Configuration information passed to standalone code and UNIX.
	 */
	struct bootparam **v_bootparam; /* Information for boot-strapped pgm */
	unsigned int *v_memorysize;	/* Total physical memory in bytes */

	/*
	 * Single character input and output.
	 */
	unsigned char	(*v_getchar)();	/* Get a character from input source */
	void		(*v_putchar)();	/* Put a character to output sink  */
	int		(*v_mayget)();	/* Maybe get a character, or "-1" */
	int		(*v_mayput)();	/* Maybe put a character, or "-1" */
	unsigned char	*v_echo;	/* Should "getchar" echo input? */
	unsigned char	*v_insource;	/* Current source of input */
	unsigned char	*v_outsink;	/* Currrent output sink */

	/*
	 * Keyboard input and frame buffer output.
	 */
	int		(*v_getkey)();	/* Get next key if one is available */
	void		(*v_initgetkey)(); /* Initialization for "getkey" */
	unsigned int	*v_translation;	/* Keyboard translation selector */
	unsigned char	*v_keybid;	/* Keyboard ID byte */
	int		*v_screen_x;	/* V2: Screen x pos (R/O) */
	int		*v_screen_y;	/* V2: Screen y pos (R/O) */
	struct keybuf	*v_keybuf;	/* Up/down keycode buffer */

	char		*v_mon_id;	/* Revision level of the monitor */

	/*
	 * Frame buffer output and terminal emulation.
	 */
	void		(*v_fwritechar)(); /* Write a char to frame buffer */
	int		*v_fbaddr;	/* Address of frame buffer */
	char		**v_font;	/* Font table for frame buffer */
	void		(*v_fwritestr)(); /* Quickly write a string to fb */

	/*
	 * Re-boot interface routine.  Resets and re-boots system.  No return.
	 */
	void		(*v_boot_me)();	/* For example, boot_me("xy()unix") */

	/*
	 * Command line input and parsing.
	 */
	unsigned char	*v_linebuf;	/* The command line buffer */
	unsigned char	**v_lineptr;	/* Current pointer into "linebuf" */
	int		*v_linesize;	/* Length of current command line */
	void		(*v_getline)();	/* Get a command line from user */
	unsigned char	(*v_getone)();	/* Get next character from "linebuf" */
	unsigned char	(*v_peekchar)();
		/* Peek at the next char without advancing pointer */
	int		*v_fbthere;	/* Is there a frame buffer? 1 = yes */
	int		(*v_getnum)();	/* Grab hex number from command line */

	/*
	 * Phrase output to current output sink.
	 */
	int		(*v_printf)();	/* Similar to Kernel's "printf" */
	void		(*v_printhex)(); /* Format N digits in hexadecimal */

	unsigned char	*v_leds;	/* RAM copy of LED register value */
	void		(*v_set_leds)(); /* Sets LEDs and RAM copy */
	/*
	 * The nmi related information.
	 */
	void		(*v_nmi)();	/* addr for the sun4 level 14 vector */
	void		(*v_abortent)(); /* Entry for keyboard abort */
	int		*v_nmiclock;	/* Counts in milliseconds */

	int		*v_fbtype;	/* Frame buffer type: see fbio.h */

	/*
	 * Assorted other things.
	 */
	unsigned int	v_romvec_version; /* Version number of "romvec". */
	struct globram	*v_gp;		/* Monitor's global variables */
	struct zscc_device *v_keybzscc; /* Address of keyboard in use */
	int		*v_keyrinit;	/* Millisecs before keyboard repeat */
	unsigned char	*v_keyrtick;	/* Millisecs between repetitions */
	unsigned int	*v_memoryavail;	/* V1: Size of usable main memory */
	long		*v_resetaddr;	/* where to jump on a RESET trap */
	long		*v_resetmap;	/* Page map entry for "resetaddr" */
	void		(*v_exit_to_mon)();	/* Exit from user program */
	unsigned char	**v_memorybitmap; /* V1: Bit map of main mem or NULL */
	void		(*v_setcxsegmap)(); /* Set segment in any context */
	void		(**v_vector_cmd)(); /* V2: 'w' (vector) handler */
	/* V3: Location of the expected trap signal.  Was trap expected? */
	unsigned long	*v_exp_trap_signal;
	/* V3: Address of the TRAP VECTOR TABLE which exists in RAM */
	unsigned long	*v_trap_vector_table_base;
	int	dummy1z;
	int	dummy2z;
	int	dummy3z;
	int	dummy4z;
};

union sunromvec {
	struct romvec_obp	obp;
	struct romvec_sunmon	sunmon;
};

extern union sunromvec *romp;

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_COMVEC_H */
