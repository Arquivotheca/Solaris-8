#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)au_open.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/au_open.spec

function	au_close
include		<bsm/libbsm.h>
declaration	int au_close(int d, int keep, short event)
version		SUNW_0.7
errno		
exception	$return == -1
end		

function	au_open
include		<bsm/libbsm.h>
declaration	int au_open(void)
version		SUNW_0.7
errno		
exception	$return == -1
end		

function	au_write
include		<bsm/libbsm.h>
declaration	int au_write(int d, token_t *m)
version		SUNW_0.7
errno		
exception	$return == -1
end		

