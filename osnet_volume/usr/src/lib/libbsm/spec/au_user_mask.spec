#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)au_user_mask.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/au_user_mask.spec

function	au_user_mask
include		<bsm/libbsm.h>
declaration	int au_user_mask( char *username, au_mask_t *mask_p)
version		SUNW_0.7
errno		
exception	$return == -1
end		

