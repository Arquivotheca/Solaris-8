/*
 * Copyright (c) 1992-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <syslog.h>

#ifdef sun
#pragma	ident	"@(#)pam_framework.c	1.23	99/11/06 SMI"
#include <dlfcn.h>
#endif

#ifdef   hpV4
#include <dl.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <strings.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <sys/mman.h>
#include "pam_impl.h"
#include "pam_loc.h"

#ifdef sun
extern void	*_dlopen(const char *, int);
extern int	_dlclose(void *);
extern void	*_dlsym(void *, const char *);
extern char	*_dlerror();

#define	dlopen 	_dlopen
#define	dlclose _dlclose
#define	dlsym 	_dlsym
#define	dlerror _dlerror
#endif

/*	PAM debugging	*/
#define	PAM_DEBUG	"/etc/pam_debug"

/*
 * This extra definition is needed in order to build this library
 * on pre-64-bit-aware systems.
 */
#if !defined(_LFS64_LARGEFILE)
#define	stat64	stat
#endif	/* !defined(_LFS64_LARGEFILE) */

static int	pam_debug = 0;

/* functions to dynamically load modules */
static int		load_modules(pam_handle_t *, int, char *);
#ifdef sun
static void 		*open_module(pam_handle_t *, char *);
static int		load_function(void *, char *, int (**func)());
#endif
#ifdef hpV4
static shl_t		open_module(char *);
static int		load_function(shl_t, char *, int (**func)());
#endif

/* functions to read and store the pam.conf configuration file */
static int		open_pam_conf(struct pam_fh **);
static void		close_pam_conf(struct pam_fh *);
static int		read_pam_conf(pam_handle_t *);
static int 		get_pam_conf_entry(struct pam_fh *, pamtab **);
static char		*read_next_token(char **);
static char		*nextline(struct pam_fh *);
static int		 verify_pam_conf(pamtab *, char *);

/* functions to clean up and free memory */
static void		clean_up(pam_handle_t *);
static void		free_pamconf(pamtab *);
static void		free_pam_conf_info(pam_handle_t *);
static void		free_env(env_list *);

/*
 *			pam_XXXXX routines
 *
 *	These are the entry points to the authentication switch
 */

/*
 * pam_start		- initiate an authentication transaction and
 *			  set parameter values to be used during the
 *			  transaction
 */

int
pam_start(
	const char *service,
	const char *user,
	const struct pam_conv	*pam_conv,
	pam_handle_t **pamh)
{
	struct	stat64	statbuf;
	int	err;

	/*
	 * turn on PAM debug if "magic" file exists
	 * if exists (original), pam_debug = 1.
	 * if has contents, pam_debug = contents.
	 *
	 * pam_debug =    1, log traditional (original) debugging.
	 *		> 1, log pam_get_item.
	 *		> 2, log module return status.
	 *		> 3, log pam.conf parsing.
	 */
	if (stat64(PAM_DEBUG, &statbuf) == 0) {
		int	debugfd;

		pam_debug = 1;
		openlog("PAM", LOG_CONS|LOG_NDELAY, LOG_AUTH);
		if ((debugfd = open(PAM_DEBUG, O_RDONLY)) >= 0) {
			char	buf[80];

			if (read(debugfd, buf, 80) > 1) {
				pam_debug = atoi(buf);
			}
			(void) close(debugfd);
		}
	}

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_start(%s %s) - debug = %d",
			service, (user)?user:"no-user", pam_debug);

	*pamh = (struct pam_handle *)calloc(1, sizeof (struct pam_handle));
	if (*pamh == NULL)
		return (PAM_BUF_ERR);

	if ((err = pam_set_item(*pamh, PAM_SERVICE, (void *)service))
		    != PAM_SUCCESS) {
		clean_up(*pamh);
		*pamh = NULL;
		return (err);
	}

	if ((err = pam_set_item(*pamh, PAM_USER, (void *) user))
			!= PAM_SUCCESS) {
		clean_up(*pamh);
		*pamh = NULL;
		return (err);
	}

	if ((err = pam_set_item(*pamh, PAM_CONV, (void *) pam_conv))
	    != PAM_SUCCESS) {
		clean_up(*pamh);
		*pamh = NULL;
		return (err);
	}

	/* read all the entries from pam.conf */
	if ((err = read_pam_conf (*pamh)) != PAM_SUCCESS) {
		clean_up (*pamh);
		*pamh = NULL;
		return (err);
	}

	return (PAM_SUCCESS);
}

/*
 * pam_end - terminate an authentication transaction
 */

int
pam_end(pam_handle_t *pamh, int pam_status)
{
	struct pam_module_data *psd, *p;
	fd_list *expired;
	fd_list *traverse;
	env_list *env_expired;
	env_list *env_traverse;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_end(): status = %s",
			pam_strerror(pamh, pam_status));

	if (pamh == NULL)
		return (PAM_SYSTEM_ERR);

	/* call the cleanup routines for module specific data */

	psd = pamh->ssd;
	while (psd) {
		if (psd->cleanup) {
			psd->cleanup(pamh, psd->data, pam_status);
		}
		p = psd;
		psd = p->next;
		free(p->module_data_name);
		free(p);
	}
	pamh->ssd = NULL;

	/* dlclose all module fds */
	traverse = pamh->fd;
	while (traverse) {
		expired = traverse;
		traverse = traverse->next;
		dlclose(expired->mh);
		free(expired);
	}
	pamh->fd = 0;

	/* remove all environment variables */
	env_traverse = pamh->pam_env;
	while (env_traverse) {
		env_expired = env_traverse;
		env_traverse = env_traverse->next;
		free_env(env_expired);
	}

	clean_up(pamh);

	/*  end syslog reporting  */

	if (pam_debug)
		closelog();

	return (PAM_SUCCESS);
}

/*
 * pam_set_item		- set the value of a parameter that can be
 *			  retrieved via a call to pam_get_item()
 */

int
pam_set_item(
	pam_handle_t 	*pamh,
	int 		item_type,
	const void 	*item)
{
	struct pam_item *pip;
	int	size;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_set_item(%d)", item_type);

	if (pamh == NULL)
		return (PAM_SYSTEM_ERR);

	/*
	 * Check if tag is Sun proprietary
	 */
	if (item_type == PAM_MSG_VERSION) {
		if (pamh->pam_client_message_version_number)
			free(pamh->pam_client_message_version_number);

		if (item == NULL)
			pamh->pam_client_message_version_number = NULL;
		else
			if ((pamh->pam_client_message_version_number =
			    strdup((char *)item)) == NULL)
				return (PAM_BUF_ERR);
		return (PAM_SUCCESS);
	}

	/*
	 * Check that item_type is within valid range
	 */

	if (item_type <= 0 || item_type >= PAM_MAX_ITEMS)
		return (PAM_SYMBOL_ERR);

	pip = &(pamh->ps_item[item_type]);

	switch (item_type) {
		case PAM_AUTHTOK:
		case PAM_OLDAUTHTOK:
			if (pip->pi_addr != NULL)
				memset(pip->pi_addr, 0, pip->pi_size);
			/*FALLTHROUGH*/
		case PAM_SERVICE:
		case PAM_USER:
		case PAM_TTY:
		case PAM_RHOST:
		case PAM_RUSER:
		case PAM_USER_PROMPT:
			if (pip->pi_addr != NULL) {
				free(pip->pi_addr);
			}

			if (item == NULL) {
				pip->pi_addr = NULL;
				pip->pi_size = 0;
			} else {
				pip->pi_addr = strdup((char *)item);
				if (pip->pi_addr == NULL) {
					pip->pi_size = 0;
					return (PAM_BUF_ERR);
				}
				pip->pi_size = strlen(pip->pi_addr);
			}
			break;
		case PAM_CONV:
			if (pip->pi_addr != NULL)
				free(pip->pi_addr);
			size = sizeof (struct pam_conv);
			if ((pip->pi_addr = (void *) calloc(1, size)) == NULL)
				return (PAM_BUF_ERR);
			if (item != NULL)
				(void) memcpy(pip->pi_addr, item,
						(unsigned int) size);
			else
				memset(pip->pi_addr, 0, size);
			pip->pi_size = size;
			break;
		default:
			return (PAM_SYMBOL_ERR);
	}

	return (PAM_SUCCESS);
}

/*
 * pam_get_item		- read the value of a parameter specified in
 *			  the call to pam_set_item()
 */

