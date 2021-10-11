#
#ident	"@(#)Makefile	1.8	99/06/04 SMI"
#
# Copyright (c) 1990,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/valtools/Makefile
#

PROG= ckint ckitem ckpath ckrange ckstr ckyorn \
      ckkeywd ckdate cktime ckuid ckgid

POFILES= ckdate.po ckgid.po ckint.po ckitem.po \
	ckkeywd.po ckpath.po ckrange.po ckstr.po \
	cktime.po ckuid.po ckyorn.po puttext.po

include ../Makefile.cmd

LIBPKGDIR=	../../lib/libpkg
LIBADMDIR=	../../lib/libadm/inc
CPPFLAGS +=	-I$(LIBADMDIR) -I$(LIBPKGDIR)
LDLIBS += -ladm -lpkg
POFILE= valtools.po
CLOBBERFILES +=	puttext $(POFILES) $(POFILE)
CLEANFILES += puttext

VCKS=	valint helpint errint  \
	helpitem erritem  \
        valpath helppath errpath  \
	valrange helprange errange  \
	valstr helpstr errstr  \
	valyorn helpyorn erryorn  \
	valtime helptime errtime  \
	valdate helpdate errdate  \
	dispuid valuid helpuid erruid  \
	dispgid valgid helpgid errgid

DISPS=	dispuid dispgid

ROOTSADMDIR=	$(ROOT)/usr/sadm/bin

ROOTPUTTEXT=	$(ROOTSADMDIR)/puttext
ROOTVCKS=	$(VCKS:%=$(ROOTSADMDIR)/%)
ROOTDISPS=	$(DISPS:%=$(ROOTBIN)/%)


# valtool install rules
$(ROOTSADMDIR)/%: %
	$(INS.file)

# there is a special case here for errange
#
$(ROOTSADMDIR)/disp% \
$(ROOTSADMDIR)/val% \
$(ROOTSADMDIR)/help% \
$(ROOTSADMDIR)/err% \
$(ROOTSADMDIR)/er% \
$(ROOTBIN)/disp%: $(ROOTBIN)/ck%
	$(RM) $@; $(LN) $< $@

 
.KEEP_STATE:

.PARALLEL:	 $(PROG) puttext

all: $(PROG) puttext 
 
install: all $(ROOTSADMDIR) $(ROOTPROG) $(ROOTPUTTEXT) $(ROOTVCKS) $(ROOTDISPS)
 
$(ROOTSADMDIR):
	$(INS.dir)

$(POFILE):	$(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

clean:

lint:   lint_SRCS
 
include ../Makefile.targ
