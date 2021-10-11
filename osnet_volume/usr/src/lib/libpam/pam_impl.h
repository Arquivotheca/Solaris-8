/*
 * Copyright (c) 1992-1995,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PAM_IMPL_H
#define	_PAM_IMPL_H

#pragma ident	"@(#)pam_impl.h	1.16	99/11/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <shadow.h>
#include <sys/types.h>

#define	PAMTXD		"SUNW_OST_SYSOSPAM"

#define	AUTH_LIB	"/usr/lib/libpam.a"
#define	PAM_CONFIG	"/etc/pam.conf"
#define	PAM_ISA		"/$ISA/"
#ifdef	__sparcv9
#define	PAM_LIB_DIR	"/usr/lib/security/sparcv9/"
#define	PAM_ISA_DIR	"/sparcv9/"
#else	/* !__sparcv9 */
#define	PAM_LIB_DIR	"/usr/lib/security/"
#define	PAM_ISA_DIR	"/"
#endif	/* __sparcv9 */

#define	PAM_AUTH_MODULE		0
#define	PAM_ACCOUNT_MODULE	1
#define	PAM_PASSWORD_MODULE	2
#define	PAM_SESSION_MODULE	3
#define	PAM_NUM_MODULE_TYPES	4

#define	PAM_REQUIRED	1	/* required flag in config file */
#define	PAM_OPTIONAL	2	/* optional flag in config file */
#define	PAM_SUFFICIENT	4	/* sufficient flag in config file */
#define	PAM_REQUISITE	8	/* requisite flag in config file */

/* XXX: Make sure this is correct in pam_appl.h */
#define	PAM_TOTAL_ERRNUM	28	/* total # PAM error numbers */

/* authentication module functions */
#define	PAM_SM_AUTHENTICATE	"pam_sm_authenticate"
#define	PAM_SM_SETCRED		"pam_sm_setcred"

/* session module functions */
#define	PAM_SM_OPEN_SESSION	"pam_sm_open_session"
#define	PAM_SM_CLOSE_SESSION	"pam_sm_close_session"

/* password module functions */
#define	PAM_SM_CHAUTHTOK		"pam_sm_chauthtok"

/* account module functions */
#define	PAM_SM_ACCT_MGMT		"pam_sm_acct_mgmt"

#define	PAM_MAX_ITEMS		64	/* Max number of items */

/* for modules when calling __pam_get_authtok() */
#define	PAM_PROMPT	1	/* prompt user for new password */
#define	PAM_HANDLE	2	/* get password from pam handle (item) */

/*
 * Definitions shared by passwd.c and the UNIX module
 */

#define	PAM_REP_DEFAULT	0x0
#define	PAM_REP_FILES	0x01
#define	PAM_REP_NIS	0x02
#define	PAM_REP_NISPLUS	0x04
#define	PAM_REP_LDAP	0x10
#define	PAM_OPWCMD	0x08	/* for nispasswd, yppasswd */
#define	IS_FILES(x)	((x & PAM_REP_FILES) == PAM_REP_FILES)
#define	IS_NIS(x)	((x & PAM_REP_NIS) == PAM_REP_NIS)
#define	IS_NISPLUS(x)	((x & PAM_REP_NISPLUS) == PAM_REP_NISPLUS)
#define	IS_LDAP(x)	((x & PAM_REP_LDAP) == PAM_REP_LDAP)
#define	IS_OPWCMD(x)	((x & PAM_OPWCMD) == PAM_OPWCMD)

/* max # of authentication token attributes */
#define	PAM_MAX_NUM_ATTR	10

/* max size (in chars) of an authentication token attribute */
#define	PAM_MAX_ATTR_SIZE	80

/* utility function prototypes */
extern void
__pam_free_resp(
	int num_msg,
	struct pam_response *resp
);

extern int
__pam_display_msg(
	pam_handle_t *pamh,
	int msg_style,
	int num_msg,
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE],
	void *conv_apdp
);

extern int
__pam_get_input(
	pam_handle_t *pamh,
	int msg_style,
	int num_msg,
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE],
	void *conv_apdp,
	struct pam_response **ret_respp
);

extern int
__pam_get_authtok(
	pam_handle_t *pamh,
	int source,
	int type,
	int len,
	char *prompt,
	char **authtok,
	void *conv_apdp
);

extern char *
__pam_get_i18n_msg(
	pam_handle_t *pamh,
	char *filename,
	int set,
	int n,
	char *string
);

/* file handle for pam.conf */
struct pam_fh {
	int	fconfig;	/* file descriptor returned by open() */
	char    line[256];
	size_t  bufsize;	/* size of the buffer which holds */
				/* the content of pam.conf */

	char   *bufferp;	/* used to process data	*/
	char   *data;		/* contents of pam.conf	*/
};

/* items that can be set/retrieved thru pam_[sg]et_item() */
struct	pam_item {
	void	*pi_addr;	/* pointer to item */
	int	pi_size;	/* size of item */
};

/* module specific data stored in the pam handle */
struct pam_module_data {
	char *module_data_name;		/* unique module data name */
	void *data;			/* the module specific data */
	void (*cleanup)(pam_handle_t *pamh, void *data, int pam_status);
	struct pam_module_data *next;	/* pointer to next module data */
};

/* each entry from pam.conf is stored here (in the pam handle) */
typedef struct pamtab {
	char	*pam_service;	/* PAM service, e.g. login, rlogin */
	int	pam_type;	/* AUTH, ACCOUNT, PASSWORD, SESSION */
	int	pam_flag;	/* required, optional, sufficient */
	char	*module_path;	/* module library */
	int	module_argc;	/* module specific options */
	char	**module_argv;
	void	*function_ptr;	/* pointer to struct holding function ptrs */
	struct pamtab *next;
} pamtab;

/* list of open fd's (modules that were dlopen'd) */
typedef struct fd_list {
	void *mh;		/* module handle */
	struct fd_list *next;
} fd_list;

/* list of PAM environment varialbes */
typedef struct env_list {
	char *name;
	char *value;
	struct env_list *next;
} env_list;

/* the pam handle */
struct pam_handle {
	struct  pam_item ps_item[PAM_MAX_ITEMS];	/* array of PAM items */
	pamtab	*pam_conf_info[PAM_NUM_MODULE_TYPES];	/* pam.conf info */
	struct	pam_module_data *ssd;		/* module specific data */
	fd_list *fd;				/* module fd's */
	env_list *pam_env;			/* environment variables */
	/* Version number requested by PAM's client */
	char	*pam_client_message_version_number;
};

/*
 * the function_ptr field in struct pamtab
 * will point to one of these modules
 */
struct auth_module {
	int			(*pam_sm_authenticate)(
					pam_handle_t *pamh,
					int flags,
					int argc,
					const char **argv);
	int			(*pam_sm_setcred)(
					pam_handle_t *pamh,
					int flags,
					int argc,
					const char **argv);
};

struct password_module {
	int			(*pam_sm_chauthtok)(
					pam_handle_t *pamh,
					int flags,
					int argc,
					const char **argv);
};

struct session_module {
	int			(*pam_sm_open_session)(
					pam_handle_t *pamh,
					int flags,
					int argc,
					const char **argv);
	int			(*pam_sm_close_session)(
					pam_handle_t *pamh,
					int flags,
					int argc,
					const char **argv);
};

struct account_module {
	int			(*pam_sm_acct_mgmt)(
					pam_handle_t *pamh,
					int flags,
					int argc,
					const char **argv);
};

#ifdef __cplusplus
}
#endif

#endif	/* _PAM_IMPL_H */
