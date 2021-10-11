/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootops.c	1.63	99/10/04 SMI"

/*
 * Implementation of the vestigial bootops vector for platforms using the
 * 1275-like boot interfaces.
 */

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/param.h>
#include <sys/obpdefs.h>
#include <sys/promif.h>
#include <sys/salib.h>

#if defined(DEBUG) || defined(lint)
static int debug = 1;
#else
static int debug = 0;
#endif
#define	dprintf	if (debug) printf

extern caddr_t	memlistpage;
extern caddr_t	tablep;
extern int	verbosemode;
extern struct memlist 	*pinstalledp, *pfreelistp, *vfreelistp;

extern void	update_memlist(char *name, char *prop, struct memlist **l);
extern void	print_memlist(struct memlist *av);
extern void	install_memlistptrs(void);
extern struct memlist	*fill_memlists(char *name, char *prop,
    struct memlist *old);

extern int 	boot1275_entry_asm(void *);
extern int 	boot1275_entry(void *);
extern void 	boot_fail_gracefully(void);
extern void 	boot_fail_gracefully_asm(void);
extern caddr_t	kmem_alloc(size_t);
extern int	bgetprop(struct bootops *, char *name, void *buf);
extern int	bgetproplen(struct bootops *, char *name);
extern char	*bnextprop(struct bootops *, char *prev);
extern char	*get_default_filename(void);

extern void	kern_killboot(void);
extern void	closeall(int);

/*
 * This is the number for this version of bootops, which is vestigial.
 * Standalones that require the old bootops will look in bootops.bsys_version,
 * see this number is higher than they expect and fail gracefully.
 * They can make this "peek" successfully even if they are ILP32 programs.
 */
int boot_version = BO_VERSION;

uint64_t memlistextent;		/* replacement for old member of bootops */
struct bootops bootops;

#define	skip_whitespc(cp) while (cp && (*cp == '\t' || *cp == '\n' || \
	*cp == '\r' || *cp == ' ')) cp++;
/*
 *  This routine is for V2+ proms only.  It assumes
 *  and inserts whitespace twixt all arguments.
 *
 *  We have 2 kinds of inputs to contend with:
 *	filename -options
 *		and
 *	-options
 *  This routine assumes buf is a ptr to sufficient space
 *  for all of the goings on here.
 */
void
get_boot_args(char *buf)
{
	char *cp, *tp;

	tp = prom_bootargs();

	if (!tp || *tp == '\0') {
		(void) strcpy(buf, get_default_filename());
		return;
	}

	skip_whitespc(tp);

	/*
	 * If we don't have an option indicator, then we
	 * already have our filename prepended. Check to
	 * see if the filename is "vmunix" - if it is, sneakily
	 * translate it to the default name.
	 */
	if (*tp && *tp != '-') {
		if (strcmp(tp, "vmunix") == 0 || strcmp(tp, "/vmunix") == 0)
			(void) strcpy(buf, get_default_filename());
		else
			(void) strcpy(buf, tp);
		return;
	}

	/* else we have to insert it */

	/* this used to be a for loop, but cstyle is buggy */
	cp = get_default_filename();
	while (cp && *cp)
		*buf++ = *cp++;

	if (*tp) {
		*buf++ = ' ';	/* whitspc separator */

		/* now copy in the rest of the bootargs, as they were */
		(void) strcpy(buf, tp);
	} else {
		*buf = '\0';
	}
}

/* I wish we could include <stddef.h>, but... */
#define	offsetof(s, m)  (size_t)(&(((s *)0)->m))

