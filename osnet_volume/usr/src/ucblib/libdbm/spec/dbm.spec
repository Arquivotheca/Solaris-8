#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)dbm.spec	1.1	99/01/25 SMI"
#
# ucblib/libdbm/spec/dbm.spec

function	delete
include		<dbm.h>
declaration	int delete(datum key)
version		SUNW_1.1
exception	$return < 0
end		

function	firstkey
include		<dbm.h>
declaration	datum firstkey(void)
version		SUNW_1.1
exception	$return.dptr == 0
end		

function	nextkey
include		<dbm.h>
declaration	datum nextkey(datum key)
version		SUNW_1.1
exception	$return.dptr == 0
end		

function	dbminit
include		<dbm.h>
declaration	int dbminit(char *file)
version		SUNW_1.1
exception	$return < 0
end		

function	dbmclose
include		<dbm.h>
declaration	int dbmclose(void)
version		SUNW_1.1
exception	$return < 0
end		

function	fetch
include		<dbm.h>
declaration	datum fetch(datum key)
version		SUNW_1.1
exception	$return.dptr == 0
end		

function	store
include		<dbm.h>
declaration	datum store(datum key, datum dat)
version		SUNW_1.1
exception	$return.dptr == 0
end		

data		bitno
version		SUNW_1.1
end		

data		blkno
version		SUNW_1.1
end		

function	calchash
declaration	long calchash(datum dat)
version		SUNW_1.1
end		

data		dbrdonly
version		SUNW_1.1
end		

data		dirbuf
version		SUNW_1.1
end		

data		dirf
version		SUNW_1.1
end		

function	hashinc
declaration	long hashinc(long h)
version		SUNW_1.1
end		

data		hmask
version		SUNW_1.1
end		

function	makdatum
declaration	datum makdatum(char *s, int l)
version		SUNW_1.1
end		

data		pagbuf
version		SUNW_1.1
end		

data		pagf
version		SUNW_1.1
end		

data		maxbno
version		SUNW_1.1
end		

