/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)roles.c	1.3	99/09/16 SMI"

#include <syslog.h>
#include <pwd.h>
#include <unistd.h>
#include <strings.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <libintl.h>
#include <pwd.h>
#include <user_attr.h>
#include <secdb.h>

extern char *__pam_get_i18n_msg();
extern int  __pam_display_msg();
extern int strtok_r();
static int roleinlist();

#define	PAM_MSG(pamh, number, string)\
	(char *)__pam_get_i18n_msg(pamh, "pam_role", 1, number, string)

/*
 * pam_sm_acct_mgmt():
 *	Account management module
 *	This module disallows roles for primary logins and adds special
 *	checks to allow roles for secondary logins.
 */

int
pam_sm_acct_mgmt(
	pam_handle_t *pamh,
	int	flags,
	int	argc,
	const char **argv)
{
	uid_t uid;
	userattr_t *user_entry;
	userattr_t *user_entry1;
	char	*kva_value;
	char	*username;
	char	*ruser;
	char	*service;
	int error;
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];

	struct passwd *pw_entry;
	int i;
	int debug;

	if ((error = pam_get_item(pamh, PAM_USER, (void **)&username))
	    != PAM_SUCCESS)
		return (error);

	if ((error = pam_get_item(pamh, PAM_RUSER, (void **)&ruser))
	    != PAM_SUCCESS)
		return (error);

	if ((error = pam_get_item(pamh, PAM_SERVICE, (void **)&service))
	    != PAM_SUCCESS)
		return (error);

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
		else
			syslog(LOG_ERR,
			    "ROLE authenticate(): illegal module option %s",
			    argv[i]);
	}

	if (debug)
		syslog(LOG_DEBUG, "roles pam_sm_authenticate, "
			"service = %s user = %s ruser = %s\n",
			(service) ? service : "service name not set",
			(username) ? username : "username not set",
			(ruser) ? ruser: "ruser not set");

	if (username == NULL)
		return (PAM_USER_UNKNOWN);

	/*
	 * If there's no user_attr entry for the primary user or it's not a
	 * role, no further checks are needed.
	 */

	if (((user_entry1 = getusernam(username)) == NULL) ||
	    ((kva_value = kva_match((kva_t *)user_entry1->attr,
	    USERATTR_TYPE_KW)) == NULL) ||
	    ((strcmp(kva_value, USERATTR_TYPE_NONADMIN_KW) != 0) &&
	    (strcmp(kva_value, USERATTR_TYPE_ADMIN_KW) != 0))) {
		free_userattr(user_entry1);
		return (PAM_IGNORE);
	}

	if (ruser == NULL) {
		/*
		 * There is no PAM_RUSER set. This means it's a service
		 * like su or some other direct login service.
		 */
		uid = getuid();
		pw_entry = getpwuid(uid);
		user_entry = getusernam(pw_entry->pw_name);

	} else {
		/*
		 * PAM_RUSER is set. This is a service like rlogin, rsh.
		 * A role can login to the same role name. The roles field
		 * in the database won't have an entry for itself.
		 */
		if (strcmp(username, ruser) == 0) {
			free_userattr(user_entry1);
			return (PAM_IGNORE);
		}
		user_entry = getusernam(ruser);
	}

	/*
	 * If the original user does not have a user_attr entry or isn't
	 * assigned the role being assumed, fail.
	 */

	if ((user_entry == NULL) ||
	    ((kva_value = kva_match((kva_t *)user_entry->attr,
	    USERATTR_ROLES_KW)) == NULL) ||
	    (roleinlist(kva_value, username) == 0)) {
		free_userattr(user_entry1);
		free_userattr(user_entry);
		strcpy(messages[0], PAM_MSG(pamh, 1,
		    "Roles can only be assumed by authorized users"));
		__pam_display_msg(pamh, PAM_ERROR_MSG, 1, messages, NULL);
		return (PAM_PERM_DENIED);
	}

	free_userattr(user_entry1);
	free_userattr(user_entry);
	return (PAM_IGNORE);
}

int
roleinlist(char *list, char *role)
{
	char *lasts = (char *)NULL;
	char *rolename = (char *)strtok_r(list, ",", &lasts);

	while (rolename) {
		if (strcmp(rolename, role) == 0)
			return (1);
		else
			rolename = (char *)strtok_r(NULL, ",", &lasts);
	}
	return (0);
}
