/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfga_cvt.c	1.3	99/06/04 SMI"

#include "cfga_scsi.h"

typedef struct {
	char *dyncomp;
	char *devlink;
	int l_errno;
	scfga_ret_t ret;
} dyn_t;

typedef struct {
	scfga_recur_t (*devlink_to_dyncomp_p)(dyn_t *dyntp);
	scfga_recur_t (*dyncomp_to_devlink_p)(dyn_t *dyntp);
} dynrules_t;

typedef struct {
	dyn_t *dynp;
	dynrules_t *rule_array;
	int nrules;
} dyncvt_t;

typedef struct {
	const char *hba_phys;
	const char *dyncomp;
	char *path;
	int l_errno;
	scfga_ret_t ret;
} devpath_t;



/* Function prototypes */

static int drv_to_hba_logid(di_node_t node, di_minor_t minor, void *arg);
static scfga_ret_t drv_dyn_to_devpath(const char *hba_phys,
    const char *dyncomp, char **pathpp, int *l_errnop);
static int do_drv_dyn_to_devpath(di_node_t node, void *arg);
static scfga_ret_t devlink_dyn_to_devpath(const char *hba_phys,
    const char *dyncomp, char **pathpp, int *l_errnop);

static scfga_recur_t disk_dyncomp_to_devlink(dyn_t *dyntp);
static scfga_recur_t tape_dyncomp_to_devlink(dyn_t *dyntp);
static scfga_recur_t disktape_dyncomp_to_devlink(const char *dir,
    scfga_recur_t (*devlink_to_dyncomp)(dyn_t *dyntp), dyn_t *dyntp);
static scfga_recur_t do_disktape_dyncomp_to_devlink(const char *lpath,
    void *arg);
static scfga_recur_t def_dyncomp_to_devlink(dyn_t *dyntp);

static scfga_ret_t devlink_to_dyncomp(char *devlink,
    char **dyncompp, int *l_errnop);
static scfga_recur_t disk_devlink_to_dyncomp(dyn_t *dyntp);
static scfga_recur_t tape_devlink_to_dyncomp(dyn_t *dyntp);
static scfga_recur_t def_devlink_to_dyncomp(dyn_t *dyntp);
static scfga_ret_t drv_to_dyncomp(di_node_t node, const char *phys,
    char **dyncompp, int *l_errnop);
static scfga_ret_t get_hba_devlink(const char *hba_phys,
    char **hba_logpp, int *l_errnop);


/* Globals */

/*
 * Rules for converting between a devlink and logical ap_id and vice-versa
 * The default rules must be the last entry.
 */
static dynrules_t dyncvt_rules[] = {
	{disk_devlink_to_dyncomp,	disk_dyncomp_to_devlink},
	{tape_devlink_to_dyncomp,	tape_dyncomp_to_devlink},
	{def_devlink_to_dyncomp,	def_dyncomp_to_devlink}
};

#define	N_DYNRULES	(sizeof (dyncvt_rules)/sizeof (dyncvt_rules[0]))

/*
 * Numbering of disk slices is assumed to be 0 through n - 1
 */
typedef struct {
	char *prefix;
	int nslices;
} slice_t;

static slice_t disk_slices[] = {
	{"s", 16},
	{"p", 5},
};

#define	N_SLICE_TYPES	(sizeof (disk_slices) / sizeof (disk_slices[0]))

static const char *tape_modes[] = {
	"",
	"b", "bn",
	"c", "cb", "cbn", "cn",
	"h", "hb", "hbn", "hn",
	"l", "lb", "lbn", "ln",
	"m", "mb", "mbn", "mn",
	"n",
	"u", "ub", "ubn", "un"
};

#define	N_TAPE_MODES	(sizeof (tape_modes) / sizeof (tape_modes[0]))


/* Various conversions routines */

/*
 * Generates the HBA logical ap_id from physical ap_id.
 */
