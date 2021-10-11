#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getauditflags.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/getauditflags.spec

function	getauditflagsbin
include		<sys/param.h>, <bsm/libbsm.h>
declaration	int getauditflagsbin(char *auditstring, au_mask_t *masks)
version		SUNW_0.7
errno		
exception	$return == -1
end		

function	getauditflagschar
include		<sys/param.h>, <bsm/libbsm.h>
declaration	int getauditflagschar(char	*auditstring, \
			au_mask_t	*masks, int verbose)
version		SUNW_0.7
errno		
exception	$return == -1
end		

