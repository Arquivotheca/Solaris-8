#
#ident	"@(#)Makefile	1.4	00/09/14 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
include ../Makefile.master

COMMON_SUBDIRS= \
	bfuld \
	codereview \
	env \
	fastfs \
	pmodes \
	gk \
	install.bin \
	protocmp \
	protolist \
	scripts

XMODS=


sparc_SUBDIRS= \
	stabs \
	tokenize

i386_subdirs=

SUBDIRS= $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS) $(XMODS)

ROOTDIRS= \
	$(ROOT)/opt/onbld \
	$(ROOT)/opt/onbld/bin \
	$(ROOT)/opt/onbld/bin/$(MACH) \
	$(ROOT)/opt/onbld/env \
	$(ROOT)/opt/onbld/gk \
	$(ROOT)/opt/onbld/man \
	$(ROOT)/opt/onbld/man/man1
OWNER=	root
GROUP=	bin


all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg

.KEEP_STATE:

all clean clobber: $(SUBDIRS)

install: $(ROOTDIRS) .WAIT $(SUBDIRS)

pkg: install
	@cd SUNWonbld; pwd; $(MAKE) install

_msg: $(MSGSUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

$(ROOTDIRS):
	$(INS.dir)

