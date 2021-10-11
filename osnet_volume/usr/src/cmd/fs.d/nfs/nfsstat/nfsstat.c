/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1984,1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nfsstat.c	1.35	99/07/29 SMI"	/* SVr4.0 1.9	*/

/*
 * nfsstat: Network File System statistics
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <kstat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/tiuser.h>
#include <sys/statvfs.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/sysmacros.h>
#include <sys/mkdev.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/nfs_sec.h>
#include <inttypes.h>

static kstat_ctl_t *kc = NULL;		/* libkstat cookie */
static kstat_t *rpc_clts_client_kstat, *rpc_clts_server_kstat;
static kstat_t *rpc_cots_client_kstat, *rpc_cots_server_kstat;
static kstat_t *nfs_client_kstat, *nfs_server_kstat;
static kstat_t *rfsproccnt_v2_kstat, *rfsproccnt_v3_kstat;
static kstat_t *rfsreqcnt_v2_kstat, *rfsreqcnt_v3_kstat;
static kstat_t *aclproccnt_v2_kstat, *aclproccnt_v3_kstat;
static kstat_t *aclreqcnt_v2_kstat, *aclreqcnt_v3_kstat;

static void getstats(void);
static void putstats(void);
static void setup(int);
static void cr_print(int);
static void sr_print(int);
static void cn_print(int);
static void sn_print(int);
static void ca_print(int);
static void sa_print(int);
static void req_print(kstat_t *, uint64_t);
static void stat_print(kstat_t *);

static void fail(int, char *, ...);
static kid_t safe_kstat_read(kstat_ctl_t *, kstat_t *, void *);
static kid_t safe_kstat_write(kstat_ctl_t *, kstat_t *, void *);

static void usage(void);
static void mi_print(void);
static int ignore(char *);

#define	MAX_COLUMNS	80

static int field_width = 0;
static int ncolumns;

static void req_width(kstat_t *);
static void stat_width(kstat_t *);

main(int argc, char *argv[])
{
	int c;
	int cflag = 0;		/* client stats */
	int sflag = 0;		/* server stats */
	int nflag = 0;		/* nfs stats */
	int rflag = 0;		/* rpc stats */
	int zflag = 0;		/* zero stats after printing */
	int mflag = 0;		/* mount table stats */
	int aflag = 0;		/* print acl statistics */

	while ((c = getopt(argc, argv, "cnrsmza")) != EOF) {
		switch (c) {
		case 'c':
			cflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'r':
			rflag++;
			break;
		case 's':
			sflag++;
			break;
		case 'm':
			mflag++;
			break;
		case 'z':
			if (geteuid())
				fail(0, "Must be root for z flag");
			zflag++;
			break;
		case 'a':
			aflag++;
			break;
		case '?':
		default:
			usage();
		}
	}

	if (argc - optind >= 1)
		usage();

	setup(zflag);

	if (mflag) {
		mi_print();
		exit(0);
	}

	getstats();

	ncolumns = (MAX_COLUMNS - 1) / field_width;

	if (sflag &&
	    (rpc_clts_server_kstat == NULL || nfs_server_kstat == NULL)) {
		fprintf(stderr,
			"nfsstat: kernel is not configured with "
			"the server nfs and rpc code.\n");
	}
	if (sflag || (!sflag && !cflag)) {
		if (rflag || (!rflag && !nflag && !aflag))
			sr_print(zflag);
		if (nflag || (!rflag && !nflag && !aflag))
			sn_print(zflag);
		if (aflag || (!rflag && !nflag && !aflag))
			sa_print(zflag);
	}
	if (cflag &&
	    (rpc_clts_client_kstat == NULL || nfs_client_kstat == NULL)) {
		fprintf(stderr,
			"nfsstat: kernel is not configured with "
			"the client nfs and rpc code.\n");
	}
	if (cflag || (!sflag && !cflag)) {
		if (rflag || (!rflag && !nflag && !aflag))
			cr_print(zflag);
		if (nflag || (!rflag && !nflag && !aflag))
			cn_print(zflag);
		if (aflag || (!rflag && !nflag && !aflag))
			ca_print(zflag);
	}

	if (zflag)
		putstats();

	return (0);
	/* NOTREACHED */
}

