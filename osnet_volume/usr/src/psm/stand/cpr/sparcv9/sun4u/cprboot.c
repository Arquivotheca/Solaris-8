/*
 * Copyright (c) 1994-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cprboot.c	1.37	99/11/05 SMI"

/*
 * cprboot - prom client that restores kadb/kernel pages
 *
 * simple cprboot overview:
 *	reset boot-file/boot-device to their original values
 * 	open cpr statefile, usually "/.CPR"
 *	read in statefile
 *	close statefile
 *	restore kernel pages
 *	jump back into kernel text
 *
 *
 * cprboot supports a restartable statefile for FAA/STARS,
 * Federal Aviation Administration
 * Standard Terminal Automation Replacement System
 */

#include <sys/types.h>
#include <sys/cpr.h>
#include <sys/debug/debug.h>
#include <sys/promimpl.h>
#include <sys/ddi.h>
#include "cprboot.h"


/*
 * local defs
 */
#define	CB_MAXPROP	256
#define	CB_MAXARGS	8


/*
 * globals
 */
struct statefile sfile;

char cpr_statefile[OBP_MAXPATHLEN];
char cpr_filesystem[OBP_MAXPATHLEN];

int cpr_debug;				/* cpr debug, set with uadmin 3 10x */
uint_t cb_sec;				/* cprboot start runtime */
uint_t cb_dents;			/* number of dtlb entries */

int halt = 0;				/* halt (enter mon) after load */
int verbose = 0;			/* verbose, traces cprboot ops */

char rsvp[] = "please reboot";
char prog[] = "cprboot";
char entry[] = "ENTRY";
char ent_fmt[] = "\n%s %s\n";


/*
 * file scope
 */
static char cb_argbuf[CB_MAXPROP];
static char *cb_args[CB_MAXARGS];

static int reusable;
static char *specialstate;


static int
cb_intro(void)
{
	static char cstr[] = "\014" "\033[1P" "\033[18;21H";

	CB_VENTRY(cb_intro);

	/*
	 * build/debug aid; this condition should not occur
	 */
	if ((uintptr_t)_end > CB_SRC_VIRT) {
		prom_printf("\ndata collision:\n"
		    "(_end=0x%p > CB_LOW_VIRT=0x%p), recompile...\n",
		    _end, CB_SRC_VIRT);
		return (ERR);
	}

	/* clear console */
	prom_printf(cstr);

	/* early timestamp for cb_terminator */
	cb_sec = prom_gettime() / 1000;

	prom_printf("Restoring the System. Please Wait... ");
	return (0);
}


/*
 * read bootargs and convert to arg vector
 *
 * sets globals:
 *	cb_argbuf
 *	cb_args
 */
static void
get_bootargs(void)
{
	char *cp, *tail, *argp, **argv;

	CB_VENTRY(get_bootargs);

	(void) prom_strcpy(cb_argbuf, prom_bootargs());
	tail = cb_argbuf + prom_strlen(cb_argbuf);

	/*
	 * scan to the trailing NULL so the last arg
	 * will be found without any special-case code
	 */
	argv = cb_args;
	for (cp = argp = cb_argbuf; cp <= tail; cp++) {
		if (prom_strchr(" \t\n\r", *cp) == NULL)
			continue;
		*cp = '\0';
		if (cp - argp) {
			*argv++ = argp;
			if ((argv - cb_args) == (CB_MAXARGS - 1))
				break;
		}
		argp = cp + 1;
	}
	*argv = NULLP;

	if (verbose) {
		for (argv = cb_args; *argv; argv++) {
			prom_printf("    %d: \"%s\"\n",
			    (argv - cb_args), *argv);
		}
	}
}


static void
usage(char *expect, char *got)
{
	if (got == NULL)
		got = "(NULL)";
	prom_printf("\nbad OBP boot args: expect %s, got %s\n"
	    "Usage: boot -F %s [-R] [-S <diskpath>]\n%s\n\n",
	    expect, got, prog, rsvp);
	prom_exit_to_mon();
}


/*
 * bootargs should start with "-F cprboot"
 *
 * may set globals:
 *	specialstate
 *	reusable
 *	halt
 *	verbose
 */
static void
check_bootargs(void)
{
	char **argv, *str, *cp;

	argv = cb_args;

	/* expect "-F" */
	str = "-F";
	if (*argv == NULL || prom_strcmp(*argv, str))
		usage(str, *argv);
	argv++;

	/* expect "cprboot" */
	if (*argv == NULL || prom_strcmp(*argv, prog))
		usage(prog, *argv);

	/*
	 * optional args
	 */
	str = "-[SR]";
	for (argv++; *argv; argv++) {
		cp = *argv;
		if (*cp != '-')
			usage(str, *argv);

		switch (*++cp) {
		case 'R':
		case 'r':
			reusable = 1;
			break;
		case 'S':
		case 's':
			if (*++argv)
				specialstate = *argv;
			else
				usage("statefile-path", *argv);
			break;
		case 'h':
			halt = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(str, *argv);
			break;
		}
	}
}


/*
 * reset prom props and get statefile info
 *
 * sets globals:
 *	cpr_filesystem
 *	cpr_statefile
 */
static int
cb_startup(void)
{
	CB_VENTRY(cb_startup);

	if (!reusable) {
		/*
		 * Restore the original values of the nvram properties modified
		 * during suspend.  Note: if we can't get this info from the
		 * defaults file, the state file may be obsolete or bad, so we
		 * abort.  However, failure to restore one or more properties
		 * is NOT fatal (better to continue the resume).
		 */
		if (cpr_reset_properties() == -1) {
			prom_printf("\n%s: cannot read saved "
			    "nvram info, %s\n", prog, rsvp);
			return (ERR);
		}
	}

	/*
	 * simple copy if using specialstate,
	 * otherwise read in fs and statefile from a config file
	 */
	if (specialstate)
		(void) prom_strcpy(cpr_statefile, specialstate);
	else if (cpr_locate_statefile(cpr_statefile, cpr_filesystem) == -1) {
		prom_printf("\n%s: cannot find cpr statefile, %s\n",
		    prog, rsvp);
		return (ERR);
	}

	return (0);
}


static int
cb_open_sf(void)
{
	CB_VENTRY(cb_open_sf);

	sfile.fd = cpr_statefile_open(cpr_statefile, cpr_filesystem);
	if (sfile.fd == -1) {
		prom_printf("\n%s: can't open %s", prog, cpr_statefile);
		if (specialstate)
			prom_printf(" on %s", cpr_filesystem);
		prom_printf("\n%s\n", rsvp);
		return (ERR);
	}

	/*
	 * Devices get left at block 1
	 */
	if (specialstate)
		(void) prom_seek(sfile.fd, 0LL);

	return (0);
}


static int
cb_close_sf(void)
{
	CB_VENTRY(cb_close_sf);

	/*
	 * close the device so the prom will free up 20+ pages
	 */
	(void) cpr_statefile_close(sfile.fd);
	return (0);
}


/*
 * to restore kernel pages, we have to open a prom device to read-in
 * the statefile contents; a prom "open" request triggers the driver
 * and various packages to allocate 20+ pages; unfortunately, some or
 * all of those pages always clash with kernel pages, and we cant write
 * to them without corrupting the prom.
 *
 * to solve that problem, the only real solution is to close the device
 * to free up those pages; this means we need to open, read-in the entire
 * statefile, and close; and to store the statefile, we need to allocate
 * plenty of space, usually around 2 to 60 MB.
 *
 * the simplest alloc means is prom_alloc(), which will "claim" both
 * virt and phys pages, and creates mappings with a "map" request;
 * "map" also causes the prom to alloc pages, and again these clash
 * with kernel pages...
 *
 * to solve the "map" problem, we just reserve virt and phys pages and
 * manage the translations by creating our own tlb entries instead of
 * relying on the prom.
 *
 * sets globals:
 *	cpr_test_mode
 *	sfile.kpages
 *	sfile.size
 * 	sfile.buf
 * 	sfile.low_ppn
 * 	sfile.high_ppn
 */
static int
cb_read_statefile(void)
{
	physaddr_t phys, dst_phys;
	char *str, *dst_virt;
	int err, cnt, mmask;
	uint_t dtlb_index;
	size_t len, resid;
	ssize_t nread;
	cdd_t cdump;

	str = "cb_read_statefile";
	CB_VPRINTF((ent_fmt, str, entry));

	/*
	 * read-in and check cpr dump header
	 */
	if (cpr_read_cdump(sfile.fd, &cdump, CPR_MACHTYPE_4U))
		return (ERR);
	if (cpr_debug)
		prom_printf("\n");
	cpr_test_mode = cdump.cdd_test_mode;
	sfile.kpages = cdump.cdd_dumppgsize;
	DEBUG4(prom_printf("%s: total kpages %d\n", prog, sfile.kpages));

	/*
	 * alloc virt and phys space with 512K alignment;
	 * alignment must be >= tte size, see TTE512K below
	 */
	sfile.size = PAGE_ROUNDUP(cdump.cdd_filesize);
	err = cb_alloc(sfile.size, MMU_PAGESIZE512K, &sfile.buf, &phys);
	CB_VPRINTF(("%s: buf size 0x%lx, virt 0x%p, phys 0x%lx\n",
	    str, sfile.size, sfile.buf, phys));
	if (err) {
		prom_printf("%s: cant alloc statefile buf, size 0x%lx\n%s\n",
		    str, sfile.size, rsvp);
		return (ERR);
	}

	/*
	 * record low and high phys page numbers for sfile.buf
	 */
	sfile.low_ppn = ADDR_TO_PN(phys);
	sfile.high_ppn = sfile.low_ppn + mmu_btop(sfile.size) - 1;

	/*
	 * setup destination virt and phys addrs for reads;
	 * mapin-mask tells when to create a new tlb entry for the
	 * next set of reads;  NB: the read and tlb method needs
	 * ((big-pagesize % read-size) == 0)
	 */
	dst_virt = sfile.buf;
	dst_phys = phys;
	mmask = (MMU_PAGESIZE512K / PROM_MAX_READ) - 1;

	cnt = 0;
	dtlb_index = cb_dents - 1;
	(void) prom_seek(sfile.fd, 0);
	DEBUG1(prom_printf("%s: reading statefile... ", prog));
	for (resid = cdump.cdd_filesize; resid; resid -= len) {
		/*
		 * do a full spin (4 spin chars)
		 * for every MB read (8 reads = 256K)
		 */
		if ((cnt & 0x7) == 0)
			cb_spin();

		/*
		 * map-in statefile buf pages in 512K blocks;
		 * see MMU_PAGESIZE512K above
		 */
		if ((cnt & mmask) == 0) {
			cb_mapin(dst_virt, ADDR_TO_PN(dst_phys),
			    TTE512K, TTE_HWWR_INT, dtlb_index);
		}

		cnt++;

		len = min(PROM_MAX_READ, resid);
		nread = prom_read(sfile.fd, dst_virt, len, 0, 0);
		if (nread != (ssize_t)len) {
			prom_printf("\n%s: prom read error, "
			    "expect %ld, got %ld\n", str, len, nread);
			return (ERR);
		}
		dst_virt += len;
		dst_phys += len;
	}
	DEBUG1(prom_printf(" \b\n"));

	/*
	 * start the statefile buffer offset at the base of
	 * the statefile buffer and skip past the dump header
	 */
	sfile.buf_offset = 0;
	SF_ADV(sizeof (cdump));

	/*
	 * finish with the first block mapped-in to provide easy virt access
	 * to machdep structs and the bitmap; for 2.8, the combined size of
	 * (cdd_t + cmd_t + csu_md_t + prom_words + cbd_t) is about 1K,
	 * leaving room for a bitmap representing nearly 32GB
	 */
	cb_mapin(sfile.buf, sfile.low_ppn,
	    TTE512K, TTE_HWWR_INT, dtlb_index);

	return (0);
}


/*
 * cprboot first stage worklist
 */
static func_t first_worklist[] = {
	cb_intro,
	cb_startup,
	cb_get_props,
	cb_open_sf,
	cb_read_statefile,
	cb_close_sf,
	cb_check_machdep,
	cb_interpret,
	cb_get_physavail,
	cb_set_bitmap,
	cb_get_newstack,
	(func_t)0
};

/*
 * cprboot second stage worklist
 */
static func_t second_worklist[] = {
	cb_relocate,
	cb_tracking_setup,
	cb_restore_kpages,
	cb_terminator,
	cb_ksetup,
	cb_mpsetup,
	(func_t)0
};


/*
 * simple loop driving major cprboot operations;
 * exits to prom if any error is returned
 */
static void
cb_drive(func_t *worklist)
{
	func_t *cb_func;

	for (cb_func = worklist; *cb_func; cb_func++) {
		if ((**cb_func)())
			prom_exit_to_mon();
	}
}


/*
 * debugging support: drop to prom if halt is set
 */
static void
check_halt(char *str)
{
	if (halt) {
		prom_printf("\n%s halted by -h flag\n==> before %s\n\n",
		    prog, str);
		prom_enter_mon();
	}
}


/*
 * main is called twice from "cb_srt0.s", args are:
 *	cookie	  ieee1275 cif handle
 *	first	  (true): first stage, (false): second stage
 *
 * first stage summary:
 *	various setup
 *	allocate a big statefile buffer
 *	read in the statefile
 *	setup the bitmap
 *	create a new stack
 *
 * return to "cb_srt0.s", switch to new stack
 *
 * second stage summary:
 *	relocate cprboot phys pages
 *	setup tracking for statefile buffer pages
 *	restore kernel pages
 *	various cleanup
 *	install tlb entries for the nucleus and cpr module
 *	restore registers and jump into cpr module
 */
int
main(void *cookie, int first)
{
	if (first) {
		prom_init(prog, cookie);
		get_bootargs();
		check_bootargs();
		check_halt("first_worklist");
		cb_drive(first_worklist);
		return (0);
	} else {
		cb_drive(second_worklist);
		if (verbose || CPR_DBG(1)) {
			prom_printf("%s: resume pc 0x%p\n", prog, mdinfo.func);
			prom_printf("%s: exit_to_kernel(0x%p, 0x%p)\n\n",
			    prog, cookie, &mdinfo);
		}
		check_halt("exit_to_kernel");
		exit_to_kernel(cookie, &mdinfo);
		return (ERR);
	}
}
