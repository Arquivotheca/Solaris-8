#
# Copyright (c) 1989-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)Makefile	1.14	99/11/24 SMI"
#
# cmd/sort/Makefile
#

PROG = sort
XPG4PROG = sort

include ../Makefile.cmd

SED = sed
SRCS =	common/check.c common/fields.c common/initialize.c common/internal.c \
	common/main.c common/merge.c common/options.c common/streams.c \
	common/streams_array.c common/streams_mmap.c common/streams_stdio.c \
	common/streams_wide.c common/utility.c
POFILES = $(SRCS:common/%.c=./%.po)
CLOBBERFILES = $(DCFILE) $(POFILE) $(POFILES)

.KEEP_STATE:

$(XPG4) := CPPFLAGS += -DXPG4
XGETFLAGS += -a -x sort.xcl

SUBDIRS =        $(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all     :=      TARGET = all
install :=      TARGET = install
clean   :=      TARGET = clean
clobber :=      TARGET = clobber
lint    :=      TARGET = lint
cscope  :=      TARGET = cscope

all : $(SUBDIRS)

clean clobber lint : $(SUBDIRS)

install : $(SUBDIRS)
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)

$(POFILE) : $(POFILES)
	echo $(SRCS)
	echo $(POFILES)
	-$(RM) $@
	$(CAT) $(POFILES) > $@

%.po : common/%.c
	$(RM) messages.po
	$(XGETTEXT) -c TRANSLATION_NOTE $<
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

$(SUBDIRS) : FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

cscope : FRC
	@cd common; pwd; $(MAKE) $(TARGET)

FRC :

include ../Makefile.targ