static void
getstats(void)
{

	if (rpc_clts_client_kstat != NULL) {
		safe_kstat_read(kc, rpc_clts_client_kstat, NULL);
		stat_width(rpc_clts_client_kstat);
	}
	if (rpc_cots_client_kstat != NULL) {
		safe_kstat_read(kc, rpc_cots_client_kstat, NULL);
		stat_width(rpc_cots_client_kstat);
	}
	if (rpc_clts_server_kstat != NULL) {
		safe_kstat_read(kc, rpc_clts_server_kstat, NULL);
		stat_width(rpc_clts_server_kstat);
	}
	if (rpc_cots_server_kstat != NULL) {
		safe_kstat_read(kc, rpc_cots_server_kstat, NULL);
		stat_width(rpc_cots_server_kstat);
	}
	if (nfs_client_kstat != NULL) {
		safe_kstat_read(kc, nfs_client_kstat, NULL);
		stat_width(nfs_client_kstat);
	}
	if (nfs_server_kstat != NULL) {
		safe_kstat_read(kc, nfs_server_kstat, NULL);
		stat_width(nfs_server_kstat);
	}
	if (rfsproccnt_v2_kstat != NULL) {
		safe_kstat_read(kc, rfsproccnt_v2_kstat, NULL);
		req_width(rfsproccnt_v2_kstat);
	}
	if (rfsproccnt_v3_kstat != NULL) {
		safe_kstat_read(kc, rfsproccnt_v3_kstat, NULL);
		req_width(rfsproccnt_v3_kstat);
	}
	if (rfsreqcnt_v2_kstat != NULL) {
		safe_kstat_read(kc, rfsreqcnt_v2_kstat, NULL);
		req_width(rfsreqcnt_v2_kstat);
	}
	if (rfsreqcnt_v3_kstat != NULL) {
		safe_kstat_read(kc, rfsreqcnt_v3_kstat, NULL);
		req_width(rfsreqcnt_v3_kstat);
	}
	if (aclproccnt_v2_kstat != NULL) {
		safe_kstat_read(kc, aclproccnt_v2_kstat, NULL);
		req_width(aclproccnt_v2_kstat);
	}
	if (aclproccnt_v3_kstat != NULL) {
		safe_kstat_read(kc, aclproccnt_v3_kstat, NULL);
		req_width(aclproccnt_v3_kstat);
	}
	if (aclreqcnt_v2_kstat != NULL) {
		safe_kstat_read(kc, aclreqcnt_v2_kstat, NULL);
		req_width(aclreqcnt_v2_kstat);
	}
	if (aclreqcnt_v3_kstat != NULL) {
		safe_kstat_read(kc, aclreqcnt_v3_kstat, NULL);
		req_width(aclreqcnt_v3_kstat);
	}
}

static void
putstats(void)
{

	if (rpc_clts_client_kstat != NULL)
		safe_kstat_write(kc, rpc_clts_client_kstat, NULL);
	if (rpc_cots_client_kstat != NULL)
		safe_kstat_write(kc, rpc_cots_client_kstat, NULL);
	if (nfs_client_kstat != NULL)
		safe_kstat_write(kc, nfs_client_kstat, NULL);
	if (rpc_clts_server_kstat != NULL)
		safe_kstat_write(kc, rpc_clts_server_kstat, NULL);
	if (rpc_cots_server_kstat != NULL)
		safe_kstat_write(kc, rpc_cots_server_kstat, NULL);
	if (nfs_server_kstat != NULL)
		safe_kstat_write(kc, nfs_server_kstat, NULL);
	if (rfsproccnt_v2_kstat != NULL)
		safe_kstat_write(kc, rfsproccnt_v2_kstat, NULL);
	if (rfsproccnt_v3_kstat != NULL)
		safe_kstat_write(kc, rfsproccnt_v3_kstat, NULL);
	if (rfsreqcnt_v2_kstat != NULL)
		safe_kstat_write(kc, rfsreqcnt_v2_kstat, NULL);
	if (rfsreqcnt_v3_kstat != NULL)
		safe_kstat_write(kc, rfsreqcnt_v3_kstat, NULL);
	if (aclproccnt_v2_kstat != NULL)
		safe_kstat_write(kc, aclproccnt_v2_kstat, NULL);
	if (aclproccnt_v3_kstat != NULL)
		safe_kstat_write(kc, aclproccnt_v3_kstat, NULL);
	if (aclreqcnt_v2_kstat != NULL)
		safe_kstat_write(kc, aclreqcnt_v2_kstat, NULL);
	if (aclreqcnt_v3_kstat != NULL)
		safe_kstat_write(kc, aclreqcnt_v3_kstat, NULL);
}

