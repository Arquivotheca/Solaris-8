/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)fnsypprot_client.c	1.13 97/11/07 SMI"

#include "fnsypprot.h"
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <xfn/xfn.h>
#include <dlfcn.h>
#include <rpc/rpc.h>
#include <netdb.h>
#include <strings.h>
#include <stdlib.h>

/* Define functions for SKI opreations */

typedef int (*ski_init)(void **, const char *, char *, const u_int,
			const u_int, const char *);

typedef int (*ski_end)(void *);

typedef int (*ski_sign)(
	void *,         	/* IN:  ski handle */
        const uchar_t *,        /* IN:  data to be signed */
        const size_t,           /* IN:  data length */
        const void *,           /* IN:  key algorithm */
        const uchar_t *,        /* IN:  signature key */
        const size_t,           /* IN:  signature key length */
        const void *,           /* IN:  signature algorithm */
        const void *,		/* IN:  certificate */
        const char **,          /* IN:  list of CAs */
        const int,              /* IN:  flag  */
        uchar_t **,             /* OUT: PKCS#7 signed data */
        size_t *                /* OUT: PKCS#7 signed data len */
);

/* Static variable to hold SKI pointers */
static mutex_t ski_lock = DEFAULTMUTEX;
static void *mh = 0;
static void *fh_ski_init = 0;
static void *fh_ski_sign = 0;
static void *fh_ski_end = 0;
static int ski_check_fail = 0;
static int ski_check_pass = 0;
static void *skih = 0;

extern unsigned
fnsp_call_client_update_nis_database(const char *username,
    int user_host, const char *domain,
    const char *map, const char *key, const char *value,
    const char *old_value, int op)
{
	CLIENT *clnt = 0;
	enum clnt_stat retval;
	unsigned result = FN_SUCCESS;
	char *server;

	xFNS_YPUPDATE_DATA yp_update_data;
	SignedData signed_data;
	XDR xdrs;
	char *buf = 0;
	bool_t status;
	time_t server_time;
	int size;

	/* Check if SKI is installed */
	char function[1024];
	uchar_t *pkcsData = 0;
	size_t pkcsDataLen = 0;
	char *keypkgId = NULL;
	u_int ski_flags = 0;

	mutex_lock(&ski_lock);
	if (ski_check_pass == 0) {
		if (ski_check_fail) {
			mutex_unlock(&ski_lock);
			return (FN_E_CTX_NO_PERMISSION);
		}
		if ((mh = dlopen("libski.so.1", RTLD_LAZY)) == 0) {
			ski_check_fail = 1;
			mutex_unlock(&ski_lock);
			return (FN_E_CTX_NO_PERMISSION);
		}

		/* dlsym the ski_init() function */
		strcpy(function, "ski_init");
		if ((fh_ski_init = dlsym(mh, function)) == 0) {
			ski_check_fail = 1;
			mutex_unlock(&ski_lock);
			return (FN_E_CTX_NO_PERMISSION);
		}

		/* dlsym the ski_end() function */
		strcpy(function, "ski_end");
		if ((fh_ski_end = dlsym(mh, function)) == 0) {
			ski_check_fail = 1;
			mutex_unlock(&ski_lock);
			return (FN_E_CTX_NO_PERMISSION);
		}

		/* dlsym the ski_sign() function */
		strcpy(function, "__ski_create_pkcs7_signedData");
		if ((fh_ski_sign = dlsym(mh, function)) == 0) {
			ski_check_fail = 1;
			mutex_unlock(&ski_lock);
			return (FN_E_CTX_NO_PERMISSION);
		}
		ski_check_pass = 1;
	}

	/* Find out ypmaster server */
	if (yp_master((char *) domain, "fns_org.ctx", &server) != 0) {
		mutex_unlock(&ski_lock);
		return (FN_E_CONFIGURATION_ERROR);
	}

	/* Create the client handle */
	clnt = clnt_create(server, FNS_YPUPDATE_PROG,
	    FNS_YPUPDATE_VERS, "netpath");
	rpcb_gettime(server, &server_time);
	free(server);
	if (clnt == (CLIENT *) NULL) {
		mutex_unlock(&ski_lock);
		return (FN_E_COMMUNICATION_FAILURE);
	}

	/* Copy the data for the RPC call */
	yp_update_data.username = (char *) username;
	yp_update_data.user_host = user_host;
	yp_update_data.domain = (char *) domain;
	yp_update_data.map = (char *) map;
	yp_update_data.key = (char *) key;
	yp_update_data.value = (char *) value;
	yp_update_data.old_value = (char *) old_value;
	yp_update_data.op = op;
	yp_update_data.timestamp = (int) server_time;

	/* XDR the struct and construct the signed data */
	size = (int) xdr_sizeof((xdrproc_t) xdr_xFNS_YPUPDATE_DATA,
	     &yp_update_data);
	buf = (char *) malloc(size);
	if (buf == 0) {
		result = FN_E_INSUFFICIENT_RESOURCES;
		goto fnsyp_client_out;
	}

	xdrmem_create(&xdrs, buf, size, XDR_ENCODE);
	status = xdr_xFNS_YPUPDATE_DATA(&xdrs, &yp_update_data);
	if (status == FALSE) {
		result = FN_E_INSUFFICIENT_RESOURCES;
		goto fnsyp_client_out;
	}
	
	/* Initialize SKI */
	if (((*((ski_init)fh_ski_init))
	    (&skih, keypkgId, 0, ski_flags, 0, 0)) != 0) {
		ski_check_fail = 1;
		result = FN_E_CTX_NO_PERMISSION;
		goto fnsyp_client_out;
	}

	/* Authenticate the user and sign the structure */
	if (((*((ski_sign)fh_ski_sign))(skih,
			       (uchar_t *) buf,
			       size,
			       0, /* key algorithm */
			       0, /* signature key */
			       0, /* signature key length */
			       0, /* signature algorithm */
			       0, /* certificate */
			       0, /* list of CAs */
			       0, /* flag */
			       &pkcsData,
			       &pkcsDataLen)) != 0) {
		result = FN_E_CTX_NO_PERMISSION;
		goto fnsyp_client_out;
        }

	/* End SKI session */
	if (((*((ski_end)fh_ski_end))(skih)) != 0) {
		result = FN_E_CTX_NO_PERMISSION;
		goto fnsyp_client_out;
	}

	signed_data.data.data_len = pkcsDataLen;
	signed_data.data.data_val = (char *) pkcsData;

	/* Perform the ONC RPC call */
	retval = fnsp_rpc_server_update_nis_database_1(
	    &signed_data, &result, clnt);
	if (retval != RPC_SUCCESS)
		result = FN_E_COMMUNICATION_FAILURE;

fnsyp_client_out:	
	/* Delete the mallocs */
	if (buf)
		free(buf);
	if (pkcsData)
		free(pkcsData);
	if (clnt)
		clnt_destroy(clnt);

	/* free(yp_update_data.username);
	free(yp_update_data.domain);
	free(yp_update_data.map);
	free(yp_update_data.key);
	free(yp_update_data.value); */

	mutex_unlock(&ski_lock);
	return (result);
}
