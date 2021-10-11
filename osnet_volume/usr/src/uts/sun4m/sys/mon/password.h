/*
 * Copyright (c) 1988, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_MON_PASSWD_H
#define	_SYS_MON_PASSWD_H

#pragma ident	"@(#)password.h	1.7	92/07/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * password.h  contains information for handling password in PROM.
 */

#define	PW_SIZE			8	/* max size of a password entry */
#define	COMMAND_SECURE_MODE	0x01	/* Command Secure Mode */
#define	FULLY_SECURE_MODE	0x5E	/* Fully Secure Mode */
					/* 0, 2-0x5D, 0x5F-0xFF Non Secure */
struct password_inf {
	unsigned short 	bad_couner;	/* illegal password count */
	char 		pw_mode;	/* Contains mode */
					/* 0x01; Command Secure */
					/* 0x5E; Fully Secure */
	char 		pw_bytes[PW_SIZE]; /* password */
};

#define	pw_seen gp->g_pw_seen 	/* Declared in globram.h */
#define	MAX_COUNT	65535	/* Maximum number of bad entries */
#define	SEC_10		10000 	/* Delay count after each password try */
#define	NULL		0

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MON_PASSWD_H */
