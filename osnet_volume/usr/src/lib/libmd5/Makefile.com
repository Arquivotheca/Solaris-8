#
# ident	"@(#)Makefile.com	1.1	99/07/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5/Makefile.com
#

LIBRARY= libmd5.a
VERS= .1

OBJECTS= md5.o
COMMON= $(SRC)/common/md5

include $(SRC)/lib/Makefile.lib

# Macros to help build the shared object
MAPFILE= $(MAPDIR)/mapfile
CLOBBERFILES += $(MAPFILE)
LIBS= $(DYNLIB) $(LINTLIB)
DYNFLAGS += -M $(MAPFILE)
LDLIBS += -lc
CPPFLAGS += -D__RESTRICT

# Macros to help build the lint library
LINTSRC= $(LINTLIB:%.ln=%)
$(LINTLIB) := SRCS= ../$(LINTSRC)
SRCS= $(OBJECTS:%.o=$(COMMON)/%.c)
ROOTLINT= $(LINTSRC:%=$(ROOTLIBDIR)/%)
CLEANFILES += lint.out
LINTFLAGS64 += -Xarch=v9
$(ROOTLIBDIR)/%: ../%
	$(INS.file)

.KEEP_STATE:

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); pwd; $(MAKE) mapfile

lint: $(LINTLIB)

include $(SRC)/lib/Makefile.targ

pics/%.o: $(COMMON)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
