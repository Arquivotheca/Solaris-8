#
#ident	"@(#)Makefile	1.7	99/03/30 SMI"
#
# Copyright (c) 1995-1998 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../../../../Makefile.master

MACHDEMOFILES= \
		regs.c \
		m_utils.c \
		rdb_mach.h \
		Makefile

DEMOFILES= \
		00README \
		Makefile.com \
		Makefile.targ \
		common/bpt.c \
		common/dis.c \
		common/disasm.c \
		common/disasm.h \
		common/main.c \
		common/ps.c \
		common/gram.y \
		common/lex.l \
		common/globals.c \
		common/help.c \
		common/utils.c \
		common/maps.c \
		common/syms.c \
		common/callstack.c \
		common/rdb.h \
		common/rdb.man \
		tests/breaks \
		tests/loadmaps \
		tests/object_padding_maps \
		tests/plt_skip \
		tests/steps \
		tests/test-sparc-regs \
		tests/simp.c \
		tests/sub.c \
		$(MACHDEMOFILES:%=sparc/% i386/% sparcv9/%)


ROOTDEMODIRBASE=	$(ROOT)/usr/demo/librtld_db

ROOTDEMODIRS=	$(ROOTDEMODIRBASE) \
		$(ROOTDEMODIRBASE)/common \
		$(ROOTDEMODIRBASE)/tests \
		$(ROOTDEMODIRBASE)/i386 \
		$(ROOTDEMODIRBASE)/sparc \
		$(ROOTDEMODIRBASE)/sparcv9

ROOTDEMOFILES=	$(DEMOFILES:%=$(ROOTDEMODIRBASE)/%)

$(ROOTDEMODIRS) :=	OWNER =		root
$(ROOTDEMODIRS) :=	GROUP =		bin
$(ROOTDEMODIRS) :=	DIRMODE =	755

SED=		sed -e "/DEMO DELETE START/,/DEMO DELETE END/d"

CLEANFILES=	*.demo */*.demo

.KEEP_STATE:
.PARALLEL:	$(ROOTDEMOFILES)

all lint:

install:	$(ROOTDEMODIRS) .WAIT $(ROOTDEMOFILES)

clean clobber: FRC
	$(RM) $(CLEANFILES)




$(ROOTDEMODIRS):
		$(INS.dir)

$(ROOTDEMODIRBASE)/Makefile.%: Makefile.%.demo
		$(INS.rename)

$(ROOTDEMODIRBASE)/%/Makefile: %/Makefile.demo
		$(INS.rename)

Makefile.%.demo: Makefile.%
		$(SED) < $< > $@

%/Makefile.demo: %/Makefile
		$(SED) < $< > $@

$(ROOTDEMODIRBASE)/%: %
	$(INS.file)


$(ROOTDEMODIRBASE)/common/%:	common/%
		$(INS.file)

$(ROOTDEMODIRBASE)/tests/%:	tests/%
		$(INS.file)

$(ROOTDEMODIRBASE)/sparc/%:	sparc/%
		$(INS.file)

$(ROOTDEMODIRBASE)/sparcv9/%:	sparcv9/%
		$(INS.file)

$(ROOTDEMODIRBASE)/i386/%:	i386/%
		$(INS.file)

FRC:
