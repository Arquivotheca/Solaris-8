#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread/Makefile
#
#pragma ident	"@(#)Makefile	1.15	99/05/18 SMI"
#

LIBTHREADDIR=.
LIB_PIC= libthread_pic.a
LIBRARY = libthread.a
VERS = .1

all :=		TARGET=all
install :=	TARGET=install
clean :=	TARGET=clean
clobber :=	TARGET=clobber
lint :=		TARGET=lint

ROOTPICSDIR=	$(ROOT)/usr/lib/pics
ROOTPICSDIR64=	$(ROOTPICSDIR)/$(MACH64)
ROOTPICS=	$(LIB_PIC:%=$(ROOTPICSDIR)/%)
ROOTPICS64=	$(LIB_PIC:%=$(ROOTPICSDIR64)/%)

# include common library definitions
include ../Makefile.lib

CPPFLAGS= $(INCLIST) $(CPPFLAGS.master) 

# do not install libthread.a. set LIBS to be equal to only DYNLIB
LIBS = $(DYNLIB)

# definitions for lint

$(LINTLIB):= SRCS=./common/llib-lthread
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINTDIR64=	$(ROOTLIBDIR64)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINTLIB=	$(LINTLIB:%=$(ROOTLINTDIR)/%)
ROOTLINTLIB64=	$(LINTLIB:%=$(ROOTLINTDIR64)/%)

.KEEP_STATE:

all:	spec lib32 $(BUILD64) lib64

lib32: $(MACHLIBS)

lib64: $(MACHLIBS64)

install:	all \
		spec \
		inslib32 \
		$(BUILD64) inslib64

inslib32:	$(ROOTLIBS) \
		$(ROOTLINKS) \
		$(ROOTLINT) \
		lint \
		$(ROOTLINTLIB)

inslib64:	$(ROOTLIBS64) \
		$(ROOTLINKS64) \
		$(ROOTLINT) \
		$(ROOTLINTLIB64)

$(ROOTLIBDIR):
	$(INS.dir)
$(ROOTLIBDIR64):
	$(INS.dir)

clean clobber lint: spec
	@cd $(MACH); pwd; $(MAKE) $@
	-@$(BUILD64) cd $(MACH64); pwd; $(MAKE) $@

ARCHITECTURES = $(MACH) $(BUILD64) $(MACH64)

CSDIR   = .
CSDIRS  = sparc sparcv9 i386 common inc sys
CSPATHS = $(CSDIRS:%=$(CSDIR)/%)
CSINCS  = $(CSPATHS:%=-I%)
CSCOPE  = cscope
CTAGS   = ctags

.PRECIOUS: cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/cscope.out

cscope.files: FRC
	@-$(RM) cscope.files
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
		-type d -name '.del-*' -prune -o -type f \
		\( -name '*.[csh]' -o -name 'Makefile*' -o -name '*.il' \) \
		  -print >> cscope.files
	@wc -l cscope.files

tags: tags.list FRC
	${CTAGS} -wt `cat tags.list`

tags.list: cscope.files
	@$(RM) tags.list
	grep '\.c$$' cscope.files > tags.list
	grep '\.s$$' cscope.files >> tags.list
	grep '\.h$$' cscope.files >> tags.list

FRC:

# install rules for lib_pic.a - not invoked thru the install target
# these exist only if sometime you do want to build libthread_pic.a
$(ROOTPICSDIR):
	$(INS.dir)
$(ROOTPICSDIR64):
	$(INS.dir)

$(ROOTPICSDIR)/%: $(MACH)/%
	$(INS.file)
$(ROOTPICSDIR64)/%: $(MACH64)/%
	$(INS.file)

$(MACH)/$(LIB_PIC): FRC
	@cd $(MACH); pwd; $(MAKE) $(LIB_PIC)
$(MACH64)/$(LIB_PIC): FRC
	@cd $(MACH64); pwd; $(MAKE) $(LIB_PIC)

# install rule for lint library target
$(ROOTLINTDIR)/%: ./common/%
	$(INS.file)

# include MACH-specific library targets
include ../Makefile.mach

spec:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