int
pam_get_item(
	const pam_handle_t	*pamh,
	int			item_type,
	void			**item)
{
	struct pam_item *pip;

	if (pam_debug > 1)
		syslog(LOG_DEBUG, "pam_get_item(%d)", item_type);

	if (pamh == NULL)
		return (PAM_SYSTEM_ERR);

	/*
	 * Check if tag is Sun proprietary
	 */
	if (item_type == PAM_MSG_VERSION) {
		*item = pamh->pam_client_message_version_number;
		return (PAM_SUCCESS);
	}

	if (item_type <= 0 || item_type >= PAM_MAX_ITEMS)
		return (PAM_SYMBOL_ERR);

	pip = (struct pam_item *)&(pamh->ps_item[item_type]);

	*item = pip->pi_addr;

	return (PAM_SUCCESS);
}

/*
 * parse_user_name         - process the user response: ignore
 *                           '\t' or ' ' before or after a user name.
 *                           user_input is a null terminated string.
 *                           *ret_username will be the user name.
 */

static int
parse_user_name(char *user_input, char **ret_username)
{
	register char *ptr;
	register int index = 0;
	char username[PAM_MAX_RESP_SIZE];

	/* Set the default value for *ret_username */
	*ret_username = NULL;

	/*
	 * Set the initial value for username - this is a buffer holds
	 * the user name.
	 */
	bzero((void *)username, PAM_MAX_RESP_SIZE);

	/*
	 * The user_input is guaranteed to be terminated by a null character.
	 */
	ptr = user_input;

	/* Skip all the leading whitespaces if there are any. */
	while ((*ptr == ' ') || (*ptr == '\t'))
		ptr++;

	if (*ptr == '\0') {
		/*
		 * We should never get here since the user_input we got
		 * in pam_get_user() is not all whitespaces nor just "\0".
		 */
		return (PAM_BUF_ERR);
	}

	/*
	 * username will be the first string we get from user_input
	 * - we skip leading whitespaces and ignore trailing whitespaces
	 */
	while (*ptr != '\0') {
		if ((*ptr == ' ') || (*ptr == '\t'))
			break;
		else {
			username[index] = *ptr;
			index++;
			ptr++;
		}
	}

	/* ret_username will be freed in pam_get_user(). */
	if ((*ret_username = (char *)malloc((index + 1)*(sizeof (char))))
	    == NULL)
		return (PAM_BUF_ERR);
	strcpy(*ret_username, username);
	return (PAM_SUCCESS);
}

/*
 * Get the value of PAM_USER. If not set, then use the convenience function
 * to prompt for the user. Use prompt if specified, else use PAM_USER_PROMPT
 * if it is set, else use default.
 */
#define	WHITESPACE	0
#define	USERNAME	1

int
pam_get_user(
	pam_handle_t *pamh,		/* PAM handle */
	char **user, 			/* User Name */
	const char *prompt_override)	/* Prompt */
{
	int	status;
	char	*prompt = NULL;
	char    *real_username;

	struct pam_response *ret_resp = (struct pam_response *)0;
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];

	if (pamh == NULL)
		return (PAM_SYSTEM_ERR);

	if ((status = pam_get_item(pamh, PAM_USER, (void **)user))
							!= PAM_SUCCESS) {
		return (status);
	}

	/* if the user is set, return it */

	if (*user != NULL && *user[0] != '\0') {
		return (PAM_SUCCESS);
	}

	/*
	 * if the module is requesting a special prompt, use it.
	 * else use PAM_USER_PROMPT.
	 */

	if (prompt_override != NULL) {
		prompt = (char *)prompt_override;
	} else {
		status = pam_get_item(pamh, PAM_USER_PROMPT, (void**)&prompt);
		if (status != PAM_SUCCESS) {
			return (status);
		}
	}

	/* if the prompt is not set, use default */

	if (prompt == NULL || prompt[0] == '\0') {
		prompt = PAM_MSG(pamh, 32, "Please enter user name: ");
	}

	/* prompt for the user */

	strncpy(messages[0], prompt, sizeof (messages[0]));

	for (;;) {
		int state = WHITESPACE;

		status = __pam_get_input(pamh, PAM_PROMPT_ECHO_ON, 1,
			messages, NULL, &ret_resp);

		if (status != PAM_SUCCESS) {
			return (status);
		}

		if (ret_resp->resp && ret_resp->resp[0] != '\0') {
			int len = strlen(ret_resp->resp);
			int i;

			for (i = 0; i < len; i++) {
				if ((ret_resp->resp[i] != ' ') &&
					(ret_resp->resp[i] != '\t')) {
					state = USERNAME;
					break;
				}
			}

			if (state == USERNAME)
				break;
		}
	}

	/* set PAM_USER */
	/* Parse the user input to get the user name. */
	status = parse_user_name(ret_resp->resp, &real_username);

	if (status != PAM_SUCCESS) {
		if (real_username != NULL)
			free(real_username);
		__pam_free_resp(1, ret_resp);
		return (status);
	}

	status = pam_set_item(pamh, PAM_USER, real_username);

	free(real_username);

	__pam_free_resp(1, ret_resp);
	if (status != PAM_SUCCESS) {
		return (status);
	}

	/*
	 * finally, get PAM_USER. We have to call pam_get_item to get
	 * the value of user because pam_set_item mallocs the memory.
	 */

	status = pam_get_item(pamh, PAM_USER, (void**)user);
	return (status);
}

/*
 * Set module specific data
 */
pam_set_data(
	pam_handle_t *pamh,		/* PAM handle */
	const char *module_data_name,	/* unique module data name */
	void *data,			/* the module specific data */
	void (*cleanup)(pam_handle_t *pamh, void *data, int pam_end_status)
)
{
	struct pam_module_data *psd;

	if (pamh == NULL || module_data_name == NULL)
		return (PAM_SYSTEM_ERR);

	/* check if module data already exists */

	for (psd = pamh->ssd; psd; psd = psd->next) {
		if (strcmp(psd->module_data_name, module_data_name) == 0) {
			/* clean up original data before setting the new data */
			if (psd->cleanup) {
				psd->cleanup(pamh, psd->data, PAM_SUCCESS);
			}
			psd->data = (void *)data;
			psd->cleanup = cleanup;
			return (PAM_SUCCESS);
		}
	}

	psd = malloc(sizeof (struct pam_module_data));
	if (psd == NULL)
		return (PAM_BUF_ERR);

	psd->module_data_name = strdup(module_data_name);
	if (psd->module_data_name == NULL) {
		free(psd);
		return (PAM_BUF_ERR);
	}

	psd->data = (void *)data;
	psd->cleanup = cleanup;
	psd->next = pamh->ssd;
	pamh->ssd = psd;
	return (PAM_SUCCESS);
}

/*
 * get module specific data
 */

int
pam_get_data(
	const pam_handle_t *pamh,
	const char *module_data_name,
	const void **data
)
{
	struct pam_module_data *psd;

	if (pamh == NULL)
		return (PAM_SYSTEM_ERR);

	for (psd = pamh->ssd; psd; psd = psd->next) {
		if (strcmp(psd->module_data_name, module_data_name) == 0) {
			*data = psd->data;
			return (PAM_SUCCESS);
		}
	}

	return (PAM_NO_MODULE_DATA);
}

/*
 * PAM error strings
 *
 * XXX: Make sure these match the errors in pam_appl.h !!!!!!!
 */

static char *pam_error_strings [PAM_TOTAL_ERRNUM] = {
/* PAM_SUCCESS */	"Success",
/* PAM_OPEN_ERR */	"Dlopen failure",
/* PAM_SYMBOL_ERR */	"Symbol not found",
/* PAM_SERVICE_ERR */	"Error in underlying service module",
/* PAM_SYSTEM_ERR */	"System error",
/* PAM_BUF_ERR */	"Memory buffer error",
/* PAM_CONV_ERR */	"Conversation failure",
/* PAM_PERM_DENIED */	"Permission denied",
/* PAM_MAXTRIES */	"Maximum number of attempts exceeded",
/* PAM_AUTH_ERR */	"Authentication failed",
/* PAM_NEW_AUTHTOK_REQD */	"Get new authentication token",
/* PAM_CRED_INSUFFICIENT */	"Insufficient credentials",
/* PAM_AUTHINFO_UNAVAIL */	"Can not retrieve authentication info",
/* PAM_USER_UNKNOWN */		"No account present for user",
/* PAM_CRED_UNAVAIL */		"Can not retrieve user credentials",
/* PAM_CRED_EXPIRED */		"User credentials have expired",
/* PAM_CRED_ERR */		"Failure setting user credentials",
/* PAM_ACCT_EXPIRED */		"User account has expired",
/* PAM_AUTHTOK_EXPIRED */	"User password has expired",
/* PAM_SESSION_ERR */		"Can not make/remove entry for session",
/* PAM_AUTHTOK_ERR */		"Authentication token manipulation error",
/* PAM_AUTHTOK_RECOVERY_ERR */	"Authentication token can not be recovered",
/* PAM_AUTHTOK_LOCK_BUSY */	"Authentication token lock busy",
/* PAM_AUTHTOK_DISABLE_AGING */	"Authentication token aging disabled",
/* PAM_NO_MODULE_DATA */	"Module specific data not found",
/* PAM_IGNORE */		"Ignore module",
/* PAM_ABORT */			"General PAM failure ",
/* PAM_TRY_AGAIN */		"Password update failed - Try again "
};

/*
 * PAM equivalent to strerror()
 */
const char *
pam_strerror(pam_handle_t *pamh, int errnum)
{
	if (errnum < 0 || errnum >= PAM_TOTAL_ERRNUM)
	    return (PAM_MSG(pamh, PAM_TOTAL_ERRNUM, "Unknown error"));
	else
	    return (PAM_MSG(pamh, errnum, (char *)pam_error_strings[errnum]));
}

/*
 * pam_authenticate - authenticate a user
 */

int
pam_authenticate(
	pam_handle_t	*pamh,
	int	flags)
{
	int			error = PAM_AUTH_ERR;
	int			first_error = PAM_AUTH_ERR;
	int			first_required_error = PAM_AUTH_ERR;
	int			required_module_failed = 0;
	int			optional_module_failed = 0;
	int			success = 0;
	pamtab			*modulep;
	struct auth_module	*authp;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_authenticate()");

	if ((error = load_modules(pamh, PAM_AUTH_MODULE, PAM_SM_AUTHENTICATE))
						!= PAM_SUCCESS) {
		if (pam_debug)
			syslog(LOG_DEBUG,
			    "pam_authenticate: load_modules failed");
		return (error);
	}

	modulep = pamh->pam_conf_info[PAM_AUTH_MODULE];
	while (modulep) {
		authp = (struct auth_module *)(modulep->function_ptr);
		if (authp && authp->pam_sm_authenticate) {
			error = authp->pam_sm_authenticate(pamh, flags,
				    modulep->module_argc,
				    (const char **)modulep->module_argv);

			if (pam_debug > 2)
				syslog(LOG_DEBUG,
				    "%s returned %s", modulep->module_path,
				    pam_strerror(pamh, error));

			switch (error) {
			case PAM_IGNORE:
				/* do nothing */
				break;
			case PAM_SUCCESS:
				if ((modulep->pam_flag & PAM_SUFFICIENT) &&
				    !required_module_failed) {
					pam_set_item(pamh,
							PAM_AUTHTOK,
							NULL);
					return (PAM_SUCCESS);
				}
				success = 1;
				break;
			default:
				if (modulep->pam_flag & PAM_REQUISITE) {
					pam_set_item(pamh, PAM_AUTHTOK, NULL);
					return (required_module_failed ?
						first_required_error :
						error);
				} else if (modulep->pam_flag & PAM_REQUIRED) {
					if (!required_module_failed)
						first_required_error = error;
					required_module_failed++;
				} else {
					if (!optional_module_failed)
						first_error = error;
					optional_module_failed++;
				}
				syslog(LOG_DEBUG,
					"pam_authenticate: error %s",
						pam_strerror(pamh, error));
				break;
			}
		}
		modulep = modulep->next;
	}

	/* this will memset the password memory to 0 */
	pam_set_item(pamh, PAM_AUTHTOK, NULL);

	if (required_module_failed)
		return (first_required_error);
	else if (success == 0)
		return (first_error);
	else
		return (PAM_SUCCESS);

}

/*
 * pam_setcred - modify or retrieve user credentials
 */
int
pam_setcred(
	pam_handle_t	*pamh,
	int	flags)
{
	int			error = PAM_CRED_ERR;
	int			first_error = PAM_CRED_ERR;
	int			first_required_error = PAM_CRED_ERR;
	int			required_module_failed = 0;
	int			optional_module_failed = 0;
	int			success = 0;
	pamtab			*modulep;
	struct auth_module	*authp;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_setcred()");

	if ((error = load_modules(pamh, PAM_AUTH_MODULE, PAM_SM_SETCRED))
						!= PAM_SUCCESS) {
		if (pam_debug)
			syslog(LOG_DEBUG, "pam_setcred: load_modules failed");
		return (error);
	}

	modulep = pamh->pam_conf_info[PAM_AUTH_MODULE];
	while (modulep) {
		authp = (struct auth_module *)(modulep->function_ptr);
		if (authp && authp->pam_sm_setcred) {
			error = authp->pam_sm_setcred(pamh, flags,
				modulep->module_argc,
				(const char **) modulep->module_argv);

			if (pam_debug > 2)
				syslog(LOG_DEBUG,
				    "%s returned %s", modulep->module_path,
				    pam_strerror(pamh, error));

			switch (error) {
			case PAM_IGNORE:
				/* do nothing */
				break;
			case PAM_SUCCESS:
				/*
				 * pam_setcred() should not look at the
				 * SUFFICIENT flag because it is only
				 * applicable to pam_authenticate().
				 */
				success = 1;
				break;
			default:
				if (modulep->pam_flag & PAM_REQUISITE) {
					return (required_module_failed ?
						first_required_error :
						error);
				} else if (modulep->pam_flag & PAM_REQUIRED) {
					if (!required_module_failed)
						first_required_error = error;
					required_module_failed++;
				} else {
					if (!optional_module_failed)
						first_error = error;
					optional_module_failed++;
				}
				syslog(LOG_DEBUG,
					"pam_setcred: error %s",
						pam_strerror(pamh, error));
				break;
			}
		}
		modulep = modulep->next;
	}

	if (required_module_failed)
		return (first_required_error);
	else if (success == 0)
		return (first_error);
	else
		return (PAM_SUCCESS);

}

/*
 * pam_acct_mgmt - check password aging, account expiration
 */

int
pam_acct_mgmt(
	pam_handle_t	*pamh,
	int 	flags)
{
	int			error = PAM_ACCT_EXPIRED;
	int			first_error = PAM_ACCT_EXPIRED;
	int			first_required_error = PAM_ACCT_EXPIRED;
	int			required_module_failed = 0;
	int			optional_module_failed = 0;
	int			success = 0;
	pamtab			*modulep;
	struct account_module	*accountp;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_acct_mgmt()");

	if ((error = load_modules(pamh, PAM_ACCOUNT_MODULE, PAM_SM_ACCT_MGMT))
						!= PAM_SUCCESS) {
		if (pam_debug)
			syslog(LOG_DEBUG,
			    "pam_acct_mgmt: load_modules failed");
		return (error);
	}

	modulep = pamh->pam_conf_info[PAM_ACCOUNT_MODULE];
	while (modulep) {
		accountp = (struct account_module *)(modulep->function_ptr);
		if (accountp && accountp->pam_sm_acct_mgmt) {
			error = accountp->pam_sm_acct_mgmt(pamh, flags,
			    modulep->module_argc,
			    (const char **) modulep->module_argv);

			if (pam_debug > 2)
				syslog(LOG_DEBUG,
				    "%s returned %s", modulep->module_path,
				    pam_strerror(pamh, error));

			switch (error) {
			case PAM_IGNORE:
				/* do nothing */
				break;
			case PAM_SUCCESS:
				if ((modulep->pam_flag & PAM_SUFFICIENT) &&
				    !required_module_failed)
					return (PAM_SUCCESS);
				success = 1;
				break;
			default:
				/*
				 * XXX
				 * Hardcode forced passwd change for sufficient
				 * module.  If a required module had failed
				 * prior to the sufficient module, return the
				 * original error.  Otherwise, return
				 * PAM_NEW_AUTHTOK_REQD.
				 */
				if ((modulep->pam_flag & PAM_SUFFICIENT) &&
				    error == PAM_NEW_AUTHTOK_REQD) {
					if (!required_module_failed)
						first_required_error = error;
					required_module_failed++;
				} else if (modulep->pam_flag & PAM_REQUISITE) {
					return (required_module_failed ?
						first_required_error :
						error);
				} else if (modulep->pam_flag & PAM_REQUIRED) {
					if (!required_module_failed)
						first_required_error = error;
					required_module_failed++;
				} else {
					if (!optional_module_failed)
						first_error = error;
					optional_module_failed++;
				}
				syslog(LOG_DEBUG,
					"pam_acct_mgmt: error %s",
					pam_strerror(pamh, error));
				break;
			}
		}
		modulep = modulep->next;
	}

	if (required_module_failed)
		return (first_required_error);
	else if (success == 0)
		return (first_error);
	else
		return (PAM_SUCCESS);

}

/*
 * pam_open_session - begin session management
 */

