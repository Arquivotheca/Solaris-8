/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)unix_utils.c	1.16	99/07/07 SMI"

#include "unix_headers.h"

static char 	*spskip();
static int	special_case();
static int	illegal_input();
static int	copy_passwd_structs(struct passwd **, struct passwd *,
			struct spwd **, struct spwd *);

void		free_passwd_structs(struct passwd *, struct spwd *);

static int base64_2_index(char, int *);

static int get_and_set_seckey(pam_handle_t *, const char *, keylen_t, algtype_t,
				const char *, int *, int *, int *, int, int);

/*
 * Support for old SunOS passwd aging
 */
char base64table [64] =
	{ '.', '/', '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
	'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b',
	'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
	'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z' };

/*
 * given a base64-encoded character, return the index (0-63) into
 * the table for that character
 */
static int
base64_2_index(char base64_value, int *index)
{
	int i;

	if (index == 0)
		return (PAM_SYSTEM_ERR);
	*index = -1;

	for (i = 0; i < 64; i++) {
		if (base64table[i] == base64_value) {
			*index = i;
			break;
		}
	}
	return ((*index < 0) ? PAM_SYSTEM_ERR : PAM_SUCCESS);
}

/*
 * given a password aging encoded string, parse it into the
 * relevant max, min, and lastchg fields
 */
int
decode_passwd_aging(char *aging_value, int *max, int *min, int *lastchg)
{
	int status = PAM_SYSTEM_ERR;
	int low = 0;
	int high = 0;

	if (aging_value == 0 || max == 0 || min == 0 || lastchg == 0)
		goto out;
	*max = -1;	*min = -1;	*lastchg = -1;

	if (aging_value[0] == ',')
		aging_value++;

	/* see if password aging field is empty */
	if (*aging_value == '\0')
		goto out;

	/* get max field and convert from weeks to days */
	if ((status = base64_2_index(*aging_value, max)) != PAM_SUCCESS)
		goto out;
	*max = *max * 7;

	/* get min field and convert from weeks to days */
	aging_value++;
	if (*aging_value == '\0') {
		/* no min or lstchg value */
		*min = 0;
		*lastchg = 0;
		status = PAM_SUCCESS;
		goto out;
	}
	if ((status = base64_2_index(*aging_value, min)) != PAM_SUCCESS)
		goto out;
	*min = *min * 7;

	/* get lastchg field and convert from weeks to days */
	aging_value++;
	if (*aging_value == '\0') {
		/* no lstchg value */
		*lastchg = 0;
		status = PAM_SUCCESS;
		goto out;
	}
	if ((status = base64_2_index(*aging_value, &low)) != PAM_SUCCESS)
		goto out;
	aging_value++;
	if (*aging_value != '\0' &&
	    (status = base64_2_index(*aging_value, &high)) != PAM_SUCCESS)
			goto out;
	*lastchg = (low + high * 64) * 7;

	aging_value++;
	if (*aging_value != '\0') {
		/* only allow 2 char lastchg field */
		status = PAM_SYSTEM_ERR;
		goto out;
	}

	status = PAM_SUCCESS;
out:
	if (status != PAM_SUCCESS) {
		*max = -1;	*min = -1;	*lastchg = -1;
	}
	return (status);
}

#ifdef PAM_NEED_SUNOS_PASSWD_AGING_ENCODING_FUNCTIONALITY

/*
 * Once a password has been updated with the passwd or login utility,
 * this function is called to update the password aging values.
 * The input is the original aging value (,xxxx) and the output
 * is the new aging value to be appended to the encrypted password.
 *
 * allowable values for orig_aging_value (x denotes a single character)
 *	,xxxx	-- max, min, and 2 char lastchg
 *	,xxx	-- max, min, and 1 char lastchg
 *	,x	-- max
 *	,xx	-- max, min
 */
int
encode_passwd_aging(char *orig_aging_value, char **new_aging_value)
{
	int	status = PAM_SYSTEM_ERR;
	char	*new_lastchg = 0;
	char	tmp_new[6];	/* room for ',', max, min, lastchg, '\0' */

	if (orig_aging_value == 0 ||
	    *orig_aging_value == '\0' ||
	    new_aging_value == 0)
		goto out;
	*new_aging_value = 0;
	memset(tmp_new, 0, sizeof (tmp_new));

	/* first get the new lastchg value */
	if ((status = encode_lastchg
			((int)DAY_NOW,
			&new_lastchg)) != PAM_SUCCESS)
		goto out;

	/* now set up the new aging value */
	if (strlen(orig_aging_value) == 1) {
		/*
		 * only a comma is present in the orig_aging_value
		 */
		status = PAM_SYSTEM_ERR;
		goto out;
	} else if (strlen(orig_aging_value) == 2) {
		if (strncmp(orig_aging_value, ",.", strlen(",.")) == 0) {
			/*
			 * special case -- just remove aging field
			 */
			tmp_new[0] = '\0';
		} else {
			/*
			 * only a max field exists (,x) --
			 * add a min field of 0 and the new lastchg
			 */
			strncpy(tmp_new, orig_aging_value, 2);
			tmp_new[2] = '.';
			strncpy(tmp_new + 3,
				new_lastchg,
				strlen(new_lastchg) + 1);
		}
	} else if (strncmp(orig_aging_value, ",..", strlen(",..")) == 0) {
		/*
		 * special case -- just remove aging field
		 */
		tmp_new[0] = '\0';
	} else {
		/*
		 * either of these scenarios (,xxxx  ,xxx  ,xx) --
		 * just update the new lastchg value
		 */
		strncpy(tmp_new, orig_aging_value, 3);
		strncpy(tmp_new + 3, new_lastchg, strlen(new_lastchg) + 1);
	}

	/* return the new aging value */
	if (((*new_aging_value) = (char *)strdup(tmp_new)) == 0) {
		status = PAM_BUF_ERR;
		goto out;
	}
	status = PAM_SUCCESS;
out:
	if (status != PAM_SUCCESS) {
		if (*new_aging_value)
			free(*new_aging_value);
		*new_aging_value = 0;
	}
	if (new_lastchg)
		free(new_lastchg);
	return (status);

}

