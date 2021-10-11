#
#pragma	ident	"@(#)Makefile.com	1.1	97/06/30 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/boot/sparc/Makefile.com
#
# SPARC architecture Makefile for Standalone Library
# Platform-specific, but shared between platforms.
# Firmware dependent.
#

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PSMSYSHDRDIR =	$(TOPDIR)/psm/stand

LIBBOOT =	libboot.a
LINTLIBBOOT =	llib-lboot.ln

# ARCHCMNDIR - common code for several machines of a given isa
# OBJSDIR - where the .o's go

ARCHCMNDIR =	$(TOPDIR)/uts/sparc/os
OBJSDIR =	objs

CMNSRCS =	bootops.c
BOOTSRCS =	$(PLATSRCS) $(CMNSRCS)
BOOTOBJS =	$(BOOTSRCS:%.c=%.o)

OBJS =		$(BOOTOBJS:%=$(OBJSDIR)/%)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS =	$(CMNSRCS:%=$(ARCHCMNDIR)/%) $(PLATSRCS)

CPPINCS +=	-I$(SRC)/uts/common
CPPINCS +=	-I$(SRC)/uts/sun
CPPINCS +=	-I$(SRC)/uts/sparc
CPPINCS +=	-I$(SRC)/uts/sparc/$(ARCHVERS)
CPPINCS +=	-I$(SRC)/uts/$(PLATFORM)
CPPINCS += 	-I$(ROOT)/usr/include/$(ARCHVERS)
CPPINCS += 	-I$(ROOT)/usr/platform/$(PLATFORM)/include
CPPINCS += 	-I$(PSMSYSHDRDIR)
CPPFLAGS =	$(CPPINCS) $(CPPFLAGS.master) $(CCYFLAG)$(PSMSYSHDRDIR)
CPPFLAGS +=	-D_KERNEL -D_BOOT
ASFLAGS =	-P -D__STDC__ -D_ASM $(CPPFLAGS.master) $(CPPINCS)
CFLAGS +=	-v

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBBOOT) .WAIT

lint: $(LINTLIBBOOT)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBBOOT) $(LINTLIBBOOT) a.out core

$(LIBBOOT): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBBOOT): $(OBJSDIR) .WAIT $(L_OBJS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LINT.lib) -o boot $(L_SRCS)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# build rules using standard library object subdirectory
#
$(OBJSDIR)/%.o: $(ARCHCMNDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.o: $(ARCHCMNDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.ln: $(ARCHCMNDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(ARCHCMNDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@