static void
setup(int zflag)
{
	if ((kc = kstat_open()) == NULL)
		fail(1, "kstat_open(): can't open /dev/kstat");

	rpc_clts_client_kstat = kstat_lookup(kc, "unix", 0, "rpc_clts_client");
	rpc_clts_server_kstat = kstat_lookup(kc, "unix", 0, "rpc_clts_server");
	rpc_cots_client_kstat = kstat_lookup(kc, "unix", 0, "rpc_cots_client");
	rpc_cots_server_kstat = kstat_lookup(kc, "unix", 0, "rpc_cots_server");
	nfs_client_kstat = kstat_lookup(kc, "nfs", 0, "nfs_client");
	nfs_server_kstat = kstat_lookup(kc, "nfs", 0, "nfs_server");
	rfsproccnt_v2_kstat = kstat_lookup(kc, "nfs", 0, "rfsproccnt_v2");
	rfsproccnt_v3_kstat = kstat_lookup(kc, "nfs", 0, "rfsproccnt_v3");
	rfsreqcnt_v2_kstat = kstat_lookup(kc, "nfs", 0, "rfsreqcnt_v2");
	rfsreqcnt_v3_kstat = kstat_lookup(kc, "nfs", 0, "rfsreqcnt_v3");
	aclproccnt_v2_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclproccnt_v2");
	aclproccnt_v3_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclproccnt_v3");
	aclreqcnt_v2_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclreqcnt_v2");
	aclreqcnt_v3_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclreqcnt_v3");
}

static void
req_width(kstat_t *req)
{
	int i, nreq, per, len;
	char fixlen[128];
	kstat_named_t *knp;
	uint64_t tot;

	tot = 0;
	knp = KSTAT_NAMED_PTR(req);
	for (i = 0; i < req->ks_ndata; i++)
		tot += knp[i].value.ui64;

	knp = kstat_data_lookup(req, "null");
	nreq = req->ks_ndata - (knp - KSTAT_NAMED_PTR(req));

	for (i = 0; i < nreq; i++) {
		len = strlen(knp[i].name) + 1;
		if (field_width < len)
			field_width = len;
		if (tot)
			per = (int)(knp[i].value.ui64 * 100 / tot);
		else
			per = 0;
		(void) sprintf(fixlen, "%" PRIu64 " %d%%",
				knp[i].value.ui64, per);
		len = strlen(fixlen) + 1;
		if (field_width < len)
			field_width = len;
	}
}

static void
stat_width(kstat_t *req)
{
	int i, nreq, len;
	char fixlen[128];
	kstat_named_t *knp;

	knp = KSTAT_NAMED_PTR(req);
	nreq = req->ks_ndata;

	for (i = 0; i < nreq; i++) {
		len = strlen(knp[i].name) + 1;
		if (field_width < len)
			field_width = len;
		(void) sprintf(fixlen, "%" PRIu64, knp[i].value.ui64);
		len = strlen(fixlen) + 1;
		if (field_width < len)
			field_width = len;
	}
}

