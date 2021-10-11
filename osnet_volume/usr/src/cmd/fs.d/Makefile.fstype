#
# @(#)Makefile.fstype 1.2 90/10/09 SMI
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
#	cmd/fs.d/Makefile.fstype
#	Definitions and targets common to "simple" file system types.
#

# FSTYPE is name of filesystem type subdirectory to build
# PROG is a list of filesystem type programs to be installed BOTH in
#	../etc/fs/$(FSTYPE) and ../usr/lib/fs/$(FSTYPE)
#	Those installed under etc/fs must be statically linked, while
#	those installed under usr/lib/fs must be dynamically linked.
# ETCPROG is a list of filesystem type programs to be installed ONLY in
#	../etc/fs/$(FSTYPE)
# LIBPROG is a list of filesystem type programs to be installed ONLY in
#	../usr/lib/fs/$(FSTYPE)
# TYPEPROG is a list of filesystem type programs to be installed ONLY in
#	../usr/lib/$(FSTYPE)	[as with nfs daemons]

# use of PROG implies need for STATPROG, hence:
STATPROG= $(PROG)

# include global command definitions; SRC should be defined in the shell.
# SRC is needed until RFE 1026993 is implemented.
include		$(SRC)/cmd/Makefile.cmd

ROOTETCFS=	$(ROOTETC)/fs
ROOTLIBFS=	$(ROOTLIB)/fs
FSDIRS=		$(ROOTETCFS) $(ROOTLIBFS)
ROOTETCFSTYPE=	$(ROOTETCFS)/$(FSTYPE)
ROOTLIBFSTYPE=	$(ROOTLIBFS)/$(FSTYPE)
ROOTETCTYPE=	$(ROOTETC)/$(FSTYPE)
ROOTLIBTYPE=	$(ROOTLIB)/$(FSTYPE)
ROOTETCFSPROG=	$(PROG:%=$(ROOTETCFSTYPE)/%) $(ETCPROG:%=$(ROOTETCFSTYPE)/%)
ROOTLIBFSPROG=	$(PROG:%=$(ROOTLIBFSTYPE)/%) $(LIBPROG:%=$(ROOTLIBFSTYPE)/%)
ROOTTYPEPROG=	$(TYPEPROG:%=$(ROOTLIBTYPE)/%)
FSTYPEDIRS=	$(FSDIRS:%=%/$(FSTYPE)) $(ROOTETCTYPE) $(ROOTLIBTYPE)
FSTYPEPROG=	$(ROOTETCFSPROG) $(ROOTLIBFSPROG) $(ROOTTYPEPROG)

$(FSTYPEDIRS) :=	OWNER = root
$(FSTYPEDIRS) :=	GROUP = sys

CLOBBERFILES +=	$(STATIC) $(ETCPROG) $(LIBPROG) $(TYPEPROG)

.KEEP_STATE:

all:		$(PROG) $(STATIC) $(ETCPROG) $(LIBPROG) $(TYPEPROG)

# FSDIRS are made by $(SRC)/Targetdirs via rootdirs in $(SRC)/Makefile
# Some FSTYPE directories are made there also and should not be made here,
# but it is easier to handle them as a class.  "install" will not remake
# a directory that already exists.

$(FSTYPEDIRS): 
		$(INS.dir)

# two rules for objects in $(ROOTETCFSTYPE), with preference for
# installing the .static version of the program from directories
# containing both dynamic and static versions.
#
$(ROOTETCFSTYPE)/%:	$(ROOTETCFSTYPE) %.static
		$(INS.rename)

$(ROOTETCFSTYPE)/%:	$(ROOTETCFSTYPE) %
		$(INS.file)

$(ROOTLIBFSTYPE)/%:	$(ROOTLIBFSTYPE) %
		$(INS.file)

$(ROOTLIBTYPE)/%:	$(ROOTLIBTYPE) %
		$(INS.file)

$(ROOTETCTYPE)/%:	$(ROOTETCTYPE) %
		$(INS.file)

include		$(SRC)/cmd/Makefile.targ

install:	all $(FSTYPEPROG) $(OTHERINSTALL)

clean:

