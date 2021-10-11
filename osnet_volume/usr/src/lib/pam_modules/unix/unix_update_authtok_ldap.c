/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)unix_update_authtok_ldap.c 1.2     99/07/14 SMI"

#include "unix_headers.h"

static int get_auth(Auth_t **, const char *, const char *);
static int ldap_to_pamerror(int);
static int update_ldapattr(pam_handle_t *, char *, char **,
struct passwd *, Auth_t *, int, int);

/*
 * update_authtok_ldap():
 * 	To update the authentication token file.
 *
 *	This function is called by either __set_authtoken_attr() to
 *	update the token attributes or pam_chauthtok() to update the
 *	authentication token.  The parameter "field" has to be specified
 * 	as "attr" if the caller wants to update token attributes, and
 *	the attribute-value pairs to be set needs to be passed in by parameter
 * 	"data".  If the function is called to update authentication
 *	token itself, then "field" needs to be specified as "passwd"
 * 	and the new authentication token has to be passed in by "data".
 *
 */

int
update_authtok_ldap(
pam_handle_t 	*pamh,
char 		*field,
char		*data[],	/* encrypted new passwd */
/* or new attribute info */
char		*oldpwd,	/* old passwd: clear */
char		*newpwd,	/* new passwd: clear */
struct passwd	*ldap_pwd,	/* password structure */
int		privileged,	/* Not used currently but may be */
/* needed later on */
int		debug,
int		nowarn)
{
	char 		*prognamep;
	char 		*usrname;
	int		retcode;
	int		ldaprc;
	char 		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	Auth_t		*authp;	/* The cred structure to be used in */
				/* libsldap calls */
#ifdef DEBUG
	fprintf(stderr, "\n[unix_update_authtok_ldap.c]\n");
	fprintf(stderr, "\tupdate_authtok_ldap()\n");
#endif

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
	    != PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
	    != PAM_SUCCESS)
		goto out;

	if (strcmp(field, "passwd") == 0) {
		/*
		 * ck_passwd() already checked the old passwd. It won't get here
		 * if the old passwd is not matched.
		 * We are just preparing the passwd update packet here.
		 */

		if (debug) {
			syslog(LOG_DEBUG,
			    "update_authtok_ldap(): update passwords");
			syslog(LOG_DEBUG,
			    "update_authtok_ldap: username = %s", usrname);
		}

		/* Fill the Auth structure to be used in __ns_ldap_repAttr */
		if ((retcode = get_auth(&authp, usrname, oldpwd))
		    != PAM_SUCCESS)
			goto out;

		if (ldap_pwd->pw_passwd) {
			memset(ldap_pwd->pw_passwd, 0,
			    strlen(ldap_pwd->pw_passwd));
			free(ldap_pwd->pw_passwd);
		}
		ldap_pwd->pw_passwd = *data;	/* encrypted new passwd */

		/* Update the password in the ldap name space */
		if ((retcode = update_ldapattr(pamh, field, data, ldap_pwd,
		    authp, privileged, nowarn)) != PAM_SUCCESS)
			goto out;

	} else {
		/*
		 * prompt for passwd: required for the options e,g,h
		 * ldap_pwd struct will be modified by update_ldapattr().
		 * The encrypted passwd remains the same because we are not
		 * changing passwd here.
		 */

		if (debug) {
			syslog(LOG_DEBUG,
			    "update_authtok_ldap(): update attributes");
			syslog(LOG_DEBUG,
			    "update_authtok_ldap: username = %s", usrname);
		}
		retcode = __pam_get_authtok(pamh, PAM_PROMPT, 0, PASSWORD_LEN,
		    PAM_MSG(pamh, 165, "Enter login(LDAP) password: "),
		    &oldpwd, NULL);
		if (retcode != PAM_SUCCESS)
			goto out;

		/* Fill the Auth structure to be used in __ns_ldap_repAttr */
		if ((retcode = get_auth(&authp, usrname, oldpwd))
		    != PAM_SUCCESS)
			goto out;

		/* Update the attributes in the ldap name space */
		if ((retcode = update_ldapattr(pamh, field, data, ldap_pwd,
		    authp, privileged, nowarn)) != PAM_SUCCESS) {

			if (retcode == -1)
				/* finger, shell, or gecos info unchanged */
				retcode = PAM_SUCCESS;

			goto out;
		}
	}

	retcode  = PAM_SUCCESS;

