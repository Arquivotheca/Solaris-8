#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)yp.spec	1.2	99/05/14 SMI"
#
# lib/libnsl/spec/yp.spec

function	yp_update
include		<rpcsvc/ypclnt.h>
declaration	int yp_update(char *domain, char *map, unsigned  ypop,\
			char *key, int keylen, char *data, int datalen)
version		SUNW_0.7
exception	$return == YPERR_KEY
end

function	yp_bind
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_bind (char *indomain)
version		SUNW_0.7
exception	$return != 0
end

function	yp_master
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_master(char *indomain, char *inmap, char **outname)
version		SUNW_0.7
exception	$return != 0
end

function	yperr_string
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	char *yperr_string(int incode)
version		SUNW_0.7
exception	$return == 0
end

function	ypprot_err
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int ypprot_err	(int incode)
version		SUNW_0.7
exception	$return != 0
end

function	yp_unbind
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	void yp_unbind(char *indomain)
version		SUNW_0.7
end

function	yp_get_default_domain
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_get_default_domain (char **outdomain)
version		SUNW_0.7
exception	$return != 0
end

function	yp_match
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_match(char *indomain, char *inmap, char *inkey,\
			int inkeylen, char **outval, int *outvallen)
version		SUNW_0.7
exception	$return != 0
end

function	yp_first
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_first(char *indomain, char *inmap, char **outkey, \
			int *outkeylen, char **outval, int *outvallen)
version		SUNW_0.7
exception	$return != 0
end

function	yp_next
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_next(char *indomain, char *inmap, char *inkey, \
			int inkeylen, char **outkey, int *outkeylen, \
			char **outval, int *outvallen)
version		SUNW_0.7
exception	$return != 0
end

function	yp_all
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_all(char *indomain, char *inmap, \
			struct ypall_callback *incallback)
version		SUNW_0.7
exception	$return != 0
end

function	yp_order
include		<rpcsvc/ypclnt.h>, <rpcsvc/yp_prot.h>
declaration	int yp_order(char *indomain, char *inmap, \
			unsigned long *outorder)
version		SUNW_0.7
exception	$return != 0
end

function	fetch
version		SUNW_0.7
end

function	firstkey
version		SUNW_0.7
end

function	nextkey
declaration	datum nextkey(datum key)
version		SUNW_0.7
end

function	store
declaration	datum store(datum key, datum dat)
version		SUNW_0.7
end
