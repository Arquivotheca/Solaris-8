/*
 * Copyright (c) 1990 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)util.c	1.12	99/11/05 SMI"

#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/fcntl.h>
#include <sys/cpr.h>
#include <sys/pte.h>
#include <sys/ddi.h>

extern struct bootops *bootops;
extern void prom_unmap(caddr_t, u_int);
extern void cpr_update_terminator(ctrm_t *, caddr_t);

extern int cpr_ufs_close(int);

static char current_fs[OBP_MAXPATHLEN];
static char read_buf[CPR_MAX_BLOCK];
static char compress_buf[CPR_MAX_BLOCK];
static char *fptr;
static size_t contains;		/* bytes in the read buffer */

/*
 * Ask prom to open a disk file given the device path representing
 * the target drive/partition and the fs-relative path of the
 * file.
 */
int
cpr_ufs_open(char *path, char *fs)
{
	/*
	 * sun4m prom (OBP) and ufsboot allow files on only one
	 * filesystem to be open simultaneously.  The root fs is
	 * mounted during ufsboot initialization.  We use current_fs
	 * to cache the name of the currently mouted fs to avoid
	 * unecessary mounts when not changing fs.
	 *
	 * XXX To avoid the use of this obnoxious "first time" test,
	 * we could make current_fs a file static and write a function
	 * to initialize it, calling this platform-specific function
	 * from common init code.  Possibly not worth it.
	 */
	if (current_fs[0] == '\0')
		(void) prom_strcpy(current_fs, prom_bootpath());

	if (prom_strcmp(fs, current_fs)) {
		if (BOP_UNMOUNTROOT(bootops) == -1)
			return (-1);
		if (BOP_MOUNTROOT(bootops, fs) == -1)
			return (-1);
		(void) prom_strcpy(current_fs, fs);
	}

	return (BOP_OPEN(bootops, path, O_RDONLY));
}

/*
 * Ask prom to open a disk file given either a physical device to be opened
 * with prom_open, or the device path representing
 * the target drive/partition and the fs-relative path of the
 * file.
 */
int
cpr_statefile_open(char *path, char *fs)
{
	extern char *specialstate;

	if (specialstate) {	/* char statefile open */
		int handle;

		handle = prom_open(path);
		/* prom_open for IEEE 1275 returns 0 on failure; we return -1 */
		return (handle ? handle : -1);
	}

	return (cpr_ufs_open(path, fs));
}

int
cpr_ufs_read(int fd, char *buf, int count)
{
	return (BOP_READ(bootops, fd, buf, count));
}

int
cpr_statefile_read(int fd, char *buf, int count)
{
	extern char *specialstate;

	if (specialstate)
		return (prom_read(fd, buf, count, 0, 0));
	else
		return (cpr_ufs_read(fd, buf, count));
}

int
cpr_ufs_close(int fd)
{
	extern char *specialstate;

	return (BOP_CLOSE(bootops, fd));
}

/*
 * A layer between cprboot and cpr_statefile_read
 * using 32k sized buffered reads.
 */
int
cpr_read(int fd, caddr_t buf, size_t size)
{
	char *str, *action;
	size_t resid, ncp;
	int nread;

	str = "cpr_read:";
	DEBUG4(errp("\n%s contains %lu, dest 0x%p, resid %lu\n",
	    str, contains, buf, size));

	for (resid = size; resid; ) {
		if (contains) {
			ncp = min(contains, resid);
			if (buf) {
				bcopy(fptr, buf, ncp);
				action = "copied";
				buf += ncp;
			} else
				action = "skipped";

			fptr += ncp;
			resid -= ncp;
			contains -= ncp;
			DEBUG4(errp("%s contains %lu, %s %d, resid %lu\n",
			    str, contains, action, ncp, resid));
			continue;
		}

		nread = cpr_statefile_read(fd, read_buf, CPR_MAX_BLOCK);
		if (nread < 0)
			return (-1);
		else if (nread == 0) {
			prom_printf("%s premature EOF, %lu bytes short\n",
			    str, resid);
			return (-1);
		}

		contains = nread;
		fptr = read_buf;
	}

	return ((int)(size - resid));
}

/*
 * Read the machdep descriptor and return the length of the machdep
 * section which follows.
 */
ssize_t
cpr_get_machdep_len(int fd)
{
	cmd_t cmach;

	if (cpr_read(fd, (caddr_t)&cmach, sizeof (cmd_t)) !=
	    sizeof (cmd_t)) {
		errp("cpr_get_machdep_len: Err reading cpr machdep "
		    "descriptor");
		return ((ssize_t)-1);
	}

	if (cmach.md_magic != CPR_MACHDEP_MAGIC) {
		errp("cpr_get_machdep_len: Bad machdep magic %x\n",
			cmach.md_magic);
		return ((ssize_t)-1);
	}

	return ((ssize_t)cmach.md_size);
}

/*
 * Read opaque platform specific info.
 */
int
cpr_read_machdep(int fd, caddr_t bufp, size_t len)
{
	if (cpr_read(fd, bufp, len) != len) {
		errp("cpr_read_machdep: failed to read machdep info\n");
		return (-1);
	}

	return (0);
}

/*
 * Read in kernel pages
 * Return pages read otherwise -1 for failure
 */
