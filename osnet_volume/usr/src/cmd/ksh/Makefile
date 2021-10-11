#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.25	99/08/24 SMI"
#

KSH_PROG=		ksh
PROG=			$(KSH_PROG)
PFKSH_PROG=		pfksh
LIBSH=			libsh.a
BUILTINS=		ksh-builtins
ROOTXPG4SYMLINK=	$(ROOTXPG4BIN)/sh
ROOTPFKSH=		$(ROOTBIN)/$(PFKSH_PROG)
ROOTLINK=		$(ROOTBIN)/r$(PROG) $(ROOTPFKSH)

include ../Makefile.cmd

PFOBJS=	sh_policy.o

OBJS= \
	arith.o		args.o		builtin.o			\
	cmd.o		ctype.o		defs.o		edit.o		\
	echo.o		error.o		expand.o	fault.o		\
	history.o	io.o		jobs.o		macro.o		\
	main.o		msg.o		name.o		print.o		\
	service.o	string.o	stak.o		test.o		\
	word.o		xec.o		emacs.o		vi.o

LIBSHOBJS= \
	adjust.o	assign.o	assnum.o	cannon.o	\
	chkid.o		convert.o	findnod.o	gettree.o	\
	strmatch.o	growaray.o	gsort.o		linknod.o	\
	namscan.o	rjust.o		strdata.o	streval.o	\
	tilde.o		unassign.o	utos.o		valup.o		\
	optget.o	csi.o

TXTS=	sh/apollo.c sh/editlib.c sh/vfork.c

MAINSRCS=	$(OBJS:%.o=sh/%.c)
PFSRCS=		$(PFOBJS:%=pfksh_objs/%)
LIBSHSRCS=	$(LIBSHOBJS:%.o=shlib/%.c)
SRCS=		$(MAINSRCS) $(LIBSHSRCS)
PFPOFILES=	$(PFOBJS:%.o=%.po)
POFILES=	$(OBJS:%.o=sh/%.po) $(PFPOFILES) $(LIBSHOBJS:%.o=shlib/%.po)

XGETFLAGS +=	-a -n -x ksh.xcl
CPPFLAGS =	-DWEXP -DKSHELL -Iinclude $(CPPFLAGS.master)
CPPFLAGS +=	-I ../sh
CPPFLAGS +=	-D_FILE_OFFSET_BITS=64
CPPFLAGS +=	-DCSI_ASCIIACCEL
LDLIBS +=	-lsocket -lnsl
LAZYLIBS = -lsecdb

CLOBBERFILES += $(PROG) $(LIBSH)

install	:= TARGET= install
clobber	:= TARGET= clobber

.KEEP_STATE:

.PARALLEL: $(OBJS) $(LIBSHOBJS)

all: $(PROG) $(TXTS)

%.o:	sh/%.c
	$(COMPILE.c) -o $@ $<

%.o:	shlib/%.c
	$(COMPILE.c) -o $@ $<

%.o:	../sh/%.c
	$(COMPILE.c) -o $@ $<

$(BUILTINS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(LIBSH): $(LIBSHOBJS)
	$(AR) cr $(LIBSH) $(LIBSHOBJS)

$(KSH_PROG): $(OBJS) $(PFOBJS) $(LIBSH)
	$(LINK.c) $(OBJS) $(PFOBJS) $(LIBSH) -o $@ $(LDLIBS) \
	-z lazyload $(LAZYLIBS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

# based on .c.po, BUILD.po from Makefile.master
%.po:	../sh/%.c
	$(COMPILE.cpp) $<  > $(<F).i
	$(XGETTEXT) $(XGETFLAGS) $(<F).i ;\
	$(RM)	$@ ;\
	sed "/^domain/d" < messages.po  > $@ ;\
	$(RM) messages.po $(<F).i

install: all $(ROOTPROG) $(BUILTINS) $(ROOTXPG4SYMLINK) $(ROOTLINK)

$(ROOTXPG4SYMLINK):
	$(RM) $@
	$(SYMLINK) ../../bin/$(KSH_PROG) $@

$(ROOTLINK): $(ROOTBIN)/$(KSH_PROG)
	$(RM) $@
	$(LN) $(ROOTBIN)/$(KSH_PROG) $@

clean:	$(BUILTINS)
	$(RM) $(OBJS) $(LIBSHOBJS) $(PFOBJS)

clobber: $(BUILTINS)

lint: lint_SRCS

include ../Makefile.targ

FRC:
