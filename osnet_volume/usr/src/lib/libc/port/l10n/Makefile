#
#pragma ident	"@(#)Makefile	1.2	97/09/23 SMI"
#

LIBRARY= methods_l10n.a
VERS = .1

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET=all
install :=	TARGET=install
clean :=	TARGET=clean
clobber :=	TARGET=clobber

include ../../../Makefile.lib

CPPFLAGS= $(INCLIST) $(CPPFLAGS.master)

LIBS = $(DYNLIB)


.KEEP_STATE:

all clean clobber install: $(SUBDIRS)

#install: all $(ROOTLIBS) $(ROOTLINKS)

install: all

$(ROOTLIBDIR):
	$(INS.dir)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

include ../../../Makefile.mach

FRC:

