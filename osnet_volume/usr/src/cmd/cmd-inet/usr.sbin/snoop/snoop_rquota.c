/*
 * Copyright (c) 1991, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)snoop_rquota.c	1.9	99/08/20 SMI"	/* SunOS	*/

#include <sys/types.h>
#include <sys/errno.h>
#include <setjmp.h>
#include <string.h>

#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpcsvc/rquota.h>
#include "snoop.h"

extern char *dlc_header;
extern jmp_buf xdr_err;

static char *procnames_short[] = {
	"Null",		/*  0 */
	"GETQUOTA",	/*  1 */
	"GETACTIVE",	/*  2 */
};

static char *procnames_long[] = {
	"Null procedure",	/*  0 */
	"Get quotas",		/*  1 */
	"Get active quotas",	/*  2 */
};

#define	MAXPROC	2

static void show_quota(void);

void
interpret_rquota(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff[RQ_PATHLEN + 1];
	int status;
	int uid;

	if (proc < 0 || proc > MAXPROC)
		return;

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) getxdr_string(buff, RQ_PATHLEN);
			uid = getxdr_long();
			(void) sprintf(line,
				"RQUOTA C %s Uid=%d Path=%s",
				procnames_short[proc],
				uid, buff);

			check_retransmit(line, xid);
		} else {
			(void) sprintf(line, "RQUOTA R %s ",
				procnames_short[proc]);
			line += strlen(line);
			status = getxdr_u_long();
			if (status == Q_OK)
				(void) sprintf(line, "OK");
			else if (status == Q_NOQUOTA)
				(void) sprintf(line, "No quota");
			else if (status == Q_EPERM)
				(void) sprintf(line, "No permission");
		}
	}

	if (flags & F_DTAIL) {
		show_header("RQUOTA:  ", "Remote Quota Check", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_long[proc]);

		if (type == CALL) {
			switch (proc) {
			case  RQUOTAPROC_GETQUOTA:
			case RQUOTAPROC_GETACTIVEQUOTA:
				(void) showxdr_string(RQ_PATHLEN,
					"Path = %s");
				(void) showxdr_long("User id = %d");
				break;
			}
		} else {
			status = getxdr_u_long();
			(void) sprintf(get_line(0, 0),
				"Status = %lu (%s)",
				status,
				status == Q_OK ? "OK" :
				status == Q_NOQUOTA ? "No quota" :
				status == Q_EPERM ? "No permission":"");

			if (status == Q_OK)
				show_quota();
		}

		show_trailer();
	}
}

static void
show_quota()
{
	int active;

	(void) showxdr_u_long("Block size = %lu");
	active = getxdr_u_long();
	(void) sprintf(get_line(0, 0),
		"         Quota checking = %lu (%s)",
		active,
		active ? "on" : "off");
	(void) showxdr_u_long("      Blocks hard limit = %lu");
	(void) showxdr_u_long("      Blocks soft limit = %lu");
	(void) showxdr_u_long("    Current block count = %lu");
	(void) show_space();
	(void) showxdr_u_long("        File hard limit = %lu");
	(void) showxdr_u_long("        File soft limit = %lu");
	(void) showxdr_u_long("     Current file count = %lu");
	(void) show_space();
	(void) showxdr_u_long("Excessive blocks limit  = %lu sec");
	(void) showxdr_u_long("Excessive files  limit  = %lu sec");
}
