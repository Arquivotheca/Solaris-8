/*
 * Copyright (c) 1991-1997, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)boot_plat.c	1.31	99/12/01 SMI"

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/obpdefs.h>
#include <sys/reboot.h>
#include <sys/promif.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/platnames.h>
#include <sys/salib.h>
#include <sys/elf.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include <sys/boot_policy.h>

#define	FAILURE		-1

#ifdef DEBUG
static int	debug = 1;
#define	HALTBOOT
#else /* DEBUG */
static	int	debug = 0;
#endif /* DEBUG */

#define	dprintf		if (debug) printf

extern	int (*readfile(int fd, int print))();
extern	void kmem_init(void);
extern	caddr_t kmem_alloc(size_t);
extern	void kmem_free(caddr_t, size_t);
extern	void get_boot_args(char *buf);
extern	void setup_bootops(void);
extern	struct	bootops bootops;
extern	int read_redirect(char *redirect);
extern	int gets(char *);
extern	void exitto(int (*entrypoint)());
extern	void exitto64(int (*entrypoint)(), void *bootvec);

int openfile(char *filename);

extern	char *kernname;
extern	char *impl_arch_name;
extern	struct memlist *pfreelistp, *vfreelistp, *pinstalledp;
extern	char *my_own_name;
extern	int boothowto;
#ifdef _LP64
int client_isLP64;
extern Elf32_Boot *elfbootvecELF32_64; /* Bootstrap vector ELF32 LP64 client */
extern Elf64_Boot *elfbootvecELF64;    /* ELF bootstrap vector for Elf64 LP64 */
#endif


/*
 *  We enable the cache by default
 *  but boot -n will leave it alone...
 *  that is, we use whatever state the PROM left it in.
 */
char	*mfg_name;
int	cache_state = 1;
char	filename2[MAXPATHLEN];

/*ARGSUSED*/
void
setup_bootargs(char *bargs)
{
	/*
	 * dummy
	 */
}

void
translate_tov2(char **v2path, char *bpath)
{

#ifndef _LP64
	if (prom_getversion() <= 0) {
		extern char *translate_v0tov2(char *s);

		*v2path = translate_v0tov2(bpath);
		return;
	}
#endif

	*v2path = bpath;
}

void
post_mountroot(char *bootfile, char *redirect)
{
	int (*go2)();
	int fd;
#ifdef MPSAS
	extern void sas_bpts(void);
#endif

	/* Save the bootfile, just in case we need it again */
	(void) strcpy(filename2, bootfile);

	for (;;) {
		if (boothowto & RB_ASKNAME) {
			char tmpname[MAXPATHLEN];

			printf("Enter filename [%s]: ", bootfile);
			(void) gets(tmpname);
			if (tmpname[0] != '\0')
				(void) strcpy(bootfile, tmpname);
		}

		if (boothowto & RB_HALT) {
			printf("Boot halted.\n");
			prom_enter_mon();
		}

		if ((fd = openfile(bootfile)) == FAILURE) {

			/*
			 * There are many reasons why this might've
			 * happened .. but one of them is that we're
			 * on the installation CD, and we need to
			 * revector ourselves off to a different partition
			 * of the CD.  Check for the redirection file.
			 */
			if (redirect != NULL &&
			    read_redirect(redirect)) {
				/* restore bootfile */
				(void) strcpy(bootfile, filename2);
				return;
				/*NOTREACHED*/
			}

			printf("%s: cannot open %s\n", my_own_name, bootfile);
			boothowto |= RB_ASKNAME;

			/* restore bootfile */
			(void) strcpy(bootfile, filename2);
			continue;
		}

		if ((go2 = readfile(fd, boothowto & RB_VERBOSE)) !=
		    (int(*)()) -1) {
#ifdef MPSAS
			sas_bpts();
#endif
			(void) close(fd);
		} else {
			printf("boot failed\n");
			boothowto |= RB_ASKNAME;
			continue;
		}

		if (boothowto & RB_HALT) {
			printf("Boot halted.\n");
			prom_enter_mon();
		}

		my_own_name = bootfile;
#ifdef _LP64
		if (client_isLP64) {
			dprintf("Calling exitto64(%p, %p)\n", go2,
			    elfbootvecELF64 ? (void *)elfbootvecELF64 :
				(void *)elfbootvecELF32_64);
			exitto64(go2,
			    elfbootvecELF64 ? (void *)elfbootvecELF64 :
				(void *)elfbootvecELF32_64);
			/* NOTREACHED */
		}
#endif
		dprintf("Calling exitto(%p)\n", go2);
		exitto(go2);
	}
}

