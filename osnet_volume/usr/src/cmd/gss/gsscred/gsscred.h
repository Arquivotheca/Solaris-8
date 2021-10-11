/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * gsscred utility Manages mapping between a security principal
 * name and unix uid.
 */

#ifndef	_GSSCRED_H
#define	_GSSCRED_H

#pragma ident	"@(#)gsscred.h	1.9	98/05/07 SMI"

#include <libintl.h>
#include <locale.h>
#include <gssapi/gssapi.h>
#include <pwd.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SUNW_OST_OSCMD"
#endif

#define	GSSCRED_FLAT_FILE	-1

/*
 * Misc functions in gsscred.
 */
int gsscred_AsHex(const gss_buffer_t inBuf, gss_buffer_t outBuf);
int gsscred_MakeName(const gss_OID mechOid, const char *name,
		const char *nameOid, gss_buffer_t OutName);
int gsscred_read_config_file(void);
int gsscred_MakeNameHeader(const gss_OID mechOid, gss_buffer_t outNameHdr);


/*
 * Flat file based gsscred functions.
 */
int file_addGssCredEntry(const gss_buffer_t hexName, const char *uid,
	const char *comment, char **errDetails);
int file_getGssCredEntry(const gss_buffer_t name, const char *uid,
	char **errDetails);
int file_deleteGssCredEntry(const gss_buffer_t name, const char *uid,
	char **errDetails);
int file_getGssCredUid(const gss_buffer_t name, uid_t *uidOut);


/*
 * GSS entry point for retrieving user uid information based on
 * exported name buffer.
 */
int gss_getGssCredEntry(const gss_buffer_t expName, uid_t *uid);

#ifdef	__cplusplus
}
#endif

#endif	/* _GSSCRED_H */
