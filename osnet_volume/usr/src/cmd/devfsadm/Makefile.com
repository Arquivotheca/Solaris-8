#
#ident	"@(#)Makefile.com	1.7	99/09/23 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/devfsadm/Makefile.com
#

include ../../Makefile.cmd

COMMON = ..

DEVFSADM_MOD = devfsadm

DEVFSADM_SRC = $(COMMON)/$(DEVFSADM_MOD:%=%.c)
DEVFSADM_OBJ = $(DEVFSADM_MOD:%=%.o)
DEVFSADM_AOBJ = inst_sync.o

DEVFSADM_DAEMON = devfsadmd

DEVLINKS= devlinks
DRVCONFIG= drvconfig

LINKMOD_DIR = linkmod
DEVFSADM_DIR = devfsadm

CLOBBERFILES = $(MODS) $(DEVLINKTAB) $(DEVFSCOMPATLINKS) $(DEVFSADM_DAEMON)

LINK_SRCS =			\
	$(COMMON)/disk_link.c	\
	$(COMMON)/tape_link.c	\
	$(COMMON)/usb_link.c	\
	$(COMMON)/port_link.c	\
	$(COMMON)/audio_link.c	\
	$(COMMON)/cfg_link.c	\
	$(COMMON)/misc_link.c	\
	$(COMMON)/lofi_link.c	\
	$(COMMON)/sgen_link.c	\
	$(MISC_LINK_ISA).c

LINK_OBJS =			\
	disk_link.o		\
	tape_link.o		\
	usb_link.o		\
	port_link.o		\
	audio_link.o		\
	cfg_link.o		\
	misc_link.o		\
	lofi_link.o		\
	sgen_link.o		\
	$(MISC_LINK_ISA).o

LINK_MODS =			\
	SUNW_disk_link.so	\
	SUNW_tape_link.so	\
	SUNW_usb_link.so	\
	SUNW_port_link.so	\
	SUNW_audio_link.so	\
	SUNW_cfg_link.so	\
	SUNW_misc_link.so	\
	SUNW_lofi_link.so	\
	SUNW_sgen_link.so	\
	SUNW_$(MISC_LINK_ISA).so

DEVLINKTAB = devlink.tab
DEVLINKTAB_SRC = $(COMMON)/$(DEVLINKTAB).sh

DEVFSADM_DEFAULT = devfsadm
DEVFSADM_DEFAULT_SRC = $(COMMON)/devfsadm.dfl

COMPAT_LINKS = disks tapes ports audlinks devlinks drvconfig

CFLAGS += -v -I.. -D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT

LINTFLAGS += -I.. -D_POSIX_PTHREAD_SEMANTICS

ASFLAGS += -P -D_ASM $(CPPFLAGS)

LDLIBS += -mt -ldevinfo -lgen -lelf -ldl -ldevfsevent -lcmd

SRCS = $(DEVFSADM_SRC) $(LINK_SRCS)
OBJS = $(DEVFSADM_OBJ) $(DEVFSADM_AOBJ) $(LINK_OBJS)
MODS = $(DEVFSADM_MOD) $(LINK_MODS)

POFILES = $(LINK_SRCS:.c=.po) $(DEVFSADM_SRC:.c=.po)
POFILE = pdevfsadm.po

# install specifics

ROOTLIB_DEVFSADM = $(ROOTLIB)/$(DEVFSADM_DIR)
ROOTLIB_DEVFSADM_LINKMOD = $(ROOTLIB_DEVFSADM)/$(LINKMOD_DIR)

ETCDEFAULT = $(ROOTETC)/default
ETCDEFAULT_DEVFSADM = $(DEVFSADM_DEFAULT:%=$(ETCDEFAULT)/%)

ROOTLIB_DEVFSADM_LINK_MODS = $(LINK_MODS:%=$(ROOTLIB_DEVFSADM_LINKMOD)/%)

ROOTUSRSBIN_COMPAT_LINKS = $(COMPAT_LINKS:%=$(ROOTUSRSBIN)/%)

ROOTUSRSBIN_DEVFSADM = $(DEVFSADM_MOD:%=$(ROOTUSRSBIN)/%)

ROOTLIB_DEVFSADM_DAEMON = $(ROOTLIB_DEVFSADM)/$(DEVFSADM_DAEMON)

ROOTETC_DEVLINKTAB = $(DEVLINKTAB:%=$(ROOTETC)/%)

OWNER= root
GROUP= sys
FILEMODE= 755

$(ROOTETC_DEVLINKTAB) := FILEMODE = 644

$(ETCDEFAULT_DEVFSADM) := FILEMODE = 444

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint


.KEEP_STATE:

all: $(MODS) $(DEVLINKTAB)

install: all				\
	$(ROOTLIB_DEVFSADM)		\
	$(ROOTLIB_DEVFSADM_LINKMOD)	\
	$(ROOTUSRSBIN_DEVFSADM)		\
	$(ROOTETC_DEVLINKTAB)		\
	$(ROOTLIB_DEVFSADM_LINK_MODS)	\
	$(ROOTUSRINCLUDE)		\
	$(ROOTLIB_DEVFSADM_DAEMON)	\
	$(ETCDEFAULT)			\
	$(ETCDEFAULT_DEVFSADM)		\
	$(ROOTUSRSBIN_COMPAT_LINKS)


clean:
	$(RM) $(OBJS) 

lint: lint_SRCS

FRC:

include ../../Makefile.targ

$(POFILE):      $(POFILES)
	$(RM) $@; cat $(POFILES) > $@

$(DEVFSADM_MOD): $(DEVFSADM_OBJ) $(DEVFSADM_AOBJ)
	$(LINK.c) -o $@ $< $(DEVFSADM_OBJ) $(DEVFSADM_AOBJ) $(LDLIBS)
	$(POST_PROCESS)

SUNW_%.so: %.o
	$(LINK.c) -o $@ -G -h $@ $<

%.o: $(COMMON)/%.c
	$(COMPILE.c) -o $@ $<


$(DEVLINKTAB): $(DEVLINKTAB_SRC)
	$(RM) $(DEVLINKTAB)
	/bin/sh $(DEVLINKTAB_SRC) > $(DEVLINKTAB)

$(ETCDEFAULT)/%: %
	$(RM) -r default
	mkdir default
	cp $(DEVFSADM_DEFAULT_SRC) default/$(DEVFSADM_DEFAULT)
	cd default ; $(INS.file)
	$(RM) -r default

$(ETCDEFAULT):
	$(INS.dir)

$(ROOTUSRSBIN):
	$(INS.dir)

$(ROOTLIB_DEVFSADM):
	$(INS.dir)

$(ROOTUSRINCLUDE):
	$(INS.dir)

$(ROOTLIB_DEVFSADM_LINKMOD):
	$(INS.dir)

$(ROOTLIB_DEVFSADM_LINKMOD)/%: %
	$(INS.file)

$(ROOTLIB_DEVFSADM_DAEMON):
	$(RM) $@; $(SYMLINK) ../../sbin/$(DEVFSADM_DIR) $@

$(ROOTUSRSBIN_COMPAT_LINKS):
	$(RM) $@ ; $(LN) $(ROOTUSRSBIN_DEVFSADM) $@