/*ARGSUSED*/
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
 * Open the given filename, expanding to it's
 * platform-dependent location if necessary.
 *
 * Boot supports OBP and IEEE1275.
 *
 * XXX: Move side effects out of this function!
 */
int
openfile(char *filename)
{
	static char *fullpath;
	static char *iarch;
	static char *orig_impl_arch_name;
	static int once;
	int fd;

	if (once == 0) {

		++once;

		/*
		 * Setup exported 'boot' properties: 'mfg-name'.
		 * XXX: This shouldn't be a side effect of openfile().
		 */
		if (mfg_name == NULL)
			mfg_name = get_mfg_name();

		/*
		 * If impl_arch_name was specified on the command line
		 * via the -I <arch> argument, remember the original value.
		 */
		if (impl_arch_name) {
			orig_impl_arch_name = (char *)
			    kmem_alloc(strlen(impl_arch_name) + 1);
			(void) strcpy(orig_impl_arch_name, impl_arch_name);
		}

		fullpath = (char *)kmem_alloc(MAXPATHLEN);
		iarch = (char *)kmem_alloc(MAXPATHLEN);
	}

	/*
	 * impl_arch_name is exported as boot property, and is
	 * set according to the following algorithm, depending
	 * on the contents of the filesystem.
	 * XXX: This shouldn't be a side effect of openfile().
	 *
	 * impl_arch_name table:
	 *
	 *			root name 	default name	neither name
	 * boot args		found		found		found
	 *
	 * relative path	root name	fail		fail
	 * absolute path	root name	default name	empty
	 * -I arch		arch		arch		arch
	 *
	 */

	/*
	 * If the caller -specifies- an absolute pathname, then we just try to
	 * open it. (Mostly for booting non-kernel standalones.)
	 *
	 * In case this absolute pathname is the kernel, make sure that
	 * impl_arch_name (exported as a boot property) is set to some
	 * valid string value.
	 */
	if (*filename == '/') {
		if (orig_impl_arch_name == NULL) {
			if (find_platform_dir(boot_isdir, iarch, 1) != 0)
				impl_arch_name = iarch;
			else
				impl_arch_name = "";
		}
		(void) strcpy(fullpath, filename);
		fd = boot_open(fullpath, NULL);
		return (fd);
	}

	/*
	 * If the -I flag has been used, impl_arch_name will
	 * be specified .. otherwise we ask find_platform_dir() to
	 * look for the existance of a directory for this platform name.
	 * Preserve the given impl-arch-name, because the 'kernel file'
	 * may be elsewhere. (impl-arch-name could be 'SUNW,Ultra-1',
	 * but the kernel file itself might be in the 'sun4u' directory).
	 *
	 * When booting any file by relative pathname this code fails
	 * if the platform-name dir doesn't exist unless some
	 * -I <iarch> argument has been given on the command line.
	 */
	if (orig_impl_arch_name == NULL) {
		if (find_platform_dir(boot_isdir, iarch, 0) != 0)
			impl_arch_name = iarch;
		else
			return (-1);
	}

	fd = open_platform_file(filename, boot_open, NULL, fullpath,
	    orig_impl_arch_name);
	if (fd == -1)
		return (-1);

	/*
	 * Copy back the name we actually found
	 */
	(void) strcpy(filename, fullpath);
	return (fd);
}

