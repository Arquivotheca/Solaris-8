#ifndef lint
static char	sccsid[] = "@(#)audit_inetd.c 1.16 99/10/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <bsm/audit.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include <netinet/in.h>
#include <generic.h>
#include <pwd.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef C2_DEBUG
#define	dprintf(x) {printf x; }
#else
#define	dprintf(x)
#endif

static void audit_inetd_session_setup(struct passwd *);
static au_tid_addr_t audit_inetd_tid;

int
audit_inetd_config(void)
{
	aug_save_event(AUE_inetd_connect);
	aug_save_namask();
	return (0);
}

/*
 * save terminal ID for user level audit record generation
 */
int
audit_inetd_termid(int fd)
{
	struct sockaddr_in6 peer;
	struct sockaddr_in6 sock;
	int peerlen = sizeof (peer);
	int socklen = sizeof (sock);
	uint_t port;
	uint32_t *addr;
	

	if (cannot_audit(0)) {
		return (0);
	}

	/* get peer name */
	if (getpeername(fd, (struct sockaddr *)&peer, (socklen_t *)&peerlen)
		< 0) {
		return (1);
	}

	addr = (uint32_t *)&peer.sin6_addr;

	/* get sock name */
	if (getsockname(fd, (struct sockaddr *)&sock, (socklen_t *)&socklen)
		< 0) {
		return (1);
	}

	bzero(&audit_inetd_tid, sizeof(audit_inetd_tid));

	port = ((peer.sin6_port<<16) | (sock.sin6_port));
	audit_inetd_tid.at_port = port;

	if (peer.sin6_family == AF_INET6) {
		aug_save_tid_ex(port, (uint32_t *)&peer.sin6_addr, AU_IPv6);

		audit_inetd_tid.at_type = AU_IPv6;
		audit_inetd_tid.at_addr[0] = addr[0];
		audit_inetd_tid.at_addr[1] = addr[1];
		audit_inetd_tid.at_addr[2] = addr[2];
		audit_inetd_tid.at_addr[3] = addr[3];
	} else {
		struct sockaddr_in *ppeer = (struct sockaddr_in *)&peer;
		aug_save_tid(port, (int)ppeer->sin_addr.s_addr);

		audit_inetd_tid.at_type = AU_IPv4;
		audit_inetd_tid.at_addr[0] = (uint32_t)ppeer->sin_addr.s_addr;
	}
}

int
audit_inetd_service(
		char          *service_name,	/* name of service */
		struct passwd *pwd)		/* password */
{
	int	rd;		/* audit record descriptor */
	int	set_audit = 0;	/* flag - set audit characteristics */
	auditinfo_addr_t ai;

	dprintf(("audit_inetd_service()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	/*
	 * set default values. We will overwrite them if appropriate.
	 */
	if (getaudit_addr(&ai, sizeof (ai))) {
		perror("inetd");
		exit(1);
	}
	aug_save_auid(ai.ai_auid);	/* Audit ID */
	aug_save_uid(getuid());		/* User ID */
	aug_save_euid(geteuid());	/* Effective User ID */
	aug_save_gid(getgid());		/* Group ID */
	aug_save_egid(getegid());	/* Effective Group ID */
	aug_save_pid(getpid());		/* process ID */
	aug_save_asid(getpid());	/* session ID */

	if ((pwd != NULL) && (pwd->pw_uid)) {
		aug_save_auid(pwd->pw_uid);	/* Audit ID */
		aug_save_uid(pwd->pw_uid);	/* User ID */
		aug_save_euid(pwd->pw_uid);	/* Effective User ID */
		aug_save_gid(pwd->pw_gid);	/* Group ID */
		aug_save_egid(pwd->pw_gid);	/* Effective Group ID */
		set_audit = 1;
	}

	aug_save_text(service_name);
	aug_save_sorf(0);
	aug_audit();

	if (set_audit)
		audit_inetd_session_setup(pwd);

	return (0);
}

/*
 * set the audit characteristics for the inetd started process.
 * inetd is setting the uid.
 */
void
audit_inetd_session_setup(struct passwd *pwd)
{
	struct auditinfo_addr info;
	au_mask_t mask;

	info.ai_auid = pwd->pw_uid;

	mask.am_success = 0;
	mask.am_failure = 0;
	au_user_mask(pwd->pw_name, &mask);
	info.ai_mask.am_success  = mask.am_success;
	info.ai_mask.am_failure  = mask.am_failure;

	info.ai_asid = getpid();

	info.ai_termid = audit_inetd_tid;

	if (setaudit_addr(&info, sizeof(info)) < 0) {
		perror("inetd: setaudit_addr");
		exit(1);
	}
}
