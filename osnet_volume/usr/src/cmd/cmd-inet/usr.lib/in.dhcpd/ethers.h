/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights Reserved.
 */

#ifndef	_ETHERS_H
#define	_ETHERS_H

#pragma ident	"@(#)ethers.h	1.4	95/12/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ethers.h - Ethers compatibility specific defines.
 */
#define	ETHERS_TFTPDIR		"/tftpboot"	/* Location of boot files */
#define	ETHERS_BOOTFILE		"inetboot"	/* root name of boot file */
#define	ETHERS_SUFFIX		".PREP"		/* Only supported KARCH */

extern int	lookup_ethers(struct in_addr *, struct in_addr *,
    ether_addr_t, PN_REC *);
extern int	ethers_encode(IF *, struct in_addr *, ENCODE **);

#ifdef	__cplusplus
}
#endif

#endif	/* _ETHERS_H */
