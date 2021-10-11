/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)fnsypd.c	1.13 98/01/15 SMI"

#include "fnsypprot.h"
#include <xfn/xfn.h>
#include <dlfcn.h>
#include <netdb.h>

extern unsigned
fnsp_server_update_nis_database(const char *name, int user_host,
    const char *domain, const char *map,
    const char *key, const char *value, const char *old_value, int op);

extern unsigned
get_login_name_from_distinguished_name(const char *dn_name,
    const char *domainname, int *matched,
    char **login_name, int *user_host);


/* Define functions for SKI operations */

typedef int (*ski_init)(void **, const char *, char *, const u_int,
			const u_int, const char *);

typedef int (*ski_end)(void *);

typedef int (*ski_verify)(
    void *,		/* IN: SKI handle */
    const uchar_t *,    /* IN: PKCS#7 signed data */
    const size_t,       /* IN: PKCS#7 signed data len */
    const void *, 	/* IN: trusted public keys */
    const char **,      /* IN: list of CAs */
    const int,          /* IN: flag */
    uchar_t **,         /* OUT: verified data */
    size_t *,           /* OUT: verified data length */
    char **,            /* OUT: signer's name */
    void **,         	/* OUT: cert chain */
    int *,              /* OUT: number of certs in chain */
    void **          	/* OUT: first expired cert */
);


static mutex_t ski_lock = DEFAULTMUTEX;
static void *mh = 0;
static void *fh_ski_init = 0;
static void *fh_ski_end = 0;
static void *fh_ski_verify = 0;
static int time_diff = 60;
static int ski_check_pass = 0;

bool_t
fnsp_rpc_server_update_nis_database_1_svc(SignedData *argp,
    u_int *result, struct svc_req *rqstp)
{
	xFNS_YPUPDATE_DATA ypupdate_data;
	XDR xdrs;
	bool_t res;
	int matches, user_host;
	char *login_name;
	char host_name[MAXHOSTNAMELEN+1];

	time_t local_time;
	char function[1024];
	uchar_t *pkcsData = (uchar_t *) (argp->data).data_val;
	u_int pkcsDataLen = (argp->data).data_len;
	uchar_t *data = 0;
	size_t data_length = 0;
	char *keypkgId = NULL;
	u_int ski_flags = 0;
	char *pSignerName = 0;
	void *skih = 0;

	/* Check if SKI is installed */
	mutex_lock(&ski_lock);
	if (ski_check_pass == 0) {
		if (geteuid() != 0) {
			mutex_unlock(&ski_lock);
			exit(1);
		}

		if ((mh = dlopen("libski.so.1", RTLD_LAZY)) == 0) {
			/* SKI is not installed */
			mutex_unlock(&ski_lock);
			*result = FN_E_CTX_NO_PERMISSION;
			mutex_unlock(&ski_lock);
			return (1);
		}

		/* dlsym the ski_init() function */
		strcpy(function, "ski_init");
		if ((fh_ski_init = dlsym(mh, function)) == 0) {
			mutex_unlock(&ski_lock);
			*result = FN_E_CTX_NO_PERMISSION;
			mutex_unlock(&ski_lock);
			return (1);
		}

		/* dlsym the ski_end() function */
		strcpy(function, "ski_end");
		if ((fh_ski_end = dlsym(mh, function)) == 0) {
			mutex_unlock(&ski_lock);
			*result = FN_E_CTX_NO_PERMISSION;
			mutex_unlock(&ski_lock);
			return (1);
		}

		/* dlsyn ski_verify function */
		strcpy(function, "__ski_verify_pkcs7_signedData");
		if ((fh_ski_verify = dlsym(mh, function)) == 0) {
			mutex_unlock(&ski_lock);
			*result = FN_E_CTX_NO_PERMISSION;
			mutex_unlock(&ski_lock);
			return (1);
		}
		ski_check_pass = 1;
	}

	/* Init SKI session */
	gethostname(host_name, MAXHOSTNAMELEN);
	if (((*((ski_init)fh_ski_init))
	    (&skih, keypkgId, host_name, ski_flags, 0, 0)) != 0) {
		mutex_unlock(&ski_lock);
		*result = FN_E_CTX_NO_PERMISSION;
		mutex_unlock(&ski_lock);
		return (1);
	}

	/* Perform SKI Authentication */
	if (((*((ski_verify)fh_ski_verify))
	    (skih,
	    pkcsData,
	    pkcsDataLen,
	    0, /* trusted public keys */
	    0, /* list of CAs */
	    0, /* flags */
	    &data,
	    &data_length,
	    &pSignerName,
	    0, /* cert chain */
	    0, /* cert chain count */
	    0)) != 0) {
		mutex_unlock(&ski_lock);
		*result = FN_E_NAME_IN_USE;
		mutex_unlock(&ski_lock);
		return (TRUE);
	}

	/* End SKI session */
	if (((*((ski_end)fh_ski_end))(skih)) != 0) {
		mutex_unlock(&ski_lock);
		*result = FN_E_CTX_NO_PERMISSION;
		mutex_unlock(&ski_lock);
		return (1);
	}

	/* Decode the SignedData to xFNS_YPUPDATE_DATA */
	xdrmem_create(&xdrs, (caddr_t) data, data_length, XDR_DECODE);

	ypupdate_data.username = 0;
	ypupdate_data.domain = 0;
	ypupdate_data.map = 0;
	ypupdate_data.key = 0;
	ypupdate_data.value = 0;
	ypupdate_data.old_value = 0;

	res = xdr_xFNS_YPUPDATE_DATA(&xdrs, &ypupdate_data);
	if (res == FALSE) {
		mutex_unlock(&ski_lock);
		(*result) = FN_E_INSUFFICIENT_RESOURCES;
		mutex_unlock(&ski_lock);
		return (TRUE);
	}

	/**** 
	  1) From distinguished name get login name
	  2) If more than one login name, return error
	  3) Compare login name with the data structure from client
	  4) Time stamp should be less than 60 sec.
	****/

	if ((*result = get_login_name_from_distinguished_name(pSignerName,
	    ypupdate_data.domain, &matches, &login_name, &user_host))
	    != FN_SUCCESS)
		goto fnsypd_out;
	if ((matches != 1) || (strcmp(login_name, ypupdate_data.username) != 0)
	    || (user_host != ypupdate_data.user_host)) {
		*result = FN_E_CTX_NO_PERMISSION;
		goto fnsypd_out;
	}
	rpcb_gettime(NULL, &local_time);
	if (((int) local_time - ypupdate_data.timestamp) > time_diff) {
		*result = FN_E_CTX_NO_PERMISSION;
		goto fnsypd_out;
	}

	/* Call the ypupdate function */
	*result = fnsp_server_update_nis_database(
	    ypupdate_data.username,
	    ypupdate_data.user_host,
 	    ypupdate_data.domain,
	    ypupdate_data.map,
	    ypupdate_data.key,
	    ypupdate_data.value,
	    ypupdate_data.old_value,
	    ypupdate_data.op);

fnsypd_out:
	free(ypupdate_data.username);
	free(ypupdate_data.domain);
	free(ypupdate_data.map);
	free(ypupdate_data.key);
	free(ypupdate_data.value);
	free(ypupdate_data.old_value);
	free(data);
	free(pSignerName);

	mutex_unlock(&ski_lock);
	return (TRUE);
}

int
fns_ypupdate_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result,
    caddr_t result)
{
        (void) xdr_free(xdr_result, result);
	return (1);
}