static void
cr_print(int zflag)
{
	int i;
	kstat_named_t *kptr;

	printf("\nClient rpc:\n");

	if (rpc_cots_client_kstat != NULL) {
		printf("Connection oriented:\n");
		stat_print(rpc_cots_client_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_cots_client_kstat);
			for (i = 0; i < rpc_cots_client_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
	if (rpc_clts_client_kstat != NULL) {
		printf("Connectionless:\n");
		stat_print(rpc_clts_client_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_clts_client_kstat);
			for (i = 0; i < rpc_clts_client_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
}

static void
sr_print(int zflag)
{
	int i;
	kstat_named_t *kptr;

	printf("\nServer rpc:\n");

	if (rpc_cots_server_kstat != NULL) {
		printf("Connection oriented:\n");
		stat_print(rpc_cots_server_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_cots_server_kstat);
			for (i = 0; i < rpc_cots_server_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
	if (rpc_clts_server_kstat != NULL) {
		printf("Connectionless:\n");
		stat_print(rpc_clts_server_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_clts_server_kstat);
			for (i = 0; i < rpc_clts_server_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
}

static void
cn_print(int zflag)
{
	int i;
	uint64_t tot;
	kstat_named_t *kptr;

	if (nfs_client_kstat == NULL)
		return;

	printf("\nClient nfs:\n");

	stat_print(nfs_client_kstat);
	if (zflag) {
		kptr = KSTAT_NAMED_PTR(nfs_client_kstat);
		for (i = 0; i < nfs_client_kstat->ks_ndata; i++)
			kptr[i].value.ui64 = 0;
	}

	if (rfsreqcnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsreqcnt_v2_kstat);
		for (i = 0; i < rfsreqcnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 2: (%" PRIu64 " calls)\n", tot);
		req_print(rfsreqcnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsreqcnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}

	if (rfsreqcnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsreqcnt_v3_kstat);
		for (i = 0; i < rfsreqcnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 3: (%" PRIu64 " calls)\n", tot);
		req_print(rfsreqcnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsreqcnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
}

static void
sn_print(int zflag)
{
	int i;
	uint64_t tot;
	kstat_named_t *kptr;

	if (nfs_server_kstat == NULL)
		return;

	printf("\nServer nfs:\n");

	stat_print(nfs_server_kstat);
	if (zflag) {
		kptr = KSTAT_NAMED_PTR(nfs_server_kstat);
		for (i = 0; i < nfs_server_kstat->ks_ndata; i++)
			kptr[i].value.ui64 = 0;
	}

	if (rfsproccnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsproccnt_v2_kstat);
		for (i = 0; i < rfsproccnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 2: (%" PRIu64 " calls)\n", tot);
		req_print(rfsproccnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsproccnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}

	if (rfsproccnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsproccnt_v3_kstat);
		for (i = 0; i < rfsproccnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 3: (%" PRIu64 " calls)\n", tot);
		req_print(rfsproccnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsproccnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
}

static void
ca_print(int zflag)
{
	int i;
	uint64_t tot;
	kstat_named_t *kptr;

	if (nfs_client_kstat == NULL)
		return;

	printf("\nClient nfs_acl:\n");

	if (aclreqcnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclreqcnt_v2_kstat);
		for (i = 0; i < aclreqcnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 2: (%" PRIu64 " calls)\n", tot);
		req_print(aclreqcnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclreqcnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}

	if (aclreqcnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclreqcnt_v3_kstat);
		for (i = 0; i < aclreqcnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 3: (%" PRIu64 " calls)\n", tot);
		req_print(aclreqcnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclreqcnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
}

static void
sa_print(int zflag)
{
	int i;
	uint64_t tot;
	kstat_named_t *kptr;

	if (nfs_server_kstat == NULL)
		return;

	printf("\nServer nfs_acl:\n");

	if (aclproccnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclproccnt_v2_kstat);
		for (i = 0; i < aclproccnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 2: (%" PRIu64 " calls)\n", tot);
		req_print(aclproccnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclproccnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}

	if (aclproccnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclproccnt_v3_kstat);
		for (i = 0; i < aclproccnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ui64;
		printf("Version 3: (%" PRIu64 " calls)\n", tot);
		req_print(aclproccnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclproccnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ui64 = 0;
		}
	}
}

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

static void
req_print(kstat_t *req, uint64_t tot)
{
	int i, j, nreq, per;
	char fixlen[128];
	kstat_named_t *knp;

	knp = kstat_data_lookup(req, "null");
	nreq = req->ks_ndata - (knp - KSTAT_NAMED_PTR(req));

	for (i = 0; i < nreq; i += ncolumns) {
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			printf("%-*s", field_width, knp[j].name);
		}
		printf("\n");
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			if (tot)
				per = (int)(knp[j].value.ui64 * 100 / tot);
			else
				per = 0;
			(void) sprintf(fixlen, "%" PRIu64 " %d%% ",
			    knp[j].value.ui64, per);
			printf("%-*s", field_width, fixlen);
		}
		printf("\n");
	}
}

static void
stat_print(kstat_t *req)
{
	int i, j, nreq;
	char fixlen[128];
	kstat_named_t *knp;

	knp = KSTAT_NAMED_PTR(req);
	nreq = req->ks_ndata;

	for (i = 0; i < nreq; i += ncolumns) {
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			printf("%-*s", field_width, knp[j].name);
		}
		printf("\n");
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			(void) sprintf(fixlen, "%" PRIu64 " ",
					knp[j].value.ui64);
			printf("%-*s", field_width, fixlen);
		}
		printf("\n");
	}
}

/*
 * my_dir and my_path could be pointers
 */
struct myrec {
	u_long my_fsid;
	char my_dir[MAXPATHLEN];
	char *my_path;
	char *ig_path;
	struct myrec *next;
};

/*
 * Print the mount table info
 */
static void
mi_print(void)
{
	FILE *mt;
	struct extmnttab m;
	struct myrec *list, *mrp, *pmrp;
	char *flavor;
	int ignored = 0;
	seconfig_t nfs_sec;
	kstat_t *ksp;
	struct mntinfo_kstat mik;
	char *timer_name[] = {
		"Lookups",
		"Reads",
		"Writes",
		"All"
	};


	mt = fopen(MNTTAB, "r");
	if (mt == NULL) {
		perror(MNTTAB);
		exit(0);
	}

	list = NULL;
	resetmnttab(mt);
	while (getextmntent(mt, &m, sizeof (struct extmnttab)) == 0) {
		/* ignore non "nfs" and save the "ignore" entries */
		if (strcmp(m.mnt_fstype, MNTTYPE_NFS) != 0)
			continue;

		if ((mrp = malloc(sizeof (struct myrec))) == 0) {
			fprintf(stderr, "nfsstat: not enough memory\n");
			exit(1);
		}
		mrp->my_fsid = makedev(m.mnt_major, m.mnt_minor);
		if (ignore(m.mnt_mntopts)) {
			/*
			* ignored entries cannot be ignored for this
			* option. We have to display the info for this
			* nfs mount. The ignore is an indication
			* that the actual mount point is different and
			* something is in between the nfs mount.
			* So save the mount point now
			*/
			if ((mrp->ig_path = malloc(
				    strlen(m.mnt_mountp) + 1)) == 0) {
				fprintf(stderr, "nfsstat: not enough memory\n");
				exit(1);
			}
			(void) strcpy(mrp->ig_path, m.mnt_mountp);
			ignored++;
		} else {
			mrp->ig_path = 0;
			(void) strcpy(mrp->my_dir, m.mnt_mountp);
		}
		if ((mrp->my_path = strdup(m.mnt_special)) == NULL) {
			fprintf(stderr, "nfsstat: not enough memory\n");
			exit(1);
		}
		mrp->next = list;
		list = mrp;
	}

	/*
	* If something got ignored, go to the beginning of the mnttab
	* and look for the cachefs entries since they are the one
	* causing this. The mount point saved for the ignored entries
	* is matched against the special to get the actual mount point.
	* We are interested in the acutal mount point so that the output
	* look nice too.
	*/
	if (ignored) {
		rewind(mt);
		resetmnttab(mt);
		while (getextmntent(mt, &m, sizeof (struct extmnttab)) == 0) {

			/* ignore non "cachefs" */
			if (strcmp(m.mnt_fstype, MNTTYPE_CACHEFS) != 0)
				continue;

			for (mrp = list; mrp; mrp = mrp->next) {
				if (mrp->ig_path == 0)
					continue;
				if (strcmp(mrp->ig_path, m.mnt_special) == 0) {
					mrp->ig_path = 0;
					(void) strcpy(mrp->my_dir,
							m.mnt_mountp);
				}
			}
		}
		/*
		* Now ignored entries which do not have
		* the my_dir initialized are really ignored; This never
		* happens unless the mnttab is corrupted.
		*/
		for (pmrp = 0, mrp = list; mrp; mrp = mrp->next) {
			if (mrp->ig_path == 0)
				pmrp = mrp;
			else if (pmrp)
				pmrp->next = mrp->next;
			else
				list = mrp->next;
		}
	}

	(void) fclose(mt);


	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		int i;

		if (ksp->ks_type != KSTAT_TYPE_RAW)
			continue;
		if (strcmp(ksp->ks_module, "nfs") != 0)
			continue;
		if (strcmp(ksp->ks_name, "mntinfo") != 0)
			continue;

		for (mrp = list; mrp; mrp = mrp->next) {
			if ((mrp->my_fsid & MAXMIN) == ksp->ks_instance)
				break;
		}
		if (mrp == 0)
			continue;

		if (safe_kstat_read(kc, ksp, &mik) == -1)
			continue;

		printf("%s from %s\n", mrp->my_dir, mrp->my_path);

		printf(" Flags:		vers=%u,proto=%s", mik.mik_vers,
		    mik.mik_proto);

		/*
		 *  get the secmode name from /etc/nfssec.conf.
		 */
		if (!nfs_getseconfig_bynumber(mik.mik_secmod, &nfs_sec)) {
			flavor = nfs_sec.sc_name;
		} else
			flavor = NULL;

		if (flavor != NULL)
			printf(",sec=%s", flavor);
		else
			printf(",sec#=%d", mik.mik_secmod);

		printf(",%s", (mik.mik_flags & MI_HARD) ? "hard" : "soft");
		if (mik.mik_flags & MI_PRINTED)
			printf(",printed");
		printf(",%s", (mik.mik_flags & MI_INT) ? "intr" : "nointr");
		if (mik.mik_flags & MI_DOWN)
			printf(",down");
		if (mik.mik_flags & MI_NOAC)
			printf(",noac");
		if (mik.mik_flags & MI_NOCTO)
			printf(",nocto");
		if (mik.mik_flags & MI_DYNAMIC)
			printf(",dynamic");
		if (mik.mik_flags & MI_LLOCK)
			printf(",llock");
		if (mik.mik_flags & MI_GRPID)
			printf(",grpid");
		if (mik.mik_flags & MI_RPCTIMESYNC)
			printf(",rpctimesync");
		if (mik.mik_flags & MI_LINK)
			printf(",link");
		if (mik.mik_flags & MI_SYMLINK)
			printf(",symlink");
		if (mik.mik_flags & MI_READDIR)
			printf(",readdir");
		if (mik.mik_flags & MI_ACL)
			printf(",acl");

		printf(",rsize=%d,wsize=%d,retrans=%d,timeo=%d",
			mik.mik_curread, mik.mik_curwrite, mik.mik_retrans,
			mik.mik_timeo);
		printf("\n");
		printf(" Attr cache:	acregmin=%d,acregmax=%d"
			",acdirmin=%d,acdirmax=%d\n", mik.mik_acregmin,
			mik.mik_acregmax, mik.mik_acdirmin, mik.mik_acdirmax);

#define	srtt_to_ms(x) x, (x * 2 + x / 2)
#define	dev_to_ms(x) x, (x * 5)

		for (i = 0; i < NFS_CALLTYPES + 1; i++) {
			int j;

			j = (i == NFS_CALLTYPES ? i - 1 : i);
			if (mik.mik_timers[j].srtt ||
			    mik.mik_timers[j].rtxcur) {
				printf(
		" %s:     srtt=%d (%dms), dev=%d (%dms), cur=%u (%ums)\n",
				timer_name[i],
				srtt_to_ms(mik.mik_timers[i].srtt),
				dev_to_ms(mik.mik_timers[i].deviate),
				mik.mik_timers[i].rtxcur,
				mik.mik_timers[i].rtxcur * 20);
			}
		}

		if (strchr(mrp->my_path, ','))
			printf(
			    " Failover:	noresponse=%d,failover=%d,"
			    "remap=%d,currserver=%s\n",
			    mik.mik_noresponse, mik.mik_failover,
			    mik.mik_remap, mik.mik_curserver);
		printf("\n");
	}
}

static char *mntopts[] = { MNTOPT_IGNORE, MNTOPT_DEV, NULL };
#define	IGNORE  0
#define	DEV	1

/*
 * Return 1 if "ignore" appears in the options string
 */
static int
ignore(char *opts)
{
	char *value;
	char *s;

	if (opts == NULL)
		return (0);
	s = strdup(opts);
	if (s == NULL)
		return (0);
	opts = s;

	while (*opts != '\0') {
		if (getsubopt(&opts, mntopts, &value) == IGNORE) {
			free(s);
			return (1);
		}
	}

	free(s);
	return (0);
}

void
usage(void)
{

	fprintf(stderr, "Usage: nfsstat [-cnrsmza]\n");
	exit(1);
}

static void
fail(int do_perror, char *message, ...)
{
	va_list args;

	va_start(args, message);
	fprintf(stderr, "nfsstat: ");
	vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		fprintf(stderr, ": %s", strerror(errno));
	fprintf(stderr, "\n");
	exit(1);
}

kid_t
safe_kstat_read(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
	kid_t kstat_chain_id = kstat_read(kc, ksp, data);

	if (kstat_chain_id == -1)
		fail(1, "kstat_read(%x, '%s') failed", kc, ksp->ks_name);
	return (kstat_chain_id);
}

kid_t
safe_kstat_write(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
	kid_t kstat_chain_id = kstat_write(kc, ksp, data);

	if (kstat_chain_id == -1)
		fail(1, "kstat_write(%x, '%s') failed", kc, ksp->ks_name);
	return (kstat_chain_id);
}
