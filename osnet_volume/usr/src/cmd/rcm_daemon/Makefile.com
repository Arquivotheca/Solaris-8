#
#ident	"@(#)Makefile.com	1.1	99/08/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/devfsadm/Makefile.com
#

include ../../Makefile.cmd

COMMON = ../common

RCM_SRC = $(COMMON)/rcm_event.c $(COMMON)/rcm_main.c $(COMMON)/rcm_impl.c \
	$(COMMON)/rcm_subr.c $(COMMON)/rcm_lock.c
RCM_OBJ = rcm_event.o rcm_main.o rcm_impl.o rcm_subr.o rcm_lock.o

MOD_SRC = $(COMMON)/filesys_rcm.c
MOD_OBJ = filesys_rcm.o

RCM_DAEMON = rcm_daemon
RCM_MODS = SUNW_filesys_rcm.so

RCM_DIR = rcm
MOD_DIR = modules

CLOBBERFILES = $(MODS)

CFLAGS += -v -I.. -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS

LINTFLAGS += -I.. -x

LDLIBS += -lgen -lelf -ldl -lcmd -ldoor -lrcm -lthread

SRCS = $(RCM_SRC) $(MOD_SRC)
OBJS = $(RCM_OBJ) $(MOD_OBJ)
MODS = $(RCM_DAEMON) $(RCM_MODS)

POFILES = $(SRCS:.c=.po)
POFILE = prcm_daemon.po

# install specifics

ROOTLIB_RCM = $(ROOTLIB)/$(RCM_DIR)
ROOTLIB_RCM_MOD = $(ROOTLIB_RCM)/$(MOD_DIR)
ROOTLIB_RCM_DAEMON = $(RCM_DAEMON:%=$(ROOTLIB_RCM)/%)
ROOTLIB_RCM_MODULES = $(RCM_MODS:%=$(ROOTLIB_RCM_MOD)/%)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint


.KEEP_STATE:

all: $(MODS)

install: all			\
	$(ROOTLIB_RCM)		\
	$(ROOTLIB_RCM_DAEMON)	\
	$(ROOTLIB_RCM_MOD)	\
	$(ROOTLIB_RCM_MODULES)

clean:
	$(RM) $(OBJS) $(POFILES)

lint: lint_SRCS

FRC:

include ../../Makefile.targ

$(POFILE):      $(POFILES)
	$(RM) $@; cat $(POFILES) > $@

$(RCM_DAEMON): $(RCM_OBJ)
	$(LINK.c) -o $@ $< $(RCM_OBJ) $(LDLIBS)
	$(POST_PROCESS)

SUNW_%.so: %.o
	$(LINK.c) -o $@ -G -h $@ $<

%.o: $(COMMON)/%.c
	$(COMPILE.c) -o $@ $<

$(ROOTLIB_RCM):
	$(INS.dir)

$(ROOTLIB_RCM)/%: %
	$(INS.file)

$(ROOTLIB_RCM_MOD):
	$(INS.dir)

$(ROOTLIB_RCM_MOD)/%: %
	$(INS.file)
