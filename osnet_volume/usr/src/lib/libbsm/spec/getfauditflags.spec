#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getfauditflags.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/getfauditflags.spec

function	getfauditflags
include		<sys/param.h>, <bsm/libbsm.h>
declaration	int getfauditflags(au_mask_t *usremasks, \
			au_mask_t *usrdmasks, au_mask_t *lastmasks)
version		SUNW_0.7
errno		
exception	$return == -1
end		

