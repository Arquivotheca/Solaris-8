#! /bin/sh
#
#ident	"@(#)libsys.sh	1.4	95/03/03 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# Stub library for programmer's interface to libsys.  Used to satisfy ld(1)
# processing, and serves as a precedence place-holder at execution-time.

awk '
/.*/ {
	if ($2 == "1") {
		printf("#pragma weak %s = _%s\n", $3, $3);
		flag = "_";
	} else
		flag = "";
	if ($1 == "f") {
		printf("void *\n%s%s()\n{\n", flag, $3);
		printf("\t/*NOTREACHED*/\n}\n\n");
	} else {
		if ($4 == "1")
			printf("%s %s%s %s %s\n\n", $5, flag, $3, $6, $7);
		else if ($4 == "2")
			printf("%s %s %s%s %s %s\n\n", $5, $6, flag, $3, $7, $8);
		else if ($4 == "3")
			printf("%s %s %s%s%s %s %s    %s\n\n", $5, $6, flag, $3, $7, $8, $9, $10);
	}
}
' libsys.list	>	libsys.c
