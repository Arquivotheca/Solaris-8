#
# ident	"@(#)Makefile	1.3	98/07/21 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

PROGS = dblfree exhaust align dirtymem general wideload failwide

all: ${PROGS}

dblfree: dblfree.c
	cc -o $@ -O $? -I../common -L../${MACH} -R../${MACH} -lmtmalloc

exhaust: exhaust.c
	cc -o $@ -O $? -I../common -L../${MACH} -R../${MACH} -lmtmalloc -lthread

align: align.c
	cc -o $@ -O $? -L../${MACH} -R../${MACH} -lmtmalloc

dirtymem: dirtymem.c
	cc -o $@ -O $? -I../common -L../${MACH} -R../${MACH} -lmtmalloc

general: general.c
	cc -o $@ -O $? -L../${MACH} -R../${MACH} -lmtmalloc -lthread

wideload: wideload.c
	cc -o $@ -O $? -L../${MACH} -R../${MACH} -lmtmalloc

failwide: failwide.c
	cc -o $@ -O $? -L../${MACH} -R../${MACH} -lmtmalloc

clobber clean:
	rm -f ${PROGS} core