/*
 * given an integer as the lastchg value, encode it into 2 chars
 */
static int
encode_lastchg(int lastchg, char **base64_lastchg)
{
	char tmp_lastchg[3];
	int max_lastchg = 64 + 64*64;
	unsigned int low = 0, high = 0;
	unsigned char lastchg_bytes[sizeof (int)];

	/* first convert from SVR4 days to BSD weeks */
	lastchg = lastchg / 7;

	if (lastchg < 0 || lastchg > max_lastchg || base64_lastchg == 0)
		return (PAM_SYSTEM_ERR);

	*base64_lastchg = 0;
	memset(tmp_lastchg, 0, sizeof (tmp_lastchg));

	high = shiftright((unsigned int)lastchg, 6);
	low = lastchg & 0x0000003f;

	tmp_lastchg[0] = base64table[low];
	tmp_lastchg[1] = base64table[high];
	tmp_lastchg[2] = '\0';

	if (((*base64_lastchg) = (char *)strdup(tmp_lastchg)) == 0)
		return (PAM_BUF_ERR);

	return (PAM_SUCCESS);
}

/*
 * shift the bits "shift_val" to the right
 */
static unsigned int
shiftright(unsigned int bytes, int shift_val)
{
	return ((bytes >> shift_val) & 0x0000003f);
}

#endif	/* PAM_NEED_SUNOS_PASSWD_AGING_ENCODING_FUNCTIONALITY */

/* ******************************************************************** */
/*									*/
/* 		Utilities Functions					*/
/*									*/
/* ******************************************************************** */

#ifdef PAM_SECURE_RPC
/*
 * Get the secret key for the given netname, key length, and algorithm
 * type and send it to keyserv if the given pw decrypts it.  Update the
 * following counter args as necessary: get_seckey_cnt, good_pw_cnt, and
 * set_seckey_cnt.
 *
 * Returns 0 on malloc failure, else 1.
 */
static int
get_and_set_seckey(
	pam_handle_t	*pamh,			/* in */
	const char	*netname,		/* in */
	keylen_t	keylen,			/* in */
	algtype_t	algtype,		/* in */
	const char	*pw,			/* in */
	int		*get_seckey_cnt,	/* out */
	int		*good_pw_cnt,		/* out */
	int		*set_seckey_cnt,	/* out */
	int		flags,			/* in */
	int		debug)			/* in */
{
	char *skey;
	int skeylen;
	char	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];

	assert(netname && keylen && pw && get_seckey_cnt &&
		good_pw_cnt && set_seckey_cnt);

	skeylen = BITS2NIBBLES(keylen) + 1;

	if (!(skey = (char *)malloc(skeylen)))
		return (0);

	if (getsecretkey_g(netname, keylen, algtype, skey, skeylen, pw)) {
		(*get_seckey_cnt)++;

		if (skey[0]) {
			/* password does decrypt secret key */
			(*good_pw_cnt)++;
			if (key_setnet_g(netname, skey, keylen, NULL, 0,
						algtype) >= 0)
				(*set_seckey_cnt)++;
			else
				if (debug)
					syslog(LOG_DEBUG,
	"PAM: get_and_set_seckey: could not set secret key for keytype %d-%d",
						keylen, algtype);
		} else
			if (pamh && !(flags & PAM_SILENT)) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 162,
	"Password does not decrypt secret key (type = %d-%d) for '%s'."),
					keylen, algtype, netname);
				__pam_display_msg(
					pamh,
					PAM_ERROR_MSG, 1,
					messages, NULL);
			}
	} else
		if (debug)
			syslog(LOG_DEBUG,
	"PAM: get_and_set_seckey: could not get secret key for keytype %d-%d",
				keylen, algtype);

	free(skey);
	return (1);
}

