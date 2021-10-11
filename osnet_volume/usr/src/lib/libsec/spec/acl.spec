#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)acl.spec	1.1	99/01/25 SMI"
#
# lib/libsec/spec/acl.spec

function	aclcheck
include		<sys/acl.h>
declaration	int aclcheck(aclent_t *aclbufp,  int nentries,  int *which)
version		SUNW_0.9
errno		EINVAL
exception	($return == GRP_ERROR	|| \
			$return == USER_ERROR	|| \
			$return == CLASS_ERROR	|| \
			$return == OTHER_ERROR	|| \
			$return == DUPLICATE_ERROR	|| \
			$return == ENTRY_ERROR	|| \
			$return == MISS_ERROR	|| \
			$return == MEM_ERROR)
end		

function	aclsort
include		<sys/acl.h>
declaration	int aclsort(int nentries, int calclass, aclent_t *aclbufp)
version		SUNW_0.9
exception	$return == -1
end		

function	acltomode
include		<sys/types.h>, <sys/acl.h>
declaration	int acltomode(aclent_t *aclbufp, int nentries, mode_t *modep)
version		SUNW_0.9
errno		EINVAL
exception	$return == -1
end		

function	aclfrommode
include		<sys/types.h>, <sys/acl.h>
declaration	int aclfrommode(aclent_t *aclbufp, int  nentries, mode_t *modep)
version		SUNW_0.9
errno		EINVAL
exception	$return == -1
end		

function	acltotext
include		<sys/acl.h>
declaration	char *acltotext(aclent_t *aclbufp, int aclcnt)
version		SUNW_0.9
exception	$return == 0
end		

function	aclfromtext
include		<sys/acl.h>
declaration	aclent_t *aclfromtext(char *acltextp, int *aclcnt)
version		SUNW_0.9
exception	$return == 0
end		

