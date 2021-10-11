#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getacinfo.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/getacinfo.spec

function	getacdir
include		<bsm/libbsm.h>
declaration	int getacdir( char	*dir, int len)
version		SUNW_0.7
errno		
exception	($return == -1 || $return == -2 || $return == -3 )
end		

function	getacmin
include		<bsm/libbsm.h>
declaration	int getacmin( int *min_val)
version		SUNW_0.7
errno		
exception	($return == -2 || $return == -3 )
end		

function	getacflg
include		<bsm/libbsm.h>
declaration	int getacflg( char	*auditstring, int len)
version		SUNW_0.7
errno		
exception	($return == -2 || $return == -3 )
end		

function	getacna
include		<bsm/libbsm.h>
declaration	int getacna( char *auditstring, int len)
version		SUNW_0.7
errno		
exception	($return == -2 || $return == -3 )
end		

function	setac
include		<bsm/libbsm.h>
declaration	void setac( void)
version		SUNW_0.7
errno		
end		

function	endac
include		<bsm/libbsm.h>
declaration	void endac( void)
version		SUNW_0.7
errno		
end		

