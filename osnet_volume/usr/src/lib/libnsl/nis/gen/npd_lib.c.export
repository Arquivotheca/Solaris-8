/*
 *	npd_lib.c
 *	Contains the encryption routines required by the server
 *	and the client-side for NIS+ passwd update deamon.
 *
 *	Copyright (c) 1994 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)npd_lib.c	1.5	97/10/30 SMI"

#include <string.h>
#include <memory.h>
#include <rpc/des_crypt.h>
#include <rpc/key_prot.h>
#include <rpcsvc/nispasswd.h>


/*
 * encrypt/decrypt ID (val1) and R (val2)
 * return FALSE on failure and TRUE on success
 */
bool_t
__npd_ecb_crypt(
	uint32_t	*val1,
	uint32_t	*val2,
	des_block	*buf,
	unsigned int	bufsize,
	unsigned int	mode,
	des_block	*deskey)
{
	return (FALSE);
}

/*
 * encrypt/decrypt R (val) and password (str)
 * return FALSE on failure and TRUE on success
 */
bool_t
__npd_cbc_crypt(
	uint32_t	*val,
	char	*str,
	unsigned int	strsize,
	npd_newpass	*buf,
	unsigned int	bufsize,
	unsigned int	mode,
	des_block	*deskey)
{
	return (FALSE);
}
