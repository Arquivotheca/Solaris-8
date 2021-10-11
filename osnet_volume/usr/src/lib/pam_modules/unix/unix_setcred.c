/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)unix_setcred.c	1.8	98/06/14 SMI"

#include "unix_headers.h"

/*
 * pam_sm_setcred		- Set the process credentials
 *
 * XXX: If this module succeeds when invoked with PAM_ESTABLISH_CREd,
 *	and a subsequent module in the stack fails, then there is no way
 *	to delete the user's Secure RPC credentials (keylogout).  Currently
 *	there is no way for PAM to correct this problem.
 */
int
pam_sm_setcred(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	int i;
	int debug = 0;
	int err = 0;
	struct pam_conv	*pam_convp;
	char	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
#ifdef PAM_SECURE_RPC
	unix_auth_data *status;
	struct nfs_revauth_args nra;	/* revoking kernel NFS creds */
#endif

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcasecmp(argv[i], "nowarn") == 0)
			flags = flags | PAM_SILENT;
	}

	/* make sure flags are valid */
	if (flags &&
	    !(flags & PAM_ESTABLISH_CRED) &&
	    !(flags & PAM_REINITIALIZE_CRED) &&
	    !(flags & PAM_REFRESH_CRED) &&
	    !(flags & PAM_DELETE_CRED) &&
	    !(flags & PAM_SILENT)) {
		syslog(LOG_ERR, "unix setcred: illegal flag %d", flags);
		err = PAM_SYSTEM_ERR;
		goto out;
	}

	if (pam_get_item(pamh, PAM_CONV, (void **) &pam_convp) !=
						PAM_SUCCESS) {
		if (debug) {
		    syslog(LOG_DEBUG,
		    "pam_sm_setcred(): unable to get conv structure");
		}
		err = PAM_SERVICE_ERR;
		goto out;
	}

	if ((flags & PAM_REINITIALIZE_CRED) ||
	    (flags & PAM_REFRESH_CRED)) {
		/* For unix, these flags are not applicable */
		err = PAM_SUCCESS;
	} else if (flags & PAM_DELETE_CRED) {
#ifdef PAM_SECURE_RPC
		/* do a keylogout */
		if (geteuid() == 0) {
			if (!(flags & PAM_SILENT)) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 50,
	"removing root credentials would break the rpc services that"));
				snprintf(messages[1],
					sizeof (messages[1]),
					PAM_MSG(pamh, 51,
	"use secure rpc on this host!"));
				snprintf(messages[2],
					sizeof (messages[2]),
					PAM_MSG(pamh, 52,
	"root may use keylogout -f to do this (at your own risk)!"));

				__pam_display_msg(pamh, PAM_ERROR_MSG,
					3, messages, NULL);
			}
			err = PAM_PERM_DENIED;
			goto out;
		}

		if (key_removesecret_g() < 0) {
			if (!(flags & PAM_SILENT)) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 53,
					"Could not unset your secret key(s)."));
				snprintf(messages[1],
					sizeof (messages[1]),
					PAM_MSG(pamh, 54,
					"Maybe the keyserver is down?"));
				__pam_display_msg(pamh, PAM_ERROR_MSG,
					2, messages, NULL);
			}
			err = PAM_AUTH_ERR;
			goto out;
		}
		nra.authtype = AUTH_DES;	/* only revoke DES creds */
		nra.uid = getuid();		/* use the real uid */
		if (_nfssys(NFS_REVAUTH, &nra) < 0) {
			if (!(flags & PAM_SILENT)) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 55,
				"Warning: NFS credentials not destroyed"));
				__pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);
			}
			err = PAM_AUTH_ERR;
			goto out;
		}
#endif
		err = PAM_SUCCESS;
	} else {
#ifdef PAM_SECURE_RPC
		/*
		 * Default case:  PAM_ESTABLISH_CRED
		 * For unix, the keylogin was already done in
		 * unix_authenticate.c.  Only print out the
		 * keylogin status here.
		 */
		if (!(flags & PAM_SILENT)) {
			if (pam_get_data(pamh, UNIX_AUTH_DATA,
				(const void**)&status) == PAM_SUCCESS) {
				switch (status->key_status) {
				case ESTKEY_SUCCESS:
					if (debug)
						syslog(LOG_DEBUG,
						"unix setcred successful");
					/*FALLTHROUGH*/
				case ESTKEY_ALREADY:
				case ESTKEY_NOCREDENTIALS:
					break;
				case ESTKEY_BADPASSWD:
					if (!(flags & PAM_SILENT)) {
						snprintf(messages[0],
							sizeof (messages[0]),
							PAM_MSG(pamh, 56,
	"Password does not decrypt any secret keys for %s."),
						status->netname);
						__pam_display_msg(
							pamh,
							PAM_ERROR_MSG, 1,
							messages, NULL);
					}
				break;
				case ESTKEY_CANTSETKEY:
					if (!(flags & PAM_SILENT)) {
						snprintf(messages[0],
							sizeof (messages[0]),
							PAM_MSG(pamh, 57,
	"Could not set secret key(s) for %s. The key server may be down."),
						status->netname);
						__pam_display_msg(
							pamh,
							PAM_ERROR_MSG, 1,
							messages, NULL);
					}
				break;
				}
			} else {
				if (debug)
					syslog(LOG_DEBUG,
					"pam_sm_setcred(): no module data");
			}
		}
#endif
		err = PAM_SUCCESS;
	}
out:
	return (err);
}
