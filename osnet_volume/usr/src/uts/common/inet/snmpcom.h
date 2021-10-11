/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_SNMPCOM_H
#define	_INET_SNMPCOM_H

#pragma ident	"@(#)snmpcom.h	1.10	98/04/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && defined(__STDC__)

extern int	snmp_append_data(mblk_t *mpdata, char *blob, int len);
extern int	snmp_append_data2(mblk_t *mpdata, mblk_t **last_mpp,
		    char *blob, int len);

extern boolean_t	snmpcom_req(queue_t *q, mblk_t *mp, pfi_t setfn,
				pfi_t getfn, int priv);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_SNMPCOM_H */
