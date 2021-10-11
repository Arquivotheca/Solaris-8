#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/libcurses/ia64/Makefile

MAPDIR=	../spec/ia64

include ../Makefile.com
include ../../Makefile.lib.64

.KEEP_STATE:

$(ROOTLIBDIR64)/$(LIBLINKS) := INS.liblink64 = \
	-$(RM) $@; \
	cd $(ROOTLIBDIR64); \
	$(RM) libtermlib.so$(VERS) libtermcap.so$(VERS);\
	$(RM) libtermlib.so libtermcap.so; \
	$(SYMLINK) $(LIBLINKS)$(VERS) $(LIBLINKS); \
	$(SYMLINK) libcurses.so$(VERS) libtermlib.so$(VERS); \
	$(SYMLINK) libcurses.so$(VERS) libtermcap.so$(VERS); \
	$(SYMLINK) libtermcap.so$(VERS) libtermcap.so; \
	$(SYMLINK) libtermlib.so$(VERS) libtermlib.so

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64)
