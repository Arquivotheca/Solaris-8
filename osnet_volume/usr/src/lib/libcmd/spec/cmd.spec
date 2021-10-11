#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)cmd.spec	1.1	99/01/25 SMI"
#
# lib/libcmd/spec/cmd.spec

function	defopen
declaration	int defopen(char *fn)
version		SUNWprivate_1.1
end		

function	defread
declaration	char *defread(char *cp)
version		SUNWprivate_1.1
end		

function	defcntl
declaration	int defcntl(int cmd, int newflags)
version		SUNWprivate_1.1
end		

function	getterm
declaration	int getterm(char *tname, char *buffer, char *filename, \
			char *deftype)
version		SUNWprivate_1.1
end		

function	mkmtab
declaration	int mkmtab(char *magfile, int cflg)
version		SUNWprivate_1.1
end		

function	ckmtab
declaration	int ckmtab(char *buf, int bufsize, int silent)
version		SUNWprivate_1.1
end		

function	prtmtab
declaration	void prtmtab(void)
version		SUNWprivate_1.1
end		

function	sumpro
include		<sum.h>
declaration	int sumpro(struct suminfo *sip)
version		SUNWprivate_1.1
end		

function	sumupd
include		<sum.h>
declaration	int sumupd(struct suminfo *sip, char *buf, int cnt)
version		SUNWprivate_1.1
end		

function	sumepi
include		<sum.h>
declaration	int sumepi(struct suminfo *sip)
version		SUNWprivate_1.1
end		

function	sumout
include		<sum.h>
declaration	int sumout(FILE *fp, struct suminfo *sip)
version		SUNWprivate_1.1
end		

