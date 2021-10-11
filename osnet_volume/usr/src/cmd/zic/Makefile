#
#ident	"@(#)Makefile	1.27	99/06/17 SMI"
#
# Copyright (c) 1989-1998 by Sun Microsystems, Inc.
# All rights reserved.
#

NATIVE=native
PROG= zic
NPROG= $(NATIVE)/$(PROG)

# "etcetera" must be listed before "backward" because it sets up links that
# "backward" needs.
TZFILES= africa antarctica asia australasia etcetera europe \
	 factory northamerica pacificnew solar87 solar88 solar89 \
	 southamerica systemv backward
SRCFILES = $(TZFILES) README

OBJS= zic.o scheck.o ialloc.o
SRCS= $(OBJS:%.o=%.c)
NOBJS= $(OBJS:%=$(NATIVE)/%)
OWNER= root
GROUP= bin

CLOBBERFILES= $(NOBJS) $(NPROG) $(NATIVE)/zic.c $(NATIVE)/tzfile.h

include ../Makefile.cmd

NCPPFLAGS=	-I$(NATIVE)
ROOTTZD=	$(ROOTSHLIB)/zoneinfo
ROOTTZSRCD=	$(ROOTSHLIB)/zoneinfo/src
DIRS=  $(ROOTTZSRCD)

ROOTLIBTZSRCFILES=	$(SRCFILES:%=$(ROOTTZSRCD)/%)

$(ROOTLIBTZSRCFILES):=	FILEMODE=	0644

.KEEP_STATE:

all: $(PROG) $(NPROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(DIRS) $(ROOTTZSRCD) $(ROOTUSRSBINPROG) $(ROOTLIBTZSRCFILES)
	$(NPROG) -d $(ROOTTZD) $(TZFILES)
	$(CH) $(CHOWN) -R $(OWNER) $(ROOTTZD)
	$(CH) $(CHGRP) -R $(GROUP) $(ROOTTZD)

$(NPROG): $(NATIVE) $(NOBJS)
	$(NATIVECC) -o $@ $(NOBJS)

$(ROOTTZSRCD)/%: %
	$(INS.file)

$(DIRS):
	$(INS.dir)

$(NATIVE):
	-@mkdir -p $@

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

$(NOBJS): $$(@F:.o=.c)
	$(NATIVECC) -O $(NCPPFLAGS) -c -o $@ $(@F:.o=.c)

$(NATIVE)/zic.o: $(NATIVE)/tzfile.h

#$(NATIVE)/scheck.o: scheck.c
#	$(NATIVECC) -c -o $@ scheck.c

#$(NATIVE)/ialloc.o: ialloc.c
#	$(NATIVECC) -c -o $@ ialloc.c

$(NATIVE)/tzfile.h: $(ROOT)/usr/include/tzfile.h
	$(RM) $@; cp $(ROOT)/usr/include/tzfile.h $@

include ../Makefile.targ
