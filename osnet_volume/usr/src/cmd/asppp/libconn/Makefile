#
#ident	"@(#)Makefile	1.5	93/10/07 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/asppp/libconn/Makefile
#

include ../../Makefile.cmd

OBJS=		callers.o getargs.o line.o uucpdefs.o	\
		ulockf.o conn.o interface.o strsave.o	\
		sysfiles.o stoa.o strecpy.o setmode.o uucpname.o

HDRS = dial.h dk.h log.h parms.h sysexits.h sysfiles.h uucp.h

SRCS=	$(OBJS:%.o=%.c)

BNUDIR= ../../bnu
LIBRARY= libconn.a

CPPFLAGS += -I$(BNUDIR)

debug:=	CFLAGS= -g

all install debug: $(LIBRARY)

$(LIBRARY): $(OBJS)
		ar rv $@ $?



$(LIBRARY)(%.o): %.o
		@true

%.c: $(BNUDIR)/%.c
	rm -f $@
	cp $(BNUDIR)/$@ $@

clean:		
		rm -f $(SRCS) $(OBJS) *~ ,*

clobber:	clean
		rm -f $(LIBRARY)

sources:	$(SRCS)

.PRECIOUS:	$(LIBRARY)

.KEEP_STATE:

