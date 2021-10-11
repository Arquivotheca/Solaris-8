/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_hostuser.cc	1.4	98/08/06 SMI"

// Free structures associated with hostent 'h'

#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>	// for inet_addr
#include <arpa/nameser.h> // for NS_INADDRSZ
#include <rpcsvc/nis.h>
#include <string.h>
#include <stdlib.h>

#include "fnsp_internal.hh"
#include "fnsp_hostuser.hh"
#include "FNSP_Address.hh"

#define	HOST_CNAME_COL 0
#define	HOST_ALIAS_COL 1
#define	HOST_ADDR_COL 2

#define	PASSWD_NAME_COL 0
#define	PASSWD_UID_COL 2
#define	PASSWD_HOMEDIR_COL 5
#define	PASSWD_SHELL_COL 6
#define	PASSWD_SHADOW_COL 7
#define	PASSWD_COLS 8

#define	CRED_PNAME_COL 0

#define	CRED_TABLE "cred.org_dir"
#define	PASSWD_TABLE "passwd.org_dir"
#define	HOST_TABLE "hosts.org_dir"
#define MAIL_TABLE "mail_aliases.org_dir"

#define MAIL_EXPANSION_COL 1
#define MAIL_TABLE_INDEX "alias"

#define	PASSWD_LINESZ 1024	// max size of passwd entry
#define	SHADOW_LINESZ 1024	// max size of shadow entry

static inline void
free_nis_result(nis_result *res)
{
	if (res)
		nis_freeresult(res);
}

void
FNSP_free_hostent(struct hostent *h)
{
	if (h != 0) {
		int i;

		// delete canonical name
		free(h->h_name);

		// delete aliases
		for (i = 0; h->h_aliases[i] != 0; i++)
			free(h->h_aliases[i]);
		free(h->h_aliases);

		// delete addresses -- all allocated in a single block
		free(h->h_addr_list);

		// delete structure itself
		free(h);
	}
}


// Extract host information from given nis_result structure and return
// it in the form of a 'hostent' structure.
// Set 'status' appropriately upon return.

static struct hostent *
extract_hostent(nis_result *res, unsigned &status)
{
	struct hostent *he;
	nis_object *obj;
	int nobjs, i, j, len;
	char *alias;
	in_addr_t addr;
	in_addr_t *addrs;
	int n_addrs = 0;

	he = (struct hostent *)calloc(1, sizeof (struct hostent));
	if (he == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	he->h_addrtype = AF_INET;
	he->h_length = NS_INADDRSZ;

	nobjs = res->objects.objects_len;
	status = FN_SUCCESS;  // initialize

	for (i = 0; i < nobjs; i++) {
		obj = &(res->objects.objects_val[i]);
		if (obj->zo_data.zo_type != NIS_ENTRY_OBJ) {
			status = FN_E_CONFIGURATION_ERROR;  // ??? what error
			break;
		}

		if (i == 0) {
			// Canonical name
			// Should be the same in all entries.
			len = ENTRY_LEN(obj, HOST_CNAME_COL);
			if (len < 2) {
				status = FN_E_CONFIGURATION_ERROR;
				break;
			}
			he->h_name = (char *)malloc(len);
			if (he->h_name == 0) {
				status = FN_E_INSUFFICIENT_RESOURCES;
				break;
			}
			strncpy(he->h_name,
			    ENTRY_VAL(obj, HOST_CNAME_COL), len);

			// Alias
			// Since we did a list using the alias name,
			// the alias name found in each entry should
			// be identical.
			he->h_aliases = (char **)calloc(2, sizeof (char *));
			if (he->h_aliases == 0) {
				status = FN_E_INSUFFICIENT_RESOURCES;
				break;
			}
			len = ENTRY_LEN(obj, HOST_ALIAS_COL);
			alias = ENTRY_VAL(obj, HOST_ALIAS_COL);
			if (len >= 2 && strcmp(alias, he->h_name) != 0) {
				he->h_aliases[0] = (char *)malloc(len);
				if (he->h_aliases[0] == 0) {
					status = FN_E_INSUFFICIENT_RESOURCES;
					break;
				}
				strncpy(he->h_aliases[0], alias, len);
			}

			// Addresses
			// Allocate storage for maximum number of address
			// pointers *and* addresses in one block.
			he->h_addr_list = (char **)malloc(
			    (nobjs + 1) * sizeof (&addr) +
			    nobjs * sizeof (addr));
			if (he->h_addr_list == 0) {
				status = FN_E_INSUFFICIENT_RESOURCES;
				break;
			}
			he->h_addr_list[0] = 0;
			addrs = (in_addr_t *)(&he->h_addr_list[nobjs + 1]);

		}

		// Addresses
		if (ENTRY_LEN(obj, HOST_ADDR_COL) < 2) {
			status = FN_E_CONFIGURATION_ERROR;
			break;
		}
		addr = inet_addr(ENTRY_VAL(obj, HOST_ADDR_COL));
		if (addr == (in_addr_t)-1) {
			continue;	// illegal address
		}
		// Check for duplicate address.
		for (j = 0; j < n_addrs; j++) {
			if (addr == addrs[j]) {
				break;
			}
		}
		if (j == n_addrs) {	// new address found
			he->h_addr_list[n_addrs] = (char *)&addrs[n_addrs];
			addrs[n_addrs++] = addr;
		}
	}

	if (status != FN_SUCCESS) {
		FNSP_free_hostent(he);
		return (0);
	} else {
		he->h_addr_list[n_addrs] = 0;
		return (he);
	}
}


// Lookup a user or host name in the passwd.org_dir/hosts.org_dir table of
// the "dirname" domain.  Return the NIS+ object.  Caller must call
// free_nis_result().  Use the same flags as getXbyY(), except host
// lookups are done without EXPAND_NAME, and "access_flags" are added in.

static nis_result *
lookup_name(const FN_string &dirname, const char *table, const FN_string &name,
    unsigned int access_flags, char *lookup_index = 0)
{
	char sname[NIS_MAXNAMELEN + 1];
	const unsigned char *domain = dirname.str();

	if (lookup_index)
		sprintf(sname, "[%s=\"%s\"],%s.%s", lookup_index,
		    name.str(), table, domain);
	else
		sprintf(sname, "[name=\"%s\"],%s.%s", name.str(),
		    table, domain);
	if (sname[strlen(sname) - 1] != '.') {
		strcat(sname, ".");
	}
	return (nis_list(sname,
	    FOLLOW_PATH|FOLLOW_LINKS|USE_DGRAM|access_flags, NULL, NULL));
}


// Obtain home domain of host.
//
// 1.  Lookup host 'aname' (alias name) in hosts.org_dir table of directory.
// 2.  Extract domainname from entry found.
// 3.  Extract host information from entry if asked for.
//
// Set 'status' appropriately upon return.

FN_string *
FNSP_find_host_entry(const FN_string &dirname,
    const FN_string &hostname,
    unsigned int access_flags,
    unsigned &status,
    struct hostent **hostinfo)
{
	nis_result *res = 0;
	int nobjs;
	FN_string *domainname = 0;

	// Step 1.  Get desired host entries from table.
	res = lookup_name(dirname, HOST_TABLE, hostname, access_flags);
	status = FNSP_map_result(res, 0);

	if (status == FN_SUCCESS) {
		nobjs = res->objects.objects_len;
		if (nobjs <= 0) {
			status = FN_E_UNSPECIFIED_ERROR;
			// ??? what is appropriate
			goto cleanup;
		}
		// Step 2.  Extract domain name from first object.
		domainname = FNSP_orgname_of(
		    (unsigned char *)(res->objects.objects_val[0].zo_domain),
		    status, 1);
		if (status != FN_SUCCESS)
			goto cleanup;

		// Step 3.  Extract host information from entries.
		if (hostinfo != 0)
			*hostinfo = extract_hostent(res, status);
	} else if (status == FN_E_NOT_A_CONTEXT)
		// hosts table not found
		status = FN_E_CONFIGURATION_ERROR; // %%% CONTEXT_NOT_FOUND

cleanup:
	free_nis_result(res);
	if (status != FN_SUCCESS) {
		delete domainname;
		domainname = 0;
	}
	return (domainname);
}


// Obtain home domain name of user.
// 1.  Lookup the user 'username' in the passwd.org_dir table of the
//	directory 'dirname'.
// 2.  Extract uid from result and lookup corresponding entry in cred.org_dir.
// 3.  If cred entry found, extract home domain from principal name in cred.
// 4.  If cred entry not found, extract home domain from passwd entry in (1).
//
FN_string *
FNSP_find_user_entry(const FN_string &dirname, const FN_string &username,
    unsigned int access_flags, unsigned &status)
{
	char sname[NIS_MAXNAMELEN + 1];
	nis_result *pass_res = 0, *cred_res = 0;
	FN_string *domainname = 0;
	int nobjs;

	// Step 1.  Get desired passwd entries from table.
	pass_res = lookup_name(dirname, PASSWD_TABLE, username, access_flags);
	status = FNSP_map_result(pass_res, 0);

	if (status == FN_SUCCESS) {
		nobjs = pass_res->objects.objects_len;
		if (nobjs <= 0) {
			status = FN_E_UNSPECIFIED_ERROR;
			// ??? what is appropriate
			goto cleanup;
		}
		char *uid_str = ENTRY_VAL(&(pass_res->objects.objects_val[0]),
		    PASSWD_UID_COL);
		int len = ENTRY_LEN(&(pass_res->objects.objects_val[0]),
		    PASSWD_UID_COL);
		uid_t uid;
		if (len == 0) {
			uid = -1;
		} else {
			char *endnum;
			uid_str[len - 1] = 0;
			// Note:: Updating object
			uid = strtol(uid_str, &endnum, 10);
			if (*endnum != 0) {
				uid = -1;
			}
		}

		if (uid != -1) {
			// Step 2.  Get cred entry of user
			sprintf(sname, "[auth_name=%d,auth_type=LOCAL],%s.%s",
			    uid, CRED_TABLE, dirname.str());
			if (sname[strlen(sname)-1] != '.')
				strcat(sname, ".");
			cred_res = nis_list(sname,
			    USE_DGRAM|NO_AUTHINFO|FOLLOW_LINKS|access_flags,
			    NULL, NULL);
			if (cred_res && cred_res->status == NIS_SUCCESS) {
				// Step 3. Extract domain from principal name
				// in cred entry.
				char *domainchar;
				domainchar = nis_domain_of(
				    ENTRY_VAL(cred_res->objects.objects_val,
				    CRED_PNAME_COL));
				domainname = new FN_string((unsigned char *)
				    domainchar);
				status = FN_SUCCESS;
				goto cleanup;
			}
		}
		// Step 4.  Extract domain name from first passwd object.
		domainname = FNSP_orgname_of((unsigned char *)
		    (pass_res->objects.objects_val[0].zo_domain),
		    status, 1);
	} else if (status == FN_E_NOT_A_CONTEXT)
		// password table not found
		status = FN_E_CONFIGURATION_ERROR; // %%% CONTEXT_NOT_FOUND

cleanup:
	free_nis_result(cred_res);
	free_nis_result(pass_res);
	if (status != FN_SUCCESS) {
		delete domainname;
		domainname = 0;
	}
	return (domainname);
}


// Lookup the contents of the entry for "username" in the
// passwd.org_dir table of the "dirname" domain.  Set the "passwd" and
// "shadow" strings in the formats of /etc/passwd and /etc/shadow.
// Return 0 on failure.
//
int
FNSP_find_passwd_shadow(const FN_string &dirname, const FN_string &username,
    unsigned int access_flags, FN_string &passwd, FN_string &shadow,
    unsigned int &status)
{
	char pline[PASSWD_LINESZ] = "";
	char sline[SHADOW_LINESZ] = "";
	int plen = 0;
	int slen = 0;
	nis_result *res =
	    lookup_name(dirname, PASSWD_TABLE, username, access_flags);

	if (res->status == NIS_NOTFOUND ||
	    res->status == NIS_NOSUCHNAME ||
	    res->status == NIS_NOSUCHTABLE) {
		status = FN_E_NO_SUCH_ATTRIBUTE;
	} else {
		status = FNSP_map_result(res);
	}
	if (status == FN_SUCCESS && NIS_RES_NUMOBJ(res) < 1) {
		status = FN_E_CONFIGURATION_ERROR;
	}
	if (status != FN_SUCCESS) {
		goto out;
	}

	// If we get more than one nis_object, ignore all but the
	// first (but this shouldn't happen).
	nis_object *obj;
	obj = NIS_RES_OBJECT(res);

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ) {
		status = FN_E_CONFIGURATION_ERROR;
		goto out;
	}

	// Concatenate fields, separated by colons.
	// Assumption:  NIS+ values are stored with trailing '\0'.
	int i;
	for (i = 0; i < PASSWD_COLS; i++) {
		char *val = ENTRY_VAL(obj, i);
		int sz = ENTRY_LEN(obj, i);

		if (i < PASSWD_SHADOW_COL) {
			// Add to password line.
			if ((plen += sz) >= PASSWD_LINESZ) {
				status = FN_E_CONFIGURATION_ERROR;
				break;
			}
			if (sz >= 2) {
				strncat(pline, val, sz);
			}
			if (i < PASSWD_SHELL_COL) {
				strcat(pline, ":");
			}
		}
		if (i < PASSWD_UID_COL || i == PASSWD_SHADOW_COL) {
			// Add to shadow line.
			if ((slen += sz) >= SHADOW_LINESZ) {
				status = FN_E_CONFIGURATION_ERROR;
				break;
			}
			if (sz >= 2) {
				strncat(sline, val, sz);
			}
			if (i < PASSWD_SHADOW_COL) {
				strcat(sline, ":");
			}
		}
	}
out:
	free_nis_result(res);
	if (status == FN_SUCCESS) {
		// Copy data into FN_strings supplied by caller.
		passwd = (unsigned char *)pline;
		shadow = (unsigned char *)sline;
		return (1);
	} else {
		return (0);
	}
}


