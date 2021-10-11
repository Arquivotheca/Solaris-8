#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.42	99/12/06 SMI"
#
# lib/libc/Makefile

LIBRARY= libc.a
LIB_PIC= libc_pic.a
VERS=	.1

# values deliverables
VALUES=	values-Xa.o values-Xc.o values-Xs.o values-Xt.o values-xpg4.o

ROOTLIB=	$(ROOT)/usr/lib
ROOTLIB64=	$(ROOTLIB)/$(MACH64)
ROOTVALUES=	$(VALUES:%=$(ROOTLIB)/%)
ROOTVALUES64=	$(VALUES:%=$(ROOTLIB64)/%)
ROOTPICSDIR=	$(ROOT)/usr/lib/pics
ROOTPICSDIR64=	$(ROOTPICSDIR)/$(MACH64)
ROOTPICS=	$(LIB_PIC:%=$(ROOTPICSDIR)/%)
ROOTPICS64=	$(LIB_PIC:%=$(ROOTPICSDIR64)/%)

# include common library definitions
include ../Makefile.lib

#
# message catalogue file
#
TEXT_DOMAIN= SUNW_OST_OSLIB

LIBS += $(DYNLIB) $(LINTLIB)

ROOTLIBS64=	$(ROOTLIBDIR64)/$(DYNLIB) $(ROOTLIBDIR64)/$(LINTLIB)
MACHLIBS64=	$(MACH64)/$(DYNLIB) $(MACH64)/$(LINTLIB)

# definitions for install_h target
HDRS=	getxby_door.h	sys/localedef.h	sys/lc_core.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKDIRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: port/gen/%
	$(INS.file)

$(ROOTHDRDIR)/sys/%:	port/i18n/%
	$(INS.file)

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINTDIR64=	$(ROOTLINTDIR)/$(MACH64)
ROOTLINT64=	$(LINTSRC:%=$(ROOTLINTDIR64)/%)

$(ROOTLIBDIR)/$(DYNLIB)	:=	FILEMODE = 755
$(ROOTLIBDIR64)/$(DYNLIB) :=	FILEMODE = 755

.KEEP_STATE:

all:	spec .WAIT lib32 $(BUILD64) .WAIT lib64

#
# Add this to lib32 and you will produce a shared profiled 32-bit library:
#	$(MACHPIC_PLIB)
# The rest of the framework is still left in place. (the Makefiles above you)
# Also, add the stuff to inslib32 (below) for installing the new library.
#
lib32:	values $(MACHLIBS) $(MACH)/$(LIB_PIC) $(MACHPLIB)

lib64:	values64 $(MACHLIBS64) $(MACH64)/$(LIB_PIC) $(MACHPIC_PLIB64)

values: FRC
	@cd $(MACH); pwd; $(MAKE) values

values64: FRC
	@cd $(MACH64); pwd; $(MAKE) values

install:	all			\
		spec			\
		insvalues		\
		inslib32		\
		$(BUILD64) inslib64

#
# Add the following to inslib32 to install a 32-bit shared profiled libc.
#		$(ROOTPIC_PLIB) $(ROOTPIC_PLIBLINK)
# You also need to add the lib32 target above to build the object.
#
inslib32:	$(ROOTLIBS) $(ROOTLINKS) $(ROOTPICSDIR) \
		$(ROOTPICS) $(ROOTPLIB) $(ROOTLINT)

inslib64:	$(ROOTLIBS64) $(ROOTLINKS64) $(ROOTPICSDIR64) \
	 	$(ROOTPICS64) \
		$(ROOTPIC_PLIB64) $(ROOTPIC_PLIB64LINK)

install_h: $(ROOTHDRS)

check:	$(CHECKHDRS)

insvalues: values $(ROOTVALUES) $(BUILD64) values64 $(ROOTVALUES64)

# install rule for VALUES
$(ROOTLIB)/%: $(MACH)/%
	$(INS.file)
$(ROOTLIB64)/%: $(MACH64)/%
	$(INS.file)

# install rules for lib_pic.a
$(ROOTPICSDIR):
	$(INS.dir)
$(ROOTPICSDIR64):
	$(INS.dir)

# install rule for lint library target
$(ROOTLINTDIR)/%: port/%
	$(INS.file)
$(ROOTLINTDIR64)/%: port/%
	$(INS.file)

$(LINTLIB):
	@cd $(MACH); pwd; $(MAKE) $@
$(LINTLIB64):
	@cd $(MACH64); pwd; $(MAKE) $@

$(ROOTPICSDIR)/%: $(MACH)/%
	$(INS.file)
$(ROOTPICSDIR64)/%: $(MACH64)/%
	$(INS.file)

$(MACH)/$(LIB_PIC): FRC
	@cd $(MACH); pwd; $(MAKE) $(LIB_PIC)
$(MACH64)/$(LIB_PIC): FRC
	@cd $(MACH64); pwd; $(MAKE) $(LIB_PIC)

all	:=	SPEC_TARGET= all
install	:=	SPEC_TARGET= install
clean	:=	SPEC_TARGET= clean
clobber	:=	SPEC_TARGET= clobber



spec: FRC
	@cd $@; pwd; $(MAKE) $(SPEC_TARGET)

clean clobber: spec FRC
	@cd $(MACH); pwd; $(MAKE) $@
	-@cd $(MACH64); pwd; $(MAKE) $@

# XXX this may be wrong
links:
	-$(RM) $(MACH)
	$(SYMLINK) `mach` $(MACH)
	$(BUILD64) -$(RM) $(MACH64)
	$(BUILD64) $(SYMLINK) $(MACH64) $(MACH64)

_msg:	$(MSGDOMAIN) catalog

catalog:
	sh ./makelibccatalog.sh $(MSGDOMAIN)

$(MSGDOMAIN):
	$(INS.dir)

ARCHITECTURES = $(MACH) $(BUILD64) $(MACH64)
 
CSDIR   = .
CSDIRS  = sparc sparcv9 i386 ia64 port inc
CSPATHS = $(CSDIRS:%=$(CSDIR)/%)
CSINCS  = $(ROOTHDRDIR:%=-I%) $(CSPATHS:%=-I%)
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
# include MACH-specific library targets
include ../Makefile.mach

