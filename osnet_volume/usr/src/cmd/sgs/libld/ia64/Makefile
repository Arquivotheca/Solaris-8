#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	99/06/23 SMI"

PLAT=		intel/ia64

include		$(SRC)/cmd/sgs/libld/Makefile.com

#
# Share machsym.c with the 32-bit i386 directory since
# they both need the Elf32 and the Elf64 code.
#
L_MACHSRCS =	machrel.c ../$(MACH)/machsym.c

CHKSRCS=	$(SRCBASE)/uts/common/krtld/reloc.h

LIB64=		libld.so.3
$(LIB64) :=	SONAME = $(LIB64)

DBGLIB =	-L ../../liblddbg/$(MACH64)
CONVLIB =	-L ../../libconv/$(MACH64)
ELFLIB =	-L ../../libelf/$(MACH64)
DYNFLAGS +=	$(DBGLIB) $(CONVLIB) $(ELFLIB)

SGSMSGCHK=	../common/libld.intel64chk.msg
SGSMSGTARG +=	$(SGSMSGINTEL)

#LINTFLAGS64 +=	-D__sparcv9

.PARALLEL:	$(LINTOUT32) $(LINTOUT64)

PICS64=		$(OBJECTS:%=pics64/%)
$(PICS64) :=	ia64_CFLAGS += -DELF_TARGET_IA64
$(PICS) :=	ia64_CFLAGS += -DELF_TARGET_386
$(PICS64) :=    CPPFLAGS += -I../../../../uts/intel/ia64/krtld
$(PICS) :=      CPPFLAGS += -I../../../../uts/intel/ia32/krtld

.PARALLEL:	$(PICS64)

ROOTLIBDIR=		$(ROOT)/usr/lib/$(MACH64)
ROOTDYNLIB64=		$(ROOTLIBDIR)/$(LIB64)
$(ROOTDYNLIB64) :=	FILEMODE= 755

.KEEP_STATE:

.PARALLEL:	$(DYNLIB) $(LIBLINKS) $(LIB64)
all:		$(DYNLIB) $(LIBLINKS) $(LIB64)


$(LIB64) $(SGSPROTO)/$(LIB64): \
		pics64 .WAIT $$(PICS64) $(MAPFILES)
		$(CC) $(ia64_XARCH) -o $@ -G $(DYNFLAGS) $(PICS64) $(LDLIBS)
		$(POST_PROCESS_SO)

lint:		$(LINTLIB64) $(LINTOUT64)

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

#
# Grab 32bit i386 files for libld.so.2
#
pics/machrel.o:	../i386/machrel.c
		$(COMPILE.c) -o $@ -D_ELF32 ../i386/machrel.c
		$(POST_PROCESS_O)

pics/machsym.o:	../i386/machsym.c
		$(COMPILE.c) -o $@ -D_ELF32 ../i386/machsym.c
		$(POST_PROCESS_O)

pics64/machsym.o:	../i386/machsym.c
		$(COMPILE.c) -o $@ -D_ELF64 ../i386/machsym.c
		$(POST_PROCESS_O)


include		$(SRC)/cmd/sgs/libld/Makefile.targ
include		../../Makefile.sub.64

$(LIBLINKS) :=	DYNLIB=$(LIB64)

pics64/%.o:	$(SRCBASE)/uts/intel/ia64/krtld/%.c
		$(COMPILE.c) -o $@ -D_ELF64 $<
		$(POST_PROCESS_O)

pics/%.o:	$(SRCBASE)/uts/intel/ia32/krtld/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)
