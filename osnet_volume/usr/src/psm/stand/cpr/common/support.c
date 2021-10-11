/*
 * Copyright (c) 1987-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)support.c	1.43	99/11/05 SMI"

#include <sys/types.h>
#include <sys/cpr.h>
#include <sys/pte.h>
#include <sys/promimpl.h>
#include <sys/prom_plat.h>

extern int	cpr_ufs_close(int);
extern int	cpr_ufs_open(char *, char *);
extern int	cpr_ufs_read(int, char *, int);
extern int	cpr_read(int, char *, int);
extern void	prom_unmap(caddr_t, u_int);

extern int cpr_debug;

static const struct prop_info cpr_prop_info[] = CPR_PROPINFO_INITIALIZER;


/*
 * Read the config file and pass back the file path, filesystem
 * device path.
 */
int
cpr_read_cprinfo(int fd, char *file_path, char *fs_path)
{
	struct cprconfig cf;

	if (cpr_ufs_read(fd, (char *)&cf, sizeof (struct cprconfig)) !=
	    sizeof (struct cprconfig) || cf.cf_magic != CPR_CONFIG_MAGIC)
		return (-1);

	(void) prom_strcpy(file_path, cf.cf_path);
	(void) prom_strcpy(fs_path, cf.cf_dev_prom);

	return (0);
}


/*
 * Read the location of the state file from the root filesystem.
 * Pass back to the caller the full device path of the filesystem
 * and the filename relative to that fs.
 */
int
cpr_locate_statefile(char *file_path, char *fs_path)
{
	int fd;
	char *boot_path = prom_bootpath();
	int rc;

	if ((fd = cpr_ufs_open(CPR_CONFIG, boot_path)) != -1) {
		rc = cpr_read_cprinfo(fd, file_path, fs_path);
		cpr_ufs_close(fd);
	} else
		rc = -1;

	return (rc);
}


/*
 * Open the "defaults" file in the root fs and read the values of the
 * properties saved during the checkpoint.  Restore the values to nvram.
 *
 * Note: an invalid magic number in the "defaults" file means that the
 * state file is bad or obsolete so our caller should not proceed with
 * the resume.
 */
int
cpr_reset_properties(void)
{
	static char default_path[] = CPR_DEFAULT;
	struct cprinfo ci;
	int fd;
	char *boot_path = prom_bootpath();

	if ((fd = cpr_ufs_open(default_path, boot_path)) == -1) {
		prom_printf("cpr_reset_properties: Unable to open %s on %s.\n",
		    default_path, boot_path);
		return (-1);
	}

	if (cpr_ufs_read(fd, (char *)&ci,
	    sizeof (struct cprinfo)) != sizeof (struct cprinfo)) {
		prom_printf("cpr_reset_properties: Unable to read "
		    "old boot-file value.\n");
		(void) cpr_ufs_close(fd);
		return (-1);
	}
	if (ci.ci_magic != CPR_DEFAULT_MAGIC) {
		prom_printf("cpr_reset_properties: Bad magic number in %s\n",
		    default_path);
		(void) cpr_ufs_close(fd);
		return (-1);
	}

	(void) cpr_ufs_close(fd);

	return (cpr_set_properties(&ci));
}


/*
 * Set the the nvram properties to the values contained in the incoming
 * cprinfo structure.
 */
int
cpr_set_properties(struct cprinfo *ci)
{
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *stk;
	char *name;
	int i;
	int failures = 0;
	const struct prop_info *pi = cpr_prop_info;

	name = "options";
	stk = prom_stack_init(sp, OBP_STACKDEPTH);
	node = prom_findnode_byname(prom_nextnode(0), name, stk);
	prom_stack_fini(stk);

	if ((node == OBP_NONODE) || (node == OBP_BADNODE)) {
		prom_printf("cpr_set_bootinfo: Cannot find \"%s\" node.\n",
		    name, name);
		return (-1);
	}

	for (i = 0;
		i < sizeof (cpr_prop_info) / sizeof (struct prop_info); i++) {
		char *prop_value = (char *)ci + pi[i].pinf_offset;
		int prop_len = prom_strlen(prop_value);

		/*
		 * Note: When doing a prom_setprop you must include the
		 * trailing NULL in the length argument, but when calling
		 * prom_getproplen() the NULL is excluded from the count!
		 */
		if (prom_setprop(node,
		    pi[i].pinf_name, prop_value, prop_len + 1) < 0 ||
		    prom_getproplen(node, pi[i].pinf_name) < prop_len) {
			prom_printf("cpr_set_bootinfo: Can't set "
			    "property %s.\nval=%s\n",
			    pi[i].pinf_name, prop_value);
			failures++;
		}
	}

	return (failures ? -1 : 0);
}


/*
 * Read and verify cpr dump descriptor
 */
int
cpr_read_cdump(int fd, cdd_t *cdp, u_short mach_type)
{
	char *str;
	int nread;

	str = "\ncpr_read_cdump:";
	nread = cpr_read(fd, (caddr_t)cdp, sizeof (*cdp));
	if (nread != sizeof (*cdp)) {
		prom_printf("%s Error reading cpr dump descriptor\n", str);
		return (-1);
	}

	if (cdp->cdd_magic != CPR_DUMP_MAGIC) {
		prom_printf("%s bad dump Magic 0x%x, expected 0x%x\n",
		    str, cdp->cdd_magic, CPR_DUMP_MAGIC);
		return (-1);
	}

	if (cdp->cdd_version != CPR_VERSION) {
		prom_printf("%s bad cpr version %d, expected %d\n",
		    str, cdp->cdd_version, CPR_VERSION);
		return (-1);
	}

	if (cdp->cdd_machine != mach_type) {
		prom_printf("%s bad machine type 0x%x, expected 0x%x\n",
		    str, cdp->cdd_machine, mach_type);
		return (-1);
	}

	if (cdp->cdd_bitmaprec <= 0) {
		prom_printf("%s bad bitmap %d\n", str, cdp->cdd_bitmaprec);
		return (-1);
	}

	if (cdp->cdd_dumppgsize <= 0) {
		prom_printf("%s Bad pg tot %d\n", str, cdp->cdd_dumppgsize);
		return (-1);
	}

	cpr_debug = cdp->cdd_debug;

	return (0);
}


/*
 * update cpr dump terminator
 */
void
cpr_update_terminator(ctrm_t *file_term, caddr_t mapva)
{
	ctrm_t *mem_term;

	/*
	 * Add the offset to reach the terminator in the kernel so that we
	 * can directly change the restored kernel image.
	 */
	mem_term = (ctrm_t *)(mapva + (file_term->va & MMU_PAGEOFFSET));

	mem_term->real_statef_size = file_term->real_statef_size;
	mem_term->tm_shutdown = file_term->tm_shutdown;
	mem_term->tm_cprboot_start.tv_sec = file_term->tm_cprboot_start.tv_sec;
	mem_term->tm_cprboot_end.tv_sec = prom_gettime() / 1000;
}


/*
 * simple bcopy for cprboot
 */
void
bcopy(const void *s, void *d, size_t count)
{
	const char *src = s;
	char *dst = d;

	while (count--)
		*dst++ = *src++;
}
