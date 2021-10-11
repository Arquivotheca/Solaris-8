#
#ident	"@(#)Makefile	1.17	99/05/14 SMI"
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread_db/Makefile
#

LIBRARY= libthread_db.a
VERS= .0

OBJECTS=  \
thread_db.o

# libsocket build rules
#objs/%.o profs/%.o pics/%.o: inet/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)
#
#objs/%.o profs/%.o pics/%.o: socket/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

# include library definitions
include ../Makefile.lib

MAPDIR=		./spec/$(MACH)
MAPFILE=	$(MAPDIR)/mapfile.0
CLOBBERFILES +=	$(MAPFILE)

SRCS=	$(OBJECTS:%.o=%.c)

ZDEFS=
THR = -I../libthread
CPPFLAGS += $(THR)/inc $(THR)/common $(THR)/$(MACH) $(THR)/$(MACH)/ml $(THR)/sys/inc
LIBS += $(DYNLIB)
LDLIBS += -ldl -lc
DYNFLAGS += -M $(MAPFILE)

.KEEP_STATE:

all: $(LIBS) FRC
	@cd spec; pwd; $(MAKE) all
	(cd $(MACH); $(MAKE) all;)
	$(BUILD64)(cd $(MACH64); $(MAKE) all;)

install: all $(ROOTLIBS) FRC
	@cd spec; pwd; $(MAKE) install
	(cd $(MACH); $(MAKE) install;)
	$(BUILD64)(cd $(MACH64); $(MAKE) install;)

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) `basename $@`

clean:	clean_mach

clean_mach:
	@cd spec; pwd; $(MAKE) clean
	(cd $(MACH); $(MAKE) clean;)
	$(BUILD64)(cd $(MACH64); $(MAKE) clean;)

clobber: clobber_spec

clobber_spec:
	@cd spec; pwd; $(MAKE) clobber

# include library targets
include ../Makefile.targ

lint: 

FRC:
