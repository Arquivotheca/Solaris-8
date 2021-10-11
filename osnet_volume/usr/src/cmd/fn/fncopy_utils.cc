/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fncopy_utils.cc	1.5	97/03/05 SMI"

#include <stdio.h>
#include <rpc/rpc.h>
#include <libintl.h>
#include <string.h>
#include <stdlib.h>
#include <xfn/fnselect.hh>
#include "fncopy_utils.hh"

extern int source_ns;
extern int destination_ns;
extern int verbose;
extern unsigned global_bind_flags;
extern char *program_name;

// Ref identifier for files context type
extern const FN_identifier FNSP_fs_reftype;
extern const FN_identifier FNSP_nisplus_user_fs_addr;
extern const FN_identifier FNSP_user_fs_addr;
extern const FN_identifier FNSP_fs_host_addr;

static const FN_composite_name canonical_fs_name((unsigned char *)"_fs/");
static const FN_composite_name custom_fs_name((unsigned char *)"fs/");

static int
__fns_xdr_encode_string(const char *str, char *buffer, size_t &len)
{
	XDR	xdr;

	xdrmem_create(&xdr, (caddr_t)buffer, len, XDR_ENCODE);
	if (xdr_string(&xdr, (char **)&str, ~0) == FALSE) {
		return (0);
	}

	len = xdr_getpos(&xdr);
	xdr_destroy(&xdr);
	return (1);
}

static char *
__fns_xdr_decode_string(const void *buf, const int bufsize)
{
	char *answer = 0;
	XDR xdrs;

	xdrmem_create(&xdrs, (caddr_t)buf, bufsize, XDR_DECODE);
	if ((xdr_string(&xdrs, (char **) &answer, ~0)) == FALSE)
		return (0);
	else
		return (answer);
}

FN_string *
get_user_name_from_ref(FN_ref *ref)
{
	FN_string *answer = 0;
	const FN_ref_addr *ref_addr;
	const FN_identifier *id;
	void *ip;

	for (ref_addr = ref->first(ip); ref_addr;
	    ref_addr = ref->next(ip)) {
		id = ref_addr->type();
		if (((*id) == FNSP_user_fs_addr) ||
		    ((*id) == FNSP_nisplus_user_fs_addr))
			break;
	}

	if (ref_addr == 0)
		return (0);

	size_t buflen = ref_addr->length();
	const void *data = ref_addr->data();
	char *fulluser_name = __fns_xdr_decode_string(data, buflen);
	char user_name[NIS_MAXNAMELEN];
	size_t start, end;

	if ((*id) == FNSP_nisplus_user_fs_addr) {
		start = strlen("[name=");
		for (end = start; fulluser_name[end] != ']'; end++) {
		}
	} else {
		end = strlen(fulluser_name);
		start = 0;
	}
	strncpy(user_name, &fulluser_name[start], end - start);
	user_name[end - start] = '\0';
	free(fulluser_name);
	answer = new FN_string((unsigned char *) user_name);
	return (answer);
}

FN_string *
get_host_name_from_ref(FN_ref *ref)
{
	FN_string *answer = 0;
	const FN_ref_addr *ref_addr;
	const FN_identifier *id;
	void *ip;

	for (ref_addr = ref->first(ip); ref_addr;
	    ref_addr = ref->next(ip)) {
		id = ref_addr->type();
		if ((*id) == FNSP_fs_host_addr)
			break;
	}

	if (ref_addr == 0)
		return (0);

	size_t buflen = ref_addr->length();
	const void *data = ref_addr->data();
	char *fullhost_name = __fns_xdr_decode_string(data, buflen);
	char host_name[NIS_MAXNAMELEN];
	size_t end;
	for (end = 0; ((fullhost_name[end] != '.') &&
	    (fullhost_name[end] != '\0')); end++) {
	}
	strncpy(host_name, fullhost_name, end);
	host_name[end] = '\0';
	free(fullhost_name);
	answer = new FN_string((unsigned char *) host_name);
	return (answer);
}

// create and return reference for user fs binding
FN_ref *
create_user_fs_ref(const FN_string &user_name, const FN_string &domain_name)
{
	FN_ref *ref = new FN_ref(FNSP_fs_reftype);
	char fs_addr[NIS_MAXNAMELEN];
	char encoded_addr[NIS_MAXNAMELEN];
	size_t len = NIS_MAXNAMELEN;

	if (ref == 0)
		return (0);

	sprintf(fs_addr, "%s", user_name.str());
	if (__fns_xdr_encode_string(fs_addr, encoded_addr, len) == 0) {
		delete ref;
		return (0);
	}

	FN_ref_addr addr(FNSP_user_fs_addr, len, encoded_addr);
	ref->append_addr(addr);

	if (destination_ns == FNSP_nisplus_ns) {
		sprintf(fs_addr, "[name=%s]passwd.org_dir.%s",
		    user_name.str(), domain_name.str());
		len = NIS_MAXNAMELEN;
		if (__fns_xdr_encode_string(fs_addr,
		    encoded_addr, len) == 0) {
			delete ref;
			return (0);
		}
		FN_ref_addr new_addr(FNSP_nisplus_user_fs_addr, len,
		    encoded_addr);
		ref->append_addr(new_addr);
	}

	return (ref);
}

// create and return reference for host fs binding
FN_ref *
create_host_fs_ref(const FN_string &host_name, const FN_string &domain_name)
{
	FN_ref *ref = new FN_ref(FNSP_fs_reftype);
	char fs_addr[NIS_MAXNAMELEN];
	char encoded_addr[NIS_MAXNAMELEN];
	size_t len = NIS_MAXNAMELEN;

	if (ref == 0)
		return (0);

	if (destination_ns == FNSP_nisplus_ns)
		sprintf(fs_addr, "%s.%s", host_name.str(),
		    domain_name.str());
	else
		sprintf(fs_addr, "%s", host_name.str());

	if (__fns_xdr_encode_string(fs_addr, encoded_addr, len) == 0) {
		delete ref;
		return (0);
	}

	FN_ref_addr addr(FNSP_fs_host_addr, len, encoded_addr);
	ref->append_addr(addr);

	return (ref);
}
