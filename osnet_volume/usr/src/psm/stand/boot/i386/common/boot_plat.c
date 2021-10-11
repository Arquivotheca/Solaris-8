/*
 * Copyright (c) 1991-1995,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)boot_plat.c	1.28	99/10/08 SMI"

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/platnames.h>
#include <sys/bootconf.h>
#include <sys/dev_info.h>
#include <sys/bootlink.h>
#include <sys/bootp2s.h>
#include <sys/bsh.h>
#include <sys/salib.h>
#include <sys/promif.h>
#include "devtree.h"

#define	VAC_DEFAULT		1
#define	PAGESHIFT_DEFAULT	12
#define	PAGESIZE_DEFAULT	(1 << PAGESHIFT_DEFAULT)

static int debug = 0;
#define	dprintf			if (debug) printf

extern	char *kernname;
extern	char *impl_arch_name;
extern	struct bootops bootops;
extern	char *makepath(char *mname, char *fname);
extern	void bsh(void);
extern	void v2_getargs(char *defname, char *buf);
extern	void compatboot_bootpath(char *bpath);
extern	int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern	int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern  void debug_init(int);
extern	void dosemul_init(void);
extern	int largepage_supported();
extern	int volume_specified();

static	void bsh_init(void);

int vac = VAC_DEFAULT;
int pagesize = PAGESIZE_DEFAULT;
int cache_state = 1;
int global_pages = 0;
struct bootops *bopp = NULL;

void
setup_bootargs(char *bargs)
{
	/*
	 * Set up some key properties (after bootops initialized)
	 * NOTE: can't set "bootpath" here, need to do some probing
	 * first, to determine bus-type, hba, other such information.
	 */
	(void) bsetprop(&bootops, "boot-args", bargs, 0, 0);
}

void
translate_tov2(char **v2path, char *bpath)
{
	/*
	 *  Note that we have to set both bootpath and boot-path
	 *  properties.  Originally we planned not to have to worry
	 *  about both by making them mirrors of one another.
	 *
	 *  Unfortunately, net booting older kernels requires that
	 *  we now withhold information that we know about network
	 *  devices.  I.E., boot-path for an older netboot must be
	 *  something akin to 'smc@0,0'.  Because we are building
	 *  real device trees now, we actually have a node in the
	 *  device tree something like 'smc@300,d0000'.  We can't
	 *  use this more useful information without causing older
	 *  kernels to choke and die.  Here's the compromise.  For
	 *  net boots, bootpath has the devtree node, but boot-path
	 *  has the old style driver@0,0 path.  Newer kernels look
	 *  first for 'bootpath' and at 'boot-path' only if they
	 *  didn't find the previous.  Older kernels always look for
	 *  'boot-path'.
	 *
	 *  At this point, we don't have any idea if this is a net
	 *  boot or anything, so at this point, the two properties
	 *  should mirror one another.
	 *
	 *  2/15/96 - timh
	 *	I believe we want the 1275 "bootpath" only to be set by
	 *  bootconf. This provides 2.6 kernels a means to determine if
	 *  it they are going to receive a real device tree.  I.E., they
	 *  would only be getting a real tree if 'bootpath' were set. I
	 *  commented out the line below in case we want to put it back
	 *  later when kernels shouldn't need to have to make such a
	 *  determination.
	 *
	 * bsetprop(&bootops, "bootpath", bpath, 0, 0);
	 */
	(void) bsetprop(&bootops, "boot-path", bpath, 0, 0);
	*v2path = bpath;
}

void
fiximp()
{
	extern int use_align;
	extern int largepage_supported();
	extern int global_bit();
	extern int enable_large_pages();
	extern int enable_global_pages();
	extern int GenuineIntel();
	extern int AuthenticAMD();
	extern int has_cpuid();

	/* We are on a brain damaged box. Bail out */
	if (!has_cpuid())
		return;

	if (AuthenticAMD() || GenuineIntel())
		use_align = largepage_supported();

	if (use_align) {
		(void) bsetprop(&bootops, "use-align", "use-align",
			strlen("use-align") + 1, 0);
		(void) enable_large_pages();
	}

	/*
	 * global_bit() uses the cpuid instruction. It does not check
	 * if that is valid. I am assuming that since we have large
	 * pages we are at least on a pentium and thus I can test with
	 * the cpuid instruction.
	 *
	 * The GenuineIntel boogery is because the global bit is not
	 * implemented on every chip that likes to call itself PPro
	 * compatible. So if we are not a GenuineIntel chip then you
	 * don't get global pages. So there.
	 */
	if (use_align && GenuineIntel())
		global_pages = global_bit();

	if (global_pages)
		(void) enable_global_pages();
}

