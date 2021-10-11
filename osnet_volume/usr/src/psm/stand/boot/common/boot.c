/*
 * Copyright (c) 1991-1994,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)boot.c	1.61	99/10/04 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/salib.h>
#include <sys/promif.h>
#include <sys/reboot.h>
#include <sys/bootconf.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/boot_redirect.h>
#include <sys/fcntl.h>

#ifdef DEBUG
static int	debug = 1;
#if !defined(__ia64)
#define	HALTBOOT
#endif
#else /* DEBUG */
static int	debug = 0;
#endif /* DEBUG */

#define	dprintf		if (debug) printf
#define	SUCCESS		0
#define	FAILURE		-1

/*
 * The file system is divided into blocks of usually 8K each.
 * Each block would then hold 16 sectors of 512 bytes each.
 * Sector zero is reserved for the label.
 * Sectors 1 through 15 are reserved for the boot block program.
 */

/*
 * Base of all platform-dependent kernel directories.
 */
#define	MACHINE_BASEDIR	"/platform"

/*
 *  These variables should be declared as pointers (and init'd
 *  as such, if you wish) so that the boot getprop code works right.
 */

#if defined(__sparcv9)
char	*defname64 = "kernel/sparcv9/unix";
#endif	/* defined(__sparcv9) */

#if defined(__ia64)
char	*defname = "kernel/ia64/unix";
#else
char	*defname = "kernel/unix";
#endif	/* defined(__ia64) */

char	*kernname;
char	*my_own_name = "boot";
char	*impl_arch_name;
char	*cmd_line_default_path;
char    *v2path, *v2args;
int	boothowto = 0;
int 	verbosemode = 0;
char	*systype;
char    filename[MAXPATHLEN];

/*  These are the various memory lists */
struct memlist 	*pfreelistp, /* physmem available */
		*vfreelistp, /* virtmem available */
		*pinstalledp;   /* physmem installed */

extern int	cache_state;

extern void	fiximp(void);
extern void	init_memlists(void);
extern void	setup_bootpath(char *bpath, char *bargs);
extern void	get_filename_from_boot_args(char *filename, char *bargs);
extern void	setup_bootargs(char *bargs);
extern int	mountroot(caddr_t devpath);
extern int	bootflags(char *s);
extern void	setup_bootops(void);
extern void	post_mountroot(char *bootfile, char *redirect);
extern void	translate_tov2(char **v2path, char *bpath);
extern caddr_t	kmem_alloc(size_t);
extern void	redirect_boot_path(char **, char *, char *);

extern void	set_default_filename(char *filename);
#if defined(_LP64) && !defined(__ia64)
extern char	*choose_default_filename(char *, char *);
extern void	get_boot_args(char *);
#endif	/* _LP64 */

#ifndef	i386
extern char	*get_default_filename(void);
#endif

#define	MAXARGS		8

/*
 * Reads in the standalone (client) program and jumps to it.  If this
 * attempt fails, prints "boot failed" and returns to its caller.
 *
 * It will try to determine if it is loading a Unix file by
 * looking at what should be the magic number.  If it makes
 * sense, it will use it; otherwise it jumps to the first
 * address of the blocks that it reads in.
 *
 * This new boot program will open a file, read the ELF header,
 * attempt to allocate and map memory at the location at which
 * the client desires to be linked, and load the program at
 * that point.  It will then jump there.
 */

