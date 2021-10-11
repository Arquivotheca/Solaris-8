/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_ECPPSYS_H
#define	_SYS_ECPPSYS_H

#pragma ident	"@(#)ecppsys.h	1.1	95/10/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ECPPIOC_SETPARMS	_IOW('p', 70, struct ecpp_transfer_parms)
#define	ECPPIOC_GETPARMS	_IOR('p', 71, struct ecpp_transfer_parms)

/* current_mode values */
#define	ECPP_CENTRONICS 	0x01 /* non-1284 */
#define	ECPP_COMPAT_MODE	0x02
#define	ECPP_BYTE_MODE		0x03
#define	ECPP_NIBBLE_MODE	0x04
#define	ECPP_ECP_MODE		0x05
#define	ECPP_FAILURE_MODE	0x06
#define	ECPP_DIAG_MODE		0x07
#define	ECPP_INIT_MODE		0x08

typedef struct p1284_ioctl_st {
	int array[10];
	char *cptr;
	char *name;
} P1284Ioctl;

struct ecpp_transfer_parms {
	int	write_timeout;
	int	mode;
};


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ECPPSYS_H */
