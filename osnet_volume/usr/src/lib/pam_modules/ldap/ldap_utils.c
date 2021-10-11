/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldap_utils.c	1.2	99/07/14 SMI"

#include "ldap_headers.h"
#include <malloc.h>

static int		copy_passwd_structs(struct passwd **, struct passwd *,
				struct spwd **, struct spwd *);
static struct passwd    *getpwnam_from(const char *, int);
static struct spwd	*getspnam_from(const char *, int);
static void		nss_ldap_passwd(nss_db_params_t *);
static void		nss_ldap_shadow(nss_db_params_t *);
static int		str2passwd(const char *, int, void *, char *, int);
static int		str2spwd(const char *, int, void *, char *, int);
static char 		*gettok(char **);
static bool_t		getfield(const char **, const char *, int, void *);

/* ******************************************************************** */
/*									*/
/* 		Utilities Functions					*/
/*									*/
/* ******************************************************************** */

/*
 * __free_msg():
 *	free storage for messages used in the call back "pam_conv" functions
 */

void
__free_msg(
	int num_msg,
	struct pam_message *msg)
{
	int 			i;
	struct pam_message 	*m;

	if (msg) {
		m = msg;
		for (i = 0; i < num_msg && m != NULL; i++, m++) {
			if (m->msg)
				free(m->msg);
		}
		free(msg);
	}
}

/*
 * __free_resp():
 *	free storage for responses used in the call back "pam_conv" functions
 */

void
__free_resp(
	int num_msg,
	struct pam_response *resp)
{
	int			i;
	struct pam_response	*r;

	if (resp) {
		r = resp;
		for (i = 0; i < num_msg && r != NULL; i++, r++) {
			if (r->resp)
				free(r->resp);
		}
		free(resp);
	}
}

/*
 * __display_errmsg():
 *	display error message by calling the call back functions
 *	provided by the application through "pam_conv" structure
 */

int
__display_errmsg(
	int (*conv_funp)(),
	int num_msg,
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE],
	void *conv_apdp)
{
	struct pam_message	*msg;
	struct pam_message	*m;
	struct pam_response	*resp;
	int			i;
	int			k;
	int			retcode;

	msg = (struct pam_message *)calloc(num_msg,
					sizeof (struct pam_message));
	if (msg == NULL) {
		return (PAM_CONV_ERR);
	}
	m = msg;

	i = 0;
	k = num_msg;
	resp = NULL;
	while (k--) {
		/*
		 * fill out the pam_message structure to display error message
		 */
		if (m != NULL) {
			m->msg_style = PAM_ERROR_MSG;
			m->msg = (char *)malloc(PAM_MAX_MSG_SIZE);
			if (m->msg != NULL)
				(void) strcpy(m->msg,
						(const char *)messages[i]);
			else
				return (PAM_BUF_ERR);
			m++;
			i++;
		}
	}

	/*
	 * Call conv function to display the message,
	 * ignoring return value for now
	 */
	retcode = conv_funp(num_msg, &msg, &resp, conv_apdp);
	__free_msg(num_msg, msg);
	__free_resp(num_msg, resp);
	return (retcode);
}

/*
 * __get_authtok():
 *	get authentication token by calling the call back functions
 *	provided by the application through "pam_conv" structure
 */

int
__get_authtok(
	int (*conv_funp)(),
	int num_msg,
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE],
	void *conv_apdp,
	struct pam_response **ret_respp)
{
	struct pam_message	*msg;
	struct pam_message	*m;
	int			i;
	int			k;
	int			retcode;

	i = 0;
	k = num_msg;

	msg = (struct pam_message *)calloc(num_msg,
						sizeof (struct pam_message));
	if (msg == NULL) {
		return (PAM_CONV_ERR);
	}
	m = msg;

	while (k--) {
		/*
		 * fill out the message structure to display error message
		 */
		if (m != NULL) {
			m->msg_style = PAM_PROMPT_ECHO_OFF;
			m->msg = (char *)malloc(PAM_MAX_MSG_SIZE);
			if (m->msg != NULL)
				(void) strcpy(m->msg, (char *)messages[i]);
			else
				return (PAM_BUF_ERR);
			m++;
			i++;
		}
	}

	/*
	 * Call conv function to display the prompt,
	 * ignoring return value for now
	 */
	retcode = conv_funp(num_msg, &msg, ret_respp, conv_apdp);
	__free_msg(num_msg, msg);
	return (retcode);
}

/*
 * __ldap_to_pamerror():
 *	converts Native LDAP errors to an equivalent PAM error
 */
int
__ldap_to_pamerror(int ldaperror)
{
	switch (ldaperror) {
		case NS_LDAP_SUCCESS:
			return (PAM_SUCCESS);

		case NS_LDAP_OP_FAILED:
			return (PAM_PERM_DENIED);

		case NS_LDAP_MEMORY:
			return (PAM_BUF_ERR);

		case NS_LDAP_CONFIG:
			return (PAM_SERVICE_ERR);

		case NS_LDAP_INTERNAL:
		case NS_LDAP_NOTFOUND:
		case NS_LDAP_PARTIAL:
		case NS_LDAP_INVALID_PARAM:
			return (PAM_SYSTEM_ERR);

		default:
			return (PAM_SYSTEM_ERR);

	}
}

/*
 * repository_to_string():
 *	Returns a string for different types of repository configurations
 */
char *
repository_to_string(int repository)
{
	switch (repository) {
		case (PAM_REP_FILES | PAM_REP_LDAP):
			return ("files and ldap");
		case (PAM_REP_FILES | PAM_REP_NISPLUS):
			return ("files and nisplus");
		case (PAM_REP_FILES | PAM_REP_NIS):
			return ("files and nis");
		case PAM_REP_NISPLUS:
			return ("nisplus");
		case PAM_REP_NIS:
			return ("nis");
		case PAM_REP_FILES:
			return ("files");
		case PAM_REP_LDAP:
			return ("ldap");
		case PAM_REP_DEFAULT:
			return ("default");
		default:
			return ("bad repository");
	}
}


/*
 * ck_perm():
 * 	Check the permission of the user specified by "usrname".
 *
 * 	It returns PAM_PERM_DENIED if (1) the user has a NULL pasword or
 * 	shadow password file entry, or (2) the caller's uid is not
 *	not equivalent to the uid specified by the user's password file entry.
 */

int
ck_perm(
	pam_handle_t *pamh,
	int repository,
	struct passwd **pwd,
	struct spwd **shpwd,
	uid_t uid,
	int debug,
	int nowarn)
{
	char	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	struct passwd *local_pwdp;
	struct spwd *local_shpwdp;
	char *prognamep;
	char *usrname;
	int retcode = 0;

	if (debug)
		syslog(LOG_DEBUG,
			"ck_perm() called: repository=%s",
			repository_to_string(repository));

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
							!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
							!= PAM_SUCCESS) {
		*pwd = NULL;  *shpwd = NULL;
		return (retcode);
	}

	/* Do not bother with anything other than LDAP */
	if (repository != PAM_REP_LDAP)
		return (PAM_PERM_DENIED);

	/* Check if user exists and get pwd */

	local_pwdp = getpwnam_from(usrname, PAM_REP_LDAP);
	local_shpwdp = getspnam_from(usrname, PAM_REP_LDAP);

	if (local_pwdp == NULL || local_shpwdp == NULL)
		return (PAM_USER_UNKNOWN);

	if (uid && uid != local_pwdp->pw_uid) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 32,
				"%s %s: Permission denied"), prognamep,
				LDAP_MSG);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
		}
		*pwd = NULL;
		*shpwd = NULL;
		return (PAM_PERM_DENIED);
	}

	return (copy_passwd_structs(pwd, local_pwdp,
					shpwd, local_shpwdp));

}

/*
 * copy_passwd_structs():
 *	Copies password and shadow structures
 */
static int
copy_passwd_structs(
	struct passwd **pwd,
	struct passwd *local_pwd,
	struct spwd **shpwd,
	struct spwd *local_shpwd)
{

	/* copy the passwd information */
	if ((*pwd = (struct passwd *)
		calloc(1, sizeof (struct passwd))) == NULL)
		return (PAM_BUF_ERR);

	if (local_pwd == NULL)
		goto out;

	if (local_pwd->pw_name) {
		if (((*pwd)->pw_name = strdup(local_pwd->pw_name)) == NULL)
			goto out;
	}
	if (local_pwd->pw_passwd) {
		if (((*pwd)->pw_passwd = strdup(local_pwd->pw_passwd)) == NULL)
			goto out;
	}

