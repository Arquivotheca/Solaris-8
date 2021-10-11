#
# Copyright (c) 1989,1990,1992-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.18	99/01/25 SMI"
#
# lib/libresolv/Makefile
#
LIBRARY= libresolv.a
VERS= .1

PICS=   pics/res_comp.o pics/res_debug.o pics/res_init.o \
	pics/res_mkquery.o pics/res_query.o pics/res_send.o \
	pics/res_gethost.o pics/res_sethost.o pics/res_strcase.o

objs/%.o pics/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

OBJECTS= \
res_gethost.o   res_comp.o      res_debug.o     res_init.o    res_mkquery.o  \
res_query.o     res_send.o      res_sethost.o   res_strcase.o

# include library definitions
include ../Makefile.lib

SUBDIRS=	spec

MAPDIR=		./spec/$(MACH)
MAPFILE=	$(MAPDIR)/mapfile

CLOBBERFILES +=	$(MAPFILE)

CFLAGS += -I.
CPPFLAGS += -DDEBUG -DSYSV -D_REENTRANT
LDLIBS += -lsocket -lnsl -lc
DYNFLAGS +=	-M $(MAPFILE)

ROOTDYNLIBS=	$(DYNLIB:%=$(ROOTLIBDIR)/%)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

.KEEP_STATE:

LIBS += $(LIBRARY) $(DYNLIB)

all: $(SUBDIRS) $(LIBS)

install: all $(ROOTDYNLIBS) $(SUBDIRS)

clean clobber: $(SUBDIRS)

spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../Makefile.targ

FRC:
