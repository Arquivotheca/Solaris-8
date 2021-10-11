/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_daemon_auth.c	1.9	98/08/12 SMI"

/*
 * DR Daemon authentication routines.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <rpc/rpc.h>
#include <pwd.h>
#include "dr_daemon.h"

/*
 * ------------------------------------------------------
 * Create a RPC (SYS) authentication structure that can
 * be used when sending RPC's to the DR daemon.  We use
 * the first element in the gid array to be a DR magic
 * number.  This function is intended to be called by
 * the DR application.
 * ------------------------------------------------------
 */
AUTH *
create_dr_auth()
{
	struct utsname	unm;
#ifdef SSP_PASSWORD
	uid_t	druid = getuid();
	gid_t	drgid = getgid();
#else SSP_PASSWORD
	uid_t	druid = (uid_t)DR_MAGIC;
	gid_t	drgid = (gid_t)DR_MAGIC;
#endif SSP_PASSWORD
	gid_t	drmagic = (gid_t)DR_MAGIC;

	if (uname(&unm) == -1) {
		perror("uname");
		return (NULL);
	}

	return (authsys_create(unm.nodename, druid, drgid, 1, &drmagic));
}

/*
 * ------------------------------------------------------
 * Validate that the given RPC request has the authority
 * to request DR services.  This function is intended to
 * be used by the DR daemon.
 * ------------------------------------------------------
 */
int
valid_dr_auth(struct svc_req *req)
{
	struct authsys_parms	*aup;
	struct opaque_auth	*oap;
#ifdef VALIDATE_HOSTNAME		/* SPR 77426 */
	struct utsname		unm;
#endif
#ifdef SSP_PASSWORD
	struct passwd		*druser;
#endif
	char	drhost[SYS_NMLN];

	if (req == NULL)
		return (0);

	oap = &req->rq_cred;
#ifdef AUTH_DEBUG
	printf("dr_auth: oa_flavor = %d\n", (int)oap->oa_flavor);
	printf("dr_auth: oa_length = %d\n", oap->oa_length);
#endif AUTH_DEBUG
	if (oap->oa_flavor != AUTH_SYS)
		return (0);

	/* LINTED */
	aup = (struct authsys_parms *)req->rq_clntcred;
	if (aup == NULL)
		return (0);

#ifdef AUTH_DEBUG
	printf("dr_auth: aup_machname = %s\n", aup->aup_machname);
	printf("dr_auth: aup_uid = %d (%#x)\n", aup->aup_uid, aup->aup_uid);
	printf("dr_auth: aup_gid = %d (%#x)\n", aup->aup_gid, aup->aup_gid);
	printf("dr_auth: aup_len = %d\n", aup->aup_len);
	if (aup->aup_len > 0)
		printf("dr_auth: aup_gids[0] = %#x\n", aup->aup_gids[0]);
#endif AUTH_DEBUG

#ifdef VALIDATE_HOSTNAME		/* SPR 77426 */
	if (uname(&unm) == -1) {
		perror("uname");
		return (0);
	}
	sprintf(drhost, "%s%s\0", unm.nodename, DR_HOST_APDX);
#else
	/*
	 * SPR 77426
	 *
	 * Need to remove hostname validation since SSP hostnames can
	 * be virtually anything!  Cannot assume "host"-ssp.
	 * Will need to get smart about this later.  For now just
	 * copy caller's hostname into our hostname, drhost, so they're
	 * the same.
	 */
	strcpy(drhost, aup->aup_machname);
#endif /* VALIDATE_HOSTNAME */

#ifdef AUTH_DEBUG
	printf("dr_auth: drmagic = %#x\n", DR_MAGIC);
	printf("dr_auth: drhost  = %s\n", drhost);
	printf("dr_auth: druser  = %s\n", DR_USER);
#endif AUTH_DEBUG

#ifdef SSP_PASSWORD
	if ((druser = getpwnam(DR_USER)) == NULL) {
		/*
		fprintf(stderr,
			"No password entry found for \"%s\"\n", DR_USER);
		*/
		return (0);
	}

#ifdef AUTH_DEBUG
	printf("dr_auth: druser->pw_uid = %s\n", druser->pw_uid);
	printf("dr_auth: druser->pw_gid = %s\n", druser->pw_gid);
#endif AUTH_DEBUG

	if (!(strcmp(aup->aup_machname, drhost)) &&
	    (aup->aup_uid == druser->pw_uid) &&
	    (aup->aup_gid == druser->pw_gid) &&
	    (aup->aup_len != 0) &&
	    (aup->aup_gids[0] == (gid_t)DR_MAGIC))
		return (1);
	else
		return (0);
#else SSP_PASSWORD
	if (!(strcmp(aup->aup_machname, drhost)) &&
	    (aup->aup_uid == (uid_t)DR_MAGIC) &&
	    (aup->aup_gid == (gid_t)DR_MAGIC) &&
	    (aup->aup_len != 0) &&
	    (aup->aup_gids[0] == (gid_t)DR_MAGIC))
		return (1);
	else
		return (0);
#endif SSP_PASSWORD
}
