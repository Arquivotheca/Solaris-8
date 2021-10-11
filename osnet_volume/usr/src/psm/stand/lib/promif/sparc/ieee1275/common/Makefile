#
#ident	"@(#)Makefile	1.11	96/09/12 SMI"
#
# Copyright (c) 1991-1994, Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/promif/sparc/ieee1275/common/Makefile
#
# create libprom.a from ieee1275 promif common source.
#
# NOTE that source is included from /usr/src/psm/promif/ieee1275/common
#

TOPDIR =	../../../../../../..

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PROMDIR =	$(TOPDIR)/psm/promif/ieee1275/common
SYSDIR =	$(TOPDIR)/uts

LIBPROM =	libprom.a
LINTLIBPROM =	llib-lprom.ln

PROM_CFILES =			\
	prom_2path.c		\
	prom_boot.c		\
	prom_devname.c		\
	prom_devtype.c		\
	prom_enter.c		\
	prom_env.c		\
	prom_exit.c		\
	prom_fb.c		\
	prom_getchar.c		\
	prom_gettime.c		\
	prom_handler.c		\
	prom_inpath.c		\
	prom_interp.c		\
	prom_io.c		\
	prom_kbd.c		\
	prom_node.c		\
	prom_outpath.c		\
	prom_panic.c		\
	prom_path.c		\
	prom_phandle.c		\
	prom_printf.c		\
	prom_prop.c		\
	prom_putchar.c		\
	prom_reboot.c		\
	prom_stdin.c		\
	prom_stdout.c		\
	prom_string.c		\
	prom_test.c		\
	prom_trap.c		\
	prom_version.c		\
	prom_wrtestr.c

PROM_SFILES =

PROM_FILES = 	$(PROM_CFILES) $(PROM_SFILES)

KARCH =		sun4u
MMU =		sfmmu

OBJSDIR =	objs

PROM_COBJ =	$(PROM_CFILES:%.c=$(OBJSDIR)/%.o)
PROM_SOBJ =	$(PROM_SFILES:%.s=$(OBJSDIR)/%.o)
OBJS =		$(PROM_COBJ) $(PROM_SOBJ)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS =	$(PROM_FILES:%=$(PROMDIR)/%)

ARCHOPTS =	-DSTACK_64BIT -Dsun4u -D__sparcv9cpu
ASFLAGS =	-P -D__STDC__ -D_BOOT -D_ASM
CPPDEFS	=	$(ARCHOPTS) -D$(KARCH) -D_BOOT -D_KERNEL -D_MACHDEP
CPPINCS =	-I. -I$(SYSDIR)/$(KARCH) -I$(SYSDIR)/$(MMU) \
		-I$(SYSDIR)/sparc/v9 -I$(SYSDIR)/sparc \
		-I$(SYSDIR)/sun -I$(SYSDIR)/common
CPPFLAGS =	$(CPPDEFS) $(CPPINCS) $(CPPFLAGS.master)
CFLAGS +=	-v

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBPROM)

lint: $(LINTLIBPROM)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBPROM) $(LINTLIBPROM) a.out core

$(LIBPROM): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBPROM): $(OBJSDIR) .WAIT $(L_OBJS)
	@$(ECHO) "\nlint library construction:" $@
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

$(OBJSDIR)/%.ln: $(PROMDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(PROMDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@
