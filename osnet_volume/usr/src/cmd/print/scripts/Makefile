#
# Copyright (c) 1994-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.19	99/07/12 SMI"
#
# cmd/lp/etc/Makefile
#

include			../Makefile.sp

ROOTPRINTLIB = 		$(ROOTLIB)/print
$(ROOTVARSPOOLPRINT)	:= OWNER=root

OWNER =			root
GROUP =			lp
FILEMODE =		0755

MSGFILES =              accept lpadmin
POFILE =                scripts.po

PROG =			conv_lp conv_lpd Makefile.yp
ROOTLIBPRINTPROG=	$(PROG:%=$(ROOTPRINTLIB)/%)
$(ROOTLIBPRINTPROG)	:= FILEMODE=0555
$(ROOTPRINTLIB)/Makefile.yp		:= FILEMODE=0444

USRSBINPROG=		accept lpsystem lpadmin
ROOTUSRSBINPROG=	$(USRSBINPROG:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINPROG)		:= FILEMODE=555

STARTPROG =		spc
ROOTLNKPROGS =		$(ROOTRC0_D)/K39spc \
			$(ROOTRCS_D)/K39spc \
			$(ROOTRC1_D)/K39spc \
			$(ROOTRC2_D)/S80spc
ROOTSTARTPROG=		$(STARTPROG:%=$(ROOTINIT_D)/%)
$(ROOTSTARTPROG)	:= OWNER=root
$(ROOTSTARTPROG)	:= GROUP=sys
$(ROOTSTARTPROG)	:= FILEMODE=0744


PCONF=	 		printers.conf
ROOTPCONF=		$(PCONF:%=$(ROOTETC)/%)
$(ROOTPCONF)		:= OWNER=root
$(ROOTPCONF)		:= GROUP=sys
$(ROOTPCONF)		:= FILEMODE=644

ACCEPTLINKS=		$(ROOTUSRSBIN)/reject $(ROOTBIN)/enable \
			$(ROOTBIN)/disable $(ROOTLIB)/accept \
			$(ROOTLIB)/reject

LIBLINKS=		$(ROOTLIB)/lpadmin $(ROOTLIB)/lpsystem


.KEEP_STATE:

all :			$(PROG)

$(ROOTLIB)/print/% $(ROOTINIT_D)/%:	%
	$(INS.file)

$(ROOTINIT_D) $(ROOTUSRSBIN) $(ROOTVARSPOOLPRINT):
	$(INS.dir)

$(ROOTLIB)/lpadmin:
	$(RM) $@; $(SYMLINK) ../sbin/lpadmin $@

$(ROOTLIB)/lpsystem:
	$(RM) $@; $(SYMLINK) ../sbin/lpsystem $@

$(ACCEPTLINKS):
	$(RM) $@; $(SYMLINK) ../sbin/accept $@

$(ROOTLNKPROGS) :	$(ROOTSTARTPROG)
			$(RM) $@; $(LN) $(ROOTSTARTPROG) $@

_msg:   $(POFILE)

$(POFILE): $(MSGFILES)
	grep gettext $(MSGFILES) | tr '`' ' ' | sed -e "s/gettext \"/gettext \(\"/" | sed -e "s/$$/);/"  > $(POFILE).i
	$(XGETTEXT) -s $(POFILE).i
	$(RM) $@ $(POFILE).i
	mv messages.po $(POFILE)

#
# Create a message file to test with
#
_msg_test:
	grep gettext $(MSGFILES) | tr '`' ' ' | sed -e "s/gettext \"/gettext \(\"/" | sed -e "s/$$/);/"  > $(POFILE).i
	$(XGETTEXT) -s -m "xxx" $(POFILE).i
	$(RM) $@ $(POFILE).i
	mv messages.po $(POFILE)

install:		$(ROOTINIT_D) $(ROOTLNKPROGS) \
			$(ROOTLIBPRINTPROG) $(ROOTSTARTPROG) \
			$(ROOTUSRSBIN) $(ROOTUSRSBINPROG) \
			$(ROOTVARSPOOLPRINT) $(ROOTPCONF) \
			$(LIBLINKS) $(ACCEPTLINKS)

$(SYMLINKS1):
	$(RM) $@; $(SYMLINK) ../sbin/$(SBINPROG1) $@

$(SYMLINKS2):
	$(RM) $@; $(SYMLINK) ../sbin/$(SBINPROG2) $@

$(REJECTLINK):
	$(RM) $@; $(SYMLINK) accept $@

clean:
	$(RM) $(POFILE)

clobber: clean

strip lint:
