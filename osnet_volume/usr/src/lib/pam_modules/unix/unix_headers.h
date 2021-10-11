/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UNIX_HEADERS_H
#define	_UNIX_HEADERS_H

#pragma	ident	"@(#)unix_headers.h	1.14	99/07/07 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
*******************************************************************
*
*	PROPRIETARY NOTICE(Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*		Copyright Notice
*
* Notice of copyright on this source code product does not indicate
* publication.
*
*	(c) 1986, 1987, 1988, 1989, 1990, 1991, 1992 Sun Microsystems, Inc
*	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
*		All rights reserved.
*******************************************************************
*/


/*
********************************************************************** *
*									*
*			Unix Scheme Header Files			*
*									*
********************************************************************** */

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

#ifdef PAM_LDAP
#include "ns_sldap.h"
#endif

#ifdef PAM_NISPLUS
#include <rpcsvc/nis.h>
#include <rpcsvc/nispasswd.h>
#endif

#if (PAM_NIS || PAM_NISPLUS)
#include <rpcsvc/yppasswd.h>
#include <rpcsvc/ypclnt.h>
#include <rpc/key_prot.h>
#include <rpc/rpc.h>
#include <nfs/nfs.h>
#include <nfs/nfssys.h>
#endif

#if (PAM_LDAP || PAM_NIS || PAM_NISPLUS)
#include <nss_dbdefs.h>
#include <nsswitch.h>
#endif

#ifdef PAM_SECURE_RPC
#include <rpcsvc/nis_dhext.h>
#endif

/*
 * Various useful files and string constants
 */
#define	SHELL		"/usr/bin/sh"
#define	DEFSHELL	"/bin/sh"
#define	LASTLOG		"/var/adm/lastlog"
#define	PWADMIN		"/etc/default/passwd"
#define	LOGINADMIN	"/etc/default/login"
#define	PASSWD		"/etc/passwd"
#define	SHADOW		"/etc/shadow"
#define	UNIX_AUTH_DATA		"SUNW-UNIX-AUTH-DATA"
#define	UNIX_AUTHTOK_DATA	"SUNW-UNIX-AUTHTOK-DATA"
#define	UNIX_MSG		"(SYSTEM)"
#define	NIS_MSG			"(NIS)"
#define	NISPLUS_MSG		"(NIS+)"
#define	LDAP_MSG		"(LDAP)"
#define	PASSWORD_LEN		8
#define	PAM_NISPLUS_PARTIAL_SUCCESS	-1

/*
 * PAM_MSG macro for return of internationalized text
 */

#define	PAM_MSG(pamh, number, string)\
	(char *) __pam_get_i18n_msg(pamh, "pam_unix", 1, number, string)

/*
 * Returned status codes for establish_key () utility function
 */

#define	ESTKEY_SUCCESS		0
#define	ESTKEY_NOCREDENTIALS	1
#define	ESTKEY_BADPASSWD		2
#define	ESTKEY_CANTSETKEY	3
#define	ESTKEY_ALREADY		4


/*
 * Miscellaneous constants
 */
#define	ROOTUID		0
#define	MINWEEKS	-1	/* minimum weeks before next password change */
#define	MAXWEEKS	-1	/* maximum weeks before password change */
#define	WARNWEEKS	-1	/* number weeks before password expires */
				/* to warn the user */
#define	MINLENGTH	6	/* minimum length for passwords */
#define	MAXLENGTH	8	/* maximum length for passwords */
#define	NUMCP		13	/* number of characters for valid password */
#define	MAX_CHANCES	3	/* 3 chances to enter new passwd */

/*
 * variables declarations
 */
mutex_t _priv_lock;

/*
 * nis+ definition
 */
#define	PKTABLE		"cred.org_dir"
#define	PKTABLELEN	12
#define	PASSTABLE	"passwd.org_dir"
#define	PASSTABLELEN	14
#define	PKMAP		"publickey.byname"

/* define error messages */
#define	NULLSTRING	""

/*
 * This extra definition is needed in order to build this library
 * on pre-64-bit-aware systems.
 */
#if !defined(_LFS64_LARGEFILE)
#define	stat64	stat
#endif	/* !defined(_LFS64_LARGEFILE) */

/*
 * Function Declarations
 */
extern int		defopen(char *);
extern char		*defread(char *);
extern int		key_setnet(struct key_netstarg *);
extern int		setusershell();
extern int		_nfssys(int, void *);

#ifdef PAM_NISPLUS
/* from npd_client.c */
extern nispasswd_status	nispasswd_auth(char *, char *, char *,
					u_char *, char *, keylen_t, algtype_t,
					des_block *, CLIENT *,
					uint32_t *, uint32_t *, int *);

extern int		nispasswd_pass(CLIENT *, uint32_t, uint32_t,
					des_block *,
					char *, char *, char *, int *,
					nispasswd_error **);

extern bool_t		npd_makeclnthandle(char *, CLIENT **, char **,
					keylen_t *, algtype_t *, char **);

extern void		__npd_free_errlist(nispasswd_error *);
#endif

/* from unix_utils.c */
extern int		ck_perm(pam_handle_t *, int,
				char *, struct passwd **, struct spwd **,
				int *, void **, uid_t, int, int);
extern char		*attr_match(register char *, register char *);
extern char		*getloginshell(pam_handle_t *, char *, int, int);
extern char		*gethomedir(pam_handle_t *, char *, int);
extern char		*getfingerinfo(pam_handle_t *, char *, int);
extern char		*repository_to_string(int);
extern void		free_passwd_structs(struct passwd *, struct spwd *);
extern void		nisplus_populate_age(struct nis_object *,
				struct spwd *);
extern int		attr_find(char *, char *[]);
extern void		setup_attr(char *[], int, char[], char[]);
extern void		free_setattr(char *[]);
extern int		__set_authtoken_attr(pam_handle_t *, const char **,
						int, const char *, int,
						const char **);

/* from switch_utils.c */
extern int		str2spwd(const char *, int, void *, char *, int);
extern struct passwd    *getpwnam_from(const char *, int);
extern struct spwd	*getspnam_from(const char *, int);
extern int		get_ns(pam_handle_t *, int, int, int);

/* from unix_update_authtok.c */
extern int		__update_authtok(pam_handle_t *, int, int,
					char *, int, const char **);

/* from update_authtok_<repository> */
extern int	update_authtok_file(pam_handle_t *, char *, char **,
				struct passwd *, int, int);
#ifdef PAM_LDAP
extern int	update_authtok_ldap(pam_handle_t *, char *, char **, char *,
				    char *, struct passwd *, int, int, int);
#endif
#ifdef PAM_NIS
extern int	update_authtok_nis(pam_handle_t *, char *, char **,
				char *, char *,
				struct passwd *, int, int);
extern bool_t	__nis_isadmin(char *, char *, char *);
#endif
#ifdef PAM_NISPLUS
extern int	update_authtok_nisplus(pam_handle_t *, char *, char *,
				char **, char *, char *,
				char *, int, struct passwd *, int,
				nis_result *, nis_result *, int, int, bool_t);
#endif

#ifdef PAM_SECURE_RPC
extern int		establish_key(pam_handle_t *, uid_t, char *, int,
					char *, int);

typedef struct _unix_auth_data_ {
	int key_status;
	char netname[MAXNETNAMELEN+1];
}unix_auth_data;
#endif

typedef struct _unix_authtok_data_ {
	int age_status;
}unix_authtok_data;

/*
 * Support for SunOS password aging
 */
int decode_passwd_aging(char *, int *, int *, int *);

#ifdef __cplusplus
}
#endif

#endif	/* _UNIX_HEADERS_H */
