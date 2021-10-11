#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.22	99/06/23 SMI"

PLAT=		sparc

include		$(SRC)/cmd/sgs/libld/Makefile.com

#
# Share machrel.c with the 32-bit sparc directory since
# they both need the Elf32 and the Elf64 code.
#
L_MACHSRCS =	../$(MACH)/machrel.c ../$(MACH)/machsym.c

CHKSRCS=	$(SRCBASE)/uts/common/krtld/reloc.h

LIB64=		libld.so.3
$(LIB64) :=	SONAME = $(LIB64)

DBGLIB =	-L ../../liblddbg/$(MACH64)
CONVLIB =	-L ../../libconv/$(MACH64)
ELFLIB =	-L ../../libelf/$(MACH64)
DYNFLAGS +=	$(DBGLIB) $(CONVLIB) $(ELFLIB)

LINTFLAGS64 +=	-D__sparcv9

SGSMSGCHK=	../common/libld.sparc64chk.msg
SGSMSGTARG +=	$(SGSMSGSPARC)

.PARALLEL:	$(LINTOUT32) $(LINTOUT64)

PICS64=		$(OBJECTS:%=pics64/%)
$(PICS64) :=	sparcv9_CFLAGS += -xregs=no%appl -K pic
CPPFLAGS +=     -I../../../../uts/sparc/krtld

.PARALLEL:	$(PICS64)

ROOTLIBDIR=		$(ROOT)/usr/lib/$(MACH64)
ROOTDYNLIB64=		$(ROOTLIBDIR)/$(LIB64)
$(ROOTDYNLIB64) :=	FILEMODE= 755

.KEEP_STATE:

.PARALLEL:	$(DYNLIB) $(LIBLINKS) $(LIB64)
all:		$(DYNLIB) $(LIBLINKS) $(LIB64)

$(LIB64) $(SGSPROTO)/$(LIB64): \
		pics64 .WAIT $$(PICS64) $(MAPFILES)
		$(CC) $(sparcv9_XARCH) -o $@ -G $(DYNFLAGS) $(PICS64) $(LDLIBS)
		$(POST_PROCESS_SO)

lint:		$(LINTLIB64) $(LINTOUT64)

#
# Share machrel.c with the base architecture, since they
# both need all the Elf32 and Elf64 code.
#
pics/%.o:	../$(MACH)/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

pics64/%.o:	../$(MACH)/%.c
		$(COMPILE.c) -D_ELF64 -o $@ $<
		$(POST_PROCESS_O)

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

delete:
		$(RM) $(DYNLIB) $(LIB64)

include		$(SRC)/cmd/sgs/libld/Makefile.targ
include		../../Makefile.sub.64

$(LIBLINKS) :=	DYNLIB=$(LIB64)

pics64/%.o:	$(SRCBASE)/uts/sparc/krtld/%.c
		$(COMPILE.c) -o $@ -D_ELF64 $<
		$(POST_PROCESS_O)

pics/%.o:	$(SRCBASE)/uts/sparc/krtld/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)
