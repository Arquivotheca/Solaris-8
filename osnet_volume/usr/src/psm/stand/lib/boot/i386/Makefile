#
#ident	"@(#)Makefile	1.1	97/06/30 SMI"
#
# Copyright (c) 1994-1997,  by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/boot/i386/Makefile
#
# i386 architecture Makefile for Standalone Library
# Platform-specific, but shared, routines.
#

TOPDIR =	../../../../..

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PSMSYSHDRDIR =	$(TOPDIR)/psm/stand

LIBBOOT =	libboot.a
LINTLIBBOOT =	llib-lboot.ln

# ARCHCMNDIR - common code for several machines of a given isa
# OBJSDIR - where the .o's go

ARCHCMNDIR =	common
OBJSDIR =	objs

BOOTSRCS =	bootops.c
BOOTOBJS =	$(BOOTSRCS:%.c=%.o)

OBJS =		$(BOOTOBJS:%=$(OBJSDIR)/%)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS =	$(BOOTSRCS:%=$(ARCHCMNDIR)/%)

PLATFORM =	i86pc

CPPINCS +=	-I$(ROOT)/usr/platform/$(PLATFORM)/include
CPPINCS	+= 	-I$(PSMSYSHDRDIR)
CPPFLAGS =	$(CPPFLAGS.master) $(CPPINCS) $(CCYFLAG)$(PSMSYSHDRDIR)
ASFLAGS =	-P -D__STDC__ -D_ASM $(CPPFLAGS.master) $(CPPINCS)
CFLAGS +=	-v -D_KERNEL -D_BOOT

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBBOOT) .WAIT lint

lint: $(LINTLIBBOOT)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBBOOT) $(LINTLIBBOOT) a.out core

$(LIBBOOT): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBBOOT): $(OBJSDIR) .WAIT $(L_OBJS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LINT.lib) $(CPPINCS) $(CFLAGS) -o boot $(L_SRCS)

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
	@($(LHEAD) $(LINT.c) $(CFLAGS) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(ARCHCMNDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@