int
pam_open_session(
	pam_handle_t    *pamh,
	int	flags)
{
	int			error = PAM_SESSION_ERR;
	int			first_error = PAM_SESSION_ERR;
	int			first_required_error = PAM_SESSION_ERR;
	int			required_module_failed = 0;
	int			optional_module_failed = 0;
	int			success = 0;
	pamtab			*modulep;
	struct session_module	*sessionp;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_open_session()");

	if ((error = load_modules(pamh, PAM_SESSION_MODULE,
				PAM_SM_OPEN_SESSION)) != PAM_SUCCESS) {
		if (pam_debug)
			syslog(LOG_DEBUG,
			    "pam_open_session: load_modules failed");
		return (error);
	}

	modulep = pamh->pam_conf_info[PAM_SESSION_MODULE];
	while (modulep) {
		sessionp = (struct session_module *)(modulep->function_ptr);
		if (sessionp && sessionp->pam_sm_open_session) {
			error = sessionp->pam_sm_open_session(pamh, flags,
				modulep->module_argc,
				(const char **) modulep->module_argv);

			if (pam_debug > 2)
				syslog(LOG_DEBUG,
				    "%s returned %s", modulep->module_path,
				    pam_strerror(pamh, error));

			switch (error) {
			case PAM_IGNORE:
				/* do nothing */
				break;
			case PAM_SUCCESS:
				if ((modulep->pam_flag & PAM_SUFFICIENT) &&
				    !required_module_failed)
					return (PAM_SUCCESS);
				success = 1;
				break;
			default:
				if (modulep->pam_flag & PAM_REQUISITE) {
					return (required_module_failed ?
						first_required_error :
						error);
				} else if (modulep->pam_flag & PAM_REQUIRED) {
					if (!required_module_failed)
						first_required_error = error;
					required_module_failed++;
				} else {
					if (!optional_module_failed)
						first_error = error;
					optional_module_failed++;
				}
				syslog(LOG_DEBUG,
					"pam_open_session: error %s",
						pam_strerror(pamh, error));
				break;
			}
		}
		modulep = modulep->next;
	}

	if (required_module_failed)
		return (first_required_error);
	else if (success == 0)
		return (first_error);
	else
		return (PAM_SUCCESS);

}

/*
 * pam_close_session - terminate session management
 */

int
pam_close_session(
	pam_handle_t	*pamh,
	int	flags)
{
	int			error = PAM_SESSION_ERR;
	int			first_error = PAM_SESSION_ERR;
	int			first_required_error = PAM_SESSION_ERR;
	int			required_module_failed = 0;
	int			optional_module_failed = 0;
	int			success = 0;
	pamtab			*modulep;
	struct session_module	*sessionp;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_close_session()");

	if ((error = load_modules(pamh, PAM_SESSION_MODULE,
				PAM_SM_CLOSE_SESSION)) != PAM_SUCCESS) {
		if (pam_debug)
			syslog(LOG_DEBUG,
			    "pam_close_session: load_modules failed");
		return (error);
	}

	modulep = pamh->pam_conf_info[PAM_SESSION_MODULE];
	while (modulep) {
		sessionp = (struct session_module *)(modulep->function_ptr);
		if (sessionp && sessionp->pam_sm_close_session) {
			error = sessionp->pam_sm_close_session(
				pamh, flags, modulep->module_argc,
				(const char **) modulep->module_argv);

			if (pam_debug > 2)
				syslog(LOG_DEBUG,
				    "%s returned %s", modulep->module_path,
				    pam_strerror(pamh, error));

			switch (error) {
			case PAM_IGNORE:
				/* do nothing */
				break;
			case PAM_SUCCESS:
				if ((modulep->pam_flag & PAM_SUFFICIENT) &&
				    !required_module_failed)
					return (PAM_SUCCESS);
				success = 1;
				break;
			default:
				if (modulep->pam_flag & PAM_REQUISITE) {
					return (required_module_failed ?
						first_required_error :
						error);
				} else if (modulep->pam_flag & PAM_REQUIRED) {
					if (!required_module_failed)
						first_required_error = error;
					required_module_failed++;
				} else {
					if (!optional_module_failed)
						first_error = error;
					optional_module_failed++;
				}
				syslog(LOG_DEBUG,
					"pam_close_session: error %s",
						pam_strerror(pamh, error));
				break;
			}
		}
		modulep = modulep->next;
	}

	if (required_module_failed)
		return (first_required_error);
	else if (success == 0)
		return (first_error);
	else
		return (PAM_SUCCESS);
}

/*
 * pam_chauthtok - change user authentication token
 */

int
pam_chauthtok(
	pam_handle_t		*pamh,
	int			flags)
{
	int			error = PAM_AUTHTOK_ERR;
	int			first_error = PAM_AUTHTOK_ERR;
	int			first_required_error = PAM_AUTHTOK_ERR;
	int			required_module_failed = 0;
	int			optional_module_failed = 0;
	int			success = 0;
	int			i;
	int			sm_flags;
	pamtab			*modulep;
	struct password_module	*passwdp;

	if (pam_debug)
		syslog(LOG_DEBUG, "pam_chauthtok()");

	/* do not let apps use PAM_PRELIM_CHECK or PAM_UPDATE_AUTHTOK */
	if (flags & PAM_PRELIM_CHECK || flags & PAM_UPDATE_AUTHTOK)
		return (PAM_SYMBOL_ERR);

	if ((error = load_modules(pamh, PAM_PASSWORD_MODULE, PAM_SM_CHAUTHTOK))
						!= PAM_SUCCESS) {
		if (pam_debug)
			syslog(LOG_DEBUG,
			    "pam_chauthtok: load_modules failed");
		return (error);
	}

	for (i = 1; i <= 2; i++) {
	    switch (i) {
	    case 1:
		/* first time thru loop do preliminary check */
		sm_flags = flags | PAM_PRELIM_CHECK;
		break;
	    case 2:
		/* 2nd time thru loop update passwords */
		success = 0;
		sm_flags = flags | PAM_UPDATE_AUTHTOK;
		break;
	    }

	    modulep = pamh->pam_conf_info[PAM_PASSWORD_MODULE];
	    while (modulep) {
		passwdp = (struct password_module *)(modulep->function_ptr);
		if (passwdp && passwdp->pam_sm_chauthtok) {
			error = passwdp->pam_sm_chauthtok(pamh, sm_flags,
				modulep->module_argc,
				(const char **) modulep->module_argv);

			if (pam_debug > 2)
				syslog(LOG_DEBUG,
				    "%s returned %s", modulep->module_path,
				    pam_strerror(pamh, error));

			switch (error) {
			case PAM_IGNORE:
				/* do nothing */
				break;
			case PAM_SUCCESS:
				if (modulep->pam_flag & PAM_SUFFICIENT) {
					if (!(sm_flags & PAM_PRELIM_CHECK) &&
					    !required_module_failed) {
						/*
						 * If updating passwords, the
						 * module is sufficient, and
						 * no required modules have
						 * failed, then memset the
						 * passwd memory to 0 and return
						 */
						pam_set_item(pamh,
							PAM_AUTHTOK, NULL);
						pam_set_item(pamh,
							PAM_OLDAUTHTOK, NULL);
						return (PAM_SUCCESS);
					}
				}
				success = 1;
				break;
			default:
				if (modulep->pam_flag & PAM_REQUISITE) {
					pam_set_item(pamh, PAM_AUTHTOK, NULL);
					pam_set_item(pamh, PAM_OLDAUTHTOK,
									NULL);
					return (required_module_failed ?
						first_required_error :
						error);
				} else if (modulep->pam_flag & PAM_REQUIRED) {
					if (!required_module_failed)
						first_required_error = error;
					required_module_failed++;
				} else {
					if (!optional_module_failed)
						first_error = error;
					optional_module_failed++;
				}
				syslog(LOG_DEBUG,
					"pam_chauthtok: error %s",
						pam_strerror(pamh, error));
				break;
			}
		}
		modulep = modulep->next;
	    } /* while */

	    /* this will memset the password memory to 0 */
	    pam_set_item(pamh, PAM_AUTHTOK, NULL);
	    pam_set_item(pamh, PAM_OLDAUTHTOK, NULL);

	    if (required_module_failed)
		return (first_required_error);
	    else if (success == 0)
		return (first_error);
	    else if (sm_flags & PAM_UPDATE_AUTHTOK) {
		/*
		 * Only return PAM_SUCCESS if this is the second
		 * time through the loop.
		 */
		return (PAM_SUCCESS);
	    }

	/*
	 * If we reach here, the prelim check succeeded.
	 * Go thru the loop again to update the passwords.
	 */

	} /* for */

	/* should never reach this point!!! */
	return (error);
}

