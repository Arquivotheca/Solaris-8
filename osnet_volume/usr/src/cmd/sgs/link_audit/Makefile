#
#ident	"@(#)Makefile	1.7	99/03/30 SMI"
#
# Copyright (c) 1996,1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../../../Makefile.master

ROOTDEMODIRBASE=	$(ROOT)/usr/demo/link_audit

DEMOFILES= \
		00README \
		Makefile \
		man/sotruss.man \
		man/perfcnt.man \
		man/symbindrep.man \
		man/whocalls.man \
		man/dumpbind.man \
		src/bindings.c \
		src/bindings.h \
		src/dumpbind.c \
		src/env.c \
		src/env.h \
		src/hash.c \
		src/hash.h \
		src/sotruss.ksh \
		src/mach.h \
		src/perfcnt.c \
		src/perfcnt.ksh \
		src/symbindrep.c \
		src/symbindrep.ksh \
		src/truss.c \
		src/who.c \
		src/who.h \
		src/whocalls.ksh

ROOTDEMODIRS=	$(ROOTDEMODIRBASE) \
		 .WAIT \
		 $(ROOTDEMODIRBASE)/man \
		 $(ROOTDEMODIRBASE)/src

ROOTDEMOFILES=	$(DEMOFILES:%=$(ROOTDEMODIRBASE)/%)

$(ROOTDEMODIRS) :=	OWNER =		root
$(ROOTDEMODIRS) :=	GROUP =		bin
$(ROOTDEMODIRS) :=	DIRMODE =	755




SUBDIRS=	$(MACH)

all:=		TARGET=	all
install:=	TARGET=	install
install_onld:=	TARGET=	install_onld
clean:=		TARGET=	clean
clobber:=	TARGET=	clobber
lint:=		TARGET=	lint

.PARALLEL: $(ROOTDEMOFILES)

.KEEP_STATE:

install_onld all clean clobber lint: \
		$(SUBDIRS)

install: $(ROOTDEMODIRS) .WAIT $(SUBDIRS) $(ROOTDEMOFILES)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)
		@if [ -d $(MACH64) ]; then                      \
			cd $(MACH64); pwd; $(MAKE) $(TARGET);   \
		else /bin/true; fi

$(ROOTDEMODIRS):
	$(INS.dir)

$(ROOTDEMODIRBASE)/man/%: man/%
	$(INS.file)

$(ROOTDEMODIRBASE)/src/%: common/%
	$(INS.file)

$(ROOTDEMODIRBASE)/%: common/%
	$(INS.file)

$(ROOTDEMODIRBASE)/%: common/%.demo
	$(INS.rename)

FRC:
