#! /bin/sh
#
#ident	"@(#)libsys.sh	1.4	95/02/24 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# Stub library for programmer's interface to libsys.  Used to satisfy ld(1)
# processing, and serves as a precedence place-holder at execution-time.

awk '
BEGIN {
	printf("\t.file\t\"libsyss.s\"\n\t.section\t\".text\"\n");
}
/.*/ {
	printf("\t.global\t%s\n%s:\n\tt 5\n\t.type\t%s,#function\n\t.size\t%s,.-%s\n", $0, $0, $0, $0, $0);
}
' libsyss.list	>	libsyss.s