/*ARGSUSED*/
void
post_mountroot(char *bootfile, char *redirect)
{

/*
 *  Redirection on install CD SHOULD go away with our
 *  new booting scheme.
 */
#ifdef	notdef
	int fd;
	static char bootrc[256] = "/etc/bootrc";
	extern	int read_redirect(char *redirect);

	/*
	 * If there's no /etc/bootrc file, then get cautious ..
	 */
	if ((fd = openfile(bootrc)) == -1) {

		/*
		 * There are several reasons why this might've
		 * happened .. but one of them is that we're
		 * on the installation CD, and we need to
		 * revector ourselves off to a different partition
		 * of the CD.  Check for the redirection file.
		 */
		if (redirect != NULL &&
		    read_redirect(redirect)) {
			return;
			/*NOTREACHED*/
		}
	} else
		(void) close(fd);
#endif

	debug_init(1);

	/* Set up DOS emulation */
	dosemul_init();

	/*
	 * Check whether /boot/solaris exists and setup boottree property,
	 * env_src_file and src[] accordingly
	 */
	bsh_init();

	/* Hand control over to the boot shell */
	bsh();
}

/*ARGSUSED1*/
static int
boot_open(char *pathname, void *arg)
{
	dprintf("trying '%s'\n", pathname);
	return (open(pathname, O_RDONLY));
}

static int
boot_isdir(char *pathname)
{
	int fd, retval;
	struct stat sbuf;

	dprintf("trying '%s'\n", pathname);
	if ((fd = open(pathname, O_RDONLY)) == -1)
		return (0);
	retval = 1;
	if (fstat(fd, &sbuf) == -1)
		retval = 0;
	else if ((sbuf.st_mode & S_IFMT) != S_IFDIR)
		retval = 0;
	(void) close(fd);
	return (retval);
}

/*
 * Open the given filename, expanding to its
 * platform-dependent location if necessary.
 */
int
openfile(char *filename)
{
	static char fullpath[MAXPATHLEN];
	static char iarch[MAXPATHLEN];
	int fd;
	phandle_t root_nodeid = root_node.dn_nodeid;

	/*
	 * Both kadb and boot.bin call open_platform_file from libnames
	 * which needs to use bootops to get/set properties.  But
	 * they have different names for the pointer that point to
	 * bootops (kadb uses bopp and boot.bin uses bop), so to make
	 * thing easier, bopp is added here.
	 */
	if (bopp == NULL)
		bopp = &bootops;

	/*
	 * If the caller -specifies- an absolute pathname, then we just try to
	 * open it. (Mostly for booting non-kernel standalones.)
	 */
	if ((*filename == '/') || (volume_specified(filename))) {
		if (impl_arch_name == NULL) {
			if (find_platform_dir(boot_isdir, iarch, 1) == 0)
				/* shouldn't have error, but just in case */
				(void) strcpy(iarch, "i86pc");
			impl_arch_name = iarch;
		}
		(void) bsetprop(&bootops, "name", impl_arch_name,
				strlen(impl_arch_name)+1, root_nodeid);
		fd = boot_open(filename, NULL);
		return (fd);
	}

	fd = open_platform_file(filename, boot_open, NULL, fullpath,
		impl_arch_name);

	if (fd == -1)
		return (-1);

	/*
	 * If the -I flag has been used, impl_arch_name will be specified ..
	 * otherwise we call find_platform_dir() to find the existence of a
	 * directory for this platform name.  It is possible that the boot
	 * file located in one directory while impl-arch-name is something
	 * different.  And then set the "name" property in / node to
	 * impl-arch-name.
	 */
	if (impl_arch_name == NULL) {
		if (find_platform_dir(boot_isdir, iarch, 1) == 0)
			/* shouldn't have error, but just in case */
			(void) strcpy(iarch, "i86pc");
		impl_arch_name = iarch;
	}
	(void) bsetprop(&bootops, "name", impl_arch_name,
				strlen(impl_arch_name)+1, root_nodeid);

	/*
	 * Copy back the name we actually found
	 */
	(void) strcpy(filename, fullpath);
	return (fd);
}

void
init_memlists()
{
	/*
	 * The memlists are already constructed with setup_memlists()
	 *   in memory.c.
	 * kmem_init() is done in bsetup.c.
	 */
}