scfga_ret_t
make_hba_logid(const char *hba_phys, char **hba_logpp, int *l_errnop)
{
	walkarg_t u;
	pathm_t pmt = {NULL};
	scfga_ret_t ret;


	if (*hba_logpp != NULL) {
		return (SCFGA_ERR);
	}

	/* A devlink for the HBA may or may not exist */
	if (get_hba_devlink(hba_phys, hba_logpp, l_errnop) == SCFGA_OK) {
		assert(*hba_logpp != NULL);
		return (SCFGA_OK);
	}

	/*
	 * No devlink based logical ap_id.
	 * Try driver name and instance number.
	 */
	u.minor_args.nodetype = DDI_NT_SCSI_ATTACHMENT_POINT;
	u.minor_args.fcn = drv_to_hba_logid;

	pmt.phys = (char *)hba_phys;
	pmt.ret = SCFGA_APID_NOEXIST;

	errno = 0;
	ret = walk_tree(pmt.phys, &pmt, DINFOMINOR | DINFOPROP, &u,
	    SCFGA_WALK_MINOR, &pmt.l_errno);
	if (ret == SCFGA_OK && (ret = pmt.ret) == SCFGA_OK) {
		assert(pmt.log != NULL);
		*hba_logpp = pmt.log;
		return (SCFGA_OK);
	}

	/* failed to create logical ap_id */
	if (pmt.log != NULL) {
		S_FREE(pmt.log);
	}


	*l_errnop = pmt.l_errno;
	return (ret);
}

static scfga_ret_t
get_hba_devlink(const char *hba_phys, char **hba_logpp, int *l_errnop)
{
	int match_minor;
	size_t len;
	scfga_ret_t ret;

	match_minor = 1;
	ret = physpath_to_devlink(CFGA_DEV_DIR, (char *)hba_phys,
	    hba_logpp, l_errnop, match_minor);
	if (ret != SCFGA_OK) {
		return (ret);
	}

	assert(*hba_logpp != NULL);

	/* Remove the "/dev/cfg/"  prefix */
	len = strlen(CFGA_DEV_DIR SLASH);

	(void) memmove(*hba_logpp, *hba_logpp + len,
	    strlen(*hba_logpp + len) + 1);

	return (SCFGA_OK);
}

/* Make logical name for HBA  based on driver and instance */
static int
drv_to_hba_logid(di_node_t node, di_minor_t minor, void *arg)
{
	int inst;
	char *drv, *mn, *log;
	pathm_t *ptp;
	const size_t loglen = MAXPATHLEN;

	ptp = (pathm_t *)arg;

	errno = 0;

	mn = di_minor_name(minor);
	drv = di_driver_name(node);
	inst = di_instance(node);
	log = calloc(1, loglen);

	if (mn != NULL && drv != NULL && inst != -1 && log != NULL) {
		/* Count does not include terminating NULL */
		if (snprintf(log, loglen, "%s%d:%s", drv, inst, mn) < loglen) {
			ptp->ret = SCFGA_OK;
			ptp->log = log;
			return (DI_WALK_TERMINATE);
		}
	}

	S_FREE(log);
	return (DI_WALK_CONTINUE);
}

/*
 * Given a bus or device ap_id <hba_phys, dyncomp>, returns the physical
 * path in pathpp.
 * Returns: SCFGA_APID_NOEXIST if the path does not exist.
 */

scfga_ret_t
apid_to_path(
	const char *hba_phys,
	const char *dyncomp,
	char **pathpp,
	int *l_errnop)
{
	scfga_ret_t ret;

	if (*pathpp != NULL) {
		return (SCFGA_LIB_ERR);
	}

	/* If a bus, the physical ap_id is the physical path */
	if (dyncomp == NULL) {
		if ((*pathpp = strdup(hba_phys)) == NULL) {
			*l_errnop = errno;
			return (SCFGA_LIB_ERR);
		}
		return (SCFGA_OK);
	}

	/* Dynamic component exists, we have a device */

	/*
	 * If the dynamic component has a '/', it was derived from a devlink
	 * Else it was derived from driver name and instance number.
	 */
	if (strchr(dyncomp, '/') != NULL) {
		ret = devlink_dyn_to_devpath(hba_phys, dyncomp, pathpp,
		    l_errnop);
	} else {
		ret = drv_dyn_to_devpath(hba_phys, dyncomp, pathpp, l_errnop);
	}

	assert(ret != SCFGA_OK || *pathpp != NULL);

	return (ret);
}

static scfga_ret_t
drv_dyn_to_devpath(
	const char *hba_phys,
	const char *dyncomp,
	char **pathpp,
	int *l_errnop)
{
	walkarg_t u;
	devpath_t dpt = {NULL};
	scfga_ret_t ret;

	/* A device MUST have a dynamic component */
	if (dyncomp == NULL || *pathpp != NULL) {
		return (SCFGA_LIB_ERR);
	}

	u.node_args.flags = DI_WALK_CLDFIRST;
	u.node_args.fcn = do_drv_dyn_to_devpath;

	dpt.hba_phys = hba_phys;
	dpt.dyncomp = dyncomp;
	dpt.ret = SCFGA_APID_NOEXIST;

	ret = walk_tree(hba_phys, &dpt, DINFOCPYALL, &u,
	    SCFGA_WALK_NODE, &dpt.l_errno);

	if (ret == SCFGA_OK && (ret = dpt.ret) == SCFGA_OK) {
		assert(dpt.path != NULL);
		*pathpp = dpt.path;
		return (SCFGA_OK);
	}

	if (dpt.path != NULL) {
		S_FREE(dpt.path);
	}


	*l_errnop = dpt.l_errno;
	return (ret);
}

/* Converts a driver and instance number based logid into a physical path */
static int
do_drv_dyn_to_devpath(di_node_t node, void *arg)
{
	int inst, rv, match_minor;
	devpath_t *dptp;
	char *physpath, *drv;
	char *drvinst, *devpath;
	const size_t drvlen = MAXPATHLEN;
	size_t devlen;

	dptp = (devpath_t *)arg;

	assert(dptp->hba_phys != NULL && dptp->dyncomp != NULL);
	assert(dptp->path == NULL);

	/*
	 * Skip stub nodes
	 */
	if (IS_STUB_NODE(node)) {
		return (DI_WALK_CONTINUE);
	}

	errno = 0;

	drv = di_driver_name(node);
	inst = di_instance(node);
	physpath = di_devfs_path(node);
	if (drv == NULL || inst == -1 || physpath == NULL) {
		rv = DI_WALK_CONTINUE;
		goto out;
	}

	devlen = strlen(DEVICES_DIR) + strlen(physpath) + 1;

	devpath = calloc(1, devlen);
	drvinst = calloc(1, drvlen);
	if (devpath == NULL || drvinst == NULL) {
		dptp->l_errno = errno;
		dptp->ret = SCFGA_LIB_ERR;
		rv = DI_WALK_TERMINATE;
		goto out;
	}

	(void) snprintf(drvinst, drvlen, "%s%d", drv, inst);

	/* Create the physical path */
	(void) snprintf(devpath, devlen, "%s%s", DEVICES_DIR, physpath);

	/* Skip node if it is the HBA */
	match_minor = 0;
	if (!dev_cmp(dptp->hba_phys, devpath, match_minor)) {
		rv = DI_WALK_CONTINUE;
		goto out;
	}

	/* Compare the base and dynamic components */
	if (!hba_dev_cmp(dptp->hba_phys, devpath) &&
	    strcmp(dptp->dyncomp, drvinst) == 0) {
		dptp->ret = SCFGA_OK;
		dptp->path = devpath;
		rv = DI_WALK_TERMINATE;
	} else {
		rv =  DI_WALK_CONTINUE;
	}

	/*FALLTHRU*/
out:
	S_FREE(drvinst);
	if (physpath != NULL) di_devfs_path_free(physpath);
	if (dptp->ret != SCFGA_OK) S_FREE(devpath);
	return (rv);
}

/* Converts a devlink based dynamic component to a path */
static scfga_ret_t
devlink_dyn_to_devpath(
	const char *hba_phys,
	const char *dyncomp,
	char **pathpp,
	int *l_errnop)
{
	const size_t pathlen = PATH_MAX;
	dyn_t dynt = {NULL};
	int i;
	scfga_ret_t ret;

	if (*pathpp != NULL) {
		return (SCFGA_LIB_ERR);
	}

	/* Convert the dynamic component to the corresponding devlink */
	dynt.dyncomp = (char *)dyncomp;
	dynt.ret = SCFGA_APID_NOEXIST;

	for (i = 0; i < N_DYNRULES; i++) {
		if (dyncvt_rules[i].dyncomp_to_devlink_p(&dynt)
		    != SCFGA_CONTINUE) {
			break;
		}
	}

	if (i >= N_DYNRULES) {
		dynt.ret = SCFGA_APID_NOEXIST;
	}

	if (dynt.ret != SCFGA_OK) {
		/* No symlink or error */
		return (dynt.ret);
	}

	assert(dynt.devlink != NULL);


	/* Follow devlink to get the physical path */
	errno = 0;
	if ((*pathpp = calloc(1, pathlen)) == NULL ||
	    realpath(dynt.devlink, *pathpp) == NULL) {
		*l_errnop = errno;
		ret = SCFGA_LIB_ERR;
		goto out;
	}

	/* Compare base components as well */
	if (!hba_dev_cmp(hba_phys, *pathpp)) {
		ret = SCFGA_OK;
	} else {
		/* Mismatched base and dynamic component */
		*l_errnop = 0;
		ret = SCFGA_APID_NOEXIST;
	}

	/*FALLTHRU*/
out:
	S_FREE(dynt.devlink);
	if (ret != SCFGA_OK) S_FREE(*pathpp);
	return (ret);
}

