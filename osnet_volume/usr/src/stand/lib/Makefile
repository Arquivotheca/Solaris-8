#
#pragma ident	"@(#)Makefile	1.8	99/10/04 SMI"
#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# stand/lib/Makefile

include ../../Makefile.master

sparcv7_ARCHITECTURES = sparc
sparcv9_ARCHITECTURES = sparcv9
sparc_ARCHITECTURES = $(sparcv9_ARCHITECTURES) $(sparcv7_ARCHITECTURES)
i386_ARCHITECTURES = i386
ia64_ARCHITECTURES = ia64
$(BUILD64)i386_ARCHITECTURES += $(ia64_ARCHITECTURES)

SUBDIRS = $($(MACH)_ARCHITECTURES)

all install clean clobber lint: $(SUBDIRS)

all	:=	TARGET = alllibs lintlibs
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint
install	:=	TARGET = install

UTSDIR= $(SRC)/uts

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

CSPATHS	= ./$(MACH) ./fs ./common
i386_CSINCS=  -I$(UTSDIR)/common  -I$(UTSDIR)/intel \
 	  -I$(UTSDIR)/i86pc  	  -I$(ROOT)/usr/include \
 	  $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
ia64_CSINCS=  -I$(UTSDIR)/common  -I$(UTSDIR)/intel \
 	  -I$(UTSDIR)/ia64plat 	  -I$(ROOT)/usr/include \
 	  $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
sparc_CSINCS= -I$(UTSDIR)/common -I$(UTSDIR)/sparc \
	-I$(UTSDIR)/sparc/$(ARCHVERS) \
	-I$(UTSDIR)/sun -I$(UTSDIR)/sfmmu -I$(UTSDIR)/srmmu \
	-I$(UTSDIR)/sun4m -I$(UTSDIR)/sun4d -I$(UTSDIR)/sun4u \
	-I$(ROOT)/usr/include \
	$(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
sparcv9_CSINCS= -I$(UTSDIR)/common -I$(UTSDIR)/sparc \
	-I$(UTSDIR)/sparc/$(ARCHVERS) \
	-I$(UTSDIR)/sun -I$(UTSDIR)/sfmmu \
	-I$(UTSDIR)/srmmu -I$(UTSDIR)/sun4u \
	-I$(ROOT)/usr/include \
	$(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
CSINCS	= $($(MACH)_CSINCS)
CSCOPE	= cscope
CSFLAGS	= -b
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files:   FRC
	@$(RM) -f cscope.files
	echo $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o \
	    -name 'mapfile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@wc -l cscope.files

#
# Create a tags data base, similar to above.
# Since many assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
#
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags: cscope.files
	${CTAGS} -wt `sed 1d cscope.files`

FRC:
