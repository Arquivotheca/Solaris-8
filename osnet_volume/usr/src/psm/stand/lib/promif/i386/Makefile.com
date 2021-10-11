#
#pragma ident	"@(#)Makefile.com	1.11	99/05/04 SMI"
#
# Copyright (c) 1991-1994,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/i386/promif/Makefile
#
# create the appropriate type of prom library, based on $(BOOTCFLAGS)
# and put the output in $(OBJSDIR); these flags are both passed in from
# the caller
#
# NOTE that source is included from the uts/intel/promif directory
#

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PROMDIR	=	$(TOPDIR)/uts/intel/promif
STANDSYSDIR=	$(TOPDIR)/stand/$(MACH)
SYSDIR	=	$(TOPDIR)/uts

LIBPROM	=	libprom.a
LINTLIBPROM = 	llib-lprom.ln

PROM_CFILES =		\
	prom_getchar.c		\
	prom_getversion.c	\
	prom_init.c		\
	prom_printf.c		\
	prom_putchar.c

PROM_SFILES =

PROM_FILES =	$(PROM_CFILES) $(PROM_SFILES)

KARCH	=	i86
KSUN	=	intel
MMU	=	i86pc

OBJSDIR	=	objs

PROM_COBJ =	$(PROM_CFILES:%.c=$(OBJSDIR)/%.o)
PROM_SOBJ =	$(PROM_SFILES:%.s=$(OBJSDIR)/%.o)
OBJS =		$(PROM_COBJ) $(PROM_SOBJ)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS =	$(PROM_FILES:%=$(PROMDIR)/%)

ARCHOPTS=	-Di386 -D__i386 -DBOOTI386
ASFLAGS =	-P -D__STDC__ -D_BOOT -D_ASM
CPPDEFS	+=	$(ARCHOPTS) -D$(KARCH) -D_BOOT -D_KERNEL -D_MACHDEP
CPPINCS =	-I. -I$(SYSDIR)/$(KSUN) -I$(SYSDIR)/$(MMU) \
		-I$(SYSDIR)/$(MACH) -I$(STANDSYSDIR) \
		-I$(SYSDIR)/common
CPPFLAGS =	$(CPPDEFS) $(CPPINCS) $(CPPFLAGS.master)
#
# XXX	This should be globally enabled!
CFLAGS +=	-v

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBPROM)

lint: $(LINTLIBPROM)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBPROM) $(LINTLIBPROM) a.out core

$(LIBPROM):  $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBPROM): $(OBJSDIR) .WAIT $(L_OBJS)
	@-$(ECHO) "lint library construction:" $@
	@$(LINT.lib) -o prom $(L_SRCS)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# build rules using standard library object subdirectory
#
$(OBJSDIR)/%.o: $(PROMDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.o: $(PROMDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.ln: $(PROMDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(PROMDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@
