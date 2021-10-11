#
# "@(#)Makefile 1.16     99/01/22 SMI"
#
# Copyright (c) 1993,1998 by Sun Microsystems, Inc.
#
# cmd/backup/Makefile
#

include Makefile.backup

COMMONPRODSUBDIRS=	dump restore
UFSPRODSUBDIRS=
LIBSUBDIRS=	lib
LIBDONE=	$(LIBSUBDIRS)/.target_done
SUBDIRS=	$(LIBSUBDIRS) $(COMMONPRODSUBDIRS) $(UFSPRODSUBDIRS)
UFSSUBDIRS=	$(LIBSUBDIRS) $(COMMONPRODSUBDIRS) $(UFSPRODSUBDIRS)

UFSINSDIRS=	$(UFSROOTETC) $(UFSROOTUSR) $(UFSROOTUSRLIB) \
		$(UFSROOTUSRLIBFS) $(UFSROOTUSRSBIN)
PODIRS=		dump restore

.KEEP_STATE:

.DONE:
	@rm -f $(LIBDONE)

all debug lint check: $(COMMONPRODSUBDIRS) $(UFSPRODSUBDIRS)

_msg:	$(PODIRS)

# `files' should contain a list of the interesting bits (basically, the
# contents of lib, restore, dump, and include).
cscope.out:	cscope.files
	cscope-fast -b -i cscope.files -u

cscope.files:	FRC
	find . -name SCCS -prune -o -name .del\* -prune -o -name '*.[ch]' -print > cscope.files

clean clobber: $(SUBDIRS) .DONE
	$(RM) cscope.files

install:	$(UFSINSDIRS) $(UFSSUBDIRS)

$(UFSINSDIRS):
	$(INS.dir)

$(COMMONPRODSUBDIRS) $(UFSPRODSUBDIRS): $(LIBSUBDIRS) FRC
	$(DO_SUBDIR)

FRC:
