#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)au_preselect.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/au_preselect.spec

function	au_preselect
include		<bsm/libbsm.h>
declaration	int au_preselect(au_event_t event, au_mask_t *mask_p, \
			int sorf, int flag)
version		SUNW_0.7
errno		
exception	($return == -1)
end		

