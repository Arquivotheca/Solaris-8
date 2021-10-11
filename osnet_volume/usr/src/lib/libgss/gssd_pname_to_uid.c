/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gssd_pname_to_uid.c	1.16	97/11/11 SMI"

#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mechglueP.h>
#include "../../cmd/gss/gsscred/gsscred.h"

extern int _getgroupsbymember(const char *, gid_t[], int, int);

/* local function used to call a mechanisms pname_to_uid */
static OM_uint32 gss_pname_to_uid(OM_uint32*, const gss_name_t,
			const gss_OID, uid_t *);

static OM_uint32 private_gsscred_expname_to_unix_cred(const gss_buffer_t,
			uid_t *, gid_t *, gid_t **, int *);

/*
 * The gsscred functions will first attempt to call the
 * mechanism'm pname_to_uid function.  In case this function
 * returns an error or if it is not provided by a mechanism
 * then the functions will attempt to look up the principal
 * in the gsscred table.
 * It is envisioned that the pname_to_uid function will be
 * provided by only a few mechanism, which may have the principal
 * name to unix credential mapping inherently present.
 */

/*
 * This routine accepts a name in export name format and retrieves
 * unix credentials associated with it.
 */
OM_uint32
gsscred_expname_to_unix_cred(expName, uidOut, gidOut, gids, gidsLen)
const gss_buffer_t expName;
uid_t *uidOut;
gid_t *gidOut;
gid_t *gids[];
int *gidsLen;
{
	gss_name_t intName;
	OM_uint32 minor, major;

	if (uidOut == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	if (expName == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	/* first check the mechanism for the mapping */
	if (gss_import_name(&minor, expName, (gss_OID)GSS_C_NT_EXPORT_NAME,
			&intName) == GSS_S_COMPLETE) {
		major = gss_pname_to_uid(&minor, intName, NULL, uidOut);
		gss_release_name(&minor, &intName);
		if (major == GSS_S_COMPLETE) {
			if (gids && gidsLen && gidOut)
				return (gss_get_group_info(*uidOut, gidOut,
						gids, gidsLen));
			return (GSS_S_COMPLETE);
		}
	}

	/*
	 * we fall back onto the gsscred table to provide the mapping
	 * start by making sure that the expName is an export name buffer
	 */
	return (private_gsscred_expname_to_unix_cred(expName, uidOut, gidOut,
						gids, gidsLen));
} /* gsscred_expname_to_unix_cred */


static const char *expNameTokId = "\x04\x01";
static const int expNameTokIdLen = 2;
/*
 * private routine added to be called from gsscred_name_to_unix_cred
 * and gsscred_expName_to_unix_cred.
 */
static OM_uint32
private_gsscred_expname_to_unix_cred(expName, uidOut, gidOut, gids, gidsLen)
const gss_buffer_t expName;
uid_t *uidOut;
gid_t *gidOut;
gid_t *gids[];
int *gidsLen;
{

	if (expName->length < expNameTokIdLen ||
		(memcmp(expName->value, expNameTokId, expNameTokIdLen) != 0))
		return (GSS_S_DEFECTIVE_TOKEN);

	if (!gss_getGssCredEntry(expName, uidOut))
		return (GSS_S_FAILURE);

	/* did caller request group info also ? */
	if (gids && gidsLen && gidOut)
		return (gss_get_group_info(*uidOut, gidOut, gids, gidsLen));

	return (GSS_S_COMPLETE);
}

/*
 * This routine accepts a name in gss internal name format together with
 * a mechanim OID and retrieves a unix credentials for that entity.
 */
OM_uint32
gsscred_name_to_unix_cred(intName, mechType, uidOut, gidOut, gids, gidsLen)
const gss_name_t intName;
const gss_OID mechType;
uid_t *uidOut;
gid_t *gidOut;
gid_t *gids[];
int *gidsLen;
{
	gss_name_t canonName;
	gss_buffer_desc expName = GSS_C_EMPTY_BUFFER;
	OM_uint32 major, minor;

	if (intName == NULL || mechType == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	if (uidOut == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	/* first try the mechanism provided mapping */
	if (gss_pname_to_uid(&minor, intName, mechType, uidOut)
		== GSS_S_COMPLETE) {
		if (gids && gidsLen && gidOut)
			return (gss_get_group_info(*uidOut, gidOut, gids,
					gidsLen));
		return (GSS_S_COMPLETE);
	}


	/*
	 * falling back onto the gsscred table to provide the mapping
	 * start by canonicalizing the passed in name and then export it
	 */
	if (major = gss_canonicalize_name(&minor, intName,
				mechType, &canonName))
		return (major);

	major = gss_export_name(&minor, canonName, &expName);
	gss_release_name(&minor, &canonName);
	if (major)
		return (major);

	major = private_gsscred_expname_to_unix_cred(&expName, uidOut, gidOut,
					gids, gidsLen);
	gss_release_buffer(&minor, &expName);
	return (major);
} /* gsscred_name_to_unix_cred */


/*
 * This routine accepts a unix uid, and retrieves the group id
 * and supplamentery group ids for that uid.
 * Callers should be aware that the supplamentary group ids
 * array may be empty even when this function returns success.
 */
OM_uint32
gss_get_group_info(uid, gidOut, gids, gidsLen)
const uid_t uid;
gid_t *gidOut;
gid_t *gids[];
int *gidsLen;
{
	struct passwd *pw;
	int maxgroups;

	/* check for output parameters */
	if (gidOut == NULL || gids == NULL || gidsLen == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*gids = NULL;
	*gidsLen = 0;

	/* determine maximum number of groups possible */
	maxgroups = sysconf(_SC_NGROUPS_MAX);
	if (maxgroups < 1)
	    maxgroups = 16;

	if ((pw = getpwuid(uid)) == NULL)
	    return (GSS_S_FAILURE);

	/*
	 * we allocate for the maximum number of groups
	 * we do not reclaim the space when the actual number
	 * is lower, just set the size approprately.
	 */
	*gids = (gid_t *)calloc(maxgroups, sizeof (gid_t));
	if (*gids == NULL)
	    return (GSS_S_FAILURE);

	*gidOut = pw->pw_gid;
	(*gids)[0] = pw->pw_gid;
	*gidsLen = _getgroupsbymember(pw->pw_name, *gids, maxgroups, 1);
	/*
	 * we will try to remove the duplicate entry from the groups
	 * array.  This can cause the group array to be empty.
	 */
	if (*gidsLen < 1)
	{
		free(*gids);
		*gids = NULL;
		return (GSS_S_FAILURE);
	} else if (*gidsLen == 1) {
		free(*gids);
		*gids = NULL;
		*gidsLen = 0;
	} else {
		/* length is atleast 2 */
		*gidsLen = *gidsLen -1;
		(*gids)[0] = (*gids)[*gidsLen];
	}

	return (GSS_S_COMPLETE);
} /* gss_get_group_info */


static OM_uint32
gss_pname_to_uid(minor, name, mech_type, uidOut)
OM_uint32 *minor;
const gss_name_t name;
const gss_OID mech_type;
uid_t *uidOut;
{
	gss_mechanism mech;
	gss_union_name_t intName;
	gss_name_t mechName = NULL;
	OM_uint32 major, tmpMinor;

	if (!minor)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*minor = 0;

	if (uidOut == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	if (name == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	intName = (gss_union_name_t)name;

	if (mech_type != NULL)
		mech = __gss_get_mechanism(mech_type);
	else {
		/*
		 * if this is a MN, then try using the mech
		 * from the name; otherwise ask for default
		 */
		mech = __gss_get_mechanism(intName->mech_type);
	}

	if (mech == NULL || mech->pname_to_uid == NULL)
		return (GSS_S_UNAVAILABLE);

	/* may need to import the name if this is not MN */
	if (intName->mech_type == NULL) {
		major = __gss_import_internal_name(minor,
				mech_type, intName,
				&mechName);
		if (major != GSS_S_COMPLETE)
			return (major);
	} else
		mechName = intName->mech_name;


	/* now call the mechanism's pname function to do the work */
	major = mech->pname_to_uid(mech->context, minor, mechName, uidOut);

	if (intName->mech_name != mechName)
		__gss_release_internal_name(&tmpMinor, &mech->mech_type,
				&mechName);

	return (major);
} /* gss_pname_to_uid */