	(*pwd)->pw_uid = local_pwd->pw_uid;
	(*pwd)->pw_gid = local_pwd->pw_gid;

	if (local_pwd->pw_gecos) {
		if (((*pwd)->pw_gecos = strdup(local_pwd->pw_gecos)) == NULL)
			goto out;
	}
	if (local_pwd->pw_dir) {
		if (((*pwd)->pw_dir = strdup(local_pwd->pw_dir)) == NULL)
			goto out;
	}
	if (local_pwd->pw_shell) {
		if (((*pwd)->pw_shell = strdup(local_pwd->pw_shell)) == NULL)
			goto out;
	}

	/* copy the shadow passwd information */
	if ((*shpwd = (struct spwd *)
		calloc(1, sizeof (struct spwd))) == NULL)
		goto out;
	**shpwd = *local_shpwd;

	if (local_shpwd == NULL)
		goto out;

	if (local_shpwd->sp_namp) {
		if (((*shpwd)->sp_namp = strdup(local_shpwd->sp_namp)) == NULL)
			goto out;
	}
	if (local_shpwd->sp_pwdp) {
		if (((*shpwd)->sp_pwdp = strdup(local_shpwd->sp_pwdp)) == NULL)
			goto out;
	}

	return (PAM_SUCCESS);
out:
	free_passwd_structs(*pwd, *shpwd);
	return (PAM_BUF_ERR);
}

/*
 * free_passwd_structs():
 *	Frees the memory allocated for the password and shadow structures
 *	passed in.
 */
void
free_passwd_structs(
	struct passwd *pwd,
	struct spwd *shpwd)
{
	if (pwd) {
		if (pwd->pw_name)
			free(pwd->pw_name);
		if (pwd->pw_passwd) {
			memset(pwd->pw_passwd, 0, strlen(pwd->pw_passwd));
			free(pwd->pw_passwd);
		}
		if (pwd->pw_gecos)
			free(pwd->pw_gecos);
		if (pwd->pw_dir)
			free(pwd->pw_dir);
		if (pwd->pw_shell)
			free(pwd->pw_shell);
		free(pwd);
	}

	if (shpwd) {
		if (shpwd->sp_namp)
			free(shpwd->sp_namp);
		if (shpwd->sp_pwdp) {
			memset(shpwd->sp_pwdp, 0, strlen(shpwd->sp_pwdp));
			free(shpwd->sp_pwdp);
		}
		free(shpwd);
	}
}

static nss_XbyY_buf_t *buffer;
static DEFINE_NSS_DB_ROOT(db_root);

#define	GETBUF()	\
	NSS_XbyY_ALLOC(&buffer, sizeof (struct passwd), NSS_BUFLEN_PASSWD)

/*
 * getpwnam_from():
 *	Calls the switch backend to retrieve the password structure for
 *	a username
 */
static struct passwd *
getpwnam_from(
	const char *name,
	int rep)
{
	nss_XbyY_buf_t  *b = GETBUF();
	nss_XbyY_args_t arg;

	if (b == 0)
		return (0);

	NSS_XbyY_INIT(&arg, b->result, b->buffer, b->buflen, str2passwd);
	arg.key.name = name;

	switch (rep) {
		case PAM_REP_LDAP:
			nss_search(&db_root, nss_ldap_passwd,
				NSS_DBOP_PASSWD_BYNAME, &arg);
			break;
		default:
			return (NULL);
	}

	return (struct passwd *) NSS_XbyY_FINI(&arg);
}

static nss_XbyY_buf_t *spbuf;
static DEFINE_NSS_DB_ROOT(spdb_root);

#define	GETSPBUF()	\
	NSS_XbyY_ALLOC(&spbuf, sizeof (struct spwd), NSS_BUFLEN_SHADOW)

/*
 * getspnam_from():
 *	Calls the switch backend to retrieve the shadow structure for
 *	a username
 */
