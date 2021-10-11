/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)entry.c	1.14	99/06/01 SMI"

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

/*
 * Print out a single `entry descriptor' entry.
 */
void
_Dbg_ent_entry(Half mach, Ent_desc *enp)
{
	Listnode	*lnp;
	char		*cp;

	dbg_print(MSG_ORIG(MSG_ECR_NAME),
	    (enp->ec_name ? enp->ec_name : MSG_INTL(MSG_STR_NULL)),
	    conv_secflg_str(mach, enp->ec_attrmask));

	dbg_print(MSG_ORIG(MSG_ECR_SEGMENT),
	    (enp->ec_segment->sg_name ? enp->ec_segment->sg_name :
	    MSG_INTL(MSG_STR_NULL)), conv_secflg_str(mach, enp->ec_attrbits));

	dbg_print(MSG_ORIG(MSG_ECR_NDX),
	    EC_WORD(enp->ec_ndx), conv_sectyp_str(mach, enp->ec_type));

	if (enp->ec_files.head) {
		dbg_print(MSG_ORIG(MSG_ECR_FILES));
		for (LIST_TRAVERSE(&(enp->ec_files), lnp, cp))
			dbg_print(MSG_ORIG(MSG_ECR_FILE), cp);
	}
}


/*
 * Print out all `entrance descriptor' entries.
 */
void
Dbg_ent_print(Half mach, List *len, Boolean dmode)
{
	Listnode	*lnp;
	Ent_desc	*enp;
	int		ndx = 1;

	if (DBG_NOTCLASS(DBG_ENTRY))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_ECR_TITLE),
		(dmode ? MSG_INTL(MSG_ECR_DYNAMIC) : MSG_INTL(MSG_ECR_STATIC)));

	for (LIST_TRAVERSE(len, lnp, enp)) {
		dbg_print(MSG_INTL(MSG_ECR_DESC), ndx);
		_Dbg_ent_entry(mach, enp);
		ndx++;
	}
}