/*
 * Establish the Secure RPC secret key(s) for the given uid using
 * the given password to decrypt the secret key(s), and store it with
 * the key service.
 *
 * If called with a nonzero 'reestablish' parameter, the keys of any
 * type listed in the NIS+ security cf are obtained from the name
 * service, decrypted, and stored even if the keyserver already has any
 * keys stored for the uid.  If the 'reestablish' parameter is zero,
 * the function will not try to reset any keys.  It will return immediately
 * with ESTKEY_ALREADY.
 *
 * Returns one of the following codes:
 *   ESTKEY_ALREADY - reestablish flag was zero, and key(s) were already set.
 *   ESTKEY_SUCCESS - successfully obtained, decrypted, and set at least one key
 *   ESTKEY_NOCREDENTIALS - the user has no credentials.
 *   ESTKEY_BADPASSWD - the password supplied didn't decrypt any key(s)
 *   ESTKEY_CANTSETKEY - decrypted key(s), but couldn't store any key(s) with
 *			    the key service.
 *
 * If netnamebuf is a non-NULL pointer, the netname will be returned in
 * netnamebuf, provided that the return status is not
 * ESTKEY_NOCREDENTIALS or ESTKEY_ALREADY.  If non-NULL, the
 * netnamebuf pointer must point to a buffer of length at least
 * MAXNETNAMELEN+1 characters.
 */

int
establish_key(
	pam_handle_t	*pamh,
	uid_t		uid,
	char		*password,
	int		reestablish,
	char		*netnamebuf,
	int		flags)

{
	char    	netname[MAXNETNAMELEN+1];
	uid_t   	orig_uid;
	int		get_seckey_cnt	= 0;
	int		good_pw_cnt	= 0;
	int		set_seckey_cnt	= 0;
	int		valid_mech_cnt	= 0;
	int		debug = 0; /* XXX */
	mechanism_t	**mechs;
	mechanism_t	**mpp;

	if (debug)
		syslog(LOG_DEBUG,
			"PAM: establish_key: uid = %d, reestablish = %d",
			uid, reestablish);

	orig_uid = geteuid();
	if (seteuid(uid) == -1)
		/* can't set uid */
		return (ESTKEY_NOCREDENTIALS);

	if (! getnetname(netname)) {
		if (debug)
			syslog(LOG_DEBUG,
				"PAM: establish_key: getnetname failed");
		/* can't construct netname */
		(void) seteuid(orig_uid);
		return (ESTKEY_NOCREDENTIALS);
	}

	if (!password) {
		if (debug)
			syslog(LOG_DEBUG,
				"PAM: establish_key: password is NULL");
		(void) seteuid(orig_uid);
		return (ESTKEY_BADPASSWD);
	}

	if (mechs = __nis_get_mechanisms(FALSE)) {

		if (!reestablish && key_secretkey_is_set_g(0, 0)) {
			/*
			 * At least one key for a mech type listed in the
			 * security cf is already established and we are
			 * not to reestablish.
			 */
			(void) seteuid(orig_uid);
			__nis_release_mechanisms(mechs);
			return (ESTKEY_ALREADY);
		}

		/*
		 * Try all the mechs listed in the NIS+ security cf (or until
		 * AUTH_DES (192bit DH keys) entry is reached).
		 */
		for (mpp = mechs; *mpp; mpp++) {
			mechanism_t *mp = *mpp;

			if (AUTH_DES_COMPAT_CHK(mp)) {
				__nis_release_mechanisms(mechs);
				goto try_auth_des;
			}

			if (!VALID_MECH_ENTRY(mp))
				continue;

			if (debug)
				syslog(LOG_DEBUG,
			"PAM: establish_keys_g: trying key type = %d-%d",
					mp->keylen, mp->algtype);

			valid_mech_cnt++;
			if (!get_and_set_seckey(pamh, netname, mp->keylen,
						mp->algtype,
						password, &get_seckey_cnt,
						&good_pw_cnt,
						&set_seckey_cnt, flags, debug))
				syslog(LOG_ERR,
					"get_and_set_seckey malloc failure");
		}
		__nis_release_mechanisms(mechs);
		/*
		 * Some GSS DH mechs were configed but the AUTH_DES compat
		 * entry was not found, but we want to try AUTH_DES anyways
		 * for the benefit of non-NIS+ services (e.g. NFS) that
		 * may depend on the classic des 192bit key being set.
		 */
		goto try_auth_des;

	/*
	 *  No useable mechs found in NIS+ security cf thus
	 *  fallback to AUTH_DES compat.
	 */
	} else {
		if (debug)
			syslog(LOG_DEBUG,
	"PAM: establish_keys_g: no valid mechs found...trying AUTH_DES...");

		if (!reestablish && key_secretkey_is_set_g(AUTH_DES_KEYLEN,
							AUTH_DES_ALGTYPE)) {
			/*
			 * AUTH_DES key is already established
			 * and we are not to reestablish.
			 */
			(void) seteuid(orig_uid);
			return (ESTKEY_ALREADY);
		}

try_auth_des:
		if (! get_and_set_seckey(pamh, netname, AUTH_DES_KEYLEN,
					AUTH_DES_ALGTYPE,
					password, &get_seckey_cnt,
					&good_pw_cnt, &set_seckey_cnt,
					flags, debug))
			syslog(LOG_ERR,
				"get_and_set_seckey malloc failure\n");

	}

	if (debug) {
		syslog(LOG_DEBUG,
			"PAM: establish_keys_g: mech key totals:\n");
		syslog(LOG_DEBUG,
			"PAM: establish_keys_g: %d valid mechanism(s)",
			valid_mech_cnt);
		syslog(LOG_DEBUG,
			"PAM: establish_keys_g: %d secret key(s) retrieved",
			get_seckey_cnt);
		syslog(LOG_DEBUG,
			"PAM: establish_keys_g: %d passwd decrypt successes",
			good_pw_cnt);
		syslog(LOG_DEBUG,
			"PAM: establish_keys_g: %d secret key(s) set",
			set_seckey_cnt);
	}

	if (get_seckey_cnt == 0) {
		(void) seteuid(orig_uid);
		return (ESTKEY_NOCREDENTIALS);
	}

	if (netnamebuf) {
		/* return copy of netname in caller's buffer */
		(void) strcpy(netnamebuf, netname);
	}

	if (good_pw_cnt == 0) {
		(void) seteuid(orig_uid);
		return (ESTKEY_BADPASSWD);
	}

	if (set_seckey_cnt == 0) {
		(void) seteuid(orig_uid);
		return (ESTKEY_CANTSETKEY);
	}

	/*
	 *  At least one secret key successfully decrypted
	 *  and stored in keyserv at this point.
	 */
	(void) seteuid(orig_uid);
	if (debug)
		syslog(LOG_DEBUG,
			"PAM: establish_key: returning SUCCESS");
	return (ESTKEY_SUCCESS);
}
#endif /* PAM_SECURE_RPC */

/* ******************************************************************** */
/*									*/
/* 		Utilities Functions					*/
/*									*/
/* ******************************************************************** */

/*
 * Check is we are on a YP master server.  Returns 1 if so, 0 otherwise.
 */
static int on_nis_master(char *prognamep) {
	char *domain;
	char *master;
	char thishost[MAXHOSTNAMELEN];
	int retval;
	if (yp_get_default_domain(&domain) != 0) {
	    syslog(LOG_ERR, "%s: can't get domain", prognamep);
	    return (0);
	}
	if (yp_master(domain, "passwd.byname", &master) != 0) {
	    syslog(LOG_ERR, "%s: can't get master for passwd map",
		    prognamep);
	    return (0);
	}

	if (gethostname(thishost, MAXHOSTNAMELEN) == -1) {
	    syslog(LOG_ERR, "%s: can't get local host's name",
		    prognamep);
	    free(master);
	    return (0);
	}
	if (strncmp(thishost, master, MAXHOSTNAMELEN) == 0)
	    retval = 1;
	else
	    retval = 0;
	free(master);
	return (retval);
}

/*
 * ck_perm():
 * 	Check the permission of the user specified by "usrname".
 *
 * 	It returns PAM_PERM_DENIED if (1) the user has a NULL pasword or
 * 	shadow password file entry, or (2) the caller is not root and
 *	its uid is not equivalent to the uid specified by the user's
 *	password file entry.
 */

int
ck_perm(pamh, repository, domain, pwd, shpwd, privileged, passwd_res, uid,
	debug, nowarn)
	pam_handle_t *pamh;
	int	repository;
	char *domain;
	struct passwd **pwd;
	struct spwd **shpwd;
	int *privileged;
	void **passwd_res;
	uid_t uid;
	int debug;
	int nowarn;
{
	FILE	*pwfp, *spfp;
	char	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	struct passwd local_pwd, *local_pwdp;
	struct spwd local_shpwd, *local_shpwdp;
	char pwdbuf[1024], shpwdbuf[1024];
	char *prognamep;
	char *usrname;
	int retcode = 0;
#ifdef PAM_NISPLUS
	char    buf[NIS_MAXNAMELEN+1];
	nis_name local_principal;
	nis_name pwd_domain;
#endif

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

	if (repository == PAM_REP_FILES) {
		if (((pwfp = fopen(PASSWD, "r")) == NULL) ||
		    ((spfp = fopen(SHADOW, "r")) == NULL)) {
			*pwd = NULL;  *shpwd = NULL;
			syslog(LOG_ERR,
				"ck_perm: can not open passwd/shadow file");
			return (PAM_PERM_DENIED);
		}
		while ((local_pwdp = fgetpwent_r(pwfp, &local_pwd, pwdbuf,
					sizeof (pwdbuf))) != NULL)
			if (strcmp(local_pwd.pw_name, usrname) == 0)
				break;
		while ((local_shpwdp = fgetspent_r(spfp, &local_shpwd,
				shpwdbuf, sizeof (shpwdbuf))) != NULL)
			if (strcmp(local_shpwd.sp_namp, usrname) == 0)
				break;
		(void) fclose(pwfp);
		(void) fclose(spfp);
		if (local_pwdp == NULL || local_shpwdp == NULL) {
			*pwd = NULL;  *shpwd = NULL;
			return (PAM_USER_UNKNOWN);
		}

		if (uid != 0 && uid != local_pwd.pw_uid) {
			/*
			 * Change passwd for another person:
			 * Even if you are nis+ admin, you can't do anything
			 * locally. Don't bother to continue.
			 */
				if (!nowarn) {
					snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 140,
						"%s %s: Permission denied"),
						prognamep,
						UNIX_MSG);
					snprintf(messages[1],
						sizeof (messages[1]),
						PAM_MSG(pamh, 141,
				"%s %s: Can't change local passwd file\n"),
						prognamep, UNIX_MSG);
					(void) __pam_display_msg(
						pamh, PAM_ERROR_MSG, 2,
						messages, NULL);
				}
				*pwd = NULL;  *shpwd = NULL;
				return (PAM_PERM_DENIED);
		}
		return (copy_passwd_structs(pwd, local_pwdp,
						shpwd, local_shpwdp));
	}

#ifdef PAM_LDAP
	if (repository == PAM_REP_LDAP) {
		/*
		 * Special case root: don't bother to get root from ldap.
		 */
		if (strcmp(usrname, "root") == 0) {
			*pwd = NULL;	*shpwd = NULL;
			return (PAM_USER_UNKNOWN);
		}

		/* Check if user exists and get pwd */

		local_pwdp = getpwnam_from(usrname, PAM_REP_LDAP);
		local_shpwdp = getspnam_from(usrname, PAM_REP_LDAP);

		if (local_pwdp == NULL || local_shpwdp == NULL)
			return (PAM_USER_UNKNOWN);

		if (uid && uid != local_pwdp->pw_uid) {
			if (!nowarn) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 140,
					"%s %s: Permission denied"), prognamep,
					LDAP_MSG);
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);
			}
			*pwd = NULL;	*shpwd = NULL;
			return (PAM_PERM_DENIED);
		}

		return (copy_passwd_structs(pwd, local_pwdp,
						shpwd, local_shpwdp));

	}
#endif /* PAM_LDAP */

#ifdef PAM_NIS
	if (repository == PAM_REP_NIS) {
		/*
		 * Special case root: don't bother to get root from nis(yp).
		 */
		if (strcmp(usrname, "root") == 0) {
			*pwd = NULL;	*shpwd = NULL;
			return (PAM_USER_UNKNOWN);
		}

		/* get pwd struct from yp */

		local_pwdp = getpwnam_from(usrname, PAM_REP_NIS);
		local_shpwdp = getspnam_from(usrname, PAM_REP_NIS);

		if (local_pwdp == NULL || local_shpwdp == NULL)
			return (PAM_USER_UNKNOWN);

		if (uid && uid != local_pwdp->pw_uid) {
			if (!nowarn) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 140,
					"%s %s: Permission denied"), prognamep,
					NIS_MSG);
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);
			}
			*pwd = NULL;	*shpwd = NULL;
			return (PAM_PERM_DENIED);
		}
		/* only privileged user for YP is root on the master server */
		if (uid == 0 && on_nis_master(prognamep))
		    *privileged = 1;
		return (copy_passwd_structs(pwd, local_pwdp,
						shpwd, local_shpwdp));
	}
#endif /* PAM_NIS */

#ifdef PAM_NISPLUS
	if (repository == PAM_REP_NISPLUS) {
		/*
		 * Special case root: don't bother to get root from nis+.
		 */
		if (strcmp(usrname, "root") == 0) {
			*pwd = NULL;	*shpwd = NULL;
			return (PAM_USER_UNKNOWN);
		}

		if (debug)
			syslog(LOG_DEBUG, "ck_perm(): NIS+ domain=%s", domain);

		/*
		 * We need to use user id to
		 * make any nis+ request. But don't give up the super
		 * user power yet. It may be needed elsewhere.
		 */
		(void) setuid(0);	/* keep real user id as root */
		(void) seteuid(uid);

		local_shpwdp = getspnam_from(usrname, PAM_REP_NISPLUS);
		local_pwdp = getpwnam_from(usrname, PAM_REP_NISPLUS);

		if (local_pwdp == NULL || local_shpwdp == NULL) {
			if (debug)
				syslog(LOG_DEBUG,
				"PAM: ck_perm(): could not get shadow pw info");
			return (PAM_USER_UNKNOWN);
		}

		/*
		 * local_principal is internal, it is not meant to be free()ed
		 */
		local_principal = nis_local_principal();

		if ((9 + strlen(usrname) + strlen(domain) + PASSTABLELEN) >
		    (size_t) NIS_MAXNAMELEN) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 140,
				"%s %s: Permission denied"),
				prognamep, NISPLUS_MSG);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			*pwd = NULL;	*shpwd = NULL;
			return (PAM_PERM_DENIED);
		}

		snprintf(buf,
			sizeof (buf),
			"[name=%s],%s.%s",
			usrname,
			PASSTABLE,
			domain);
		if (buf[strlen(buf) - 1] != '.')
			(void) strcat(buf, ".");

		/*
		 * We must use an authenticated handle to get the cred
		 * table information for the user we want to modify the
		 * cred info for. If we can't even read that info, we
		 * definitely wouldn't have modify permission. Well..
		 */
		*passwd_res = (void *) nis_list(buf,
			USE_DGRAM+FOLLOW_LINKS+FOLLOW_PATH+MASTER_ONLY,
			NULL, NULL);
		if (((nis_result *)(*passwd_res))->status != NIS_SUCCESS) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 140,
				"%s %s: Permission denied"),
				prognamep, NISPLUS_MSG);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			*pwd = NULL;	*shpwd = NULL;
			return (PAM_PERM_DENIED);
		}

		pwd_domain =
			NIS_RES_OBJECT((nis_result *)(*passwd_res))->zo_domain;
		if (strcmp(nis_leaf_of(pwd_domain), "org_dir") == 0) {
			pwd_domain = nis_domain_of(
			NIS_RES_OBJECT((nis_result *)(*passwd_res))->zo_domain);

		}
		*privileged = __nis_isadmin(local_principal, "passwd",
			pwd_domain);

		if (debug)
			syslog(LOG_DEBUG,
				"PAM: ck_perm: returning pw tbl info");

		return (copy_passwd_structs(pwd, local_pwdp,
						shpwd, local_shpwdp));
	}