static struct spwd *
getspnam_from(
	const char *name,
	int rep)
{
	nss_XbyY_buf_t  *b = GETSPBUF();
	nss_XbyY_args_t arg;

	if (b == 0)
		return (0);

	NSS_XbyY_INIT(&arg, b->result, b->buffer, b->buflen, str2spwd);
	arg.key.name = name;
	switch (rep) {
		case PAM_REP_LDAP:
			nss_search(&spdb_root, nss_ldap_shadow,
			    NSS_DBOP_SHADOW_BYNAME, &arg);
			break;
		default:
			return (NULL);
	}
	return (struct spwd *) NSS_XbyY_FINI(&arg);
}

/*
 * nss_ldap_passwd():
 *	Initializes a strucuture to be used in getpwnam_from
 */
static void
nss_ldap_passwd(nss_db_params_t	*p)
{
	if (p != NULL) {
		p->name = NSS_DBNAM_PASSWD;
		p->flags |= NSS_USE_DEFAULT_CONFIG;
		p->default_config = "ldap";
	}
}

/*
 * nss_ldap_shadow():
 *	Initializes a strucuture to be used in getspnam_from
 */
static void
nss_ldap_shadow(nss_db_params_t	*p)
{
	if (p != NULL) {
		p->name = NSS_DBNAM_SHADOW;
		/* Use config for "passwd" */
		p->config_name    = NSS_DBNAM_PASSWD;
		p->flags |= NSS_USE_DEFAULT_CONFIG;
		p->default_config = "ldap";
	}
}


/*
 * str2passwd():
 * 	Return values: 0 = success, 1 = parse error, 2 = erange ...
 * 	The structure pointer passed in is a structure in the caller's space
 * 	wherein the field pointers would be set to areas in the buffer if
 * 	need be. instring and buffer should be separate areas.
 */
static int
str2passwd(
	const char *instr,
	int lenstr,
	void *ent,
	char *buffer,
	int buflen)
{
	struct passwd	*passwd	= (struct passwd *)ent;
	char		*p, *next;
	int		black_magic;	/* "+" or "-" entry */

	if (lenstr + 1 > buflen) {
		return (NSS_STR_PARSE_ERANGE);
	}
	/*
	 * We copy the input string into the output buffer and
	 * operate on it in place.
	 */
	(void) memcpy(buffer, instr, lenstr);
	buffer[lenstr] = '\0';

	next = buffer;

	passwd->pw_name = p = gettok(&next);		/* username */
	if (*p == '\0') {
		/* Empty username;  not allowed */
		return (NSS_STR_PARSE_PARSE);
	}
	black_magic = (*p == '+' || *p == '-');
	if (black_magic) {
		passwd->pw_uid	= UID_NOBODY;
		passwd->pw_gid	= GID_NOBODY;
		/*
		 * pwconv tests pw_passwd and pw_age == NULL
		 */
		passwd->pw_passwd = "";
		passwd->pw_age	= "";
		/*
		 * the rest of the passwd entry is "optional"
		 */
		passwd->pw_comment = "";
		passwd->pw_gecos = "";
		passwd->pw_dir	= "";
		passwd->pw_shell = "";
	}

	passwd->pw_passwd = p = gettok(&next);		/* password */
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	for (; *p != '\0'; p++) {			/* age */
		if (*p == ',') {
			*p++ = '\0';
			break;
		}
	}
	passwd->pw_age = p;

	p = next;					/* uid */
	if (p == 0 || *p == '\0') {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	if (!black_magic) {
		passwd->pw_uid = strtol(p, &next, 10);
		if (next == p) {
			/* uid field should be nonempty */
			return (NSS_STR_PARSE_PARSE);
		}
		/*
		 * The old code (in 2.0 thru 2.5) would check
		 * for the uid being negative, or being greater
		 * than 60001 (the rfs limit).  If it met either of
		 * these conditions, the uid was translated to 60001.
		 *
		 * Now we just check for negative uids; anything else
		 * is administrative policy
		 */
		if (passwd->pw_uid < 0)
			passwd->pw_uid = UID_NOBODY;
	}
	if (*next++ != ':') {
		if (black_magic)
			p = gettok(&next);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	p = next;					/* gid */
	if (p == 0 || *p == '\0') {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	if (!black_magic) {
		passwd->pw_gid = strtol(p, &next, 10);
		if (next == p) {
			/* gid field should be nonempty */
			return (NSS_STR_PARSE_PARSE);
		}
		/*
		 * gid should be non-negative; anything else
		 * is administrative policy.
		 */
		if (passwd->pw_gid < 0)
			passwd->pw_gid = GID_NOBODY;
	}
	if (*next++ != ':') {
		if (black_magic)
			p = gettok(&next);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	passwd->pw_gecos = passwd->pw_comment = p = gettok(&next);
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	passwd->pw_dir = p = gettok(&next);
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	passwd->pw_shell = p = gettok(&next);
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	/* Better not be any more fields... */
	if (next == 0) {
		/* Successfully parsed and stored */
		return (NSS_STR_PARSE_SUCCESS);
	}
	return (NSS_STR_PARSE_PARSE);
}

typedef const char *constp;

/*
 * getfield():
 * 	1 means success and more input, 0 means error or no more
 */
static bool_t
getfield(
	constp *nextp,
	constp limit,
	int uns,
	void *valp)
{
	constp		p = *nextp;
	char		*endfield;
	char		numbuf[12];  /* Holds -2^31 and trailing \0 */
	int		len;
	long		x;
	unsigned long	ux;

	if (p == 0 || p >= limit) {
		return (0);
	}
	if (*p == ':') {
		p++;
		*nextp = p;
		return (p < limit);
	}
	if ((len = limit - p) > sizeof (numbuf) - 1) {
		len = sizeof (numbuf) - 1;
	}
	/*
	 * We want to use strtol() and we have a readonly non-zero-terminated
	 *   string, so first we copy and terminate the interesting bit.
	 *   Ugh.  (It's convenient to terminate with a colon rather than \0).
	 */
	if ((endfield = memccpy(numbuf, p, ':', len)) == 0) {
		if (len != limit - p) {
			/* Error -- field is too big to be a legit number */
			return (0);
		}
		numbuf[len] = ':';
		p = limit;
	} else {
		p += (endfield - numbuf);
	}
	if (uns) {
		ux = strtoul(numbuf, &endfield, 10);
		*((unsigned int *)valp) = (unsigned int) (ux&0xffffffff);
	} else {
		x = strtol(numbuf, &endfield, 10);
		*((int *)valp) = (int) x;
	}
	if (*endfield != ':') {
		/* Error -- expected <integer><colon>, got something else */
		return (0);
	}
	*nextp = p;
	return (p < limit);
}

/*
 *  str2spwd() -- convert a string to a shadow passwd entry.  The parser is
 *	more liberal than the passwd or group parsers;  since it's legitimate
 *	for almost all the fields here to be blank, the parser lets one omit
 *	any number of blank fields at the end of the entry.  The acceptable
 *	forms for '+' and '-' entries are the same as those for normal entries.
 *  === Is this likely to do more harm than good?
 *
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
static int
str2spwd(
	const char *instr,
	int lenstr,
	void *ent,
	char *buffer,
	int buflen)
{
	struct spwd	*shadow	= (struct spwd *)ent;
	const char	*p = instr, *limit;
	char		*bufp;
	int		lencopy, black_magic;

	limit = p + lenstr;
	if ((p = memchr(instr, ':', lenstr)) == 0 ||
		++p >= limit ||
		(p = memchr(p, ':', limit - p)) == 0) {
		lencopy = lenstr;
		p = 0;
	} else {
		lencopy = p - instr;
		p++;
	}
	if (lencopy + 1 > buflen) {
		return (NSS_STR_PARSE_ERANGE);
	}
	(void) memcpy(buffer, instr, lencopy);
	buffer[lencopy] = 0;

	black_magic = (*instr == '+' || *instr == '-');
	shadow->sp_namp = bufp = buffer;
	shadow->sp_pwdp	= 0;
	shadow->sp_lstchg = -1;
	shadow->sp_min	= -1;
	shadow->sp_max	= -1;
	shadow->sp_warn	= -1;
	shadow->sp_inact = -1;
	shadow->sp_expire = -1;
	shadow->sp_flag	= 0;

	if ((bufp = strchr(bufp, ':')) == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	*bufp++ = '\0';

	shadow->sp_pwdp = bufp;
	if (instr == 0) {
		if ((bufp = strchr(bufp, ':')) == 0) {
			if (black_magic)
				return (NSS_STR_PARSE_SUCCESS);
			else
				return (NSS_STR_PARSE_PARSE);
		}
		*bufp++ = '\0';
		p = bufp;
	} /* else p was set when we copied name and passwd into the buffer */

	if (!getfield(&p, limit, 0, &shadow->sp_lstchg))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_min))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_max))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_warn))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_inact))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_expire))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 1, &shadow->sp_flag))
			return (NSS_STR_PARSE_SUCCESS);
	if (p != limit) {
		/* Syntax error -- garbage at end of line */
		return (NSS_STR_PARSE_PARSE);
	}
	return (NSS_STR_PARSE_SUCCESS);
}

/*
 * gettok():
 *	Used in reading a password/shadow entry
 */
static char *
gettok(char **nextpp)
{
	char	*p = *nextpp;
	char	*q = p;
	char	c;

	if (p == 0) {
		return (0);
	}
	while ((c = *q) != '\0' && c != ':') {
		q++;
	}
	if (c == '\0') {
		*nextpp = 0;
	} else {
		*q++ = '\0';
		*nextpp = q;
	}
	return (p);
}

/*
 * authenticate():
 *	Attempts to authenticate the user with the password passed in.
 *	Returns PAM_AUTH_ERR if authorization fails otherwise PAM_SUCCESS
 *	Authentication is checked by calling __ns_ldap_auth.
 *	Returns the auth structure in authpp if authorization is successful
 *	otherwise it will return NULL.
 */
int
authenticate(
	Auth_t **authpp,
	char *usrname,
	char *pwd)
{
	int		result = PAM_AUTH_ERR;
	ns_ldap_error_t	*errorp = NULL;
	int		authtype;
	int		*sectypep = NULL;
	void		**configVal = NULL;
	void		**countpp = NULL;
	int		numauths = 0;
	int		ldaprc;
	int		retcode;
	int		i;

	if ((*authpp = (Auth_t *)calloc(1, sizeof (Auth_t))) == NULL)
		return (PAM_BUF_ERR);

	/* Fill in the user name and password */
	if ((usrname == NULL) || (pwd == NULL))
		goto out;

	(*authpp)->cred.unix_cred.userID = strdup(usrname);
	(*authpp)->cred.unix_cred.passwd = strdup(pwd);

	/*
	 * The Authentication mechanism and the transport security types
	 * are obtained from the LDAP cache file directly.
	 * If the cache file is corrupted, then an error is returned
	 * and no modification is allowed to take place.
	 */

	/* Load Transport Sec type */
	ldaprc = __ns_ldap_getParam(NULL, NS_LDAP_TRANSPORT_SEC_P,
	    &configVal, &errorp);
	if (retcode = __ldap_to_pamerror(ldaprc) != PAM_SUCCESS)
		goto out;

	sectypep = (int *) (*configVal);
	(*authpp)->security = *sectypep;
	(void) __ns_ldap_freeParam(&configVal);
	configVal = NULL;

	/*
	 * Load the Authentication Mechanism(s). This case is more
	 * complicated as there could be multiple authentication mechanisms.
	 * An attempt is made at authentication for each of the mechanisms
	 * in the order they appear in the cache file. If the first one
	 * fails, then the next one is tried and so on until either the
	 * authorization succeeds or all the authorization mechanisms
	 * are exhausted.
	 */
	ldaprc = __ns_ldap_getParam(NULL, NS_LDAP_AUTH_P, &configVal, &errorp);
	if (retcode = __ldap_to_pamerror(ldaprc) != PAM_SUCCESS)
		goto out;

	/* Count the number of auth mechanisms in the cache file */
	for (countpp = configVal; *countpp != NULL; countpp++)
		numauths++;

	for (i = 0; i < numauths; i++) {
		(*authpp)->type = *(int *)(configVal[i]);

		if ((*authpp)->type != NS_LDAP_AUTH_NONE) {

			if (__ns_ldap_auth(*authpp, NULL, 0, &errorp)
							== NS_LDAP_SUCCESS) {
				result = PAM_SUCCESS;
				goto out;
			}

		}

	}

out:
	if (configVal)
		(void) __ns_ldap_freeParam(&configVal);

	if (errorp)
		__ns_ldap_freeError(&errorp);

	if (result != PAM_SUCCESS)
		__ns_ldap_freeAuth(authpp);

	return (result);

}
