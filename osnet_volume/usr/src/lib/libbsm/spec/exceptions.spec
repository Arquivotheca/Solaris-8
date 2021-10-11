#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)exceptions.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/exceptions.spec

function	setauclassfile
declaration	int setauclassfile(char *fname)
version		SUNW_0.7
end		

function	setaueventfile
declaration	int setaueventfile(char *fname)
version		SUNW_0.7
end		

function	setauuserfile
declaration	int setauuserfile(char *fname)
version		SUNW_0.7
end		

function	testac
declaration	int testac(void)
version		SUNW_0.7
end		

