#
#ident	"@(#)Makefile.com	1.36	99/10/19 SMI"
#
# Copyright (c) 1994-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/common/Makefile.com
#
GREP	=	egrep
WC	=	wc
TOPDIR	=	../../../../..

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/Makefile.psm
include $(TOPDIR)/psm/stand/lib/Makefile.lib

SYSDIR	=  	$(TOPDIR)/uts
BOOTDIR	= 	../../boot
COMDIR	=  	../../common
SPECDIR=	$(TOPDIR)/uts/common/os
ARCHDIR	= 	$(SYSDIR)/$(ARCH)
MACHDIR	= 	$(SYSDIR)/$(MACH)
MMUDIR	=	$(SYSDIR)/$(MMU)
PROMLIBDIR=	$(TOPDIR)/psm/stand/lib/promif/$(ARCH_PROMDIR)
PROMLIB	=	$(PROMLIBDIR)/libprom.a
CPRLIB	=	libcpr.a

LINTLIBCPR =	llib-lcpr.ln
LINTFLAGS.lib =	-ysxmun

CPRLDLIB =	-L. -lcpr
PROMLDLIBS =	-L$(PROMLIBDIR) -lprom
LDLIBS +=	$(CPRLDLIB) $(PROMLDLIBS) $(PLATLDLIBS)
OBJSDIR=	.

CPRCOMLIBOBJ =	support.o
CPRSPECLIBOBJ = compress.o
CPRLIBOBJ =	$(CPRCOMLIBOBJ) $(CPRSPECLIBOBJ)

L_SRCS	=	$(CPRCOMLIBOBJ:%.o=$(COMDIR)/%.c)
L_SRCS +=	$(CPRSPECLIBOBJ:%.o=$(SPECDIR)/%.c)
L_COBJ	=	$(CPRBOOTOBJ:%.o=%.ln)

CPPDEFS =	$(ARCHOPTS) -D$(ARCH) -D__$(ARCH) -D$(MACH) -D__$(MACH)
CPPDEFS +=	-D_KERNEL -D_MACHDEP -D__ELF

CPPINCS =	-I. -I$(COMDIR) -I$(ARCHDIR) -I$(MMUDIR) -I$(MACHDIR)
CPPINCS +=	-I$(MACHDIR)/$(ARCHVER)	-I$(SYSDIR)/sun
CPPINCS +=	-I$(SYSDIR)/common -I$(TOPDIR)/head

CPPFLAGS =	$(CPPDEFS) $(CPPINCS) $(CPPFLAGS.master)
CPPFLAGS +=	$(CCYFLAG)$(SYSDIR)/common

CFLAGS =	-v -O

ASFLAGS = 	-P -D_ASM $(CPPDEFS) -DLOCORE -D_LOCORE -D__STDC__
AS_CPPFLAGS =	$(CPPINCS) $(CPPFLAGS.master)


# install values
CPRFILES=	$(ALL:%=$(ROOT_PSM_DIR)/$(ARCH)/%)
FILEMODE=	644
OWNER=		root
GROUP=		sys

# lint stuff
LINTFLAGS += -Dlint
LOPTS = -hbxn

# install rule
$(ROOT_PSM_DIR)/$(ARCH)/%: %
	$(INS.file)


all:	$(ALL)

install: all $(CPRFILES)


LINT.c=	$(LINT) $(LINTFLAGS.c) $(LINT_DEFS) $(CPPFLAGS) -c
LINT.s=	$(LINT) $(LINTFLAGS.s) $(LINT_DEFS) $(CPPFLAGS) -c

# build rule

$(OBJSDIR)/compress.o: $(SPECDIR)/compress.c
	$(COMPILE.c) $(SPECDIR)/compress.c

$(OBJSDIR)/%.o: $(COMDIR)/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ./%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ../common/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.ln: ./%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)

$(OBJSDIR)/%.ln: ../common/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)

$(OBJSDIR)/%.ln: ./%.s
	@$(LHEAD) $(LINT.s) $< $(LTAIL)

.KEEP_STATE:

.PARALLEL:	$(CPRBOOTOBJ)

cprboot: ucpr.o $(CPRBOOT_MAPFILE) 
	$(LD) -dn -M $(CPRBOOT_MAPFILE) $(MAP_FLAG) -o $@ ucpr.o
	$(POST_PROCESS)

ucpr.o: $(CPRBOOTOBJ) $(CPRLIB) $(PROMLIB) $(PLATLIB)
	$(LD) -r -o $@ $(CPRBOOTOBJ) $(OBJ) $(LDLIBS)

$(CPRLIB): $(CPRLIBOBJ)
	$(AR) $(ARFLAGS) $@ $(CPRLIBOBJ)

$(ROOTDIR):
	$(INS.dir)

lint: $(CPRBOOTLINT)

cprboot_lint: $(L_COBJ) $(LINTLIBCPR)
	@$(ECHO) "\nperforming global crosschecks: $@"
	@$(LINT.2) $(L_COBJ) $(LDLIBS)

$(LINTLIBCPR): $(L_SRCS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LHEAD) $(LINT.lib) $(CPPOPTS) -o cpr $(L_SRCS) $(LTAIL)

clean.lint:
	$(RM) $(OBJSDIR)/*.ln

clean: clean.lint
	$(RM) $(OBJSDIR)/*.o $(OBJSDIR)/*.a

clobber: clean
	$(RM) $(ALL)

FRC:
