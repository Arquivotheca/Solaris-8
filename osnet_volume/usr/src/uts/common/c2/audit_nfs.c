/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * @(#)audit_nfs.c 2.11 92/01/20 SMI; SunOS CMW
 * @(#)audit_nfs.c 4.2.1.2 91/05/08 SMI; BSM Module
 */

#pragma ident	"@(#)audit_nfs.c	1.19	99/10/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/pathname.h>
/* #include <sys/au_membuf.h> */		/* for so_to_bl() */
#include <netinet/in.h>
#include <net/route.h>
#include <netinet/in_pcb.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/auth_des.h>
#include <sys/t_kuser.h>
#include <sys/tiuser.h>
#include <rpc/svc.h>
#include <nfs/nfs.h>
#include <nfs/export.h>
#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_record.h>
#include <c2/audit_kevents.h>

extern int audit_policy;

#ifdef NFSSERVER
/*ARGSUSED*/
/* exportfs start function */
aus_exportfs(struct p_audit_data *pad)
{
	/* XXX64 au_to_data64() */
	klwp_id_t clwp = ttolwp(curthread);

	struct a {
		long	dname;		/* char * */
		long	uex;	/* struct exportdata * */
	} *uap = (struct a *)clwp->lwp_ap;

	int error;
	STRUCT_DECL(exportdata, kex);
	caddr_t ptr;
	int	num;
	int	i;

	if (uap->uex == 0) {
			/* freeing export entry */
		au_uwrite(au_to_arg(1, "freeing export entry", (uint_t)0));
		return;
	}

	/*
	 * Load in everything, and do sanity checking
	 */
	error = copyin((caddr_t)uap->uex,
			STRUCT_BUF(kex),
			STRUCT_SIZE(kex));
	if (error) {
		goto error_return;
	}
	if (STRUCT_FGET(kex, ex_flags) & ~(EX_RDONLY | EX_RDMOSTLY)) {
		goto error_return;
	}
	if (STRUCT_FGET(kex, ex_flags) & EX_RDMOSTLY) {
		error = loadaddrs(STRUCT_FADDR(kex, ex_writeaddrs));
		if (error) {
			goto error_return;
		}
	}
	switch (STRUCT_FGET(kex, ex_auth)) {
	case AUTH_UNIX:
		error = loadaddrs(STRUCT_FADDR(kex, ex_unix.rootaddrs));
		break;
	case AUTH_DES:
		error = loadrootnames(kex);
		break;
	default:
		error = EINVAL;
	}
	if (error) {
		goto errors;
	}

		/* audit system call here */
	au_uwrite(au_to_arg(2, "ex_flags", (uint_t)STRUCT_FGET(kex, ex_flags)));
	au_uwrite(au_to_arg(3, "ex_anon", (uint_t)STRUCT_FGET(kex, ex_anon)));
	au_uwrite(au_to_arg(4, "ex_auth", (uint_t)STRUCT_FGET(kex, ex_auth)));
	switch (STRUCT_FGET(kex, ex_auth)) {
	case AUTH_UNIX:
		num = STRUCT_FGET(kex, ex_unix.rootaddrs.naddrs);
		au_uwrite(au_to_arg(5, "unix rootaddrs", (uint_t)num));
		ptr = STRUCT_FGETP(kex, ex_unix.rootaddrs.addrvec);
		for (i = 0; i < num; i++) {
			/* WRONG!!! we have to copy in before */
			au_uwrite(au_to_data(AUP_HEX, AUR_SHORT, 1, ptr));
			ptr += 2;
			au_uwrite(au_to_data(AUP_HEX, AUR_CHAR, 14, ptr));
			ptr += 14;
		}
		break;
	case AUTH_DES:
		num = STRUCT_FGET(kex, ex_des.nnames);
		au_uwrite(au_to_arg(5, "des rootnames", (uint_t)num));
		for (i = 0; i < num; i++) {
			ptr = STRUCT_FGETP(kex, ex_des.rootnames);
			/* WRONG!!! we have to copy in before */
			au_uwrite(au_to_text((uint_t)name[i]));
		}
		break;
	}
	if (STRUCT_FGET(kex, ex_flags) & EX_RDMOSTLY) {
		num = STRUCT_FGET(kex, ex_writeaddrs.naddrs);
		au_uwrite(au_to_arg(6, "ex_writeaddrs", (uint_t)num));
		ptr = STRUCT_FGET(kex, ex_writeaddrs.addrvec);
		for (i = 0; i < num; i++) {
			/* WRONG!!! we have to copy in before */
			au_uwrite(au_to_data(AUP_HEX, AUR_SHORT, 1, ptr));
			ptr += 2;
			au_uwrite(au_to_data(AUP_HEX, AUR_CHAR, 14, ptr));
			ptr += 14;
		}
	}

		/* free up resources */
	switch (STRUCT_FGET(kex, ex_auth)) {
	case AUTH_UNIX:
		mem_free((char *)STRUCT_FGETP(kex, ex_unix.rootaddrs.addrvec),
			(STRUCT_FGET(kex, ex_unix.rootaddrs.naddrs) *
			    sizeof (struct sockaddr)));
		break;
	case AUTH_DES:
		freenames(kex);
		break;
	}

errors:
	if (STRUCT_FGET(kex, ex_flags) & EX_RDMOSTLY) {
		mem_free((char *)STRUCT_FGET(kex, ex_writeaddrs.addrvec),
			STRUCT_FGET(kex, ex_writeaddrs.naddrs) *
					sizeof (struct sockaddr));
	}

error_return:
/*
	mem_free((char *)kex, sizeof (struct exportdata));
*/
}
#endif	/* NFSSERVER */
