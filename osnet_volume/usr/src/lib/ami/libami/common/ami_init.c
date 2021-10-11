/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_init.c	1.2 99/07/23 SMI"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <nl_types.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ami.h"
#include "ami_local.h"
#include "ami_proto.h"
#include "global.h"
#include "bsafe.h"
#include "cryptoki.h"

AMI_STATUS
ami_init(const char *appName, const char *backend,
    const char *keypkg_id, const char *host_name, uint_t flags,
    uint_t crypto_define, const char *ldd, ami_handle_t *amih)
{

	AMI_STATUS ami_status = AMI_EPARM;

	if (amih == 0)
		return (ami_status);

	if ((ami_status = __ami_init(amih, keypkg_id, host_name,
	    flags, crypto_define, ldd)) == AMI_OK) {
		/* get RPC client handle */
		ami_status = __ami_rpc_get_amiserv_client_handle(
		    (CLIENT **) &((*amih)->clnt));
	}
	if (ami_status != AMI_OK) {
		if (*amih) {
			ami_end(*amih);
			*amih = 0;
		}
	} else if (backend != NULL)
		(*amih)->backend = strdup(backend);

	return (ami_status);
}

AMI_STATUS
__ami_init(ami_handle_t *amih, const char *keypkg_id, const char *host_name,
    const uint_t flags, const uint_t crypto_define, const char *ldd)
{
	ami_handle_t new_amih = NULL;
	AMI_STATUS ami_status = AMI_EPARM;
	char *saved_keypkg_id = AMI_DEFAULT_KEYID;

	struct hostent *hp;
	struct hostent host_result;
	struct in_addr in;
	char *host_ip = 0;
	char host_buffer[1024];
	int host_error;


	if (!amih || flags || crypto_define || ldd)
		return (AMI_EPARM);
	*amih = NULL;

	if (keypkg_id != 0 && keypkg_id[0] != '\0')
		saved_keypkg_id = (char *) keypkg_id;

	if ((new_amih = (ami_handle_t) calloc(1, sizeof (ami_handle)))
	    == NULL) {
		ami_status = AMI_ENOMEM;
		goto out;
	}
	/* copy in the key_id */
	if ((new_amih->keypkg_id = strdup(saved_keypkg_id)) == NULL) {
		ami_status = AMI_ENOMEM;
		goto out;
	}
	host_ip = (char *) host_name;
	if (host_name) {
		if ((long) inet_addr(host_name) == -1) {
			/* we passed a name instead of IP address */
			if ((hp = gethostbyname_r(host_name, &host_result,
				    host_buffer, 1024, &host_error)) == NULL) {
				ami_status = AMI_UNKNOWN_USER;
				goto out;
			}
			memcpy(&in.s_addr, hp->h_addr_list[0],
			    sizeof (in.s_addr));
			host_ip = inet_ntoa(in);
		}
	} else {
		host_ip = __ami_getIPaddress();
	}

	/* copy in the host IP if provided */
	if ((new_amih->host_ip = strdup(host_ip)) == NULL) {
		ami_status = AMI_ENOMEM;
		goto out;
	}

	/* success */
	*amih = new_amih;
	ami_status = AMI_OK;

out:
	if (ami_status != AMI_OK) {
		if (new_amih) {
			ami_end(*amih);
		}
	}
	return (ami_status);
}

AMI_STATUS
ami_end(ami_handle_t amih)
{
	B_ALGORITHM_OBJ dhKeyAgreeAlg = 0;

	if (!amih)
		return (AMI_EPARM);

	if (amih->backend)
		free(amih->backend);
	if (amih->keypkg_id)
		free(amih->keypkg_id);
	if (amih->host_ip)
		free(amih->host_ip);
	if (amih->fd)
		catclose(amih->fd);
	if (amih->dhKeyAgreeAlg) {
		dhKeyAgreeAlg =
		    (B_ALGORITHM_OBJ) (amih->dhKeyAgreeAlg);
		B_DestroyAlgorithmObject(&dhKeyAgreeAlg);
	}
	if (amih->hCkSession)
	    C_CloseSession(amih->hCkSession);
	/*
	 * Since CLIENT handle is cached, it should not be destroyed
	 *
	 * if (amih->clnt) clnt_destroy((CLIENT *) amih->clnt);
	 */
	free(amih);
	return (AMI_OK);
}