void
setup_bootpath(char *bpath, char *bargs)
{
	dnode_t node;

#ifndef	_LP64
	if (prom_getversion() <= 0) {
		char	*cp;
		int 	n;

		extern void sunmon_getargs(char *defname, char *buf);
		struct bootparam *bp = prom_bootparam();

		cp = (char *)strchr(bp->bp_argv[0], ')');
		n = cp - bp->bp_argv[0] + 1;
		(void) strncpy(bpath, bp->bp_argv[0], n);
		*(bpath + n) = '\0';
		sunmon_getargs(kernname, bargs);
		return;
	}
#endif

	/*
	 * 1115931 - strip options from network device types
	 * (So standalone can handle boot net:IPADDRESS.)
	 * We don't want to do this for non-network devices,
	 * otherwise we may strip disk partition information.
	 */

	/*
	 * Convert pathname to phandle, so we can get devicetype
	 */
	node = prom_finddevice(prom_bootpath());
	if (prom_devicetype(node, "network"))
		prom_strip_options(prom_bootpath(), bpath);
	else
		(void) strcpy(bpath, prom_bootpath());

	get_boot_args(bargs);
}

/*
 * Given the boot path in the native firmware format
 * (e.g. 'sd(0,3,2)' or '/sbus@.../.../sd@6,0:d', use
 * the redirection string to mutate the boot path to the new device.
 * Fix up the 'v2path' so that it matches the new firmware path.
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
	if (slicec < '0' || slicec > '7') {
		printf("boot: bad redirection slice '%c'\n", slicec);
		return;
	}

#ifndef	_LP64
	if (prom_getversion() <= 0) {

		/*
		 * Horrible old SunMON names.
		 */
		while (--p >= bpath)
			if (*p == ',')
				break;
		if (*p++ == ',') {
			/*
			 * Slice letter is the same as the partition number.
			 */
			*p = slicec;

			/*
			 * Because we know that the v2path has been forged,
			 * we're quite confident to assert that we know its
			 * format exactly.
			 *
			 * XXX  Note that we can't just call translate_tov2()
			 *	here because that routine looks at the
			 *	'bootparam' array as part of the translation.
			 */
			p = *v2path_p;
			*(p + strlen(p) - 1) = 'a' + slicec - '0';
			return;
		}
		prom_panic("redirect_boot_path: mangled boot path!");
		/* NOTREACHED */
	}
#endif /* _LP64 */

	/*
	 * Handle fully qualified OpenBoot pathname.
	 */
	while (--p >= bpath && *p != '@' && *p != '/')
		if (*p == ':')
			break;
	if (*p++ == ':') {
		/*
		 * Convert slice number to partition 'letter'.
		 */
		*p++ = 'a' + slicec - '0';
		*p = '\0';
		translate_tov2(v2path_p, bpath);
		return;
	}
	prom_panic("redirect_boot_path: mangled boot path!");
}

#ifndef _LP64

#define	MAXARGS 8

/*
 *  Here we know everything we need except for the
 *  name of standalone we want to boot.
 *  This routine for V0/sunmon only.
 */
void
sunmon_getargs(char *defname, char *buf)
{
	struct bootparam *bp;
	char *cp;
	int i;

	bp = prom_bootparam();

	cp = bp->bp_argv[0];

	/*
	 * Since sunmon's consider the filename as part of the device
	 * string, we gotta strip it off to get at it.
	 */
	cp = strrchr(cp, ')');

	cp++;
	if (cp && *cp) {
		/*
		 * There's already a file in the bootparam
		 * so we use what the user wants .. UNLESS
		 * the name is 'vmunix' which we quietly
		 * translate to the default name anyway. Ick.
		 */
		if (strcmp(cp, "vmunix") == 0 || strcmp(cp, "/vmunix") == 0)
			(void) strcpy(buf, defname);
		else
			(void) strcpy(buf, cp);
	} else {
		/*
		 * gotta roll our own anyway
		 */
		(void) strcpy(buf, defname);
	}

	for (i = 1; i < MAXARGS; i++) {
		/* see if we have any more args */
		if ((cp = bp->bp_argv[i]) == NULL)
			break;
		/* if so, then insert blanks twixt them */
		if (*cp) {
			(void) strcat(buf, " ");
			(void) strcat(buf, cp);
		}
	}
}
#endif

#ifdef _LP64
char *warn_about_default;
static char promrev_msg[] =
	"\n"
	"NOTICE: The firmware on this system does not support the 64-bit OS.\n"
	"\tPlease upgrade to at least the following version:\n\n\t%s\n";

static char cputype_msg[] =
	"\n"
	"NOTICE: 64-bit OS installed, but the 32-bit OS is the default\n"
	"\tfor the processor(s) on this system.\n"
	"\tSee boot(1M) for more information.\n";

static char boot32_msg[] =
	"\n"
	"Booting the 32-bit OS ...\n"
	"\n";

/*
 * After we mount (or remount) root, we want to choose the
 * default filename (which gets exported as a boot property,
 * even if we don't boot from the default filename).
 * If specified on the command line, use that.
 * If the 64-bit default filename exists, and the running
 * firmware is 64-bit OS ready, then use the 64-bit filename.
 * Otherwise, fallback to the 32-bit default.
 *
 * As a side effect, if we chose the 32-bit kernel because
 * we're running down-rev firmware, leave a message behind
 * in 'warn_about_default', so we can print it later.
 */

#define	ULTRASPARC1_POLICY	"ALLOW_64BIT_KERNEL_ON_UltraSPARC_1_CPU="

char *
choose_default_filename(char *defname32, char *defname64)
{
	int fd;
	char *defname = defname32;
	dnode_t node;
	char *buf, *p;
	int downrev_prom, us1_present, us1_boot_policy;
	extern int verbosemode;
	extern char *cmd_line_default_path;
	extern int cpu_is_ultrasparc_1(void);

	if (cmd_line_default_path) {
		if (verbosemode)
			printf("boot file default (from cmd line): %s\n",
			    cmd_line_default_path);
		return (cmd_line_default_path);
	}

	/*
	 * open_platform_file writes the full pathname back to 'buf'.
	 */
	buf = (char *)kmem_alloc(MAXPATHLEN);

	fd = open_platform_file(defname64, boot_open, NULL, buf,
	    impl_arch_name);

	if (fd == -1)  {
		kmem_free(buf, MAXPATHLEN);
		if (verbosemode)
			printf("boot file default: %s\n", defname);
		return (defname);
	}

	(void) close(fd);

	/*
	 * If the 64-bit file exists but we're running down-rev
	 * firmware, back-off to the 32-bit kernel.
	 *
	 * If we're running on an UltraSPARC-1, apply cpu type policy.
	 * The default on an UltraSPARC-1 is to load the 32-bit kernel.
	 *
	 * If we end up choosing the 32-bit kernel in either or both
	 * cases, leave messages behind in case we end using the
	 * default file.
	 */

	defname = defname64;

	downrev_prom = us1_present = us1_boot_policy = 0;

	if (prom_version_check(buf, MAXPATHLEN, &node) == PROM_VER64_UPGRADE)
		downrev_prom = 1;

	if (cpu_is_ultrasparc_1()) {
		us1_present = 1;
		if (verbosemode)
			printf("CPU type default: 32-bit\n");
		policy_open();
		p = policy_lookup(ULTRASPARC1_POLICY, 1);
		if (p && (strcasecmp(p, "true") == 0)) {
			us1_boot_policy = 1;
			if (verbosemode)
				printf("CPU policy default: 64-bit\n");
		}
		policy_close();
	}

	if ((downrev_prom) || (us1_present && (us1_boot_policy == 0))) {
		defname = defname32;
		warn_about_default = p = (char *)kmem_alloc(1024);
		*p = (char)0;
		if (downrev_prom)
			(void) sprintf(p, promrev_msg, buf);
		if (us1_present && (us1_boot_policy == 0))
			(void) strcat(p, cputype_msg);
		(void) strcat(p, boot32_msg);
	}

	kmem_free(buf, MAXPATHLEN);
	if (verbosemode)
		printf("boot file default: %s\n", defname);
	return (defname);
}
#endif /* _LP64 */
