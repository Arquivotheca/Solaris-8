/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Private extensions and utilities to the GSS-API.
 * These are not part of the GSS-API specification
 * but may be useful to GSS-API users.
 */

#ifndef _GSSAPI_EXT_H
#define	_GSSAPI_EXT_H

#pragma ident	"@(#)gssapi_ext.h	1.15	98/05/08 SMI"

#include <gssapi/gssapi.h>
#ifdef	_KERNEL
#include <sys/systm.h>
#else
#include <strings.h>
#endif


#ifdef	__cplusplus
extern "C" {
#endif

/* MACRO for comparison of gss_OID's */
#define	g_OID_equal(o1, o2) \
	(((o1)->length == (o2)->length) && \
	(memcmp((o1)->elements, (o2)->elements, (int) (o1)->length) == 0))


/*
 * MACRO for copying of OIDs - memory must already be allocated
 * o2 is copied to o1
 */
#define	g_OID_copy(o1, o2) \
	bcopy((o2)->elements, (o1)->elements, (o2)->length);\
	(o1)->length = (o2)->length;


/* MACRO to check if input buffer is valid */
#define	GSS_EMPTY_BUFFER(buf)	((buf) == NULL ||\
	(buf)->value == NULL || (buf)->length == 0)


/*
 * GSSAPI Extension functions -- these functions aren't
 * in the GSSAPI specification, but are provided in our
 * GSS library.
 */

#ifndef	_KERNEL

/*
 * qop configuration file handling.
 */
#define	MAX_QOP_NUM_PAIRS	128
#define	MAX_QOPS_PER_MECH	128

typedef struct _qop_num {
	char *qop;
	OM_uint32 num;
	char *mech;
} qop_num;

OM_uint32
__gss_qop_to_num(
	char		*qop,		/* input qop string */
	char		*mech,		/* input mech string */
	OM_uint32	*num		/* output qop num */
);

OM_uint32
__gss_num_to_qop(
	char		*mech,		/* input mech string */
	OM_uint32	num,		/* input qop num */
	char		**qop		/* output qop name */
);

OM_uint32
__gss_get_mech_info(
	char		*mech,		/* input mech string */
	char		**qops		/* buffer for return qops */
);

OM_uint32
__gss_mech_qops(
	char *mech,			/* input mech */
	qop_num *mech_qops,		/* mech qops buffer */
	int *numqops			/* buffer to return numqops */
);

OM_uint32
__gss_mech_to_oid(
	const char *mech,		/* mechanism string name */
	gss_OID *oid			/* mechanism oid */
);

const char *
__gss_oid_to_mech(
	const gss_OID oid		/* mechanism oid */
);

OM_uint32
__gss_get_mechanisms(
	char *mechArray[],		/* array to populate with mechs */
	int arrayLen			/* length of passed in array */
);

OM_uint32
__gss_get_mech_type(
	gss_OID oid,			/* mechanism oid */
	const gss_buffer_t token	/* token */
);

OM_uint32
gsscred_expname_to_unix_cred(
	const gss_buffer_t,	/* export name */
	uid_t *,		/* uid out */
	gid_t *,		/* gid out */
	gid_t * [],		/* gid array out */
	int *);			/* gid array length */

OM_uint32
gsscred_name_to_unix_cred(
	const gss_name_t,	/* gss name */
	const gss_OID,		/* mechanim type */
	uid_t *,		/* uid out */
	gid_t *,		/* gid out */
	gid_t * [],		/* gid array out */
	int *);			/* gid array length */


/*
 * The following function will be used to resolve group
 * ids from a UNIX uid.
 */
OM_uint32
gss_get_group_info(
	const uid_t,		/* entity UNIX uid */
	gid_t *,		/* gid out */
	gid_t *[],		/* gid array */
	int *);			/* length of the gid array */

#else	/*	_KERNEL	*/

OM_uint32
kgsscred_expname_to_unix_cred(
	const gss_buffer_t expName,
	uid_t *uidOut,
	gid_t *gidOut,
	gid_t *gids[],
	int *gidsLen,
	uid_t uid);

OM_uint32
kgsscred_name_to_unix_cred(
	const gss_name_t intName,
	const gss_OID mechType,
	uid_t *uidOut,
	gid_t *gidOut,
	gid_t *gids[],
	int *gidsLen,
	uid_t uid);

OM_uint32
kgss_get_group_info(
	const uid_t puid,
	gid_t *gidOut,
	gid_t *gids[],
	int *gidsLen,
	uid_t uid);

#endif


#ifdef	__cplusplus
}
#endif

#endif	/* _GSSAPI_EXT_H */
