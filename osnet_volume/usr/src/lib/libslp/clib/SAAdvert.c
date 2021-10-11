/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)SAAdvert.c	1.1	99/04/02 SMI"

/*
 * SAAdverts are used internally by libslp to discover scopes in the
 * absence of configured scopes and scopes found by active and
 * passive DA discovery. They can also be solicited by SLPFindSrv()
 * and SLPFindAttr calls with the "service:service-agent" type.
 * slp_unpackSAAdvert() unpacks a SAAdvert and returns SA info.
 */

#include <stdio.h>
#include <slp-internal.h>

SLPError slp_unpackSAAdvert(char *reply, char **surl,
				char **scopes, char **attrs) {
	SLPError err = SLP_OK;
	size_t off, len;
	/* authentication components */
	struct iovec iov[3];
	size_t tmp_off;
	int auth_cnt;
	size_t abLen = 0;

	*surl = *scopes = *attrs = NULL;

	len = slp_get_length(reply);
	off = SLP_HDRLEN + slp_get_langlen(reply);

	/* service URL */
	iov[0].iov_base = reply + off;
	tmp_off = off;
	if ((err = slp_get_string(reply, len, &off, surl)) != SLP_OK) {
	    goto fail;
	}
	iov[0].iov_len = off - tmp_off;

	/* scope list */
	iov[2].iov_base = reply + off;
	tmp_off = off;
	if ((err = slp_get_string(reply, len, &off, scopes)) != SLP_OK) {
	    goto fail;
	}
	iov[2].iov_len = off - tmp_off;

	/* attributes */
	iov[1].iov_base = reply + off;
	tmp_off = off;
	if ((err = slp_get_string(reply, len, &off, attrs)) != SLP_OK) {
	    goto fail;
	}
	iov[1].iov_len = off - tmp_off;

	/* auth blocks */
	if ((err = slp_get_byte(reply, len, &off, &auth_cnt)) != SLP_OK) {
	    goto fail;
	}
	if (slp_get_security_on() || auth_cnt > 0) {
	    if ((err = slp_verify(iov, 3,
				    reply + off,
				    len - off,
				    auth_cnt,
				    &abLen)) != SLP_OK) {
		goto fail;
	    }
	}

	return (SLP_OK);

fail:
	if (*surl) free(*surl);
	if (*scopes) free(*scopes);
	if (*attrs) free(*attrs);

	return (err);
}
