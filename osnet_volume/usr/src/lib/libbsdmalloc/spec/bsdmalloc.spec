#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)bsdmalloc.spec	1.1	99/01/25 SMI"
#
# lib/libbsdmalloc/spec/bsdmalloc.spec

function	free
declaration	int free(char *cp)
version		SUNW_1.1
end		

function	malloc
declaration	char *malloc(unsigned nbytes)
version		SUNW_1.1
end		

function	realloc
declaration	char *realloc(char *cp, unsigned nbytes)
version		SUNW_1.1
end		

