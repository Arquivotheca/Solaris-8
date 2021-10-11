#
#ident	"@(#)Makefile	1.54	99/09/14 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/Makefile

include		$(SRC)/cmd/Makefile.cmd

SUBDIRS-common= libdl		\
		libelf		\
		libconv		\
		.WAIT		\
		liblddbg	\
		.WAIT		\
		libld		\
		libldmake	\
		libldstab	\
		librtld		\
		libcrle		\
		sgsdemangler	\
		.WAIT		\
		0@0		\
		ld		\
		ldd		\
		lddstub		\
		rtld		\
		link_audit	\
		.WAIT		\
		librtld_db	\
		ldprof		\
		libldfeature	\
		pvs		\
		crle		\
		ar		\
		dis		\
		dump		\
		elfdump		\
		error		\
		gprof		\
		lex		\
		lorder		\
		m4		\
		mapfiles	\
		mcs		\
		nm		\
		prof		\
		ranlib		\
		size		\
		symorder	\
		tsort		\
		unifdef		\
		yacc

SUBDIRS-i386=
SUBDIRS-sparc=	rtld.4.x

SUBDIRS=	$(SUBDIRS-common) $(SUBDIRS-$(MACH))

# Messaging support
#
POSUBDIRS=	ar		m4		nm \
		tsort		yacc
POFILE=		sgs.po
POFILES=	$(POSUBDIRS:%=%/%.po)

MSGSUBDIRS=	ld		ldd		libld		liblddbg \
		libldstab	librtld		rtld		libelf \
		ldprof		libcrle		pvs		elfdump	\
		crle
MSGDIR=		messages


all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
lint :=		TARGET= lint
_msg :=		TARGET= catalog
chkmsg :=	TARGET= chkmsg


.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install:	native-add .WAIT $(SUBDIRS)

include		$(SRC)/cmd/Makefile.targ

# Messaging support
#
_msg:		$(MSGDOMAIN)/$(POFILE) $(MSGDIR)

$(MSGDOMAIN)/$(POFILE): \
		$(MSGDOMAIN) $(POFILE)

$(POFILE):	$(POSUBDIRS)
		$(RM) $(POFILE)
		cat $(POFILES) > $(POFILE)

$(MSGDIR):	$(MSGSUBDIRS) FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)

chkmsg:		libconv $(MSGSUBDIRS) FRC

# built from lib/Makefile
install_lib:	FRC
		@ cd lex; pwd; $(MAKE) $@
		@ cd yacc; pwd; $(MAKE) $@

lint:		$(SUBDIRS)

delete \
clean clobber:	native-clobber .WAIT $(SUBDIRS) $(MSGDIR)

$(SUBDIRS):	FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)


# Integration of ld and ld.so.1 in some developement cycles requires that both
# of these modules be built using the new ld.  This `native' target allows us
# to build a local ld which will then be used to build the delivered version of
# itself and ld.so.1.  Once this new functionality appears in the standard ld
# this target can be disabled.

native-add:	native-proto FRC
		@ cd tools/$(MACH); pwd; $(MAKE) native
		@ cd libdl/$(MACH); pwd; $(MAKE) native
		@ cd libelf/$(MACH); pwd; $(MAKE) native
		@ cd libconv/$(MACH); pwd; $(MAKE)
		@ cd liblddbg/$(MACH); pwd; $(MAKE) native
		@ cd libldstab/$(MACH); pwd; $(MAKE) native
		@ cd libld/$(MACH); pwd; $(MAKE) native
		@ cd ld/$(MACH); pwd; $(MAKE) native

native-clobber:
		@ cd tools; pwd; $(MAKE) $(TARGET)
		rm -rf proto/$(MACH)

native-proto:
		-@mkdir -p proto/$(MACH)

# Cscope target.
# The cscope.out file is made in the current directory and spans the entire
# source tree.
#
# Things to note:
#       1. We use relative names for cscope.
#       2. We *don't* remove the old cscope.out file, because cscope is smart
#          enough to only build what has changed.  It can be confused, however,
#          if files are renamed or removed, so it may be necessary to manually
#          remove cscope.out if a lot of reorganization has occurred.

CSDIRS=		. \
		../../head \
		../../uts/common/sys \
		../../uts/$(MACH)/sys \
		../../uts/$(MACH)/krtld
CSINCS=		$(CSDIRS:%=-I%)

CSCOPE=		cscope-fast
CSFLAGS=	-b

.PRECIOUS:	cscope.out

cscope.out:	cscope.files FRC
		$(CSCOPE) $(CSFLAGS)

cscope.files:	FRC
		@ -$(RM) cscope.files
		echo "$(CSINCS)" > cscope.files
		-find $(CSDIRS) -name SCCS -prune -o \
			-type d \( -name '.del-*' -o -name packages \
			-o -name m4 -o -name rtld.4.x -o -name yacc \) -prune \
			-o -type f \( -name '*.[Ccsh]' -o -name '*.msg' \
			-o -name 'mapfile*' -a ! -name 'kobj_*' \) \
			-print >> cscope.files

cscope.clean:
		-$(RM) cscope.files cscope.out

FRC:
