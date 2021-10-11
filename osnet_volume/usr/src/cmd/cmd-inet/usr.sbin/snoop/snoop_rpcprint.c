/*
 * Copyright (c) 1991, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)snoop_rpcprint.c	1.8	99/08/20 SMI"	/* SunOS	*/

#include <string.h>
#include <sys/types.h>
#include <sys/tiuser.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include "snoop.h"

int rpcsec_gss_control_proc(int type, int flags, int xid);

int rpcsec_gss_pre_proto(int type, int flags, int xid,
				int prog, int vers, int proc);

void rpcsec_gss_post_proto(int flags, int xid);

void
protoprint(flags, type, xid, prog, vers, proc, data, len)
	ulong_t xid;
	int flags, type, prog, vers, proc;
	char *data;
	int len;
{
	char *name;
	void (*interpreter)(int, int, int, int, int, char *, int);

	switch (prog) {
	case 100000:	interpreter = interpret_pmap;		break;
	case 100001:	interpreter = interpret_rstat;		break;
	case 100003:	interpreter = interpret_nfs;		break;
	case 100004:	interpreter = interpret_nis;		break;
	case 100005:	interpreter = interpret_mount;		break;
	case 100007:	interpreter = interpret_nisbind;	break;
	case 100011:	interpreter = interpret_rquota;		break;
	case 100021:	interpreter = interpret_nlm;		break;
	case 100026:	interpreter = interpret_bparam;		break;
	case 100227:	interpreter = interpret_nfs_acl;	break;
	case 100300:	interpreter = interpret_nisplus;	break;
	case 100302:	interpreter = interpret_nisp_cb;	break;
	case 150006:	interpreter = interpret_solarnet_fw;	break;
	default:	interpreter = NULL;
	}

	/*
	 *  If the RPC header indicates it's using the RPCSEC_GSS_*
	 *  control procedure, print it.
	 */
	if (rpcsec_gss_control_proc(type, flags, xid)) {
			return;
	}

	if (interpreter == NULL) {
		if (!(flags & F_SUM))
			return;
		name = nameof_prog(prog);
		if (*name == '?' || strcmp(name, "transient") == 0)
			return;
		(void) sprintf(get_sum_line(), "%s %c",
			name,
			type == CALL ? 'C' : 'R');
	} else {
		/* Pre-processing based on different RPCSEC_GSS services. */
		if (rpcsec_gss_pre_proto(type, flags, xid, prog, vers, proc))
			return;

		(*interpreter) (flags, type, xid, vers, proc, data, len);

		/* Post-processing based on different RPCSEC_GSS services. */
		rpcsec_gss_post_proto(flags, xid);
	}
}
