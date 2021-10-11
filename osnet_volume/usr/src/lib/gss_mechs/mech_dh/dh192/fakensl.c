/*
 *	fakensl.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)fakensl.c	1.1	97/11/19 SMI"

#include <rpc/rpc.h>
#include <rpc/key_prot.h>

#ifndef HEX_KEY_BYTES
#define	HEX_KEY_BYTES HEXKEYBYTES
#endif

extern int key_encryptsession_pk(const char *, netobj *, des_block *);
extern int key_decryptsession_pk(const char *, netobj *, des_block *);

/*ARGSUSED*/
int
__getpublickey_cached_g(const char remotename[MAXNETNAMELEN], int keylen,
    int algtype, char *pkey, size_t pkeylen, int *cached)
{
	return (getpublickey(remotename, pkey));
}

/*ARGSUSED*/
int
getpublickey_g(const char remotename[MAXNETNAMELEN], int keylen,
    int algtype, char *pkey, size_t pkeylen)
{
	return (getpublickey(remotename, pkey));
}
#pragma weak getpublickey_g

/*ARGSUSED*/
int
key_encryptsession_pk_g(const char *remotename, const char *pk, int keylen,
    int algtype, des_block deskeys[], int no_keys)
{
	int i;
	netobj npk;

	npk.n_len = HEX_KEY_BYTES;
	npk.n_bytes = (char *)pk;

	for (i = 0; i < no_keys; i++) {
		if (key_encryptsession_pk(remotename, &npk, &deskeys[i]))
			return (-1);
	}
	return (0);
}
#pragma weak key_encryptsession_pk_g

/*ARGSUSED*/
int
key_decryptsession_pk_g(const char *remotename, const char *pk, int keylen,
    int algtype, des_block deskeys[], int no_keys)
{
	int i;
	netobj npk;

	npk.n_len = HEX_KEY_BYTES;
	npk.n_bytes = (char *)pk;

	for (i = 0; i < no_keys; i++) {
		if (key_decryptsession_pk(remotename, &npk, &deskeys[i]))
			return (-1);
	}
	return (0);
}
#pragma weak key_decryptsession_pk_g

int
key_gendes_g(des_block deskeys[], int no_keys)
{
	int i;

	memset(deskeys, 0, no_keys* sizeof (des_block));
	for (i = 0; i < no_keys; i++) {
		if (key_gendes(&deskeys[i]))
			return (-1);
	}
	return (0);
}
#pragma weak key_gendes_g

/*ARGSUSED*/
int
key_secretkey_is_set_g(int Keylen, int algtype)
{
	return (key_secretkey_is_set());
}
#pragma weak key_secretkey_is_set_g

void
des_setparity_g(des_block *key)
{
	des_setparity((char *)key);
}
#pragma weak des_setparity
