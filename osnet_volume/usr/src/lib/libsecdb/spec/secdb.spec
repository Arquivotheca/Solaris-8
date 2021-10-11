#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)secdb.spec	1.1	99/04/07 SMI"
#
# lib/libsecdb/spec/secdb.spec

function	kva_match
include		<secdb.h>
declaration	char *kva_match(kva_t *kva, char *key)
version		SUNW_1.1
exception	$return == NULL
end
