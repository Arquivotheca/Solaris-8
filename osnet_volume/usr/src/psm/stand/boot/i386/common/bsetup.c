/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bsetup.c	1.33	99/10/07 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/bootdef.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/saio.h>
#include <sys/sysmacros.h>
#include <sys/pfuncs.h>
#include <sys/sysenvmt.h>
#include <sys/bootinfo.h>
#include <sys/bootvfs.h>
#include <sys/machine.h>
#include <sys/salib.h>
/*
 * The hacks in bootsvcs.h make it difficult to get
 * the boot services vector w/o picking up some
 * preprocessor substitutions of common functions.
 * Include it here and then undo the damage - ugh.
 */
#include <sys/bootsvcs.h>
#undef	printf
#undef	getchar
#undef	putchar
#undef	ischar
#undef	get_fonts

char boot_banner[] = "SunOS Secondary Boot version 3.00\n\n";

extern char _end[];
extern struct int_pb ic;
extern int BootDev;
extern int Oldstyleboot;

/*
 * bsetup calls all the PC specific setup routines that enable the boot
 * system to provide prom like services to the higher level Sun code
 */

extern caddr_t bot_realmem, top_realmem;
extern caddr_t memlistpage;
extern caddr_t scratchmemp;
extern char    *my_own_name;
extern int pagesize;

struct sysenvmt sysenvmt, *sep;
struct bootinfo bootinfo, *bip;
struct bootenv bootenv, *btep;
extern struct bootops *bop;
extern struct pri_to_secboot *realp;

extern caddr_t resalloc(enum RESOURCES type, size_t bytes, caddr_t virthint,
    int align);
extern void compatboot_createprops(char *outline);
extern void probe(void);
extern void setup_memlists(void);
extern void init_paging(void);
extern void probe_eisa(void);
extern void kmem_init(void);
extern void reset_alloc(void);
extern void debug_init(int);
extern int doint(void);
extern void mc386setup(void);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);

struct  pri_to_secboot boot_geometry, *compatboot_ip;
char	outline[256];
int	boot_device;

extern void	*memcpy();
extern int	memcmp();
extern int	getchar();
extern void	putchar();
extern int	ischar();
extern int	goany();
extern int	gets();
extern void	*memset();
extern char	*malloc();
extern char	*get_fontptr();
extern uint_t	vlimit();

struct boot_syscalls sc =  {
	printf,
	strcpy,
	strncpy,
	strcat,
	strlen,
	memcpy,
	memcmp,
	getchar,
	putchar,
	ischar,
	goany,
	gets,
	memset,
	open,
	read,
	lseek,
	close,
	fstat,
	malloc,
	get_fontptr,
	vlimit
	};

struct boot_syscalls *sysp = &sc;

void
bsetup()
{
	int bdev;

	ic.intval = 0x10;

	/* Cursor to 0,0 */
	ic.ax = 0x0200;
	ic.bx = ic.cx = ic.dx = 0x0;
	(void) doint();

	/* Write black-on-white spaces to the entire screen */
	ic.ax = 0x0920;
	ic.bx = 0x0070;
	ic.cx = 25*80;
	(void) doint();

	printf(boot_banner);

	sep = &sysenvmt;
	bip = &bootinfo;
	btep = &bootenv;

	if (realp) {
		/*
		 *  The boot info provided by the 1st level boot is likely to
		 *  occupy real estate in the "realmem" arena.  Make a copy of
		 *  it here so we can reuse the original memory.
		 */
		Oldstyleboot = 1;
		(void) memcpy(&boot_geometry, realp, sizeof (boot_geometry));
		compatboot_ip = realp = &boot_geometry;
	}

	/*
	 *  Pre-allocate low memory between end of text ("scratchmemp") and
	 *  the magic 1MB boundary ("top_realmem").  Address delivered by
	 *  resalloc becomes the bottom of the free realmode memory pool
	 *  (see "rm_malloc/rm_free").
	 *
	 *  NOTE: Explicit call to reset_alloc() ensures that "scratchmemp"
	 *  is set correctly.
	 */

	sep->base_mem = (ushort_t)MEM_BASE();
	reset_alloc();

	bot_realmem = scratchmemp;
	top_realmem = (caddr_t)(1024 * MEM_BASE());
	scratchmemp = (caddr_t)roundup((uint_t)_end, pagesize);

	/* BOOTSCRATCH now setup to allow kmem_alloc's */
	kmem_init();

#if (defined(BOOT_DEBUG))
	/*
	 *  Print initial memory locations for debugging purposes.
	 */
	printf("bsetup: realmem = 0x%x - 0x%x\n", bot_realmem, top_realmem);
	printf("bsetup: bootscratch starts 0x%x\n", scratchmemp);
#endif /* BOOT_DEBUG */

	/* Turn off the boot talk NB: take this out after testing */
	btep->db_flag &= ~BOOTTALK;
	btep->db_flag &= ~MEMDEBUG;

	(void) bsetprop(bop, "machine-type", "unknown", 0, 0);

	/* Allow bootconf to identify the BIOS boot device */
	bdev = boot_geometry.bootfrom.ufs.boot_dev;
	if (bdev == 0)
		bdev = BootDev;
	if (bdev >= 0x80) {
		char buf[16];

		(void) sprintf(buf, "%x", bdev);
		(void) bsetprop(bop, "bios-boot-device", buf, 0, 0);
	}

	(void) memset(outline, 0, sizeof (outline)); /* reset for bootpath */
	if (compatboot_ip)
		compatboot_createprops(outline);

	/* keep lint happy that we are using my_own_name */
	(void) bsetprop(bop, "whoami", my_own_name, 0, 0);

	/* Turn on the boot talk */
/*	btep->db_flag |= BOOTTALK; */

	probe();
	setup_memlists();
	init_paging();
	probe_eisa();   /* do this after kmem is available */

	/*
	 * Turn on this call if early debugger entry is necessary.
	 * Normally the debugger gets turned on from post_mountroot
	 * if the symbol table file is present.
	 */
#ifdef ENABLE_DEBUGGER
	debug_init(0);
#endif
}

/* temporary stubs */

int
splimp()
{
	return (0);
}

/*ARGSUSED*/
int
splx(int rs)
{
	return (0);
}

int
splnet()
{
	return (0);
}
