/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LDAP_HEADERS_H
#define	_LDAP_HEADERS_H

#pragma ident   "@(#)ldap_headers.h 1.1     99/07/07 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_impl.h>
#include <syslog.h>
#include <pwd.h>
#include <shadow.h>
#include <lastlog.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libintl.h>
#include <signal.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <crypt.h>
#include <assert.h>
#include <nss_dbdefs.h>
#include <nsswitch.h>

#include "ns_sldap.h"

#define	bool_t  int

/*
 * Various useful files and string constants
 */
#define	PWADMIN		"/etc/default/passwd"
#define	LOGINADMIN	"/etc/default/login"
#define	LDAP_MSG	"(LDAP)"
#define	PASS_ATTR	"userpassword"
#define	PASSWORD_LEN	8

/*
 * Miscellaneous constants
 */
#define	MINLENGTH	6	/* minimum length for passwords */
#define	MAXLENGTH	8	/* maximum length for passwords */
#define	NUMCP		13	/* number of characters in encrypted password */
#define	MAX_CHANCES	3	/* 3 chances to enter new passwd */

/* define error messages */
#define	NULLSTRING	""

void
__free_msg(int num_msg, struct pam_message *msg);

void
__free_resp(int num_msg, struct pam_response *resp);

int
__display_errmsg(
	int (*conv_funp)(),
	int num_msg,
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE],
	void *conv_apdp
);

int
__get_authtok(
	int (*conv_funp)(),
	int num_msg,
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE],
	void *conv_apdp,
	struct pam_response	**ret_respp
);


/* LDAP specific functions */

int
__ldap_to_pamerror(int ldaperror);

/*
 * PAM_MSG macro for return of internationalized text
 */
extern char *__pam_get_i18n_msg(pam_handle_t *, char *, int, int, char *);

#define	PAM_MSG(pamh, number, string)\
	(char *) __pam_get_i18n_msg(pamh, "pam_ldap", 1, number, string)

/* from ldap_utils.c */

extern char	*repository_to_string(int);
extern int	ck_perm(pam_handle_t *, int, struct passwd **,
			struct spwd **, uid_t, int, int);
extern int 	authenticate(Auth_t **, char *, char *);
extern void	free_passwd_structs(struct passwd *, struct spwd *);

/* from update_password.c */

extern int	__update_passwd(pam_handle_t *, int,
				int, const char **);

#ifdef __cplusplus
}
#endif

#endif	/* _LDAP_HEADERS_H */
