/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootops.c	1.33	99/05/04 SMI"

#include <sys/types.h>
#include <sys/dev_info.h>
#include <sys/bootconf.h>
#include <sys/bootinfo.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/bootp2s.h>
#include <sys/bootvfs.h>
#include <sys/pte.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/psw.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/machine.h>
#include <sys/salib.h>

#include "devtree.h"

#define	__ctype _ctype		/* Incredibly stupid hack used by	*/
#include <ctype.h>		/* ".../stand/lib/i386/subr_i386.c"	*/

#ifdef DEBUG
static int DeBug = 1;
#else
#define	DeBug 0
#endif DEBUG

#define	dprintf	if (DeBug) printf

extern struct memlist *pinstalledp, *pfreelistp, *vfreelistp;
extern caddr_t memlistpage, tablep;
extern caddr_t magic_phys, max_bootaddr;
extern int verbosemode;
extern struct pri_to_secboot *realp;
extern struct bootops  *bop;
extern void *memset(void *s, int c, size_t n);
extern int doint(void);
extern int dofar(ulong_t ptr);
extern void rm_free(caddr_t virt, size_t size);
extern int setup_memlist(int type, struct bootmem *bmem, int cnt);
extern void silence_nets(void);
extern void setup_devtree(void);
extern void bootabort(void);

#define	MAXARGS	8

int boot_version = BO_VERSION;
char *new_root_type = "";

/* Device tree walking functions ... */
extern phandle_t bpeer(struct bootops *, phandle_t);
extern phandle_t bchild(struct bootops *, phandle_t);
extern phandle_t bparent(struct bootops *, phandle_t);
extern phandle_t bmknod(struct bootops *, phandle_t);
extern ihandle_t bmyself(struct bootops *);
extern int	 binst2path(struct bootops *, ihandle_t, char *, int);
extern phandle_t binst2pkg(struct bootops *, ihandle_t);
extern int	 bpkg2path(struct bootops *, phandle_t, char *, int);

/* Device tree property mgmt */
extern int	boldgetproplen(struct bootops *bop, char *name);
extern int	boldgetprop(struct bootops *bop, char *name, void *value);
extern int	boldsetprop(struct bootops *bop, char *name, char *value);
extern char	*boldnextprop(struct bootops *bop, char *prevprop);
extern int	bgetproplen(struct bootops *, char *, phandle_t);
extern int	bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int	bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int	bnextprop(struct bootops *, char *, char *, phandle_t);

/* ix86 real mode interface */
extern int 	rm_exec(struct bootops *, int, char **);
extern void 	bold_rm_call(struct bootops *, int, struct bop_regs *);
extern int 	rm_call(struct bootops *, caddr_t, struct bop_regs *);
extern long	rm_cvt(struct bootops *, unsigned long, int);

/* Misc memlist stuff */
extern void	 update_memlist(char *, char *, struct memlist **);
extern void  print_memlist(struct memlist *);

/* Stubs for un-implemented fsw functions */
#define	mount(x, y, z) -1
#define	umount(x)	   -1

/* Bootops snarfing on behalf of dos modules */
extern void	snarf_succeed(void);
extern void	snarf_fail(void);

/*
 *  File Services:
 *
 *	These functions are supported thru the standalone file system switch
 *	(see ".../stand/lib/fs/common/fsswitch.c").  All we have to do here
 *	is get the arguments in the right order.
 */

/*ARGSUSED*/
static int
bkern_open(struct bootops *bop, char *str, int flags)
{
	/* Open a file ... */

	return (kern_open(str, flags));
}

/*ARGSUSED*/
static int
bkern_close(struct bootops *bop, int fd)
{
	/* Close a file ... */

	return (kern_close(fd));
}

/*ARGSUSED*/
static int
bkern_read(struct bootops *bop, int fd, caddr_t buf, size_t size)
{
	/* Read from open file ... */

	return (kern_read(fd, buf, size));
}

/*ARGSUSED*/
static int
bkern_lseek(struct bootops *bop, int fd, off_t hi, off_t lo)
{
	/* Seek to given file offset (64-bits) ... */

	return ((kern_lseek(fd, hi, lo) < 0) ? -1 : 0);
}

/*ARGSUSED*/
static int
bkern_getdents(struct bootops *bop, int fd, struct dirent *buf, size_t size)
{
	/* Get directory entries (like "readir") ... */

	return (kern_getdents(fd, buf, size));
}

/*ARGSUSED*/
static int
bkern_fstat(struct bootops *bop, int fd, struct stat *buf)
{
	/* Get attributes of an open file ... */

	return (kern_fstat(fd, buf));
}

/*ARGSUSED*/
int
bkern_mount(struct bootops *bop, char *dev, char *mpt, char *type)
{
	/*
	 *  Mount a file system:
	 *
	 *    This is pretty straight forward except that we have to watch for
	 *    users attempting to mount the root device "/" and do a bit of
	 *    special checking here.
	 */
	int dfd, rc = -1;

	/*
	 * Here we "reserve" a spot in the snarf output, by marking our
	 * snarf buffer a success to begin with.
	 */
	snarf_succeed();

	if ((new_root_type = type) == 0) {
		/*
		 *  Caller has not supplied a file system type, which
		 *  means we have to use "prom_open" to probe the device
		 *  to determine just what the type is.  If it works,
		 *  prom_open returns the file system type in "new_root_type".
		 */
		struct pri_to_secboot *rxp = realp;
		int fd;

		fd = prom_open(dev);

		type = new_root_type;
		realp = rxp;

		if (fd <= 0) {
			/*
			 *  Can't determine device type, which probably
			 *  means the device doesn't support stand-alone
			 *  file systems!
			 */
			snarf_fail();
			return (-1);
		}

		(void) prom_close(fd);
	}

	if (strcmp(mpt, "/") == 0) {
		/*
		 *  Caller is trying to mount the root.  This means we have
		 *  to make sure the file system type is valid, then call
		 *  the corresponding "mountroot" entry in the fsw.
		 */
		static int root_is_mounted = 0;
		struct boot_fs_ops *fop;

		if (root_is_mounted) {
			/*
			 *  Root is already mounted.  Generate error message
			 *  and exit.  Note that main's call to "mountroot"
			 *  doesn't count as a real root mount (it was
			 *  required to find the "bootrc" file that
			 *  set the real mount device).
			 */
			snarf_fail();
			printf("mount: root already mounted\n");

		} else if (fop = get_fs_ops_pointer(type)) {
			/*
			 *  File system type is valid, now we can actually
			 *  do something, namely:  Unmount the current root
			 *  file system and mount the new one.
			 */
			new_root_type = fop->fsw_name;

			if ((dfd = prom_open(dev)) > 0) {
				/*
				 *  We just opened the target device in
				 *  support of the requested file system type.
				 *  Now lets see if we can mount it over the
				 *  root ...
				 */
				if (!(rc = mountroot(dev))) {
					/*
					 *  New root is now mounted.  Unmount
					 *  the old root and change file system
					 *  type code!
					 */
					snarf_succeed();

				} else {
					(void) prom_close(dfd);
					printf("Mountroot failed.");
					snarf_fail();
				}

			} else {
				snarf_fail();
			}

			root_is_mounted |= !rc;

		} else {
			/*
			 *  File system type argument is bogus!
			 */
			snarf_fail();
			printf("mount: invalid file system type (%s)\n", type);
		}

	} else {
		/*
		 *  Nothing special about the mount point; use the file
		 *  system switch to perform the mount.
		 */
		if ((rc = mount(dev, mpt, type)) == 0) {
			snarf_succeed();
		} else {
			snarf_fail();
		}
	}

	new_root_type = "";
	return (rc);
}

/*ARGSUSED*/
static int
bkern_umount(struct bootops *bop, char *mpt)
{
	/*
	 *  Unmount a file system:
	 */

	if (strcmp(mpt, "/") == 0) {
		/*
		 *  User cannot unmount the root, although (s)he gets one
		 *  chance to mount over it -- see above.
		 */
		printf("umount: can't unmount root\n");
		return (-1);

	} else {
		/*
		 *  Call thru the fsw.
		 */

		return (umount(mpt));
	}
}

/*
 *  Memory Managment Services:
 *
 *	The basic operations are allocate/free and map/unmap.  This version
 *	takes some flags:
 *
 *      BOPF_X86_ALLOC_CLIENT:  Free the memory when the client pgm exits.
 *      BOPF_X86_ALLOC_REAL:    Allocate realmode memory (i.e, below 1MB)
 *
 *	The older version of alloc is what we currently export via the
 *	bootops.  The wrapper function below just sets the flags argument
 *	to zero and calls our version.
 */
static caddr_t
bkern_resalloc(struct bootops *bop, caddr_t virt, size_t size, int align,
    int flags);

static caddr_t
bkern_oldalloc(struct bootops *bop, caddr_t virt, size_t size, int align)
{
	return (bkern_resalloc(bop, virt, size, align, 0));
}

/*ARGSUSED*/
static caddr_t
bkern_resalloc(struct bootops *bop, caddr_t virt, size_t size, int align,
    int flags)
{
	/* Allocate memory ... */

	extern caddr_t kern_resalloc(caddr_t virthint, size_t size, int align);
	extern caddr_t rm_malloc(size_t, uint_t, caddr_t);

	caddr_t rc = (flags & BOPF_X86_ALLOC_REAL)
	    ? rm_malloc(size, align, virt) : kern_resalloc(virt, size, align);

#ifdef	notdef
	if (rc && (flags & BOPF_X86_ALLOC_CLIENT)) {
		/*
		 *  +++ ENHANCEMENT +++
		 *  If allocation is successful, build a "freeup" list entry
		 *  that we can use to free this memory when the client program
		 *  exits!
		 */

	}
#endif	/* notdef */

	return (rc);
}

/*ARGSUSED*/
static void
bkern_free(struct bootops *bop, caddr_t virt, size_t size)
{
	/* Free memory ... */

	if (virt < (caddr_t)USER_START) {
		rm_free(virt, size);	/* Low memory below 1MB	*/
	} else if (virt < magic_phys)
		bkmem_free(virt, size);	/* Boot scratch memory	*/

#ifdef	notdef
	else
		resfree(RES_CHILDVIRT, virt, size);
#endif	/* notdef */
}

/*ARGSUSED*/
static caddr_t
bkern_map(struct bootops *bop, caddr_t virt, int space, caddr_t phys,
    size_t size)
{
	/* Map physical to virtual (not yet supported) ... */
	return (0);
}

/*ARGSUSED*/
static void
bkern_unmap(struct bootops *bop, caddr_t virt, size_t size)
{
	/* Unmap & free both memory and address space ... */
}

/*ARGSUSED*/
static void
bkern_printf(struct bootops *bop, char *fmt, ...)
{
	/*
	 *  The C pre-processor is too stupid to allow this to be a macro.  We
	 *  simply install this in the bootops struct as-is.
	 */
	va_list adx;

	va_start(adx, fmt);
	prom_vprintf(fmt, adx);
	va_end(adx);
}

/*
 * Need to work around problems booting old kernels.
 */
int client_vers;

#define	NO_CLIENT_VERS 0
#define	OLD_CLIENT_VERS 1
#define	CUR_CLIENT_VERS 2

/*ARGSUSED*/
int
get_end(struct dnode *dnp, int *buf, int len, int *value)
{
	/*
	 * Return the end of boot. The caller wants to know
	 * the lowest physical address after boot. Have
	 * to give the conservative answer which is the
	 * max value that the bkmem_alloc arena can grow.
	 */


	if (!buf || (len > sizeof (int)))
		len = sizeof (int);
	if (buf)
		*buf = (int)max_bootaddr;
	/*
	 * Hack alert - only new clients know to ask for
	 * this property. If this property is read before
	 * memory-update, then the client knows how to handle a
	 * real memlist.
	 */
	if (client_vers == NO_CLIENT_VERS)
		client_vers = CUR_CLIENT_VERS;

	return (len);
}