scfga_ret_t
make_dyncomp(
	di_node_t node,
	const char *physpath,
	char **dyncompp,
	int *l_errnop)
{
	int match_minor;
	char *devlink = NULL;
	scfga_ret_t ret;

	if (*dyncompp != NULL) {
		return (SCFGA_LIB_ERR);
	}

	/* Get the corresponding devlink from the physical path */
	match_minor = 0;
	ret = physpath_to_devlink(DEV_DIR, (char *)physpath, &devlink,
	    l_errnop, match_minor);
	if (ret == SCFGA_OK) {
		assert(devlink != NULL);

		/* Create dynamic component. */
		ret = devlink_to_dyncomp(devlink, dyncompp, l_errnop);
		S_FREE(devlink);
		if (ret == SCFGA_OK) {
			assert(*dyncompp != NULL);
			return (SCFGA_OK);
		}

		/*
		 * Failed to get devlink based dynamic component.
		 * Try driver and instance
		 */
	}

	ret = drv_to_dyncomp(node, physpath, dyncompp, l_errnop);
	assert(ret != SCFGA_OK || *dyncompp != NULL);

	return (ret);
}

/*ARGSUSED*/
static scfga_ret_t
drv_to_dyncomp(di_node_t node, const char *phys, char **dyncompp, int *l_errnop)
{
	char *drv;
	int inst;
	const int dynlen = MAXPATHLEN;
	scfga_ret_t ret;

	*l_errnop = 0;

	if ((*dyncompp = calloc(1, dynlen)) == NULL) {
		*l_errnop = errno;
		return (SCFGA_LIB_ERR);
	}

	drv = di_driver_name(node);
	inst = di_instance(node);
	if (drv != NULL && inst != -1) {
		if (snprintf(*dyncompp, dynlen, "%s%d", drv, inst) < dynlen) {
			return (SCFGA_OK);
		} else {
			ret = SCFGA_LIB_ERR;
		}
	} else {
		ret = SCFGA_APID_NOEXIST;
	}

	S_FREE(*dyncompp);
	return (ret);
}

/* Get a dynamic component from a physical path if possible */
static scfga_ret_t
devlink_to_dyncomp(char *devlink, char **dyncompp, int *l_errnop)
{
	int i;
	dyn_t dynt = {NULL};

	*l_errnop = 0;

	if (*dyncompp != NULL) {
		return (SCFGA_LIB_ERR);
	}

	/* Convert devlink to dynamic component */
	dynt.devlink = devlink;
	dynt.ret = SCFGA_APID_NOEXIST;

	for (i = 0; i < N_DYNRULES; i++) {
		if (dyncvt_rules[i].devlink_to_dyncomp_p(&dynt)
		    != SCFGA_CONTINUE) {
			break;
		}
	}

	if (i >= N_DYNRULES) {
		dynt.ret = SCFGA_APID_NOEXIST;
	}

	if (dynt.ret == SCFGA_OK) {
		assert(dynt.dyncomp != NULL);
		*dyncompp = dynt.dyncomp;
	}

	return (dynt.ret);
}