int
cpr_read_phys_page(int fd, u_int free_va, int *is_compressed)
{
	u_int sum, len = 0;
	cpd_t cpgdesc;		/* cpr page descriptor */
	char *datap;
	physaddr_t cpr_pa;
	char *str = "cpr_read_phys_page:";

	/*
	 * First read page descriptor
	 */
	if ((cpr_read(fd, (caddr_t)&cpgdesc, sizeof (cpd_t))) !=
	    sizeof (cpd_t)) {
		errp("%s Error reading page desc\n", str);
		return (-1);
	}

	if (cpgdesc.cpd_magic != CPR_PAGE_MAGIC) {
		errp("BAD PAGE_MAGIC (0x%x) should be (0x%x), cpg=0x%p\n",
		    cpgdesc.cpd_magic, CPR_PAGE_MAGIC, &cpgdesc);
		return (-1);
	}

	/*
	 * Get physical address, should be page aligned
	 */
	cpr_pa = PN_TO_ADDR(cpgdesc.cpd_pfn);

	DEBUG4(errp("about to read: pa=0x%x pfn=0x%x len=%d\n",
	    cpr_pa, cpgdesc.cpd_pfn, cpgdesc.cpd_length));

	/*
	 * XXX: Map the physical page to the virtual address,
	 * and read into the virtual.
	 * XXX: There is potential problem in the OBP, we just
	 * want to predefine an virtual address for it, so that
	 * OBP won't mess up any of its own memory allocation.
	 * REMEMBER to change it later !
	 */
	prom_unmap((caddr_t)free_va, mmu_ptob(cpgdesc.cpd_pages));
	prom_map_plat((caddr_t)free_va, cpr_pa, mmu_ptob(cpgdesc.cpd_pages));

	/*
	 * For compressed data, decompress from compress_buf;
	 * copy non-compressed data directly to the mapped vitrual.
	 */
	if (cpgdesc.cpd_flag & CPD_COMPRESS)
		datap = compress_buf;
	else
		datap = (char *)free_va;

	if (cpr_read(fd, datap, cpgdesc.cpd_length) != cpgdesc.cpd_length) {
		errp("%s Err reading page: len %d\n", str, cpgdesc.cpd_length);
		return (-1);
	}

	/*
	 * Decompress data into physical page directly
	 */
	if (cpgdesc.cpd_flag & CPD_COMPRESS) {
		if (cpgdesc.cpd_flag & CPD_CSUM) {
			sum = checksum32(datap, cpgdesc.cpd_length);
			if (sum != cpgdesc.cpd_csum) {
				errp("%s bad checksum on compressed data\n",
				    str);
				DEBUG4(errp("Read csum 0x%x, expected 0x%x\n",
				    sum, cpgdesc.cpd_csum));
				return (-1);
			}
		}
		len = decompress(datap, (void *)free_va, cpgdesc.cpd_length,
		    mmu_ptob(cpgdesc.cpd_pages));
		if (len != mmu_ptob(cpgdesc.cpd_pages)) {
			errp("%s bad decompressed len %d compressed len %d\n",
			    str, len, cpgdesc.cpd_length);
			return (-1);
		}
		*is_compressed = 1;
	} else {
		*is_compressed = 0;
	}

	if (cpgdesc.cpd_flag & CPD_USUM) {
		sum = checksum32((void *)free_va, mmu_ptob(cpgdesc.cpd_pages));
		if (sum != cpgdesc.cpd_usum) {
			errp("%s bad checksum on uncompressed data\n", str);
			DEBUG4(errp("Read usum 0x%x, expected 0x%x\n",
			    sum, cpgdesc.cpd_usum));
			return (-1);
		}
	}

	DEBUG4(errp("Read: pa=0x%x pfn=0x%x\n", cpr_pa, cpgdesc.cpd_pfn));

	return (cpgdesc.cpd_pages);
}

/*
 * cprboot:cpr_skip_bitmaps calls seek on the underlying file.
 * We have to forget that we've ever read the file so we'll do
 * another read to get in sync.  The alternative was to do 32K reads
 * for each bitmap descriptor
 */
void
cpr_reset_read(void)
{
	contains = 0;
	fptr = read_buf;
}


/*
 * Read/verify/update cpr dump terminator
 */
int
cpr_read_terminator(int fd, ctrm_t *ctp, caddr_t mapva)
{
	ctrm_t ct_saved, *cp;	/* terminator from the statefile */
	char *str;

	str = "cpr_read_terminator";
	if (cpr_read(fd, (caddr_t)&ct_saved, sizeof (ct_saved)) !=
	    sizeof (ct_saved)) {
		errp("%s: err reading cpr terminator\n", str);
		return (-1);
	}

	if (ct_saved.magic != CPR_TERM_MAGIC) {
		errp("%s: bad terminator magic %x (vs. %x)\n",
		    str, ct_saved.magic, CPR_TERM_MAGIC);
		return (-1);
	}

	prom_unmap(mapva, MMU_PAGESIZE);
	if (prom_map(mapva, 0, PN_TO_ADDR(ct_saved.pfn), MMU_PAGESIZE) == 0) {
		errp("%s: prom_map error, virt 0x%p, phys 0x%x\n",
		    str, mapva, PN_TO_ADDR(ct_saved.pfn));
		return (-1);
	}
	cpr_update_terminator(&ct_saved, mapva);
	prom_unmap(mapva, MMU_PAGESIZE);
	return (0);
}