// Lookup the home directory of "username" in the passwd.org_dir table
// of the "dirname" domain.
//
static FN_string *
FNSP_get_homedir(const FN_string &dirname, const FN_string &username,
    unsigned int access_flags, unsigned int &status)
{
	FN_string *homedir = NULL;
	nis_result *res =
	    lookup_name(dirname, PASSWD_TABLE, username, access_flags);

	status = FNSP_map_result(res);
	if (status == FN_SUCCESS && NIS_RES_NUMOBJ(res) < 1) {
		status = FN_E_CONFIGURATION_ERROR;
	}
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (NULL);
	}

	// If we get more than one nis_object, ignore all but the
	// first (but this shouldn't happen).
	nis_object *obj;
	obj = NIS_RES_OBJECT(res);

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ) {
		status = FN_E_CONFIGURATION_ERROR;
		free_nis_result(res);
		return (NULL);
	}

	// Assumption:  NIS+ values are stored with trailing '\0'.
	char *val = ENTRY_VAL(obj, PASSWD_HOMEDIR_COL);
	int sz = ENTRY_LEN(obj, PASSWD_HOMEDIR_COL);

	if (sz < 2) {
		status = FN_E_CONFIGURATION_ERROR;
	} else {
		homedir = new FN_string((unsigned char *)val, sz - 1);
		if (homedir == NULL) {
			status = FN_E_INSUFFICIENT_RESOURCES;
		}
	}
	free_nis_result(res);
	return (homedir);
}


