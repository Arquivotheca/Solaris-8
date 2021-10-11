#pragma ident	"@(#)osconf.h	1.1	99/07/18 SMI"
/*
 * include/krb5/stock/osconf.h
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * Site- and OS- dependant configuration.
 */

#ifndef KRB5_OSCONF__
#define KRB5_OSCONF__

    /* Don't try to pull in autoconf.h for Windows, since it's not used */
#ifndef KRB5_AUTOCONF__
#define KRB5_AUTOCONF__
#include "autoconf.h"
#endif

#define DEFAULT_PROFILE_PATH	"/etc/krb5/krb5.conf"
#define	DEFAULT_KEYTAB_NAME	"FILE:/etc/krb5/krb5.keytab"
#define	DEFAULT_KEYTAB		"WRFILE:/etc/krb5/krb5.keytab"


#define DEFAULT_KDB_FILE        "/var/krb5/principal"
#define	DEFAULT_KEYFILE_STUB	"/var/krb5/.k5."
#define KRB5_DEFAULT_ADMIN_ACL	"/etc/krb5/krb5_adm.acl"

/* Location of KDC profile */
#define	DEFAULT_KDC_PROFILE	"/etc/krb5/kdc.conf"
#define	KDC_PROFILE_ENV		"KRB5_KDC_PROFILE"

#define	DEFAULT_KDC_ENCTYPE	ENCTYPE_DES_CBC_CRC
#define KDCRCACHE		"dfl:krb5kdc_rcache"

#define KDC_PORTNAME		"kerberos" /* for /etc/services or equiv. */
#define KDC_SECONDARY_PORTNAME	"kerberos-sec" /* For backwards */
					    /* compatibility with */
					    /* port 750 clients */

#define KRB5_DEFAULT_PORT	88
#define KRB5_DEFAULT_SEC_PORT	750

#define DEFAULT_KDC_PORTLIST	"88,750"

/*
 * Defaults for the KADM5 admin system.
 */
#define DEFAULT_KADM5_KEYTAB	"/etc/krb5/kadm5.keytab"
#define DEFAULT_KADM5_ACL_FILE	"/etc/krb5/kadm5.acl"
#define DEFAULT_KADM5_PORT	749 /* assigned by IANA */

#define MAX_DGRAM_SIZE	4096
#define MAX_SKDC_TIMEOUT 30
#define SKDC_TIMEOUT_SHIFT 2		/* left shift of timeout for backoff */
#define SKDC_TIMEOUT_1 1		/* seconds for first timeout */

#define RCTMPDIR	"/usr/tmp"	/* directory to store replay caches */

#define KRB5_ENV_CCNAME	"KRB5CCNAME"

/*
 * krb5 slave support follows
 */

#define KPROP_DEFAULT_FILE "/var/krb5/slave_datatrans"
#define KPROPD_DEFAULT_FILE "/var/krb5/from_master"
#define KPROPD_DEFAULT_KDB5_UTIL "/usr/sbin/kdb5_util"
#define KPROPD_DEFAULT_KRB_DB DEFAULT_KDB_FILE
#define KPROPD_ACL_FILE "/etc/krb5/kpropd.acl"

#endif /* KRB5_OSCONF__ */