void
setup_bootops(void)
{
	/* sanity-check bsys_printf - old kernels need to fail with a message */
#if !defined(lint)
	if (offsetof(struct bootops, bsys_printf) != 60) {
		printf("boot: bsys_printf is at offset %d instead of 60\n"
		    "boot: this will likely make old kernels die without "
		    "printing a message.\n",
		    offsetof(struct bootops, bsys_printf));
	}
	/* sanity-check bsys_1275_call - if it moves, kernels cannot boot */
	if (offsetof(struct bootops, bsys_1275_call) != 24) {
		printf("boot: bsys_1275_call is at offset %d instead of 24\n"
			"boot: this will likely break the kernel\n",
		    offsetof(struct bootops, bsys_1275_call));
	}
#endif
	bootops.bsys_version = boot_version;
#ifdef _LP64
	bootops.bsys_1275_call = (uint64_t)boot1275_entry_asm;
	/* so old kernels die with a message */
	bootops.bsys_printf = (uint32_t)boot_fail_gracefully_asm;
#else
	bootops.bsys_1275_call = (uint64_t)boot1275_entry;
	bootops.bsys_printf = (uint32_t)boot_fail_gracefully;
#endif

	if (!memlistpage) /* paranoia runs rampant */
		prom_panic("\nMemlistpage not setup yet.");
#ifndef _LP64
	bootops.boot_mem = (struct bsys_mem *)memlistpage;
#endif
	/*
	 * The memory list should always be updated last.  The prom
	 * calls which are made to update a memory list may have the
	 * undesirable affect of claiming physical memory.  This may
	 * happen after the kernel has created its page free list.
	 * The kernel deals with this by comparing the n and n-1
	 * snapshots of memory.  Updating the memory available list
	 * last guarantees we will have a current, accurate snapshot.
	 * See bug #1260786.
	 */
	update_memlist("virtual-memory", "available", &vfreelistp);
	update_memlist("memory", "available", &pfreelistp);

	dprintf("\nPhysinstalled: ");
	if (debug) print_memlist(pinstalledp);
	dprintf("\nPhysfree: ");
	if (debug) print_memlist(pfreelistp);
	dprintf("\nVirtfree: ");
	if (debug) print_memlist(vfreelistp);
}

void
install_memlistptrs(void)
{

	/* prob only need 1 page for now */
	memlistextent = tablep - memlistpage;

#ifndef _LP64
	/* Actually install the list ptrs in the 1st 3 spots */
	/* Note that they are relative to the start of boot_mem */
	bootops.boot_mem->physinstalled = pinstalledp;
	bootops.boot_mem->physavail = pfreelistp;
	bootops.boot_mem->virtavail = vfreelistp;

	/* prob only need 1 page for now */
	bootops.boot_mem->extent = memlistextent;
#endif

	dprintf("physinstalled = %p\n", pinstalledp);
	dprintf("physavail = %p\n", pfreelistp);
	dprintf("virtavail = %p\n", vfreelistp);
	dprintf("extent = %llx\n", memlistextent);
}

/*
 *      A word of explanation is in order.
 *      This routine is meant to be called during
 *      boot_release(), when the kernel is trying
 *      to ascertain the current state of memory
 *      so that it can use a memlist to walk itself
 *      thru kvm_init().
 *      There are 3 stories to tell:
 *      SunMon:  We will have been keeping
 *              memlists for this prom all along,
 *              so we just export the internal list.
 *      V0:     Again, we have been keeping memlists
 *              for V0 all along, so we just export
 *              the internally-kept one.
 *      V2+:    For V2 and later (V2+) we need to
 *              reread the prom memlist structure
 *              since we have been making prom_alloc()'s
 *              fast and furious until now.  We just
 *              call fill_memlists() again to take
 *              another V2 snapshot of memory.
 */

void
update_memlist(char *name, char *prop, struct memlist **list)
{
	if (prom_getversion() > 0) {
		/* Just take another prom snapshot */
		*list = fill_memlists(name, prop, *list);
	}
	install_memlistptrs();
}

/*
 *  This routine is meant to be called by the
 *  kernel to shut down all boot and prom activity.
 *  After this routine is called, PROM or boot IO is no
 *  longer possible, nor is memory allocation.
 */
