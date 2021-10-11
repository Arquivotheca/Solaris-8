/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ptm.c	1.3	99/10/04 SMI"

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

#include <sys/types.h>
#include <sys/strsubr.h>
#include <sys/ptms.h>

typedef struct pt_flags {
	const char *pt_name;
	const char *pt_descr;
} ptflags_t;

static const struct pt_flags pf[] = {
	{ "PTLOCK",		"Master/slave pair is locked" },
	{ "PTMOPEN",		"Master side is open" },
	{ "PTSOPEN",		"Slave side is open" },
	{ "PTSTTY",		"Slave side is tty" },
	{ NULL },
};

static int
pt_parse_flag(const ptflags_t ftable[],  const char *arg, uint32_t *flag)
{
	int i;

	for (i = 0; ftable[i].pt_name != NULL; i++) {
		if (strcasecmp(arg, ftable[i].pt_name) == 0) {
			*flag |= (1 << i);
			return (0);
		}
	}

	return (-1);
}

static void
pt_flag_usage(const ptflags_t ftable[])
{
	int i;

	for (i = 0; ftable[i].pt_name != NULL; i++)
		mdb_printf("%12s %s\n",
		    ftable[i].pt_name, ftable[i].pt_descr);
}



static void
ptms_pr_qinfo(char *buf, size_t nbytes, struct pt_ttys *pt, char *peername,
    queue_t *peerq, char *procname)
{
	(void) mdb_snprintf(buf, nbytes,
	    "pts/%d:%s:	%p\nprocess:	%d(%s)",
	    pt->pt_minor, peername, peerq, pt->pt_pid, procname);
}

static int
ptms(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const int PT_FLGDELT = (int)(sizeof (uintptr_t) * 2 + 5);

	struct pt_ttys pt;
	char c[MAXCOMLEN + 1];
	const char *flag = NULL, *not_flag = NULL;
	proc_t p;
	uint_t verbose = FALSE;
	uint32_t mask = 0, not_mask = 0;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &verbose,
	    'f', MDB_OPT_STR, &flag,
	    'F', MDB_OPT_STR, &not_flag, NULL) != argc)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags) && flag == NULL && not_flag == NULL) {
		(void) mdb_printf("%?-s %s %s %?-s %?-s %-6s %s\n",
		    "ADDR", "PTY", "FL", "MASTERQ", "SLAVEQ",
		    "PID", "PROC");
	}

	if (flag != NULL && pt_parse_flag(pf, flag, &mask) == -1) {
		mdb_warn("unrecognized pty flag '%s'\n", flag);
		pt_flag_usage(pf);
		return (DCMD_USAGE);
	}

	if (not_flag != NULL && pt_parse_flag(pf, not_flag, &not_mask) == -1) {
		mdb_warn("unrecognized queue flag '%s'\n", flag);
		pt_flag_usage(pf);
		return (DCMD_USAGE);
	}

	if (mdb_vread(&pt, sizeof (pt), addr) == -1) {
		mdb_warn("failed to read pty structure");
		return (DCMD_ERR);
	}

	if (mask != 0 && !(pt.pt_state & mask))
		return (DCMD_OK);

	if (not_mask != 0 && (pt.pt_state & not_mask))
		return (DCMD_OK);

	/*
	 * Options are specified for filtering, so If any option is specified on
	 * the command line, just print address and exit.
	 */
	if (flag != NULL || not_flag != NULL) {
		mdb_printf("%0?p\n", addr);
		return (DCMD_OK);
	}

	if (pt.pt_pid != 0) {
		if (mdb_pid2proc(pt.pt_pid, &p) == NULL)
			(void) strcpy(c, "<defunct>");
		else
			(void) strcpy(c, p.p_user.u_comm);
	} else
		(void) strcpy(c, "<unknown>");

	(void) mdb_printf("%0?p %3d %1x %0?p %0?p %6d %s\n",
	    addr, pt.pt_minor, pt.pt_state, pt.ptm_rdq, pt.pts_rdq,
	    pt.pt_pid, c);

	if (verbose) {
		int i, arm = 0;

		for (i = 0; pf[i].pt_name != NULL; i++) {
			if (!(pt.pt_state & (1 << i)))
				continue;
			if (!arm) {
				mdb_printf("%*s|\n%*s+-->  ",
				    PT_FLGDELT, "", PT_FLGDELT, "");
				arm = 1;
			} else
				mdb_printf("%*s      ", PT_FLGDELT, "");

			mdb_printf("%-12s %s\n",
			    pf[i].pt_name, pf[i].pt_descr);
		}
	}

	return (DCMD_OK);
}

static void
ptms_qinfo(const queue_t *q, char *buf, size_t nbytes, int ismaster)
{
	char c[MAXCOMLEN + 1];
	struct pt_ttys pt;
	proc_t p;

	(void) mdb_vread(&pt, sizeof (pt), (uintptr_t)q->q_ptr);

	if (pt.pt_pid != 0) {
		if (mdb_pid2proc(pt.pt_pid, &p) == NULL)
			(void) strcpy(c, "<defunct>");
		else
			(void) strcpy(c, p.p_user.u_comm);
	} else
		(void) strcpy(c, "<unknown>");

	if (ismaster)
		ptms_pr_qinfo(buf, nbytes, &pt, "slave", pt.pts_rdq, c);
	else
		ptms_pr_qinfo(buf, nbytes, &pt, "master", pt.ptm_rdq, c);
}

void
ptm_qinfo(const queue_t *q, char *buf, size_t nbytes)
{
	ptms_qinfo(q, buf, nbytes, 1);
}

void
pts_qinfo(const queue_t *q, char *buf, size_t nbytes)
{
	ptms_qinfo(q, buf, nbytes, 0);
}

static const mdb_dcmd_t dcmds[] = {
	{ "ptms", ":", "print ptms structure", ptms },
	{ NULL }
};

static const mdb_qops_t ptm_qops = {
	ptm_qinfo, mdb_qrnext_default, mdb_qwnext_default
};

static const mdb_qops_t pts_qops = {
	pts_qinfo, mdb_qrnext_default, mdb_qwnext_default
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, NULL };

const mdb_modinfo_t *
_mdb_init(void)
{
	GElf_Sym sym;

	if (mdb_lookup_by_obj("ptm", "ptmwint", &sym) == 0)
		mdb_qops_install(&ptm_qops, (uintptr_t)sym.st_value);
	if (mdb_lookup_by_obj("pts", "ptswint", &sym) == 0)
		mdb_qops_install(&pts_qops, (uintptr_t)sym.st_value);

	return (&modinfo);
}

void
_mdb_fini(void)
{
	GElf_Sym sym;

	if (mdb_lookup_by_obj("ptm", "ptmwint", &sym) == 0)
		mdb_qops_remove(&ptm_qops, (uintptr_t)sym.st_value);
	if (mdb_lookup_by_obj("pts", "ptswint", &sym) == 0)
		mdb_qops_remove(&pts_qops, (uintptr_t)sym.st_value);
}
