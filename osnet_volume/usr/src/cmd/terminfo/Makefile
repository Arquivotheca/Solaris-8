#
#ident	"@(#)Makefile	1.15	99/03/30 SMI"
#
# Copyright (c) 1989,1998 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=	terminfo

TABSETSRC= \
	3101 beehive hds hds3 std stdcrt teleray vt100 wyse-adds xerox1720

TXTS=	Doc.sed README ckout cvt.ex cvt.sed $(TABSETSRCS) termcap 

TISRC=	adds.ti annarbor.ti ansi.ti att.ti beehive.ti \
	cdc.ti colorscan.ti datamedia.ti dec.ti diablo.ti \
	fortune.ti general.ti hardcopy.ti hazeltine.ti hds.ti \
	heath.ti homebrew.ti hp.ti lsi.ti microterm.ti \
	misc.ti pc.ti perkinelmer.ti print.ti special.ti \
	sperry.ti tektronix.ti teleray.ti televideo.ti ti.ti \
	tymshare.ti visual.ti wyse.ti

PARTS=	header $(TISRC) trailer

include ../Makefile.cmd

DIRMODE=	0755
FILEMODE=	0644
TIC=		tic
CAT=		/usr/bin/cat
ECHO=		echo

ROOTSHARELIBDIR	= $(ROOT)/usr/share/lib
ROOTTABDIR	= $(ROOTSHARELIBDIR)/tabset
ROOTTERMDIR	= $(ROOTSHARELIBDIR)/terminfo
OWNER		= root
GROUP		= bin
DIRS=		$(ROOTTERMDIR) $(ROOTTABDIR)

ROOTSYMLINK	= $(ROOTETC)/termcap
RELROOTETC	= ../usr/share/lib
ROOTLIBSYMLINK	= $(ROOTLIB)/tabset
RELROOTLIB	= ../share/lib

ROOTTABSET	= $(TABSETSRC:%=$(ROOTTABDIR)/%)

$(ROOTTABDIR)/% :	tabset/%
	$(INS.file)

$(ROOTSHARELIBDIR)/% :	%
	$(INS.file)

.KEEP_STATE:

all:		$(PROG).src $(TXTS)

$(DIRS):
	$(INS.dir)

$(PROG).src:	$(PARTS)
		@$(CAT) $(PARTS) > $(PROG).src

$(ROOTTERMDIR)/s/sun: $(PROG).src
	TERMINFO=$(ROOTTERMDIR) 2>&1 $(TIC) -v $(PROG).src > errs
	$(CHOWN) -R $(OWNER) $(ROOTTERMDIR)
	$(CHGRP) -R $(GROUP) $(ROOTTERMDIR)
	@$(ECHO) "\n`2>/dev/null cat errs|wc -l` entries have been compiled\n"
	@-(	2>/dev/null cat errs|grep -iv "^mkdir"|grep -iv "^create"|grep -iv "^link"|grep -vi terminfo.src|grep -vi touch|grep -vi "working"; \
	 	if [ $$? -ne 0 ] ; \
		then \
			$(ECHO) "\tNo errors\n"; \
		else \
			$(ECHO) "\n\tErrors can be found in `pwd`/errs\n"; \
		fi \
	)

#
#	Note that order is significant here. $(ROOTTERMDIR)/s/sun will set the
#	entire subtree to ownership root/bin.
#
install:	all $(ROOTTERMDIR) $(ROOTTERMDIR)/s/sun $(ROOTTABDIR) \
		$(ROOTTABSET) $(ROOTSHARELIBDIR)/termcap  $(ROOTSYMLINK) \
		$(ROOTLIBSYMLINK)

$(ROOTSYMLINK):
	-$(RM) $@; $(SYMLINK) $(RELROOTETC)/termcap $@

$(ROOTLIBSYMLINK):
	-$(RM) $@; $(SYMLINK) $(RELROOTLIB)/tabset $@

lint:

clean:
		$(RM) $(PROG).src errs

clobber: 	clean