void
kern_killboot(void)
{
	if (verbosemode) {
		dprintf("Entering boot_release()\n");
		dprintf("\nPhysinstalled: ");
		if (debug) print_memlist(pinstalledp);
		dprintf("\nPhysfree: ");
		if (debug) print_memlist(pfreelistp);
		dprintf("\nVirtfree: ");
		if (debug) print_memlist(vfreelistp);
	}
	if (debug) {
		printf("Calling quiesce_io()\n");
		prom_enter_mon();
	}

	/* close all open devices */
	closeall(1);

	/*
	 *  Now we take YAPS (yet another Prom snapshot) of
	 *  memory, just for safety sake.
	 *
	 * The memory list should always be updated last.  The prom
	 * calls which are made to update a memory list may have the
	 * undesirable affect of claiming physical memory.  This may
	 * happen after the kernel has created its page free list.
	 * The kernel deals with this by comparing the n and n-1
	 * snapshots of memory.  Updating the memory available list
	 * last guarantees we will have a current, accurate snapshot.
	 * See bug #1260786.
	 */
	update_memlist("virtual-memory", "available", &vfreelistp);
	update_memlist("memory", "available", &pfreelistp);

	if (verbosemode) {
	dprintf("physinstalled = %p\n", pinstalledp);
	dprintf("physavail = %p\n", pfreelistp);
	dprintf("virtavail = %p\n", vfreelistp);
	dprintf("extent = %llx\n", memlistextent);
	dprintf("Leaving boot_release()\n");
	dprintf("Physinstalled: \n");
		if (debug) print_memlist(pinstalledp);
		dprintf("Physfree:\n");
		if (debug) print_memlist(pfreelistp);
		dprintf("Virtfree: \n");
		if (debug) print_memlist(vfreelistp);
	}

#ifdef DEBUG_MMU
	dump_mmu();
	prom_enter_mon();
#endif
}

/*
 *  This routine creates a V0 prom device path from a V2 prom
 *  device path.
 */
/* ARGSUSED */
void
translate_v2tov0(char *v2name, char *netpath)
{
	switch (prom_getversion()) {

	case SUNMON_ROMVEC_VERSION:
	case OBP_V0_ROMVEC_VERSION:
		if (strstr(v2name, "/le@") != NULL) {
			(void) sprintf(netpath, "le(0,0,0)");
		} else {
			(void) sprintf(netpath, "ie(0,0,0)");
		}
		break;

	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
	case OBP_PSEUDO_ROMVEC_VERSION:
		(void) strcpy(netpath, v2name);
		break;

	default:
		prom_panic("Bad romvec version");
	}
}

void
boot_fail_gracefully(void)
{
	prom_panic(
	    "mismatched version of /boot interface: new boot, old kernel");
}

#ifndef _LP64

static char buf[OBP_MAXPATHLEN];

/*
 *  This routine will conz up a name from a V0 PROM which
 *  the kernel can understand as a V2 name.
 *  As long as the kernel gets its device
 *  path from boot, this routine is really only needed by boot.
 */
char *
translate_v0tov2(char *name)
{
	struct bootparam *bp = prom_bootparam();

	switch (prom_getversion()) {

	case OBP_V0_ROMVEC_VERSION:
		if (strncmp(name, "sd", 2) == 0) {
			static char targs[] = "31204567";
			dnode_t opt_node;
			dnode_t sp[OBP_STACKDEPTH];
			pstack_t *stk;

			stk = prom_stack_init(sp, sizeof (sp));
			opt_node = prom_findnode_byname(prom_nextnode(0),
			    "options", stk);
			prom_stack_fini(stk);
			if (prom_getproplen(opt_node, "sd-targets") > 0)
				(void) prom_getprop(opt_node, "sd-targets",
				    targs);
			dprintf("sd-targets is '%s'\n", targs);
			(void) sprintf(buf,
			    "/sbus@1,f8000000/esp@0,800000/sd@%c,0:%c",
			    targs[bp->bp_unit], bp->bp_part + 'a');
			dprintf("boot_dev_name: '%s'\n", buf);
			return (buf);
		} else if (strncmp(name, "le", 2) == 0) {
			(void) strcpy(buf, "/sbus@1,f8000000/le@0,c00000");
			dprintf("boot_dev_name: '%s'\n", buf);
			return (buf);
		} else if (strncmp(name, "fd", 2) == 0) {
			(void) sprintf(buf,
			    "/fd@1,f7200000:%c", bp->bp_part + 'a');
			dprintf("boot_dev_name: '%s'\n", buf);
			return (buf);
		} else {
			printf("boot device '%c%c' not supported by V0 OBP.\n",
			    *name, *(name+1));
			return ((char *)0);
		}

	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
		prom_panic("Should not find V0 name on this machine");
		/*NOTREACHED*/

	default:
		prom_panic("Bad romvec version");
		/*NOTREACHED*/
	}
	/*NOTREACHED*/
	return ((char *)0);
}
#endif
