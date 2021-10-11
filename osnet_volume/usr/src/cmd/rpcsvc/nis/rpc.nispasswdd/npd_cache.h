/*
 *	npd_cache.h
 *
 *	Copyright (c) 1994 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)npd_cache.h	1.2	94/04/02 SMI"

#include <rpcsvc/nis.h>

/*
 * cache for useful information
 */
struct	update_item {
	NIS_HASH_ITEM	ul_item;	/* generic tag */
	bool_t		ul_sameuser;	/* changing their own passwd */
	char		*ul_user;	/* username */
	char		*ul_domain;	/* domainname */
	u_long		ul_rval;	/* random value */
	u_long		ul_ident;	/* identifier */
	des_block	ul_key;		/* session key */
	char		*ul_oldpass;	/* old clear passwd */
	int		ul_attempt;	/* failed attempts per session */
	u_long		ul_expire;	/* expiration time */
};
