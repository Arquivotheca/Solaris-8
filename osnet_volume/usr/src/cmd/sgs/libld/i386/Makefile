#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.16	99/06/23 SMI"

# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6
package :=	INTLLIB = /usr/lib/libintl.so.1

PLAT=		intel/ia32

include		$(SRC)/cmd/sgs/libld/Makefile.com
COPTFLAG=

LIB64=		libld.so.3
$(LIB64) :=	SONAME = $(LIB64)

DYNFLAGS +=	$(DBGLIB) $(CONVLIB) $(ELFLIB)

SGSMSGTARG +=	$(SGSMSGINTEL) $(SGSMSG32)

PICS64=		$(OBJECTS:%=pics64/%)
$(PICS64) :=	i386_CFLAGS += -K pic -DELF_TARGET_IA64
$(PICS64) :=	CPPFLAGS += -I../../../../uts/intel/ia64/krtld
$(PICS) :=	CPPFLAGS += -I../../../../uts/intel/ia32/krtld
$(LINTOUT64) :=	CPPFLAGS += -I../../../../uts/intel/ia64/krtld
$(LINTOUT32) :=	CPPFLAGS += -I../../../../uts/intel/ia32/krtld

.PARALLEL:	$(PICS64)

ROOTDYNLIB64=		$(ROOTLIBDIR)/$(LIB64)
$(ROOTDYNLIB64) :=	FILEMODE= 755

.KEEP_STATE:

.PARALLEL:	$(DYNLIB) $(LIBLINKS) $(LIB64)
all:		$(DYNLIB) $(LIBLINKS) $(LIB64)

$(LIB64) $(SGSPROTO)/$(LIB64): \
		pics64 .WAIT $$(PICS64) $(MAPFILE)
		$(CC) -o $@ -dy -G $(DYNFLAGS) $(PICS64) $(LDLIBS)
		$(POST_PROCESS_SO)

native:		$(SGSPROTO)/$(DYNLIB) $(SGSPROTO)/$(LIB64)

native-proto:
		-@mkdir -p proto/$(MACH) proto/$(MACH64)

pics64:
		-@mkdir -p $@

install package:	all $(ROOTDYNLIB) $(ROOTDYNLIB64)

clean: clean64

clean64:
		-$(RM) $(PICS64)

clobber: clobber64
clobber64:
		-$(RM) $(LIB64)

pics64/%.o:	../ia64/%.c
		$(COMPILE.c) -o $@ -D_ELF64 $<
		$(POST_PROCESS_O)

include		$(SRC)/cmd/sgs/libld/Makefile.targ

pics64/%.o:	$(SRCBASE)/uts/intel/ia64/krtld/%.c
		$(COMPILE.c) -o $@ -D_ELF64 $<
		$(POST_PROCESS_O)

pics/%.o:	$(SRCBASE)/uts/intel/ia32/krtld/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)