#endif /* PAM_NISPLUS */

	if (!nowarn) {
		snprintf(messages[0],
			sizeof (messages[0]),
			PAM_MSG(pamh, 142,
			"%s %s: System error: repository out of range"),
			prognamep, UNIX_MSG);
		(void) __pam_display_msg(pamh, PAM_ERROR_MSG, 1,
			messages, NULL);
	}
	*pwd = NULL;	*shpwd = NULL;
	return (PAM_PERM_DENIED);
}

/*
 * attr_match():
 *
 *	Check if the attribute name in string s1 is equivalent to
 *	that in string s2.
 *	s1 is either name, or name=value
 *	s2 is name=value
 *	if names match, return value of s2, else NULL
 */

char *
attr_match(s1, s2)
	register char *s1, *s2;
{
	while (*s1 == *s2++)
		if (*s1++ == '=')
		return (s2);
	if (*s1 == '\0' && *(s2-1) == '=')
		return (s2);
	return (NULL);
}

/*
 * attr_find():
 *
 *	Check if the attribute name in string s1 is present in the
 *	attribute=value pairs array pointed by s2.
 *	s1 is name
 *	s2 is an array of name=value pairs
 *	if s1 match the name of any one of the name in the name=value pairs
 *	pointed by s2, then 1 is returned; else 0 is returned
 */

int
attr_find(s1, s2)
	register char *s1, *s2[];
{
	int 	i;
	char 	*sa, *sb;

	i = 0;
	while (s2[i] != NULL) {
		sa = s1;
		sb = s2[i];
		while (*sa++ == *sb++) {
			if ((*sa == '\0') && (*sb == '='))
				return (1); /* find */
		}
		i++;
	}

	return (0); /* not find */
}

/*
 * free_setattr():
 *	free storage pointed by "setattr"
 */

void
free_setattr(setattr)
	char * setattr[];
{
	int i;

	for (i = 0; setattr[i] != NULL; i++)
		free(setattr[i]);

}

/*
 * setup_attr():
 *	allocate memory and copy in attribute=value pair
 *	into the array of attribute=value pairs pointed to
 *	by "dest_attr"
 */

void
setup_attr(dest_attr, k, attr, value)
	char *dest_attr[];
	int k;
	char attr[];
	char value[];
{
	if (attr != NULL) {
		dest_attr[k] = (char *)calloc(PAM_MAX_ATTR_SIZE, sizeof (char));
		(void) strncpy(dest_attr[k], attr, PAM_MAX_ATTR_SIZE);
		(void) strncat(dest_attr[k], value, PAM_MAX_ATTR_SIZE);
	} else
		dest_attr[k] = NULL;
}

#ifdef PAM_NISPLUS
static char *
spskip(p)
	register char *p;
{
	while (*p && *p != ':' && *p != '\n')
		++p;
	if (*p == '\n')
		*p = '\0';
	else if (*p)
		*p++ = '\0';
	return (p);
}

void
nisplus_populate_age(enobj, sp)
	struct nis_object *enobj;
	struct spwd *sp;
{
	char *oldage, *p, *end;
	long x;

	/*
	 * shadow (col 7)
	 */

	sp->sp_lstchg = -1;
	sp->sp_min = -1;
	sp->sp_max = -1;
	sp->sp_warn = -1;
	sp->sp_inact = -1;
	sp->sp_expire = -1;
	sp->sp_flag = 0;

	if ((p = ENTRY_VAL(enobj, 7)) == NULL)
		return;
	oldage = strdup(p);

	p = oldage;

	x = strtol(p, &end, 10);
	if (end != memchr(p, ':', strlen(p)))
		return;
	if (end != p)
		sp->sp_lstchg = (int)x;

	p = spskip(p);
	x = strtol(p, &end, 10);
	if (end != memchr(p, ':', strlen(p)))
		return;
	if (end != p)
		sp->sp_min = (int)x;

	p = spskip(p);
	x = strtol(p, &end, 10);
	if (end != memchr(p, ':', strlen(p)))
		return;
	if (end != p)
		sp->sp_max = (int)x;

	p = spskip(p);
	x = strtol(p, &end, 10);
	if (end != memchr(p, ':', strlen(p)))
		return;
	if (end != p)
		sp->sp_warn = (int)x;

	p = spskip(p);
	x = strtol(p, &end, 10);
	if (end != memchr(p, ':', strlen(p)))
		return;
	if (end != p)
		sp->sp_inact = (int)x;

	p = spskip(p);
	x = strtol(p, &end, 10);
	if (end != memchr(p, ':', strlen(p)))
		return;
	if (end != p)
		sp->sp_expire = (int)x;

	p = spskip(p);
	x = strtol(p, &end, 10);
	if ((end != memchr(p, ':', strlen(p))) &&
	    (end != memchr(p, '\n', strlen(p))))
		return;
	if (end != p)
		sp->sp_flag = (unsigned int)x;

	free(oldage);
}
#endif /* PAM_NISPLUS */

/*
 * getloginshell() displays old login shell and asks for new login shell.
 *	The new login shell is then returned to calling function.
 */
char *
getloginshell(pamh, oldshell, privileged, nowarn)
	pam_handle_t *pamh;
	char *oldshell;
	int privileged;
	int nowarn;
{
	char newshell[PAM_MAX_MSG_SIZE];
	char *cp, *valid, *getusershell();
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	struct pam_response *ret_resp = (struct pam_response *)0;

	if (oldshell == 0 || *oldshell == '\0')
		oldshell = DEFSHELL;

	if (privileged == 0) {
		mutex_lock(&_priv_lock);
		setusershell();
		for (valid = getusershell(); valid; valid = getusershell())
			if (strcmp(oldshell, valid) == 0)
				break;
		mutex_unlock(&_priv_lock);
		if (valid == NULL) {
			if (!nowarn) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 143,
				"Cannot change from restricted shell %s"),
					oldshell);
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);
			}
			return (NULL);
		}
	}
	snprintf(messages[0],
		sizeof (messages[0]),
		PAM_MSG(pamh, 144, "Old shell: %s"), oldshell);
	(void) __pam_display_msg(pamh, PAM_TEXT_INFO, 1, messages, NULL);
	snprintf(messages[0],
		sizeof (messages[0]),
		PAM_MSG(pamh, 145, "New shell: "));
	(void) __pam_get_input(pamh, PAM_PROMPT_ECHO_ON,
		1, messages, NULL, &ret_resp);
	strncpy(newshell, ret_resp->resp, PAM_MAX_MSG_SIZE);
	newshell[PAM_MAX_RESP_SIZE-1] = '\0';
	__pam_free_resp(1, ret_resp);

	cp = strchr(newshell, '\n');
	if (cp)
		*cp = '\0';
	if (newshell[0] == '\0' || (strcmp(newshell, oldshell) == 0)) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 146,
				"Login shell unchanged."));
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
		}
		return (NULL);
	}

	mutex_lock(&_priv_lock);
	/*
	 * XXX:
	 * Keep in mind that, for whatever this validation is worth,
	 * a root on a machine can edit /etc/shells and get any shell
	 * accepted as a valid shell in the NIS+ table.
	 */
	if (privileged) {
		valid = newshell;
	} else {
		setusershell();
		for (valid = getusershell(); valid; valid = getusershell()) {
			/*
			 * Allow user to give shell w/o preceding pathname.
			 */
			if (newshell[0] == '/') {
				cp = valid;
			} else {
				cp = strrchr(valid, '/');
				if (cp == 0)
					cp = valid;
				else
					cp++;
			}
			if (strcmp(newshell, cp) == 0) {
				strncpy(newshell, valid, strlen(valid));
				break;
			}
		}
	}
	mutex_unlock(&_priv_lock);

	if (valid == NULL) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 147,
				"%s is unacceptable as a new shell"),
				newshell);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
		}
		return (NULL);
	}
	if (access(newshell, X_OK) < 0) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 148,
				"warning: %s is unavailable on this machine"),
				newshell);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
		}
	}
	return (strdup(newshell));
}

/*
 * Get name.
 */
char *
getfingerinfo(pamh, old_gecos, nowarn)
	pam_handle_t *pamh;
	char	*old_gecos;
	int nowarn;
{
	char 	new_gecos[PAM_MAX_MSG_SIZE];
	char	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	struct pam_response *ret_resp = (struct pam_response *)0;

	if (!nowarn) {
		snprintf(messages[0],
			sizeof (messages[0]),
			PAM_MSG(pamh, 149,
			"Default values are printed inside of '[]'."));
		snprintf(messages[1],
			sizeof (messages[1]),
			PAM_MSG(pamh, 150,
			"To accept the default, type <return>."));
		snprintf(messages[2],
			sizeof (messages[2]),
			PAM_MSG(pamh, 151,
			"To have a blank entry, type the word 'none'."));
		(void) __pam_display_msg(pamh, PAM_TEXT_INFO,
			3, messages, NULL);
	}

	/*
	 * Get name.
	 */
	do {
		snprintf(messages[0], sizeof (messages[0]), " ");
		(void) __pam_display_msg(pamh, PAM_TEXT_INFO, 1,
			messages, NULL);
		snprintf(messages[0],
			sizeof (messages[0]),
			PAM_MSG(pamh, 152,
			"Name [%s]: "), old_gecos);
		(void) __pam_get_input(pamh, PAM_PROMPT_ECHO_ON,
			1, messages, NULL, &ret_resp);
		strncpy(new_gecos, ret_resp->resp, PAM_MAX_MSG_SIZE);
		new_gecos[PAM_MAX_MSG_SIZE-1] = '\0';
		__pam_free_resp(1, ret_resp);
		if (special_case(new_gecos, old_gecos))
			break;
	} while (illegal_input(pamh, new_gecos, nowarn));
	if (strcmp(new_gecos, old_gecos) == 0) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 153,
				"Finger information unchanged."));
			(void) __pam_display_msg(pamh, PAM_TEXT_INFO,
				1, messages, NULL);
		}
		return (NULL);
	}
	return (strdup(new_gecos));
}

/*
 * Get Home Dir.
 */
char *
gethomedir(pamh, olddir, nowarn)
	pam_handle_t *pamh;
	char *olddir;
	int nowarn;
{
	char newdir[PAM_MAX_MSG_SIZE];
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	struct pam_response *ret_resp = (struct pam_response *)0;

	if (!nowarn) {
		snprintf(messages[0],
			sizeof (messages[0]),
			PAM_MSG(pamh, 149,
			"Default values are printed inside of '[]'."));
		snprintf(messages[1],
			sizeof (messages[1]),
			PAM_MSG(pamh, 150,
			"To accept the default, type <return>."));
		snprintf(messages[2],
			sizeof (messages[2]),
			PAM_MSG(pamh, 151,
			"To have a blank entry, type the word 'none'."));
		(void) __pam_display_msg(pamh, PAM_TEXT_INFO,
			3, messages, NULL);
	}
	do {
		snprintf(messages[0], sizeof (messages[0]), " ");
		(void) __pam_display_msg(pamh, PAM_TEXT_INFO,
				1, messages, NULL);
		snprintf(messages[0], sizeof (messages[0]), PAM_MSG(pamh, 154,
			"Home Directory [%s]: "), olddir);
		(void) __pam_get_input(pamh, PAM_PROMPT_ECHO_ON,
			1, messages, NULL, &ret_resp);
		strncpy(newdir, ret_resp->resp, PAM_MAX_MSG_SIZE);
		newdir[PAM_MAX_MSG_SIZE-1] = '\0';
		__pam_free_resp(1, ret_resp);
		if (special_case(newdir, olddir))
			break;
	} while (illegal_input(pamh, newdir, nowarn));
	if (strcmp(newdir, olddir) == 0) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 155,
				"Homedir information unchanged."));
			(void) __pam_display_msg(pamh, PAM_TEXT_INFO,
				1, messages, NULL);
		}
		return (NULL);
	}
	return (strdup(newdir));
}

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
 * Prints an error message if a ':' or a newline is found in the string.
 * A message is also printed if the input string is too long.
 * The password sources use :'s as seperators, and are not allowed in the "gcos"
 * field.  Newlines serve as delimiters between users in the password source,
 * and so, those too, are checked for.  (I don't think that it is possible to
 * type them in, but better safe than sorry)
 *
 * Returns '1' if a colon or newline is found or the input line is too long.
 */
static int
illegal_input(pamh, input_str, nowarn)
	pam_handle_t *pamh;
	char *input_str;
	int nowarn;
{
	char *ptr;
	int error_flag = 0;
	int length = (int)strlen(input_str);
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];

	if (strchr(input_str, ':')) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 156,
				"':' is not allowed."));
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
		}
		error_flag = 1;
	}
	if (input_str[length-1] == '\n') {
		/*
		 * Delete newline by shortening string by 1.
		 */
		input_str[length-1] = '\0';
		length--;
	}
	if (length > PAM_MAX_MSG_SIZE -1) {
		/* the newline and the '\0' eat up two characters */
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 157,
				"Maximum number of characters allowed is %d."),
				PAM_MAX_MSG_SIZE-2);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
		}
		error_flag = 1;
	}
	/*
	 * Don't allow control characters, etc in input string.
	 */
	for (ptr = input_str; *ptr != '\0'; ptr++) {
		/* 040 is ascii char "space" */
		if ((int) *ptr < 040) {
			if (!nowarn) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 158,
					"Control characters are not allowed."));
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);
			}
			error_flag = 1;
			break;
		}
	}
	return (error_flag);
}



/*
 *  special_case returns true when either the default is accepted
 *  (str = '\n'), or when 'none' is typed.  'none' is accepted in
 *  either upper or lower case (or any combination).  'str' is modified
 *  in these two cases.
 */
static int
special_case(str, default_str)
	char *str, *default_str;
{
	static char word[] = "none\n";
	char *ptr, *wordptr;

	/*
	 *  If the default is accepted, then change the old string do the
	 *  default string.
	 */
	if (*str == '\n') {
		(void) strcpy(str, default_str);
		return (1);
	}
	/*
	 *  Check to see if str is 'none'.  (It is questionable if case
	 *  insensitivity is worth the hair).
	 */
	wordptr = word - 1;
	for (ptr = str; *ptr != '\0'; ++ptr) {
		++wordptr;
		if (*wordptr == '\0')	/* then words are different sizes */
			return (0);
		if (*ptr == *wordptr)
			continue;
		if (isupper(*ptr) && (tolower(*ptr) == *wordptr))
			continue;
		/*
		 * At this point we have a mismatch, so we return
		 */
		return (0);
	}
	/*
	 * Make sure that words are the same length.
	 */
	if (*(wordptr+1) != '\0')
		return (0);
	/*
	 * Change 'str' to be the null string
	 */
	*str = '\0';
	return (1);
}

static int
copy_passwd_structs(struct passwd **pwd, struct passwd *local_pwd,
		struct spwd **shpwd, struct spwd *local_shpwd)
{

	/* copy the passwd information */
	if ((*pwd = (struct passwd *)
		calloc(1, sizeof (struct passwd))) == NULL)
		return (PAM_BUF_ERR);
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

void
free_passwd_structs(struct passwd *pwd, struct spwd *shpwd)
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
