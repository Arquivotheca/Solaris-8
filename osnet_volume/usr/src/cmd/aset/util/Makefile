#
#ident	"@(#)Makefile	1.2	99/04/13 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# cmd/aset/util/Makefile

PROG= geteuid homedir is_readable is_writable minmode realpath str_to_mode nls

SCRIPTS= addcksum file_attr taskstat

include ../../Makefile.cmd

SRCS= $(PROG:=.c)

# we assume Makefile above created ROOTDIR
ROOTDIR= $(ROOT)/usr/aset/util

ROOTPROG= $(PROG:%=$(ROOTDIR)/%)
ROOTSCRIPTS= $(SCRIPTS:%=$(ROOTDIR)/%)

DIRMODE= 700
FILEMODE= 700
OWNER= root

CLOBBERFILES= $(SCRIPTS)

# install rule
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(SCRIPTS)

install: all $(ROOTPROG) $(ROOTSCRIPTS)

lint: lint_SRCS

clean:

include ../../Makefile.targ