int
install_memlistptrs(void)
{
	/*
	 * If we publish the memlists before we are asked
	 * for boot-end, assume an old client.
	 */
	if (client_vers == NO_CLIENT_VERS) {
		/*
		 * Booting old kernel; protect the
		 * boot pages by removing them from physavail.
		 * Old kernels already protect below 1Mb, so
		 * just worry about the rest.
		 */
		extern struct bootinfo *bip;
		paddr_t	one_meg = (paddr_t)(1024*1024);
		long size;
		int i;
		struct memlist *mcur, *mnext;

		size = (long)(max_bootaddr - (char *)one_meg);
		for (i = 0; i < B_MAXARGS; i++) {
			if (bip->memavail[i].base != one_meg)
				continue;
			if (bip->memavail[i].extent <= size)
				continue;
			bip->memavail[i].base += size;
			bip->memavail[i].extent -= size;
			break;
		}
		if (i == B_MAXARGS) {
			printf("cannot protect boot\n");
			bootabort();
		}
		mcur = pfreelistp;
		pfreelistp = 0;
		while (mcur) {
			mnext = mcur->next;
			rm_free((char *)mcur, sizeof (*mcur));
			mcur = mnext;
		}


		/* Setup available list */
		(void) setup_memlist(MEM_AVAIL, bip->memavail,
			bip->memavailcnt);

		client_vers = OLD_CLIENT_VERS;
	}
	/* Actually install the list ptrs in the 1st 3 spots */
	/* Note that they are relative to the start of boot_mem */

	bop->boot_mem->physinstalled = pinstalledp;
	bop->boot_mem->physavail = pfreelistp;
	bop->boot_mem->virtavail = vfreelistp;

	/* prob only need 1 page for now */
	bop->boot_mem->extent = tablep - memlistpage;

	/*CONSTCOND*/
	dprintf("physinstalled = %x\n", bop->boot_mem->physinstalled);
	/*CONSTCOND*/
	dprintf("physavail = %x\n", bop->boot_mem->physavail);
	/*CONSTCOND*/
	dprintf("virtavail = %x\n", bop->boot_mem->virtavail);
	/*CONSTCOND*/
	dprintf("extent = %x\n", bop->boot_mem->extent);

	return (0);
}

/*
 *  This routine is meant to be called by the
 *  kernel to shut down all boot and prom activity.
 *  After this routine is called, PROM or boot IO is no
 *  longer possible, nor is memory allocation.
 */

void
bkern_killboot(struct bootops *bop)
{
	/*CONSTCOND*/
	if (DeBug) {
	    if (verbosemode) {
		printf("Entering boot_release()\n");
		printf("\nPhysinstalled: "); print_memlist(pinstalledp);
		printf("\nPhysfree: "); print_memlist(pfreelistp);
		printf("\nVirtfree: "); print_memlist(vfreelistp);
	    }

	    printf("Calling quiesce_io()\n");
	    (void) goany();
	}

	/*
	 *  open and then close all network devices
	 *  must walk devtree for this
	 */

	silence_nets();

	/* close all open devices */
	closeall(1);
	(void) install_memlistptrs();

	/*CONSTCOND*/
	if (DeBug) {
	    if (verbosemode) {
			printf("physinstalled = %x\n",
			    bop->boot_mem->physinstalled);
			printf("physavail = %x\n", bop->boot_mem->physavail);
			printf("virtavail = %x\n", bop->boot_mem->virtavail);
			printf("extent = %x\n", bop->boot_mem->extent);
			printf("Leaving boot_release()\n");
			printf("Physinstalled: \n"); print_memlist(pinstalledp);
			printf("Physfree:\n"); print_memlist(pfreelistp);
			printf("Virtfree: \n"); print_memlist(vfreelistp);
		}
	}

#if (defined(DEBUG_MMU))
{
		dump_mmu();
		(void) goany();
}
#endif /* DEBUG_MMU */
}

/*
 * we currently support up through "set 1" of the bootops extensions.
 * these are functions at the end of the struct bootops (see bootconf.h)
 * that must be checked for at run-time before the client uses them.
 *
 * cachefs boot revs the bootops extensions to version 2.
 */
int bootops_extensions = 2;

struct bootops bootops =
{
	/* stuff required by BO_VERSION == 5 ... */

	BO_VERSION,	/* "major" version number */
	0, 0,		/* parent & memlist pointers */
	bkern_open,	/* Open a file */
	bkern_read,	/* Read from a file */
	bkern_lseek,	/* Seek into a file */
	bkern_close,	/* Close a file */
	bkern_oldalloc,	/* G.P. memory allocator */
	bkern_free,	/* G.P. memory release */
	bkern_map,	/* Map physical addrs to virtual */
	bkern_unmap,	/* Remove a mapping */
	bkern_killboot,	/* Shutdown boot services */
	boldgetproplen,	/* Get property length */
	boldgetprop,	/* Get property value */
	boldsetprop,	/* Set property value */
	boldnextprop,	/* Get next property and its value */
	bkern_printf,	/* BIOS printf */
	bold_rm_call,	/* Perform a real-mode software int */

	/* end of stuff required by BO_VERSION == 5 ... */

	/* stuff required by bootops_extensions >= 1 ... */

	bgetproplen,	/* 1275-ish Get property length */
	bgetprop,	/* 1275-ish Get property value */
	bsetprop,	/* 1275-ish Set property value */
	bnextprop,	/* 1275-ish Get next property and its value */
	bkern_mount,	/* Mount a file system */
	bkern_umount,	/* Unmount a file system */
	bkern_fstat,	/* File status */
	bkern_getdents,	/* Read directories */
	bpeer,		/* 1275-ish Get sibling node */
	bchild,		/* 1275-ish Get child node */
	bparent,	/* 1275-ish Get parent node */
	bmyself,	/* 1275-ish Get current node */
	binst2path,	/* 1275-ish Get full device path */
	binst2pkg,	/* 1275-ish Get "file" part of path */
	bpkg2path,	/* 1275-ish Get "dir" part of path */
	bmknod,		/* 1275-ish Create a device node */
	rm_exec,	/* Execute a realmode (DOS) program */
	rm_call,	/* Call into real-mode (far call or software int) */
	rm_cvt,		/* Convert linear adr to segment/offset	*/
	bkern_resalloc,	/* Enhanced version of alloc */

	/* end of stuff required by bootops_extensions >= 1 ... */
};

/*
 *	setbopvers -- If required, lie about the bootops version.
 *
 *	This is another compatibility booting change. The bootops
 *	version number changed between release 2.4 and 2.5 of Solaris.
 *	The 2.4 bootops are a proper subset of the 2.5 bootops. This
 *	means we can support booting a 2.4 kernel even though we have
 *      a different bootops version number.  However, the kernel will
 *	check to ensure it has the right version number during its
 *	startup() routine.  So, this booter must lie about the version
 *	number when booting these old kernels.
 *
 *	So how do we know we are booting an old kernel?  Well, another
 *	significant change between 2.4 and 2.5 was the first phase of KBI
 *	changes.  This resulted in the addition of the new "/platform"
 *	directory on the root.  So, we check to see if that directory exists
 *	on the root.  If it does NOT we assume we are booting an older kernel.
 */
void
setbopvers(void)
{
	int fd;

	if ((fd = open("/platform", O_RDONLY)) >= 0) {
		(void) close(fd);
		/*CONSTCOND*/
		if (DeBug)
			printf("setbopvers:BO_VERSION is cool.\n");
		bop->bsys_version = boot_version;
	} else {
		/*CONSTCOND*/
		if (DeBug)
			printf("setbopvers:Kernel requires old BO_VERSION\n");
		bop->bsys_version = 4;
	}
}

void
setup_bootops(void)
{
	/*
	 *  Initialize the bootops struct and establish a pointer to it ("bop")
	 *  for use by standalone clients.
	 */
	setup_devtree();
	bop = &bootops;

	if ((bootops.boot_mem = (struct bsys_mem *)memlistpage) == 0)
		prom_panic("\nMemlistpage not setup yet.");

	/*CONSTCOND*/
	if (DeBug) {
		printf("\nPhysinstalled: "); print_memlist(pinstalledp);
		printf("\nPhysfree: "); print_memlist(pfreelistp);
		printf("\nVirtfree: "); print_memlist(vfreelistp);
	}
}

void
v2_getargs(char *defname, char *buf)
{
	/*
	 *  This routine is for V2+ proms only.  It assumes and inserts
	 *  whitespace twixt all arguments.  We have 2 kinds of inputs
	 *  to contend with:
	 *
	 *	    filename -options
	 *
	 *		   and
	 *
	 *	    -options
	 *
	 *  This routine assumes "buf" is a pointer to sufficient space for
	 *  all of the goings on here.
	 */
	char *cp, *tp;

	tp = prom_bootargs();
	while (isascii(*tp) && isspace(*tp)) tp++;

	if (*tp && *tp != '-') {
		/*
		 * If we don't have an option indicator, then we already
		 * have our filename prepended. Check to see if the filename
		 * is "unix" - if it is, sneakily translate it to the default
		 * name.
		 */

		if (!(strcmp(tp, "unix") == 0) ||
		    !(strcmp(tp, "/unix") == 0))
			(void) strcpy(buf, defname);
		else
			(void) strcpy(buf, tp);
		return;
	}

	/* else we have to insert it */
	for (cp = defname; cp && *cp; *buf++ = *cp++);

	if (*tp) {
		*buf++ = ' ';	/* whitspace separator */

		/* now copy in the rest of the bootargs, as they were */
		(void) strcpy(buf, tp);
	} else {
		*buf = '\0';
	}
}

