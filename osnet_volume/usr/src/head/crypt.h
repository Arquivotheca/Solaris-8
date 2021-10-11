/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_CRYPT_H
#define	_CRYPT_H

#pragma ident	"@(#)crypt.h	1.10	92/09/14 SMI"	/* SVr4.0 1.1	*/

#ifdef	__cplusplus
extern "C" {
#endif

/* Password and file encryption functions */

#if defined(__STDC__)
extern char *crypt(const char *, const char *);
extern int crypt_close(int *);
extern char *des_crypt(const char *, const char *);
extern void des_encrypt(char *, int);
extern void des_setkey(const char *);
extern void encrypt(char *, int);
extern int run_crypt(long, char *, unsigned, int *);
extern int run_setkey(int *, const char *);
extern void setkey(const char *);
#else
extern char *crypt();
extern int crypt_close();
extern char *des_crypt();
extern void des_encrypt();
extern void des_setkey();
extern void encrypt();
extern int run_crypt();
extern int run_setkey();
extern void setkey();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _CRYPT_H */