/*
 * pam_putenv - add an environment variable to the PAM handle
 *	if name_value == 'NAME=VALUE'	then set variable to the value
 *	if name_value == 'NAME='	then set variable to an empty value
 *	if name_value == 'NAME'		then delete the variable
 */

int
pam_putenv(pam_handle_t *pamh, const char *name_value)
{
	int		error = PAM_SYSTEM_ERR;
	char		*equal_sign = 0;
	char		*name = NULL, *value = NULL, *tmp_value = NULL;
	env_list	*traverse, *trail;

	if (pamh == 0 || name_value == 0)
		goto out;

	/* see if we were passed 'NAME=VALUE', 'NAME=', or 'NAME' */
	if ((equal_sign = strchr(name_value, '=')) != 0) {
		if ((name = (char *)calloc(equal_sign - name_value + 1,
					sizeof (char))) == 0) {
			error = PAM_BUF_ERR;
			goto out;
		}
		strncpy(name, name_value, equal_sign - name_value);
		if ((value = strdup(++equal_sign)) == 0) {
			error = PAM_BUF_ERR;
			goto out;
		}
	} else {
		if ((name = strdup(name_value)) == 0) {
			error = PAM_BUF_ERR;
			goto out;
		}
	}

	/* check to see if we already have this variable in the PAM handle */
	traverse = pamh->pam_env;
	trail = traverse;
	while (traverse && strncmp(traverse->name,
				name,
				strlen(name))) {
		trail = traverse;
		traverse = traverse->next;
	}

	if (traverse) {
		/* found a match */
		if (value == 0) {
			/* remove the env variable */
			if (pamh->pam_env == traverse)
				pamh->pam_env = traverse->next;
			else
				trail->next = traverse->next;
			free_env(traverse);
		} else if (strlen(value) == 0) {
			/* set env variable to empty value */
			if ((tmp_value = strdup("")) == 0) {
				error = PAM_SYSTEM_ERR;
				goto out;
			}
			free(traverse->value);
			traverse->value = tmp_value;
		} else {
			/* set the new value */
			if ((tmp_value = strdup(value)) == 0) {
				error = PAM_SYSTEM_ERR;
				goto out;
			}
			free(traverse->value);
			traverse->value = tmp_value;
		}

	} else if (traverse == 0 && value) {
		/*
		 * could not find a match in the PAM handle.
		 * add the new value if there is one
		 */
		if ((traverse = (env_list *)calloc
					(1,
					sizeof (env_list))) == 0) {
			error = PAM_BUF_ERR;
			goto out;
		}
		if ((traverse->name = strdup(name)) == 0) {
			free_env(traverse);
			error = PAM_BUF_ERR;
			goto out;
		}
		if ((traverse->value = strdup(value)) == 0) {
			free_env(traverse);
			error = PAM_BUF_ERR;
			goto out;
		}
		if (trail == 0) {
			/* new head of list */
			pamh->pam_env = traverse;
		} else {
			/* adding to end of list */
			trail->next = traverse;
		}
	}

	error = PAM_SUCCESS;
out:
	if (error != PAM_SUCCESS) {
		if (traverse) {
			if (traverse->name)
				free(traverse->name);
			if (traverse->value)
				free(traverse->value);
			free(traverse);
		}
	}
	if (name)
		free(name);
	if (value)
		free(value);
	return (error);

}

/*
 * pam_getenv - retrieve an environment variable from the PAM handle
 */
char *
pam_getenv(pam_handle_t *pamh, const char *name)
{
	int		error = PAM_SYSTEM_ERR;
	env_list	*traverse;

	if (pamh == 0 || name == 0)
		goto out;

	/* check to see if we already have this variable in the PAM handle */
	traverse = pamh->pam_env;
	while (traverse && strncmp(traverse->name,
				name,
				strlen(name))) {
		traverse = traverse->next;
	}
	error = (traverse ? PAM_SUCCESS : PAM_SYSTEM_ERR);
out:
	return (error ?
		(char *)0 :
		strdup(traverse->value));
}

/*
 * pam_getenvlist - retrieve all environment variables from the PAM handle
 */
char **
pam_getenvlist(pam_handle_t *pamh)
{
	int		error = PAM_SYSTEM_ERR;
	char		**list = 0;
	int		length = 0;
	env_list	*traverse;
	char		env_buf[1024];

	if (pamh == 0)
		goto out;

	/* find out how many environment variables we have */
	traverse = pamh->pam_env;
	while (traverse) {
		length++;
		traverse = traverse->next;
	}

	/* allocate the array we will return to the caller */
	if ((list = (char **)calloc(length + 1, sizeof (char *))) == 0) {
		error = PAM_BUF_ERR;
		goto out;
	}

	/* add the variables one by one */
	length = 0;
	traverse = pamh->pam_env;
	while (traverse) {
		snprintf(env_buf, sizeof (env_buf), "%s=%s",
			traverse->name, traverse->value);
		if ((list[length] = strdup(env_buf)) == 0) {
			error = PAM_BUF_ERR;
			goto out;
		}
		length++;
		traverse = traverse->next;
	}

	/* null terminate the list */
	list[length] = 0;

	error = PAM_SUCCESS;
out:
	if (error != PAM_SUCCESS) {
		/* free the partially constructed list */
		if (list) {
			length = 0;
			while (list[length] != NULL) {
				free(list[length]);
				length++;
			}
			free(list);
		}
	}
	return (error ? 0 : list);
}

/*
 * load_modules()
 * open_module()
 * load_function()
 *
 * Routines to load a requested module on demand
 */

/*
 * load_modules - load the requested module.
 *		  if the dlopen or dlsym fail, then
 *		  the module is ignored.
 */

