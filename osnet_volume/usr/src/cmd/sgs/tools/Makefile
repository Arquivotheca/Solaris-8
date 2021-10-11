#ident	"@(#)Makefile	1.27	99/01/11 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# Makefile to support tools used for linker development.  GEXECS are those
# tools that the installed for distribution in the SUNWonld package.
#
#  o	`c' programs are built from the $(MACH)/Makefile
#
#  o	info: summarizes relocation overhead, and a sorted string listing.
#
#  o	intf_file: produces a `physical binary interface' for a file.
#
#  o	proto: creates a proto directory structure suitable for sgs make
#	installs (basically a subset of Targetdirs).
#
#  o	lsd: analyze the different interfaces between two shared objects.
#
#  o	so425.sh: convert a 4.x shared object into a 5.x version suitable for
#	the link-editor to process.
#
#  o	intf_create: generates a shared object version definition database.
#
#  o	intf_cmp: compares two version definition databases.
#
# cmd/sgs/tools/Makefile

include		$(SRC)/Makefile.master

LSSRCS=		info.sh			intf_file.sh \
		proto.sh

PSRCS=		check_mapfile.pl	check_syms.pl \
		extract_exports.pl	find_bind.pl \
		find_libs.pl		find_scoped.pl \
		list_scoped.pl		print_globs.pl \
		run_print.pl

GSSRCS=		lsd.sh			so425.sh
GKSRCS=		intf_create.ksh		intf_cmp.ksh

SRCS=		$(LSSRCS) $(GSSRCS) $(GKSRCS) $(PSRCS)

LEXECS=		$(LSSRCS:%.sh=%)
GEXECS=		$(GSSRCS:%.sh=%) $(GKSRCS:%.ksh=%)

EXECS=		$(LEXECS) $(GEXECS)

ROOTDIR =	$(ROOT)/opt/SUNWonld/bin
ROOTPROGS =	$(GEXECS:%=$(ROOTDIR)/%)
ROOTPROGS +=	$(PSRCS:%=$(ROOTDIR)/%)

SUBDIRS=	$(MACH) man

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint


.KEEP_STATE:

all install:	$(EXECS)  $(SUBDIRS) $(ROOTPROGS)

clean clobber:	$(SUBDIRS)
		$(RM) $(RMFLAGS) $(EXECS)

delete:

lint:		$(SUBDIRS)

%:		%.ksh
		$(RM) $@
		cat $< > $@
		chmod +x $@

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

$(ROOTDIR)/%:	%
		$(INS.file)

$(ROOTDIR)/%:	scoping/%
		$(INS.file)

FRC:

execs:		$(EXECS)