/*
 *  Realmode interface routines (except for "rm_exec" which lives in
 *  "./misc_utls.c").
 */
/*ARGSUSED*/
long
rm_cvt(struct bootops *bop, unsigned long a, int dir)
{
	/*
	 *  Converts to/from linear/real addresses:
	 *
	 *  The specified "a"ddr is converted from real to linear or from linear
	 *  to real depending on the "dir"ection argument.
	 */

	return (dir ? mk_ea((a >> 16), (a & 0xFFFF)) :
	    ((segpart(a)) << 16) | (offpart(a)));
}

void
bold_rm_call(struct bootops *bop, int loc, struct bop_regs *rp)
{
	(void) rm_call(bop, (caddr_t)loc, rp);
}

/*ARGSUSED*/
int
rm_call(struct bootops *bop, caddr_t loc, struct bop_regs *rp)
{
	/*
	 *  Realmode callouts:
	 *
	 *  This routine is used to call realmode routines from protected mode
	 *  (boot client) code.  If the target "loc"action is less that the DOS
	 *  program load address (0x100), we assume that the boot client wants
	 *  to call into the BIOS (i.e, issue an "int" instruction).  Otherwise,
	 *  we call the realmode subroutine at the target address.
	 *
	 *  Arguments to appear in the (16-bit) registers upon entry to the
	 *  realmode routine are passed in the "bop_regs" structure at "*rp".
	 *  This structure is also used to return values delivered in registers
	 *  by the target routine.
	 *
	 *  Return value depends on the target "loc"ation.  For BIOS calls, it's
	 *  the value of the carry bit upon return from the software interrupt.
	 *  For subroutine calls, it's the value of the "ax" register upon re-
	 *  turn from the target function.
	 */
	int rc;
	extern struct int_pb ic;

	ic.ax = rp->eax.word.ax;  /* Copy register values from client's	*/
	ic.bx = rp->ebx.word.bx;  /* .. parameter area to low-core.	*/
	ic.cx = rp->ecx.word.cx;
	ic.dx = rp->edx.word.dx;
	ic.bp = rp->ebp.word.bp;
	ic.si = rp->esi.word.si;
	ic.di = rp->edi.word.di;
	ic.es = rp->es;
	ic.ds = rp->ds;

	if (loc >= (caddr_t)0x100) {
		/*
		 *  Caller wants to call a realmode subroutine.  Convert the
		 *  subroutine "loc"ation to segment:offset format and
		 *  "dofar" it!
		 */
		ic.intval = 0;
		rc = dofar(mk_farp((ulong_t)loc));
		/*
		 * XXX -- In future we need to retrieve the actual flags
		 *   values that resulted from the call!!!!
		 */
	} else {
		/*
		 *  Caller wants to issue a BIOS call.  Store the interrupt
		 *  number in the low-core parameter area and "doint" it!
		 */
		ic.intval = (ushort_t)(ulong_t)loc;
		rc = doint();
		rp->eflags = (rc ? PS_C : 0);
	}

	(void) memset(rp, 0, sizeof (*rp));	/* J.I.C. */

	/*
	 * This next line is a horrid hack, just for the moment.
	 *
	 * . The only client we have right now is the PCI stuff.
	 * . Nothing that requires 32 bits will work until
	 *   we implement it here.
	 * . Things that require only 16 bits should be looking
	 *   at the 16 bit registers.
	 *
	 * Therefore we might as well give PCI what it wants,
	 * so it won't have to change when we go to 32 bits.
	 *
	 * (Note that the low-order half is overwritten below.
	 * I include the whole thing here for clarity.)
	 */
	rp->edx.edx = 'P' | 'C'<<8 | 'I'<<16 | ' '<<24;

	rp->eax.word.ax = ic.ax; /* Copy result regs back into client's	*/
	rp->ebx.word.bx = ic.bx; /* .. parameter struct.		*/
	rp->ecx.word.cx = ic.cx;
	rp->edx.word.dx = ic.dx;
	rp->ebp.word.bp = ic.bp;
	rp->esi.word.si = ic.si;
	rp->edi.word.di = ic.di;
	rp->es = ic.es;
	rp->ds = ic.ds;

	return (rc);
}
