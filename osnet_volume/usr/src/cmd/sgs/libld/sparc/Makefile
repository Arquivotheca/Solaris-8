#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.38	99/06/23 SMI"

# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6
package :=	INTLLIB = /usr/lib/libintl.so.1

PLAT=sparc

include		$(SRC)/cmd/sgs/libld/Makefile.com

CHKSRCS=	$(SRCBASE)/uts/common/krtld/reloc.h

LIB64=		libld.so.3
$(LIB64) :=	SONAME = $(LIB64)

PICS64=		$(OBJECTS:%=pics64/%)
$(PICS64)  :=	sparc_CFLAGS += -xregs=no%appl -K pic
CPPFLAGS +=	-I../../../../uts/sparc/krtld

DYNFLAGS +=	$(DBGLIB) $(CONVLIB) $(ELFLIB)

SGSMSGTARG +=	$(SGSMSGSPARC) $(SGSMSG32)

.PARALLEL:	$(PICS64)

ROOTDYNLIB64=		$(ROOTLIBDIR)/$(LIB64)
$(ROOTDYNLIB64) :=	FILEMODE= 755

.KEEP_STATE:

.PARALLEL:	$(DYNLIB) $(LIBLINKS) $(LIB64)
all:		$(DYNLIB) $(LIBLINKS) $(LIB64)


$(LIB64) $(SGSPROTO)/$(LIB64): \
		pics64 .WAIT $$(PICS64) $(MAPFILES)
		$(CC) -o $@ -G $(DYNFLAGS) $(PICS64) $(LDLIBS)
		$(POST_PROCESS_SO)

native:		$(SGSPROTO)/$(DYNLIB) $(SGSPROTO)/$(LIB64)

native-proto:
		-@mkdir -p proto/$(MACH) proto/$(MACH64)

lint:		$(LINTLIB64) $(LINTOUT64)
.PARALLEL:	$(LINTOUT32) $(LINTOUT64)

pics64:
		-@mkdir -p $@

install \
package:	all $(ROOTDYNLIB) $(ROOTDYNLIB64)


clean: clean64

clean64:
		-$(RM) $(PICS64)

clobber:	clobber64

clobber64:
		-$(RM) $(LIB64)

delete:
		$(RM) $(DYNLIB) $(LIB64)


include		$(SRC)/cmd/sgs/libld/Makefile.targ

pics64/%.o:	$(SRCBASE)/uts/sparc/krtld/%.c
		$(COMPILE.c) -o $@ -D_ELF64 $<
		$(POST_PROCESS_O)

pics/%.o:	$(SRCBASE)/uts/sparc/krtld/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)