FN_string *
FNSP_nisplus_get_homedir(const FNSP_Address &addr,
    const FN_string &username,
    unsigned &status)
{
	FN_string *orgname = FNSP_orgname_of(addr.get_table_name(), status);
	if (orgname == NULL) {
		return (NULL);
	}
	FN_string *homedir = FNSP_get_homedir(*orgname, username,
	    addr.get_access_flags(), status);
	delete orgname;
	return (homedir);
}

// Lookup the contents of the entry for "username" in the
// mail_aliases.org_dir table of the "dirname" domain.  Set the "mailentry".
// Return 0 on failure.
//
static int
FNSP_local_find_mailentry(const FN_string &dirname, const FN_string &username,
    unsigned int access_flags, char *&mailentry, unsigned int &status)
{
	nis_result *res =
	    lookup_name(dirname, MAIL_TABLE, username, access_flags,
	    MAIL_TABLE_INDEX);

	if (res->status == NIS_NOTFOUND ||
	    res->status == NIS_NOSUCHNAME ||
	    res->status == NIS_NOSUCHTABLE) {
		status = FN_E_NO_SUCH_ATTRIBUTE;
	} else {
		status = FNSP_map_result(res);
	}
	if (status == FN_SUCCESS && NIS_RES_NUMOBJ(res) < 1) {
		status = FN_E_CONFIGURATION_ERROR;
	}
	if (status != FN_SUCCESS) {
		goto mail_out;
	}

	// If we get more than one nis_object, ignore all but the
	// first (but this shouldn't happen).
	nis_object *obj;
	obj = NIS_RES_OBJECT(res);

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ) {
		status = FN_E_CONFIGURATION_ERROR;
		goto mail_out;
	}

	// Obtain the mail expansion entry
	mailentry = (char *) calloc(ENTRY_LEN(obj, MAIL_EXPANSION_COL),
	    sizeof(char));
	strncpy(mailentry, ENTRY_VAL(obj, MAIL_EXPANSION_COL),
	    ENTRY_LEN(obj, MAIL_EXPANSION_COL));

 mail_out:
	free_nis_result(res);
	if (status == FN_SUCCESS)
		return (1);
	else
		return (0);
}

int FNSP_find_mailentry(const FN_string &dirname, const FN_string &username,
    unsigned int access_flags, char *&mailentry, unsigned int &status)
{
	unsigned int stat;
	FN_string *alias_string = new FN_string(username);
	char *alias_entry;

	mailentry = 0;
	while (FNSP_local_find_mailentry(dirname, *alias_string, access_flags,
	    alias_entry, stat)) {
		delete alias_string;
		alias_string = new FN_string((unsigned char *) alias_entry);
		if (mailentry)
			free (mailentry);
		mailentry = strdup(alias_entry);
		free (alias_entry);
	}
	delete alias_string;
	if (mailentry) {
		status = FN_SUCCESS;
		return (1);
	} else {
		status = stat;
		return (0);
	}
}
