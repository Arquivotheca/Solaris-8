/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)ami_utils.c	1.1 99/07/11 SMI"
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <thread.h>
#include <syslog.h>
#include <strings.h>
#include <rpc/rpc.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <security/pam_appl.h>

/* External AMI functions */
extern char *ami_keymgnt_get_default_rsa_signature_key_alias();
extern char *ami_keymgnt_get_default_rsa_encryption_key_alias();
extern char *ami_keymgnt_get_default_dsa_key_alias();
extern char *ami_keymgnt_get_default_dh_key_alias();
extern int ami_keymgnt_get_keystore(const char *, void **, int *);
extern int ami_keymgnt_set_keystore(const char *, const void *, int);
extern int __ami_rpc_set_keystore(CLIENT *cl, const char *username,
    const char *hostname, const char *hostip, long uid, const char *password,
    const char *rsaSignAlias, const char *rsaEncyAlias, const char *dsaAlias,
    int flag, const void *data, int data_len);
extern int __ami_rpc_change_keystore_password(CLIENT *cl, const char *old_keystore,
    int old_keystore_len, const char *password, void **keystore, int *keystore_len,
    const char *new_password);

#define NUM_ARGS_TO_AUTH 2

int
ami_attempt_authentication(long uid,	/* IN */
	char *userName,			/* IN */
	char *password,			/* IN */
	int   flags,                    /* IN */
	int   debug			/* IN */
)
{
	uid_t orig_uid, orig_euid;
	char *rsaSignAlias, *rsaEncyAlias, *dsaAlias;
	char hostname[MAXHOSTNAMELEN], host_buffer[1024], *host_ip;
	void *keystore;
	int keystore_len, host_error, status = PAM_AUTH_ERR;
	struct hostent *hp, host_result;
        struct in_addr  in;

	if (debug)
		syslog(LOG_DEBUG, "ami_attempt_authentication started\n");

	/*
	 * We need to do a setreuid to set uid to the user's uid
	 * since AMI routies assumes this to be set
	 */
	orig_uid = getuid();
	orig_euid = geteuid();

	if (debug)
		syslog(LOG_DEBUG, "AMI PAM module switching to "
		    "uid=%d, euid=%d", orig_uid, orig_euid);
	setreuid(uid, orig_euid);

	rsaSignAlias = ami_keymgnt_get_default_rsa_signature_key_alias();
	rsaEncyAlias = ami_keymgnt_get_default_rsa_encryption_key_alias();
	dsaAlias = ami_keymgnt_get_default_dsa_key_alias();
	if ((ami_keymgnt_get_keystore(NULL, &keystore, &keystore_len) != 0) ||
	    (keystore == NULL))
		goto auth_out;
	gethostname(hostname, MAXHOSTNAMELEN);
	if ((hp = gethostbyname_r(hostname, &host_result, host_buffer,
	    1024, &host_error)) == NULL)
		goto auth_out;
	memcpy(&in.s_addr, hp->h_addr_list[0], sizeof (in.s_addr));
	host_ip = inet_ntoa(in);

	if (__ami_rpc_set_keystore(NULL, userName, hostname, host_ip,
	    uid, password, rsaSignAlias, rsaEncyAlias, dsaAlias, 0,
	    keystore, keystore_len) != 0) {
		if (debug)		
			syslog(LOG_DEBUG, "AMI PAM module: "
			    "register keystore with amiserv failed\n");
		goto auth_out;
	}
	if (rsaSignAlias)
		free (rsaSignAlias);
	if (rsaEncyAlias)
		free (rsaEncyAlias);
	if (dsaAlias)
		free (dsaAlias);
	free (keystore);
	status = PAM_SUCCESS;

 auth_out:
	setreuid(orig_uid, orig_euid);
	if (debug) {
		syslog(LOG_DEBUG, "AMI PAM module: "
		    "Switching back to euid=%d before return\n", geteuid());
		syslog(LOG_DEBUG, "AMI PAM module: "
		    "ami_attempt_authentication ended\n");
	}
        return (status);
}

#define NUM_ARGS_TO_VERIFY	3

int
ami_attempt_chauthtok(long uid,	        /* IN */
	char *userName,			/* IN */
	char *oldPassword,		/* IN */
	char *newPassword,		/* IN */
	int   flags,                    /* IN */
	int   debug			/* IN */
)
{
	int old_ks_len, status = PAM_AUTHTOK_ERR;
	void *old_ks;
	uid_t orig_uid, orig_euid;

	if (debug)
		syslog(LOG_DEBUG, "AMI ami_attempt_chauthtok started\n");

	/*
	 * We must make sure the uid is the uid for the target user
	 * when the Java method is called. But don't change the
	 * euid since if orig_uid and orig_euid are both 0 (e.g.,
	 * when "login" gets here), setting euid to the user's uid
	 * would prevent us to restore the orig_uid and orig_euid.
	 */
	orig_uid = getuid();
	orig_euid = geteuid();
	setreuid(uid, orig_euid);

	if (debug)
	        syslog(LOG_DEBUG,
		       "Before calling change password routine");

	if ((ami_keymgnt_get_keystore(NULL, &old_ks, &old_ks_len) != 0) ||
	    (old_ks == NULL))
		goto chauth_out;

	/* Change the password */
	/* if (__ami_rpc_change_keystore_password(NULL, old_ks, old_ks_len,
	    oldPassword, &new_ks, &new_ks_len, newPassword) != 0) {
		free (old_ks);
		goto chauth_out;
	} */

	/* Save the new keystore */
	free (old_ks);
	/* if (ami_keymgnt_set_keystore(NULL, new_ks, new_ks_len) == 0)
		status = PAM_SUCCESS;
	free (new_ks);
	*/
	if (debug)
	        syslog(LOG_DEBUG,
		       "After calling change password routine");

 chauth_out:
	setreuid(orig_uid, orig_euid);
	return (status);
}