/*ARGSUSED1*/
int
main(void *cookie, char **argv, int argc)
{
	static	char    bpath[256], bargs[256];
	int	once = 0;
#ifdef	sun4u
	extern void retain_nvram_page();
#endif

	prom_init("boot", cookie);
	fiximp();

	dprintf("\nboot: V%d /boot interface.\n", BO_VERSION);
#ifdef HALTBOOT
	prom_enter_mon();
#endif /* HALTBOOT */

#ifdef DEBUG_MMU
	dump_mmu();
#endif /* DEBUG_MMU */

	init_memlists();

#ifdef DEBUG_LISTS
	dprintf("Physmem avail:\n");
	if (debug) print_memlist(pfreelistp);
	dprintf("Virtmem avail:\n");
	if (debug) print_memlist(vfreelistp);
	dprintf("Phys installed:\n");
	if (debug) print_memlist(pinstalledp);
	prom_enter_mon();
#endif /* DEBUG_LISTS */

	set_default_filename(defname);
	setup_bootpath(bpath, bargs);
	get_filename_from_boot_args(filename, bargs);

	dprintf("bootpath: %p %s bootargs: %p %s filename: %p %s\n",
	    bpath, bpath, bargs, bargs, filename, filename);

	/* translate bpath to v2 format */
	translate_tov2(&v2path, bpath);
	v2args = bargs;

	/*
	 * Our memory lists should be "up" by this time
	 */

	setup_bootops();

	if (bargs && *bargs)
		boothowto = bootflags(bargs);

#ifdef	sun4u
	retain_nvram_page();
#endif

	setup_bootargs(bargs);

	systype = set_fstype(v2path, bpath);

loop:
	if (verbosemode)
		printf("device path '%s'\n", v2path);

	/*
	 * Open our native device driver
	 */
	if (mountroot(bpath) != SUCCESS)
		prom_panic("Could not mount filesystem.\n");

#if defined(_LP64) && !defined(__ia64)
	/*
	 * Now that root is mounted, we can choose the default
	 * filename based on what actually exists in the filesystem.
	 * That may change the filename we boot from, so lets
	 * run it through the algorithm again.
	 */
	set_default_filename(choose_default_filename(defname, defname64));
	get_boot_args(bargs);	/* using new defaults */
	get_filename_from_boot_args(filename, bargs);
#endif	/* _LP64 */

	if (once == 0 &&
	    (strcmp(systype, "ufs") == 0 || strcmp(systype, "hsfs") == 0)) {
		char redirect[256];

		post_mountroot(filename, redirect);

		/*
		 * If we return at all, it's because we discovered
		 * a redirection file - the 'redirect' string now contains
		 * the name of the disk slice we should be looking at.
		 *
		 * Unmount the filesystem, tweak the boot path and retry
		 * the whole operation one more time.
		 */
		closeall(1);
		once++;
		redirect_boot_path(&v2path, bpath, redirect);
		if (verbosemode)
			printf("%sboot: using '%s'\n", systype, bpath);

		goto loop;
		/*NOTREACHED*/
	}
	post_mountroot(filename, NULL);
	/*NOTREACHED*/
	return (0);
}

#if !defined(i386) && !defined(__ia64)
/*
 * The slice redirection file is used on the install CD
 */
int
read_redirect(char *redirect)
{
	int fd;
	char slicec;
	size_t nread = 0;

	if ((fd = open(BOOT_REDIRECT, O_RDONLY)) != -1) {
		/*
		 * Read the character out of the file - this is the
		 * slice to use, in base 36.
		 */
		nread = read(fd, &slicec, 1);
		(void) close(fd);
		if (nread == 1)
			*redirect++ = slicec;
	}
	*redirect = '\0';

	return (nread == 1);
}
#endif /* !defined(i386) */

/*
 * Fill the filename buffer (fp) from the filename in the arg buffer (bp)
 * The filename is the first string in the argument buffer, and is
 * either NULL or space terminated.
 */
void
get_filename_from_boot_args(char *fp, char *bp)
{
	while ((*bp) && (*bp != ' '))
		*fp++ = *bp++;
	*fp = (char)0;
}

/*
 * These routines aid us in selecting the default filename on systems
 * capable of booting a 64-bit OS, that also support a 32-bit OS.
 * We want the ability to default to the 64-bit OS if it exists,
 * but fallback to the 32-bit OS if it doesn't exist or the prom
 * isn't 64-bit ready.  In this case, the default filename isn't
 * static, and has to be chosen after we mount root because we have
 * to look at the root filesystem and see which files exist.
 *
 * NB: kernname is always exported as the boot property 'default-name'
 * which is imported by kadb and used as its default load file.
 */

void
set_default_filename(char *filename)
{
	kernname = filename;
}

#if !defined(i386) && !defined(__ia64)
char *
get_default_filename(void)
{
#ifdef _LP64
	/*
	 * This is really just a sun4u thing, so we can warn if we're
	 * using the 32-bit default because we have down-rev firmware.
	 * We can't tell at the time we choose the default if we're
	 * actually going to use the default, so we look for and print
	 * a message here in case that's what happened.
	 */
	extern char *warn_about_default;
	extern char *boot_message;

	if (warn_about_default) {
		boot_message = warn_about_default;	/* export the msg */
		printf(warn_about_default);
		warn_about_default = 0;
	}
#endif /* _LP64 */
	return (kernname);
}
#endif
