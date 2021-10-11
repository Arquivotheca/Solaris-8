/*
 *	keylogout.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)keylogout.c	1.10	97/11/19 SMI"

/*
 * unset the secret key on local machine
 */
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <nfs/nfs.h>
#include <nfs/nfssys.h>

extern int key_removesecret_g();

/* for revoking kernel NFS credentials */
struct nfs_revauth_args nra;

main(argc, argv)
	int argc;
	char *argv[];
{
	static char secret[HEXKEYBYTES + 1];
	int err = 0;

	if (geteuid() == 0) {
		if ((argc != 2) || (strcmp(argv[1], "-f") != 0)) {
			fprintf(stderr,
"keylogout by root would break the rpc services that");
			fprintf(stderr, " use secure rpc on this host!\n");
			fprintf(stderr,
"root may use keylogout -f to do this (at your own risk)!\n");
			exit(-1);
		}
	}

	if (key_removesecret_g() < 0) {
			fprintf(stderr, "Could not unset your secret key.\n");
			fprintf(stderr, "Maybe the keyserver is down?\n");
			err = 1;
	}
	if (key_setsecret(secret) < 0) {
		if (!err) {
			fprintf(stderr, "Could not unset your secret key.\n");
			fprintf(stderr, "Maybe the keyserver is down?\n");
			err = 1;
		}
	}

	nra.authtype = AUTH_DES;	/* only revoke DES creds */
	nra.uid = getuid();		/* use the real uid */
	if (_nfssys(NFS_REVAUTH, &nra) < 0) {
		perror("Warning: NFS credentials not destroyed");
		err = 1;
	}

	exit(err);
	/* NOTREACHED */
}