/* For disks remove partition information, (s or p) */
static scfga_recur_t
disk_devlink_to_dyncomp(dyn_t *dyntp)
{
	char *cp = NULL, *cp1 = NULL;

	assert(dyntp->devlink != NULL);

	dyntp->l_errno = 0;

	if (dyntp->dyncomp != NULL) {
		goto lib_err;
	}

	/* Check if a disk devlink */
	if (strncmp(dyntp->devlink, DEV_DSK SLASH, strlen(DEV_DSK SLASH)) &&
	    strncmp(dyntp->devlink, DEV_RDSK SLASH, strlen(DEV_RDSK SLASH))) {
		return (SCFGA_CONTINUE);
	}

	cp = dyntp->devlink + strlen(DEV_DIR SLASH);

	if ((dyntp->dyncomp = strdup(cp)) == NULL) {
		dyntp->l_errno = errno;
		goto lib_err;
	}

	/* Get the leaf component from dsk/cXtYdZsN */
	cp1 = strrchr(dyntp->dyncomp, '/');

	/* Blank out partition information */
	dyntp->ret = SCFGA_OK;
	if ((cp = strchr(cp1 + 1, 's')) != NULL) {
		*cp = '\0';
	} else if ((cp = strchr(cp1 + 1, 'p')) != NULL) {
		*cp = '\0';
	} else {
		S_FREE(dyntp->dyncomp);
		dyntp->ret = SCFGA_ERR;
	}

	return (SCFGA_TERMINATE);

lib_err:
	dyntp->ret = SCFGA_LIB_ERR;
	return (SCFGA_TERMINATE);
}


static scfga_recur_t
disk_dyncomp_to_devlink(dyn_t *dyntp)
{
	char *dir = NULL;
	char buf[MAXPATHLEN], *cp = NULL;
	int i, j;
	size_t len;
	struct stat sbuf;

	assert(dyntp->dyncomp != NULL);

	dyntp->l_errno = 0;

	if (dyntp->devlink != NULL) {
		dyntp->ret = SCFGA_LIB_ERR;
		return (SCFGA_TERMINATE);
	}

	/* A disk link can only be from DEV_DSK and DEV_RDSK */
	if (strncmp(dyntp->dyncomp, DSK_DIR SLASH,
	    strlen(DSK_DIR SLASH)) == 0) {
		dir = DEV_DSK;
	} else if (strncmp(dyntp->dyncomp, RDSK_DIR SLASH,
	    strlen(RDSK_DIR SLASH)) == 0) {
		dir = DEV_RDSK;
	} else {
		return (SCFGA_CONTINUE);	/* not a disk link */
	}

	(void) snprintf(buf, sizeof (buf), "%s%s", DEV_DIR SLASH,
	    dyntp->dyncomp);

	len = strlen(buf);
	cp = buf + len;
	len = sizeof (buf) - len;

	for (i = 0; i < N_SLICE_TYPES; i++) {
		for (j = 0; j < disk_slices[i].nslices; j++) {
			if (snprintf(cp, len, "%s%d", disk_slices[i].prefix, j)
			    >= len) {
				continue;
			}


			if (lstat(buf, &sbuf) != -1 && S_ISLNK(sbuf.st_mode)) {
				if ((dyntp->devlink = strdup(buf)) == NULL) {
					dyntp->l_errno = errno;
					dyntp->ret = SCFGA_LIB_ERR;
					return (SCFGA_TERMINATE);
				}
				dyntp->ret = SCFGA_OK;
				return (SCFGA_TERMINATE);
			}
		}
	}

	/*
	 * None of the slices from the list seem to be there.
	 * Use the reverse rule to reverse the derivation process
	 */
	return (disktape_dyncomp_to_devlink(dir, disk_devlink_to_dyncomp,
	    dyntp));
}

/* For tapes, remove mode(minor) information from link */
static scfga_recur_t
tape_devlink_to_dyncomp(dyn_t *dyntp)
{
	char *cp = NULL;

	assert(dyntp->devlink != NULL);

	dyntp->l_errno = 0;

	if (dyntp->dyncomp != NULL) {
		goto lib_err;
	}

	if (strncmp(dyntp->devlink, DEV_RMT SLASH, strlen(DEV_RMT SLASH))) {
		return (SCFGA_CONTINUE);	/* not a tape */
	}

	cp = dyntp->devlink + strlen(DEV_DIR SLASH);
	if ((dyntp->dyncomp = strdup(cp)) == NULL) {
		dyntp->l_errno = errno;
		goto lib_err;
	}

	/* Get the leaf component from rmt/xyz */
	cp = strrchr(dyntp->dyncomp, '/');

	/* Remove the mode part */
	while (isdigit(*(++cp)));
	*cp = '\0';


	dyntp->ret = SCFGA_OK;
	return (SCFGA_TERMINATE);

lib_err:
	dyntp->ret = SCFGA_LIB_ERR;
	return (SCFGA_TERMINATE);
}

static scfga_recur_t
tape_dyncomp_to_devlink(dyn_t *dyntp)
{
	char buf[MAXPATHLEN], *cp = NULL;
	int i;
	size_t len = 0;
	struct stat sbuf;

	assert(dyntp->dyncomp != NULL);

	dyntp->l_errno = 0;

	if (dyntp->devlink != NULL) {
		goto lib_err;
	}

	if (strncmp(dyntp->dyncomp, RMT_DIR SLASH, strlen(RMT_DIR SLASH))) {
		return (SCFGA_CONTINUE);	/* not a tape */
	}

	/* A tape device */
	(void) snprintf(buf, sizeof (buf), "%s%s", DEV_DIR SLASH,
	    dyntp->dyncomp);

	len = strlen(buf);
	cp = buf + len;
	len = sizeof (buf) - len;

	for (i = 0; i < N_TAPE_MODES; i++) {
		(void) snprintf(cp, len, "%s", tape_modes[i]);

		if (lstat(buf, &sbuf) != -1 && S_ISLNK(sbuf.st_mode)) {
			if ((dyntp->devlink = strdup(buf)) == NULL) {
				dyntp->l_errno = errno;
				goto lib_err;
			}
			dyntp->ret = SCFGA_OK;
			return (SCFGA_TERMINATE);
		}
	}

	/*
	 * None of the modes from the list seem to be there.
	 * Use the reverse rule to reverse the derivation process
	 */
	return (disktape_dyncomp_to_devlink(DEV_RMT, tape_devlink_to_dyncomp,
	    dyntp));

lib_err:
	dyntp->ret = SCFGA_LIB_ERR;
	return (SCFGA_TERMINATE);

}

static scfga_recur_t
disktape_dyncomp_to_devlink(
	const char *dir,
	scfga_recur_t (*reverse_rule_p)(dyn_t *dyntp),
	dyn_t *dyntp)
{
	dyncvt_t dcvt = {NULL};
	dynrules_t revrule_a[1] = {NULL};
	scfga_ret_t ret;

	assert(dyntp->dyncomp != NULL);

	dyntp->l_errno = 0;

	if (dyntp->devlink != NULL) {
		dyntp->ret = SCFGA_LIB_ERR;
		return (SCFGA_TERMINATE);
	}

	dcvt.dynp = dyntp;
	dcvt.dynp->ret = SCFGA_APID_NOEXIST;

	revrule_a[0].devlink_to_dyncomp_p = reverse_rule_p;
	dcvt.rule_array = revrule_a;
	dcvt.nrules = 1;

	ret = recurse_dev(dir, &dcvt, do_disktape_dyncomp_to_devlink);

	if (ret == SCFGA_OK && (ret = dcvt.dynp->ret) == SCFGA_OK) {
		assert(dcvt.dynp->devlink != NULL);
		dyntp->ret = SCFGA_OK;
		return (SCFGA_TERMINATE);
	}

	if (dcvt.dynp->devlink != NULL) {
		S_FREE(dcvt.dynp->devlink);
	}

	dyntp->ret = ret;
	return (SCFGA_TERMINATE);
}