void
setup_bootpath(char *bpath, char *bargs)
{
	/* BEGIN CSTYLED */
	/*
	 *  Boot-path assignment mechanism:
	 *
	 *  If we were booted from the old (pre 2.5) "blueboot" off
	 *  of an extended MDB device (i.e., one whose device code
	 *  is other than 0x80), then we assume that we are probably
	 *  installing a new system and no bootpath information exists
	 *  in the /etc/bootrc file.  In this case we generate
	 *  the boot path, by calling the compatboot_bootpath() routine.
	 *
	 *  If we were booted from the new (post 2.5) "strap.com"
	 *  (or if the boot device code is 0x80) we assume that we are
	 *  running a configured system (or one that will configure itself).
	 *  We do not generate a boot path in this case because a default
	 *  value will either be coming from the /etc/bootrc file,
	 *  from the plug-n-play device tree builder ("bootconf"),
	 *  or via user input.
	 *
	 *  NOTE: This code used to live in ".../uts/i86/promif/prom_boot.c",
	 *        but was moved here (and simplified somewhat) as part of the
	 *        version 5 boot changes.
	 *
	 *        Note also that we're assuming that the buffer supplied by
	 *        the caller is big enough to hold the bootpath.  This really
	 *        needs to be changed, either to have a length passed in or
	 *        to dynamically allocate the path and return it.
	 */
	/* END CSTYLED */
	extern struct pri_to_secboot *realp;

	if ((realp != 0) && (realp->bootfrom.ufs.boot_dev != 0) &&
	    (realp->bootfrom.ufs.boot_dev != 0x80)) {

		compatboot_bootpath(bpath);

	} else {
		/*
		 *  We don't have enough information to construct a bootpath
		 *  at this time.  Return a null string; the system will have
		 *  to get the bootpath from the /etc/bootrc file or the user
		 *  will have to type it in at the boot shell command line.
		 */
		*bpath = '\0';
	}

	/*
	 *  2/15/96 - timh
	 *	I believe we want the 1275 "bootpath" only to be set by
	 *  bootconf. This provides 2.6 kernels a means to determine if
	 *  it they are going to receive a real device tree.  I.E., they
	 *  would only be getting a real tree if 'bootpath' were set. I
	 *  commented out the line below in case we want to put it back
	 *  later when kernels shouldn't need to have to make such a
	 *  determination.
	 *
	 *  bsetprop(&bootops, "bootpath", bpath, 0, 0);
	 */
	(void) bsetprop(&bootops, "boot-path", bpath, 0, 0);
	v2_getargs(kernname, bargs);
}

/*
 * Given the boot path, use the redirection string to mutate the boot
 * path to the new device.
 */
void
redirect_boot_path(char **v2path_p, char *bpath, char *redirect)
{
	char slicec = *redirect;
	char *p = bpath + strlen(bpath);

	/*
	 * If the redirection character doesn't fall in this
	 * range, something went horribly wrong.
	 */
	if (!(('0' <= slicec && slicec <= '7') ||
	    ('a' <= slicec && slicec <= 'z'))) {
		printf("boot: bad redirection slice '%c'\n", slicec);
		return;
	}

	/*
	 * Fully qualified OpenBoot-style pathname.
	 */
	while (--p >= bpath && *p != '@' && *p != '/')
		if (*p == ':')
			break;
	if (*p++ == ':') {
		/*
		 * Convert slice number to partition 'letter'.
		 */
		*p++ = (slicec > '9') ?
			'k' + slicec - 'a' : 'a' + slicec - '0';
		*p = '\0';
		translate_tov2(v2path_p, bpath);
		return;
	}
	prom_panic("redirect_boot_path: mangled boot path!");
}

#define	ENV_SRC_FILE	"/platform/i86pc/boot/solaris/bootenv.rc"
#define	BTREE		"/platform/i86pc/boot"
/*			 0123456789012345	*/
#define	NEW_BOOT_INDEX	15

static void
bsh_init(void)
{
	char *newbt;
	extern char *env_src_file;
	extern int srcx;
	extern struct src src[];
	extern unsigned char old_config_source[];

	if (boot_isdir("/boot/solaris/drivers")) {
		/* boottree is "/boot" */
		newbt = (char *)BTREE + NEW_BOOT_INDEX;
		(void) bsetprop(&bootops, "boottree", newbt, strlen(newbt) + 1,
			chosen_node.dn_nodeid);
		env_src_file = (char *)ENV_SRC_FILE + NEW_BOOT_INDEX;
	} else {
		/* boottree is "/platform/i86pc/boot" */
		(void) bsetprop(&bootops, "boottree", BTREE, sizeof (BTREE),
			chosen_node.dn_nodeid);
		env_src_file = (char *)ENV_SRC_FILE;
		src[srcx].buf = src[srcx].nextchar = old_config_source;
	}
}
