#
#ident	"@(#)Makefile	1.24	00/09/14 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# cmd/vi/port/Makefile

PROG= ex
XPG4PROG= ex
LIBPROGS= expreserve exrecover
XD= exobjs.xpg4

EXOBJS=	bcopy.o ex.o ex_addr.o ex_cmds.o ex_cmds2.o \
	ex_cmdsub.o ex_data.o ex_extern.o ex_get.o \
	ex_io.o ex_put.o ex_re.o ex_set.o ex_subr.o \
	ex_temp.o ex_tty.o ex_unix.o ex_v.o ex_vadj.o \
	ex_vget.o ex_vmain.o ex_voper.o ex_vops.o \
	ex_vops2.o ex_vops3.o ex_vput.o ex_vwind.o \
	printf.o
XPG4EXOBJS= ${EXOBJS:%=$(XD)/%} compile.o
EXRECOVEROBJS=	exrecover.o ex_extern.o
OBJS= $(EXOBJS) $(XPG4EXOBJS) expreserve.o exrecover.o
SRCS= $(EXOBJS:%.o=%.c) expreserve.c exrecover.c
TXTS = READ_ME makeoptions asfix.c70 ex.news port.mk.370 port.mk.70	\
	port.mk.c70 port.mk.usg ovdoprnt.s ovprintf.c rofix

include ../../Makefile.cmd
#
# For message catalogue files
#
POFILES= $(EXOBJS:%.o=%.po) expreserve.po exrecover.po
POFILE= port.po

$(XPG4) := CFLAGS += -DXPG4
CPPFLAGS +=  -DUSG -DSTDIO -DVMUNIX -DTABS=8 -DSINGLE -DTAG_STACK
CLOBBERFILES += $(LIBPROGS)
ex :=		LDLIBS += -lmapmalloc -lcurses -lcrypt_i -lgen
$(XPG4) :=	LDLIBS += -lmapmalloc -lcurses -lcrypt_i -lgen
exrecover :=	LDLIBS += -lmapmalloc -lcrypt_i

ROOTLIBPROGS= $(LIBPROGS:%=$(ROOTLIB)/%)

# hard links to ex
ROOTLINKS= $(ROOTBIN)/vi $(ROOTBIN)/view $(ROOTBIN)/edit $(ROOTBIN)/vedit
ROOTXPG4LINKS= $(ROOTXPG4BIN)/vi $(ROOTXPG4BIN)/view $(ROOTXPG4BIN)/edit \
	$(ROOTXPG4BIN)/vedit
.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG) $(XPG4) $(LIBPROGS)

$(PROG): $(EXOBJS)
	$(LINK.c) $(EXOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

ex.xpg4: $(XD) $(XPG4EXOBJS)
	$(LINK.c) $(XPG4EXOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

compile.o $(XD)/compile.o: ../../expr/compile.c
	$(COMPILE.c) -o $@ ../../expr/compile.c

$(XPG4EXOBJS): $(XD)

$(XD)/%.o:	%.c
	$(COMPILE.c) -o $@ $<

$(XD):
	-@mkdir -p $@

exrecover:	$(EXRECOVEROBJS)
	$(LINK.c) $(EXRECOVEROBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

catalog: $(POFILE)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@


install: all $(ROOTPROG) $(ROOTLIBPROGS) $(ROOTLINKS) \
	$(ROOTXPG4PROG) $(ROOTXPG4LINKS)

$(ROOTLINKS): $(ROOTPROG)
	-$(RM) $@
	-$(LN) -f $(ROOTPROG) $@

$(ROOTXPG4LINKS): $(ROOTXPG4PROG)
	-$(RM) $@
	-$(LN) -f $(ROOTXPG4PROG) $@

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../../Makefile.targ