/*ARGSUSED*/
static scfga_recur_t
do_disktape_dyncomp_to_devlink(const char *lpath, void *arg)
{
	int i;
	dyncvt_t *dcvtp;
	dyn_t *dyntp, l_dynt = {NULL};
	scfga_recur_t rc;

	dcvtp = (dyncvt_t *)arg;
	dyntp = dcvtp->dynp;

	assert(dyntp->dyncomp != NULL);

	dyntp->l_errno = 0;

	if (dyntp->devlink != NULL) {
		dyntp->ret = SCFGA_LIB_ERR;
		return (SCFGA_TERMINATE);
	}

	/*
	 * Reverse the derivation of dynamic component
	 * Get the dynamic component corresponding to the devlink path.
	 */
	l_dynt.devlink = (char *)lpath;
	l_dynt.ret = SCFGA_APID_NOEXIST;

	for (i = 0; i < dcvtp->nrules; i++) {
		if (dcvtp->rule_array[i].devlink_to_dyncomp_p(&l_dynt)
		    != SCFGA_CONTINUE) {
			break;
		}
	}

	if (i >= dcvtp->nrules) {
		l_dynt.ret = SCFGA_APID_NOEXIST;
	}

	assert(l_dynt.ret != SCFGA_OK || l_dynt.dyncomp != NULL);

	if (l_dynt.ret == SCFGA_OK &&
	    strcmp(dyntp->dyncomp, l_dynt.dyncomp) == 0) {
		/* We found our link */
		if ((dyntp->devlink = strdup(lpath)) == NULL) {
			dyntp->l_errno = errno;
			dyntp->ret = SCFGA_LIB_ERR;
		} else {
			dyntp->ret = SCFGA_OK;
		}

		rc = SCFGA_TERMINATE;
	} else {
		/* Error or no match. Ignore errors */
		dyntp->ret = SCFGA_APID_NOEXIST;
		dyntp->l_errno = 0;

		rc = SCFGA_CONTINUE;
	}

	S_FREE(l_dynt.dyncomp);
	return (rc);
}


/*
 * Default rules
 */
static scfga_recur_t
def_devlink_to_dyncomp(dyn_t *dyntp)
{
	size_t len = 0;
	char *cp = NULL;

	assert(dyntp->devlink != NULL);

	dyntp->l_errno = 0;

	if (dyntp->dyncomp != NULL) {
		dyntp->ret = SCFGA_LIB_ERR;
		return (SCFGA_TERMINATE);
	}

	/* Is it a link in DEV_DIR directory ? */
	len = strlen(DEV_DIR SLASH);
	if (strncmp(dyntp->devlink, DEV_DIR SLASH, len)) {
		return (SCFGA_CONTINUE);
	}

	/* Check if this is a top level devlink */
	if (strchr(dyntp->devlink + len, '/') != NULL) {
		/* not top level - Remove DEV_DIR SLASH prefix */
		cp = dyntp->devlink + len;
	} else {
		/* top level, leave DEV_DIR SLASH part in */
		cp = dyntp->devlink;
	}

	if ((dyntp->dyncomp = strdup(cp)) == NULL) {
		dyntp->l_errno = errno;
		dyntp->ret = SCFGA_LIB_ERR;
	} else {
		dyntp->ret = SCFGA_OK;
	}

	return (SCFGA_TERMINATE);

}

static scfga_recur_t
def_dyncomp_to_devlink(dyn_t *dyntp)
{
	struct stat sbuf;
	int top;
	size_t prelen, linklen;

	assert(dyntp->dyncomp != NULL);

	dyntp->l_errno = 0;

	if (dyntp->devlink != NULL) {
		goto lib_err;
	}

	prelen = strlen(DEV_DIR SLASH);
	linklen = strlen(dyntp->dyncomp) + 1;

	/*
	 * Check if the dynamic component was derived from a top level entry
	 * in "/dev"
	 */
	if (strncmp(dyntp->dyncomp, DEV_DIR SLASH, prelen) == 0) {
		top = 1;
	} else if (*dyntp->dyncomp != '/' && linklen > 1 &&
	    strchr(dyntp->dyncomp + 1, '/') != NULL) {
		top = 0;
		linklen += prelen;  /* The "/dev/" needs to be prepended */
	} else {
		/* Not a dynamic component we handle */
		return (SCFGA_CONTINUE);
	}

	if ((dyntp->devlink = calloc(1, linklen)) == NULL) {
		dyntp->l_errno = errno;
		goto lib_err;
	}

	*dyntp->devlink = '\0';
	if (!top) {
		(void) strcpy(dyntp->devlink, DEV_DIR SLASH);
	}
	(void) strcat(dyntp->devlink, dyntp->dyncomp);

	if (lstat(dyntp->devlink, &sbuf) != -1 && S_ISLNK(sbuf.st_mode)) {
		dyntp->ret = SCFGA_OK;
		return (SCFGA_TERMINATE);
	}


	S_FREE(dyntp->devlink);
	return (SCFGA_CONTINUE);

lib_err:
	dyntp->ret = SCFGA_LIB_ERR;
	return (SCFGA_TERMINATE);
}