static int
load_modules(pam_handle_t *pamh, int type, char *function_name)
{

#ifdef sun
	void *mh;
#endif

#ifdef hpV4
	shl_t mh;
#endif

	pamtab *pam_entry;
	struct auth_module *authp;
	struct account_module *accountp;
	struct session_module *sessionp;
	struct password_module *passwdp;
	int loading_functions = 0; /* are we currently loading functions? */

	if ((pam_entry = pamh->pam_conf_info[type]) == NULL) {
		syslog(LOG_ERR, "load_modules: no module present");
		return (PAM_SYSTEM_ERR);
	}

	while (pam_entry != NULL) {
		if (pam_debug)
			syslog(LOG_DEBUG, "load_modules: %s",
				pam_entry->module_path);

		switch (type) {
		case PAM_AUTH_MODULE:

			/* if the function has already been loaded, return */
			authp = pam_entry->function_ptr;
			if (!loading_functions &&
				(((strcmp(function_name, PAM_SM_AUTHENTICATE)
									== 0) &&
				authp && authp->pam_sm_authenticate) ||
				((strcmp(function_name, PAM_SM_SETCRED) == 0) &&
				authp && authp->pam_sm_setcred))) {
				return (PAM_SUCCESS);
			}

			/* function has not been loaded yet */
			loading_functions = 1;
			authp = (struct auth_module *)
				calloc(1, sizeof (struct auth_module));
			if (authp == NULL)
				return (PAM_BUF_ERR);

			/* if open_module fails, return error */
			if ((mh = open_module
					(pamh,
					pam_entry->module_path)) == NULL) {
				syslog(LOG_ERR,
					"load_modules: can not open module %s",
					pam_entry->module_path);
				free(authp);
				return (PAM_OPEN_ERR);
			}

			/* load the authentication function */
			if (strcmp(function_name, PAM_SM_AUTHENTICATE) == 0) {
				if (load_function(mh, PAM_SM_AUTHENTICATE,
				    &authp->pam_sm_authenticate) !=
								PAM_SUCCESS) {
					/* return error if dlsym fails */
					free(authp);
					return (PAM_SYMBOL_ERR);
				}

			/* load the setcred function */
			} else if (strcmp(function_name, PAM_SM_SETCRED) == 0) {
				if (load_function(mh, PAM_SM_SETCRED,
				    &authp->pam_sm_setcred) != PAM_SUCCESS) {
					/* return error if dlsym fails */
					free(authp);
					return (PAM_SYMBOL_ERR);
				}
			}
			pam_entry->function_ptr = authp;
			break;
		case PAM_ACCOUNT_MODULE:
			accountp = pam_entry->function_ptr;
			if (!loading_functions &&
			    (strcmp(function_name, PAM_SM_ACCT_MGMT) == 0) &&
			    accountp && accountp->pam_sm_acct_mgmt) {
				return (PAM_SUCCESS);
			}

			loading_functions = 1;
			accountp = (struct account_module *)
				calloc(1, sizeof (struct account_module));
			if (accountp == NULL)
				return (PAM_BUF_ERR);

			/* if open_module fails, return error */
			if ((mh = open_module
					(pamh,
					pam_entry->module_path)) == NULL) {
				syslog(LOG_ERR,
					"load_modules: can not open module %s",
					pam_entry->module_path);
				free(accountp);
				return (PAM_OPEN_ERR);
			}

			if (load_function(mh,
					PAM_SM_ACCT_MGMT,
					&accountp->pam_sm_acct_mgmt)
					!= PAM_SUCCESS) {
				syslog(LOG_ERR,
				"load_modules: pam_sm_acct_mgmt() missing");
				free(accountp);
				return (PAM_SYMBOL_ERR);
			}
			pam_entry->function_ptr = accountp;
			break;
		case PAM_SESSION_MODULE:
			sessionp = pam_entry->function_ptr;
			if (!loading_functions &&
			    (((strcmp(function_name, PAM_SM_OPEN_SESSION)
								== 0) &&
			    sessionp && sessionp->pam_sm_open_session) ||
			    ((strcmp(function_name, PAM_SM_CLOSE_SESSION)
								== 0) &&
			    sessionp && sessionp->pam_sm_close_session))) {
				return (PAM_SUCCESS);
			}

			loading_functions = 1;
			sessionp = (struct session_module *)
				calloc(1, sizeof (struct session_module));
			if (sessionp == NULL)
				return (PAM_BUF_ERR);

			/* if open_module fails, return error */
			if ((mh = open_module
					(pamh,
					pam_entry->module_path)) == NULL) {
				syslog(LOG_ERR,
					"load_modules: can not open module %s",
					pam_entry->module_path);
				free(sessionp);
				return (PAM_OPEN_ERR);
			}

			if ((strcmp(function_name, PAM_SM_OPEN_SESSION) == 0) &&
			    load_function(mh, PAM_SM_OPEN_SESSION,
				&sessionp->pam_sm_open_session)
				!= PAM_SUCCESS) {
				free(sessionp);
				return (PAM_SYMBOL_ERR);
			} else if ((strcmp(function_name,
					PAM_SM_CLOSE_SESSION) == 0) &&
				    load_function(mh, PAM_SM_CLOSE_SESSION,
					&sessionp->pam_sm_close_session)
					!= PAM_SUCCESS) {
				free(sessionp);
				return (PAM_SYMBOL_ERR);
			}
			pam_entry->function_ptr = sessionp;
			break;
		case PAM_PASSWORD_MODULE:
			passwdp = pam_entry->function_ptr;
			if (!loading_functions &&
			    (strcmp(function_name, PAM_SM_CHAUTHTOK) == 0) &&
			    passwdp && passwdp->pam_sm_chauthtok) {
				return (PAM_SUCCESS);
			}

			loading_functions = 1;
			passwdp = (struct password_module *)
				calloc(1, sizeof (struct password_module));
			if (passwdp == NULL)
				return (PAM_BUF_ERR);

			/* if open_module fails, continue */
			if ((mh = open_module
					(pamh,
					pam_entry->module_path)) == NULL) {
				syslog(LOG_ERR,
					"load_modules: can not open module %s",
					pam_entry->module_path);
				free(passwdp);
				return (PAM_OPEN_ERR);
			}

			if (load_function(mh, PAM_SM_CHAUTHTOK,
				&passwdp->pam_sm_chauthtok) != PAM_SUCCESS) {
				free(passwdp);
				return (PAM_SYMBOL_ERR);
			}
			pam_entry->function_ptr = passwdp;
			break;
		default:
			if (pam_debug) {
				syslog(LOG_DEBUG,
				"load_modules: unsupported type %d", type);
			}
			break;
		}

		pam_entry = pam_entry->next;
	} /* while */

	return (PAM_SUCCESS);
}

/*
 * open_module		- Open the module first checking for
 *			  propers modes and ownerships on the file.
 */