out:
	/* Printing appropriate messages */
	if (retcode != PAM_SUCCESS) {
		snprintf(messages[0],
		    sizeof (messages[0]),
		    PAM_MSG(pamh, 167,
		    "%s %s: Couldn't change passwd/attributes for %s"),
		    prognamep, LDAP_MSG, usrname);
		(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
		    1, messages, NULL);
	} else {
		snprintf(messages[0],
		    sizeof (messages[0]),
		    PAM_MSG(pamh, 168,
		    "LDAP passwd/attributes changed for %s"), usrname);
		(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
		    1, messages, NULL);
	}

	__ns_ldap_freeAuth(&authp);

	return (retcode);
}

static int
get_auth(
Auth_t 		**authpp,
const char	*usrname,
const char	*pwd)
{
	int		*authtypep = NULL;
	int		*sectypep = NULL;
	void		**configVal = NULL;
	int		ldaprc;
	int		retcode;
	ns_ldap_error_t	*errorp = NULL;
	int		i;

#ifdef DEBUG
	fprintf(stderr, "\n[unix_update_authtok_ldap.c]\n");
	fprintf(stderr, "\tget_auth()\n");
#endif

	*authpp = (Auth_t *)calloc(1, sizeof (Auth_t));

	/* Fill in the user name */
	i = strlen(usrname);
	(*authpp)->cred.unix_cred.userID = (char *)
	    calloc(i + 1, sizeof (char));
	strncpy((*authpp)->cred.unix_cred.userID, usrname, i);
	(*authpp)->cred.unix_cred.userID[i] = '\0';

	i = strlen(pwd);
	(*authpp)->cred.unix_cred.passwd = (char *)
	    calloc(i + 1, sizeof (char));
	strncpy((*authpp)->cred.unix_cred.passwd, pwd, i);
	(*authpp)->cred.unix_cred.passwd[i] = '\0';

	/*
	 * The Authentication mechanism and the transport security types
	 * are obtained from the LDAP cache file directly.
	 * If the cache file is corrupted, then an error is returned
	 * and no modification is allowed to take place.
	 *
	 * XXX Modify to reflect mutlti-valued Auth.
	 * What if Auth type is NONE. Should I catch it here or later?
	 */

	/* Load the Authentication Mechanism */
	ldaprc = __ns_ldap_getParam(NULL, NS_LDAP_AUTH_P, &configVal, &errorp);
	if (retcode = ldap_to_pamerror(ldaprc) != PAM_SUCCESS)
		goto out;

	authtypep = (int *) (*configVal);
	(*authpp)->type = *authtypep;
	(void) __ns_ldap_freeParam(&configVal);
	configVal = NULL;

	/* If authtype is NONE, then return with permission denied */
	if (*authtypep == NS_LDAP_AUTH_NONE) {
		retcode = PAM_PERM_DENIED;
		goto out;
	}

	/* Load Transport Sec type */
	ldaprc = __ns_ldap_getParam(NULL, NS_LDAP_TRANSPORT_SEC_P,
	    &configVal, &errorp);
	if (retcode = ldap_to_pamerror(ldaprc) != PAM_SUCCESS)
		goto out;

	sectypep = (int *) (*configVal);
	(*authpp)->security = *sectypep;

out:
	if (configVal)
		(void) __ns_ldap_freeParam(&configVal);
	if (errorp)
		__ns_ldap_freeError(&errorp);

	return (retcode);
}

static int
ldap_to_pamerror(int ldaperror)
{
	int pamerror;

	switch (ldaperror) {
	case NS_LDAP_SUCCESS:
		pamerror = PAM_SUCCESS;
		break;

	case NS_LDAP_OP_FAILED:
		pamerror = PAM_PERM_DENIED;
		break;

	case NS_LDAP_MEMORY:
		pamerror = PAM_BUF_ERR;
		break;

	case NS_LDAP_CONFIG:
		pamerror = PAM_SERVICE_ERR;
		break;

	case NS_LDAP_INTERNAL:
		pamerror = PAM_SYSTEM_ERR;
		break;

	default:
		pamerror = PAM_SYSTEM_ERR;
		break;

	}

	return (pamerror);

}

static int
update_ldapattr(pam_handle_t *pamh,
char *field,
char **data,
struct passwd *ldap_pwd,
Auth_t *authp, 		/* Auth structure for validation */
int privileged,
int nowarn)
{
	char		*username;
	char		*value;
	char 		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	int		retcode;
	int		ldaprc;
	ns_ldap_error_t	*errorp = NULL;
	char		*dn;
	char		**newattr;
	char		buffer[BUFSIZ];
	ns_ldap_attr_t	*attrs[100];	/* allocated more than needed */
	int		count = 0;	/* number of attr type specified */
	int		i = 0;

	if (ldap_pwd == NULL) {
		if (!nowarn) {
			pam_get_item(pamh, PAM_USER, (void **)&username);
			snprintf(messages[0],
			    sizeof (messages[0]),
			    PAM_MSG(pamh, 166,
			    "System error: no LDAP passwd record for %s"),
			    username);
			(void) __pam_display_msg(pamh,
			    PAM_ERROR_MSG, 1, messages, NULL);
		}
		retcode = PAM_USER_UNKNOWN;
		goto out;
	}

	/* Convert the user name to a distinguished name */
	ldaprc = __ns_ldap_uid2dn(ldap_pwd->pw_name, NULL, &dn, NULL, &errorp);

	if (retcode = ldap_to_pamerror(ldaprc) != PAM_SUCCESS)
		goto out;

	if (strcmp(field, "attr") == 0) {

		while (*data != NULL) {

			/* Changing the login shell */
			if ((value = attr_match("AUTHTOK_SHELL", *data))
			    != NULL) {

				if (strcmp(value, "1") != 0) {
					if (!nowarn) {
						snprintf(messages[0],
						    sizeof (messages[0]),
						    PAM_MSG(pamh, 169,
						    "%s: System error %s: "
						    "shell is set illegally"),
						    value, LDAP_MSG);
						(void) __pam_display_msg(pamh,
						    PAM_ERROR_MSG, 1,
						    messages, NULL);
					}
					retcode = PAM_SYSTEM_ERR;
					goto out;
				}
				ldap_pwd->pw_shell =
				    getloginshell(pamh, ldap_pwd->pw_shell,
				    privileged, nowarn);
				/* if NULL, shell unchanged */
				if (ldap_pwd->pw_shell == NULL) {
					retcode = -1;
					goto out;
				}

				newattr = (char **)calloc(2, sizeof (char *));
				newattr[0] = strdup(ldap_pwd->pw_shell);
				newattr[1] = NULL;
				if ((attrs[count] = (ns_ldap_attr_t *)calloc(1,
					sizeof (ns_ldap_attr_t))) == NULL) {
					retcode = -1;
					goto out;
				}
				attrs[count]->attrname = "loginshell";
				attrs[count]->attrvalue = newattr;
				attrs[count]->value_count = 1;
				count++;
				data++;
				continue;
			}

			/* Changing the home directory */
			if ((value = attr_match("AUTHTOK_HOMEDIR", *data))
			    != NULL) {

				if (strcmp(value, "1") != 0) {
					if (!nowarn) {
						snprintf(messages[0],
						    sizeof (messages[0]),
						    PAM_MSG(pamh, 170,
						    "System error %s: homedir "
						    "is set illegally."),
						    LDAP_MSG);
						(void) __pam_display_msg(
						    pamh,
						    PAM_ERROR_MSG, 1,
						    messages, NULL);
					}
					retcode = PAM_SYSTEM_ERR;
					goto out;
				}
				ldap_pwd->pw_dir =
				    gethomedir(pamh, ldap_pwd->pw_dir, nowarn);
				/* if NULL, homedir unchanged */
				if (ldap_pwd->pw_dir == NULL) {
					retcode = -1;
					goto out;

				}

				newattr = (char **)calloc(2, sizeof (char *));
				newattr[0] = strdup(ldap_pwd->pw_dir);
				newattr[1] = NULL;
				if ((attrs[count] = (ns_ldap_attr_t *)calloc(1,
					sizeof (ns_ldap_attr_t))) == NULL) {
					retcode = -1;
					goto out;
				}
				attrs[count]->attrname = "homedirectory";
				attrs[count]->attrvalue = newattr;
				attrs[count]->value_count = 1;
				count++;
				data++;
				continue;
			}

			/* Changing the GECOS */
			if ((value = attr_match("AUTHTOK_GECOS", *data))
			    != NULL) {

				/* finger information */
				if (strcmp(value, "1") != 0) {
					if (!nowarn) {
						snprintf(messages[0],
						    sizeof (messages[0]),
						    PAM_MSG(pamh, 171,
						    "System error %s: gecos "
						    "is set illegally."),
						    LDAP_MSG);
						(void) __pam_display_msg(
						    pamh,
						    PAM_ERROR_MSG, 1,
						    messages, NULL);
					}
					retcode = PAM_SYSTEM_ERR;
					goto out;
				}
				ldap_pwd->pw_gecos =
				    getfingerinfo(pamh, ldap_pwd->pw_gecos,
				    nowarn);
				/* if NULL, gecos unchanged */
				if (ldap_pwd->pw_gecos == NULL) {
					retcode = -1;
					goto out;
				}

				newattr = (char **)calloc(2, sizeof (char *));
				newattr[0] = strdup(ldap_pwd->pw_gecos);
				newattr[1] = NULL;
				if ((attrs[count] = (ns_ldap_attr_t *)calloc(1,
					sizeof (ns_ldap_attr_t))) == NULL) {
					retcode = -1;
					goto out;
				}
				attrs[count]->attrname = "gecos";
				attrs[count]->attrvalue = newattr;
				attrs[count]->value_count = 1;
				count++;
				data++;
				continue;
			}
		} /* while */
		attrs[count] = NULL;

		/* make the ldap call */
		ldaprc = __ns_ldap_repAttr(dn,
			(const ns_ldap_attr_t * const *)attrs,
			authp, 0, &errorp);
		retcode = ldap_to_pamerror(ldaprc);

	} else if (strcmp(field, "passwd") == 0) {

		/* Need to add {crypt} before the userpassword */
		sprintf(buffer, "{crypt}%s", ldap_pwd->pw_passwd);
		newattr = (char **)calloc(2, sizeof (char *));
		newattr[0] = strdup(buffer);
		newattr[1] = NULL;
		if ((attrs[0] = (ns_ldap_attr_t *)calloc(1,
				sizeof (ns_ldap_attr_t))) == NULL) {
			retcode = -1;
			goto out;
		}
		attrs[0]->attrname = "userpassword";
		attrs[0]->attrvalue = newattr;
		attrs[0]->value_count = 1;
		attrs[1] = NULL;
		count = 1;

		/* Change password */
		ldaprc = __ns_ldap_repAttr(dn,
		    (const ns_ldap_attr_t * const *)attrs,
		    authp, 0, &errorp);

		retcode = ldap_to_pamerror(ldaprc);
	}

out:
	for (i = 0; i < count && attrs[i] != NULL; i++) {
		if (attrs[i]->attrvalue != NULL) {
			char **ptr = attrs[i]->attrvalue;
			if (ptr[0] != NULL)
				free(ptr[0]);
			free(attrs[i]->attrvalue);
		}
		free(attrs[i]);
	}

	if (errorp)
		__ns_ldap_freeError(&errorp);

	return (retcode);

}
