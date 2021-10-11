#
#pragma ident	"@(#)Makefile	1.21	99/01/06 SMI"
#
# Copyright (c) 1991-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/format/Makefile

PROG= format

COBJS=	add_definition.o analyze.o checkmount.o ctlr_scsi.o \
	defect.o init_menus.o io.o label.o main.o \
	menu.o menu_analyze.o menu_cache.o menu_command.o menu_defect.o \
	menu_partition.o misc.o modify_partition.o partition.o \
	prompts.o startup.o menu_scsi.o auto_sense.o disk_generic.o \
	menu_developer.o menu_fdisk.o

OBJS_sparc=   init_csparc.o ctlr_md21.o ctlr_ata.o

OBJS_i386=    init_ci386.o ctlr_ata.o ix_altsctr.o

OBJS_=                non-existent

OBJS=  $(OBJS_$(MACH))  $(COBJS)

DATA= format.dat

include ../Makefile.cmd

SRCS= $(OBJS:.o=.c)

ROOTSYMLINK= $(ROOTETCPROG)
ROOTETCDATA= $(ROOTETC)/$(DATA)

$(ROOTETCDATA) := FILEMODE = 0644
$(ROOTETCDATA) := OWNER = root
$(ROOTETCDATA) := GROUP = sys

LDLIBS +=	-ladm

.KEEP_STATE:

all: $(PROG) $(DATA)

$(PROG): $(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

#
# Install format as /usr/sbin/format, with link from /etc/format
#
install: $(ROOTUSRSBINPROG) $(ROOTSYMLINK) $(ROOTETCDATA)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../usr/sbin/$(PROG) $@

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

tags: $(SRCS)
	ctags $(SRCS)

sb: $(SRCS)
	$(COMPILE.c) -xsbfast $(SRCS)

include ../Makefile.targ