#ifdef sun
static void *
open_module(pam_handle_t *pamh, char *module_so)
{
#endif
#ifdef hpV4
static shl_t
open_module(pam_handle_t *pamh, char *module_so)
{
#endif
	struct stat64	stb;
	char		*errmsg;
#ifdef sun
	void		*lfd;
#endif /* sun */
#ifdef hpV4
	shl_t		lfd;
#endif /* hpV4 */
	extern int	errno;
	fd_list		*module_fds = 0;
	fd_list		*trail = 0;
	fd_list		*traverse = 0;

	/*
	 * Stat the file so we can check modes and ownerships
	 */
	if (stat64(module_so, &stb) < 0) {
		syslog(LOG_ERR, "open_module: stat(%s) failed: %s",
				module_so, strerror(errno));
		return (NULL);
	}

	/*
	 * Check the ownership of the file
	 */
	if (stb.st_uid != (uid_t)0) {
		syslog(LOG_ALERT,
			"open_module: Owner of the module %s is not root",
			module_so);
		return (NULL);
	}

	/*
	 * Check the modes on the file
	 */
	if (stb.st_mode&S_IWGRP) {
		syslog(LOG_ALERT,
			"open_module: module %s writable by group",
			module_so);
		return (NULL);
	}
	if (stb.st_mode&S_IWOTH) {
		syslog(LOG_ALERT,
			"open_module: module %s writable by world", module_so);
		return (NULL);
	}

	/*
	 * Perform the dlopen()
	 */
#ifdef sun
	lfd = (void *) dlopen(module_so, RTLD_LAZY);
#endif /* sun */

#ifdef hpV4
	lfd = shl_load(module_so, BIND_DEFERRED, 0L);
#endif /* hpV4 */

	if (lfd == NULL) {
		if (pam_debug) {
			errmsg = (char *)strerror(errno);
			syslog(LOG_DEBUG, "open_module: %s failed: %s",
				module_so, errmsg);
		}
		return (NULL);
	} else {
		/* add this fd to the pam handle */
		if ((module_fds = (fd_list *)calloc
				(1, sizeof (fd_list))) == 0) {
			dlclose(lfd);
			lfd = 0;
			return (NULL);
		}
		module_fds->mh = lfd;

		if (pamh->fd == 0) {
			/* adding new head of list */
			pamh->fd = module_fds;
		} else {
			/* appending to end of list */
			traverse = pamh->fd;
			while (traverse) {
				trail = traverse;
				traverse = traverse->next;
			}
			trail->next = module_fds;
		}
	}

	return (lfd);

}

/*
 * load_function - call dlsym() to resolve the function address
 */
#ifdef sun
static int
load_function(void *lfd, char *name, int (**func)())
{
#endif
#ifdef hpV4
static int
load_function(shl_t lfd, char *name, int (**func)())
{
#endif
	char *errmsg = NULL;

#ifdef hpV4
extern int errno;
void *proc_addr = NULL;
int stat;

#endif

	if (lfd == NULL)
		return (PAM_SYMBOL_ERR);

	/*
	 * The APIs for opening the shared objects are palatform dependent
	 * and hence the ifdef platform
	 */
#ifdef sun
	*func = (int (*)())dlsym(lfd, name);
	if (*func == NULL) {
		if (pam_debug) {
			errmsg = (char *)dlerror();
			syslog(LOG_DEBUG,
			"dlsym failed %s: error %s",
			name, errmsg != NULL ? errmsg : "");
		}
		return (PAM_SYMBOL_ERR);
	}
#endif

#ifdef hpV4

	stat = shl_findsym(&lfd, name, TYPE_PROCEDURE, proc_addr);

	*func = (int (*)())proc_addr;

	if (stat) {
		if (pam_debug) {
			strerror_r(errno, errmsg, MAX_ERRMESSAGE_LENGTH);
			syslog(LOG_DEBUG, "shl_findsym failed %s: error %s",
				name, errmsg != NULL ? errmsg : "");
		}
		return (PAM_SYMBOL_ERR);
	}
#endif
	if (pam_debug) {
		syslog(LOG_DEBUG,
			"load_function: successful load of %s", name);
	}
	return (PAM_SUCCESS);
}

/*
 * open_pam_conf()
 * close_pam_conf()
 * read_pam_conf()
 * get_pam_conf_entry()
 * read_next_token()
 * nextline()
 * verify_pam_conf()
 *
 * Routines to read the pam.conf configuration file
 */

/*
 * open_pam_conf - open the pam.conf config file
 */

static int
open_pam_conf(struct pam_fh **pam_fh)
{
	struct stat64	stb;
	extern int	errno;

	/*
	 * Stat the file so we can check modes and ownerships
	 */
	if (stat64(PAM_CONFIG, &stb) < 0) {
		syslog(LOG_ALERT, "open_pam_conf: stat(%s) failed: %s",
			PAM_CONFIG, strerror(errno));
		return (0);
	}

	/*
	 * Check the ownership of the file
	 */
	if (stb.st_uid != (uid_t)0) {
		syslog(LOG_ALERT,
		    "open_pam_conf: Owner of %s is not root", PAM_CONFIG);
		return (0);
	}

	/*
	 * Check the modes on the file
	 */
	if (stb.st_mode&S_IWGRP) {
		syslog(LOG_ALERT,
		    "open_pam_conf: %s writable by group", PAM_CONFIG);
		return (0);
	}
	if (stb.st_mode&S_IWOTH) {
		syslog(LOG_ALERT,
			"open_pam_conf: %s writable by world", PAM_CONFIG);
		return (0);
	}

	*pam_fh = calloc(1, sizeof (struct pam_fh));
	if (*pam_fh == NULL)
		return (0);

	(*pam_fh)->fconfig = open(PAM_CONFIG, O_RDONLY);
	if ((*pam_fh)->fconfig != -1) {
		(*pam_fh)->bufsize = (size_t)stb.st_size;
		(*pam_fh)->data = mmap((caddr_t)0, (*pam_fh)->bufsize,
				PROT_READ, MAP_PRIVATE, (*pam_fh)->fconfig, 0);
		if ((*pam_fh)->data == (caddr_t)-1) {
			(void) close((*pam_fh)->fconfig);
			free (*pam_fh);
			return (0);
		}
		(*pam_fh)->bufferp = (*pam_fh)->data;
	} else {
		free (*pam_fh);
		return (0);
	}

	return (1);
}

/*
 * close_pam_conf - close pam.conf
 */

static void
close_pam_conf(struct pam_fh *pam_fh)
{

	(void) munmap(pam_fh->data, pam_fh->bufsize);
	close(pam_fh->fconfig);
	free(pam_fh);
}

/*
 * read_pam_conf - read in each entry in pam.conf and store info
 *		   under the pam handle.
 */

static int
read_pam_conf(pam_handle_t *pamh)
{
	struct pam_fh *pam_fh;
	pamtab *pamentp;
	pamtab *tpament;
	char *service;
	int error;
	/*
	 * service types:
	 * error (-1), "auth" (0), "account" (1), "session" (2), "password" (3)
	 */
	int service_found[PAM_NUM_MODULE_TYPES+1] = {0, 0, 0, 0, 0};

	if ((error = pam_get_item(pamh, PAM_SERVICE, (void **)&service))
							!= PAM_SUCCESS)
		return (error);

	if (open_pam_conf(&pam_fh) == 0)
		return (PAM_SYSTEM_ERR);

	while ((error = get_pam_conf_entry(pam_fh, &pamentp)) == PAM_SUCCESS &&
		pamentp) {

		/* See if entry is this service and valid */
		if (verify_pam_conf(pamentp, service)) {
			if (pam_debug > 3)
				syslog(LOG_DEBUG, "bad entry error %s",
				    service);

			error = PAM_SYSTEM_ERR;
			free_pamconf(pamentp);
			goto out;
		}
		if (strcasecmp(pamentp->pam_service, service) == 0) {
			if (pam_debug > 3)
				syslog(LOG_DEBUG, "processing %s", service);
			/* process first service entry */
			if (service_found[pamentp->pam_type + 1] == 0) {
				/* purge "other" entries */
				while ((tpament =
				    pamh->pam_conf_info[pamentp->pam_type]) !=
				    NULL) {
					if (pam_debug > 3)
						syslog(LOG_DEBUG,
						    "purging \"other\"");
					pamh->pam_conf_info[pamentp->pam_type] =
					    tpament->next;
					free_pamconf(tpament);
				}
				/* add first service entry */
				if (pam_debug > 3)
					syslog(LOG_DEBUG, "adding first %s[%d]",
					    service, pamentp->pam_type);
				pamh->pam_conf_info[pamentp->pam_type] =
				    pamentp;
				service_found[pamentp->pam_type + 1] = 1;
			} else {
				/* append more service entries */
				if (pam_debug > 3)
					syslog(LOG_DEBUG, "adding more %s[%d]",
					    service, pamentp->pam_type);
				tpament =
				    pamh->pam_conf_info[pamentp->pam_type];
				while (tpament->next != NULL) {
					tpament = tpament->next;
				}
				tpament->next = pamentp;
			}
		} else if (service_found[pamentp->pam_type + 1] == 0) {
			/* See if "other" entry available and valid */
			if (verify_pam_conf(pamentp, "other")) {
				if (pam_debug > 3)
					syslog(LOG_DEBUG, "bad entry error %s"
					    " \"other\"",
					    service);

				error = PAM_SYSTEM_ERR;
				free_pamconf(pamentp);
				goto out;
			}
			if (strcasecmp(pamentp->pam_service, "other") == 0) {
				if (pam_debug > 3)
					syslog(LOG_DEBUG, "processing "
					    "\"other\"");
				if ((tpament =
				    pamh->pam_conf_info[pamentp->pam_type]) ==
				    NULL) {
					/* add first "other" entry */
					if (pam_debug > 3)
						syslog(LOG_DEBUG,
						    "adding first other[%d]",
						    pamentp->pam_type);
					pamh->pam_conf_info[pamentp->pam_type] =
					    pamentp;
				} else {
					/* append more "other" entries */
					if (pam_debug > 3)
						syslog(LOG_DEBUG,
						    "adding more other[%d]",
						    pamentp->pam_type);
					while (tpament->next != NULL) {
						tpament = tpament->next;
					}
					tpament->next = pamentp;
				}
			} else {
				/* irrelevent entry */
				free_pamconf(pamentp);
			}
		} else {
			/* irrelevent entry */
			free_pamconf(pamentp);
		}
	}

out:
	(void) close_pam_conf(pam_fh);
	if (error != PAM_SUCCESS)
		free_pam_conf_info(pamh);
	return (error);
}

/*
 * get_pam_conf_entry - get a pam.conf entry
 */

static int
get_pam_conf_entry(struct pam_fh *pam_fh, pamtab **pam)
{
	char		*cp, *arg;
	int		argc;
	char		*tmp, *tmp_free;
	int		i;
	char		*current_line = 0;
	int		error = PAM_SYSTEM_ERR;	/* preset to error */

	/* get the next line from pam.conf */
	if ((cp = nextline(pam_fh)) == 0) {
		/* no more lines in pam.conf ==> return */
		error = PAM_SUCCESS;
		*pam = 0;
		goto out;
	}

	if ((*pam = (pamtab *)calloc(1, sizeof (pamtab))) == NULL) {
		syslog(LOG_ERR, "strdup: out of memory");
		goto out;
	}

	/* copy full line for error reporting */
	if ((current_line = strdup(cp)) == NULL) {
		syslog(LOG_ERR, "strdup: out of memory");
		goto out;
	}

	if (pam_debug > 3)
		syslog(LOG_DEBUG, "pam.conf entry:\n\t%s", current_line);

	/* get service name (e.g. login, su, passwd) */
	if ((arg = read_next_token(&cp)) == 0) {
		syslog(LOG_CRIT,
		"illegal pam.conf entry: %s: missing SERVICE NAME",
			current_line);
		goto out;
	}
	if (((*pam)->pam_service = strdup(arg)) == 0) {
		syslog(LOG_ERR, "strdup: out of memory");
		goto out;
	}

	/* get module type (e.g. authentication, acct mgmt) */
	if ((arg = read_next_token(&cp)) == 0) {
		syslog(LOG_CRIT,
		"illegal pam.conf entry: %s: missing MODULE TYPE",
			current_line);
		(*pam)->pam_type = -1;	/* 0 is a valid value */
		goto getflag;
	}
	if (strcasecmp(arg, "auth") == 0)
		(*pam)->pam_type = PAM_AUTH_MODULE;
	else if (strcasecmp(arg, "account") == 0)
		(*pam)->pam_type = PAM_ACCOUNT_MODULE;
	else if (strcasecmp(arg, "session") == 0)
		(*pam)->pam_type = PAM_SESSION_MODULE;
	else if (strcasecmp(arg, "password") == 0)
		(*pam)->pam_type = PAM_PASSWORD_MODULE;
	else {
		/* error */
		syslog(LOG_CRIT, "illegal pam.conf entry: %s", current_line);
		syslog(LOG_CRIT, "\tinvalid module type: %s", arg);
		(*pam)->pam_type = -1;	/* 0 is a valid value */
	}

getflag:
	/* get pam flag (e.g. requisite, required, sufficient, optional) */
	if ((arg = read_next_token(&cp)) == 0) {
		syslog(LOG_CRIT,
			"illegal pam.conf entry: %s: missing FLAG",
			current_line);
		goto getpath;
	}
	if (strcasecmp(arg, "requisite") == 0)
		(*pam)->pam_flag = PAM_REQUISITE;
	else if (strcasecmp(arg, "required") == 0)
		(*pam)->pam_flag = PAM_REQUIRED;
	else if (strcasecmp(arg, "optional") == 0)
		(*pam)->pam_flag = PAM_OPTIONAL;
	else if (strcasecmp(arg, "sufficient") == 0)
		(*pam)->pam_flag = PAM_SUFFICIENT;
	else {
		/* error */
		syslog(LOG_CRIT, "illegal pam.conf entry: %s", current_line);
		syslog(LOG_CRIT, "\tinvalid flag: %s", arg);
	}

getpath:
	/* get module path (e.g. /usr/lib/security/pam_unix.so.1) */
	if ((arg = read_next_token(&cp)) == 0) {
		syslog(LOG_CRIT,
			"illegal pam.conf entry: %s: missing MODULE PATH",
			current_line);
		error = PAM_SUCCESS;	/* success */
		goto out;
	}
	if (arg[0] != '/') {
		/*
		 * If module path does not start with "/", then
		 * prepend PAM_LIB_DIR (/usr/lib/security/).
		 */
		if (((*pam)->module_path = (char *)
			calloc(strlen(PAM_LIB_DIR) + strlen(arg) + 1,
				sizeof (char))) == NULL) {
			syslog(LOG_ERR, "strdup: out of memory");
			goto out;
		}
		sprintf((*pam)->module_path, "%s%s",
			PAM_LIB_DIR, arg);
	} else {
		/* Full path provided for module */
		char *isa;

		/* Check for Instruction Set Architecture indicator */
		if ((isa = strstr(arg, PAM_ISA)) != NULL) {
			/* substitute the architecture dependent path */
			if (((*pam)->module_path = (char *)
			    calloc(strlen(arg) - strlen(PAM_ISA) +
			    strlen(PAM_ISA_DIR) + 1, sizeof (char))) == NULL) {
				syslog(LOG_ERR, "strdup: out of memory");
				goto out;
			}
			*isa = '\000';
			isa += strlen(PAM_ISA);
			sprintf((*pam)->module_path, "%s%s%s",
			    arg, PAM_ISA_DIR, isa);
		} else if (((*pam)->module_path = strdup(arg)) == 0) {
			syslog(LOG_ERR, "strdup: out of memory");
			goto out;
		}
	}

	/* count the number of module-specific options first */
	argc = 0;
	if ((tmp = strdup(cp)) == NULL) {
		syslog(LOG_ERR, "strdup: out of memory");
		goto out;
	}
	tmp_free = tmp;
	for (arg = read_next_token(&tmp); arg; arg = read_next_token(&tmp))
		argc++;
	free(tmp_free);

	/* allocate array for the module-specific options */
	if (argc > 0) {
		if (((*pam)->module_argv = (char **)
			calloc(argc+1, sizeof (char *))) == 0) {
			syslog(LOG_ERR, "calloc: out of memory");
			goto out;
		}
		i = 0;
		for (arg = read_next_token(&cp); arg;
			arg = read_next_token(&cp)) {
			(*pam)->module_argv[i] = strdup(arg);
			if ((*pam)->module_argv[i] == NULL) {
				syslog(LOG_ERR, "strdup failed");
				goto out;
			}
			i++;
		}
		(*pam)->module_argv[argc] = NULL;
	}
	(*pam)->module_argc = argc;

	error = PAM_SUCCESS;	/* success */

out:
	if (current_line)
		free(current_line);
	if (error != PAM_SUCCESS) {
		/* on error free this */
		if (*pam)
			free_pamconf(*pam);
	}
	return (error);
}


/*
 * read_next_token - skip tab and space characters and return the next token
 */

static char *
read_next_token(char **cpp)
{
	register char *cp = *cpp;
	char *start;

	if (cp == (char *)0) {
		*cpp = (char *)0;
		return ((char *)0);
	}
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		*cpp = (char *)0;
		return ((char *)0);
	}
	start = cp;
	while (*cp && *cp != ' ' && *cp != '\t')
		cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	*cpp = cp;
	return (start);
}

static char *
pam_conf_strnchr(char *sp, int c, intptr_t count)
{
	while (count) {
		if (*sp == (char)c)
			return ((char *)sp);
		else {
			sp++;
			count--;
		}
	};
	return (NULL);
}

/*
 * nextline - skip all blank lines and comments
 */

static char *
nextline(struct pam_fh *pam_fh)
{
	char	*ll;
	int	find_a_line = 0;
	char	*data = pam_fh->data;
	char	*bufferp = pam_fh->bufferp;
	char	*bufferendp = &data[pam_fh->bufsize];

	/*
	 * Skip the blank line, comment line
	 */
	while (!find_a_line) {
		/* if we are at the end of the buffer, there is no next line */
		if (bufferp == bufferendp)
			return (NULL);

		/* skip blank line */
		while (*bufferp == '\n') {
			/*
			 * If we are at the end of the buffer, there is
			 * no next line.
			 */
			if (++bufferp == bufferendp)
				return (NULL);
			/* else we check *bufferp again */
		}

		/* skip comment line */
		while (*bufferp == '#') {
			if ((ll = pam_conf_strnchr(bufferp, '\n',
				bufferendp - bufferp)) != NULL) {
				bufferp = ll;
			}
			else
			/* this comment line the last line. no next line */
				return (NULL);

			/*
			 * If we are at the end of the buffer, there is
			 * no next line.
			 */
			if (bufferp == bufferendp)
				return (NULL);
		}

		if ((*bufferp != '\n') && (*bufferp != '#'))
			find_a_line = 1;
	}

	/* now we find one line */
	if ((ll = pam_conf_strnchr(bufferp, '\n', bufferendp - bufferp))
		!= NULL) {
		strncpy(pam_fh->line, bufferp, ll - bufferp);
		pam_fh->line[ll - bufferp] = '\0';
		pam_fh->bufferp = ll++;
	} else {
		ll = bufferendp;
		strncpy(pam_fh->line, bufferp, ll - bufferp);
		pam_fh->line[ll - bufferp] = '\0';
		pam_fh->bufferp = ll;
	}

	return (pam_fh->line);
}

/*
 * verify_pam_conf - verify that the pam_conf entry is filled in.
 *
 *	Error if there is no service.
 *	Error if there is a service and it matches the requested service
 *		but, the type, flag, or path is in error.
 */

static int
verify_pam_conf(pamtab *pam, char *service)
{

	return ((pam->pam_service == (char *)NULL) ||
	    ((strcasecmp(pam->pam_service, service) == 0) &&
	    ((pam->pam_type == -1) ||
	    (pam->pam_flag == 0) ||
	    (pam->module_path == (char *)NULL))));
}

/*
 * clean_up()
 * free_pamconf()
 * free_pam_conf_info()
 *
 * Routines to free allocated storage
 */

/*
 * clean_up -  free allocated storage in the pam handle
 */

static void
clean_up(pam_handle_t *pamh)
{
	int i;

	if (pamh) {
		/* Cleanup Sun proprietary tag information */
		if (pamh->pam_client_message_version_number)
			free(pamh->pam_client_message_version_number);

		free_pam_conf_info(pamh);
		for (i = 0; i < PAM_MAX_ITEMS; i++) {
			if (pamh->ps_item[i].pi_addr != NULL)
				free(pamh->ps_item[i].pi_addr);
		}
		free(pamh);
	}
}

/*
 * free_pamconf - free memory used to store pam.conf entry
 */

static void
free_pamconf(pamtab *cp)
{
	int i;

	if (cp) {
		if (cp->pam_service)
			free(cp->pam_service);
		if (cp->module_path)
			free(cp->module_path);
		for (i = 0; i < cp->module_argc; i++) {
			if (cp->module_argv[i])
				free(cp->module_argv[i]);
		}
		if (cp->module_argc > 0)
			free(cp->module_argv);
		if (cp->function_ptr)
			free(cp->function_ptr);

		free(cp);
	}
}

/*
 * free_pam_conf_info - free memory used to store all pam.conf info
 *			under the pam handle
 */

static void
free_pam_conf_info(pam_handle_t *pamh)
{

	pamtab *pamentp;
	pamtab *pament_trail;
	int i;

	for (i = 0; i < PAM_NUM_MODULE_TYPES; i++) {
		pamentp = pamh->pam_conf_info[i];
		pament_trail = pamentp;
		while (pamentp) {
			pamentp = pamentp->next;
			free_pamconf(pament_trail);
			pament_trail = pamentp;
		}
	}

}

static void
free_env(env_list *pam_env)
{
	if (pam_env) {
		if (pam_env->name)
			free(pam_env->name);
		if (pam_env->value)
			free(pam_env->value);
		free(pam_env);
	}
}
