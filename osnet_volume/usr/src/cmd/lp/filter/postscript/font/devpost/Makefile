#
# ident	"@(#)Makefile	1.7	90/10/12 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/font/devpost/Makefile
#

include			../../../../Makefile.lp

FONTFILES =		AB AI AR AX BI CB CI CO	\
			CW CX GR HB HI HX Hb Hi	\
			Hr Hx KB KI KR KX NB NI	\
			NR NX PA PB PI PX S1 VB	\
			VI VR VX ZD ZI B  H  I	\
			R  S	
FONTLNK1 =		G HM HK HL
FONTLNK2 =		GI
DESCFILE =		DESC
TXTS =			DESC.big DESC.small S.big S.small LINKFILE

FONTNAMES =		$(FONTFILES:%=%.name)
FONTLNK1OUTS =		$(FONTLNK1:%=%.out)
FONTLNK2OUTS =		$(FONTLNK2:%=%.out)
FONTOUTS =		$(FONTFILES:%=%.out) $(DESCFILE:%=%.out)

SUBDIRS =		charlib

NATIVEMAKEDEV =		../native/makedev

ROOTLIBFONT =		$(ROOT)/usr/lib/font
ROOTDEVPOST =		$(ROOTLIBFONT)/devpost

ROOTFONTFILES =		$(FONTFILES:%=$(ROOTDEVPOST)/%) \
			$(ROOTDEVPOST)/$(DESCFILE)
ROOTFONTNAMES =		$(FONTNAMES:%=$(ROOTDEVPOST)/%)
ROOTFONTOUTS =		$(FONTOUTS:%=$(ROOTDEVPOST)/%)		\
			$(FONTLNK1OUTS:%=$(ROOTDEVPOST)/%)	\
			$(FONTLNK2OUTS:%=$(ROOTDEVPOST)/%)
		
FILEMODE =		0444

.KEEP_STATE:

all :			$(TXTS) $(FONTOUTS) $(FONTNAMES) $(SUBDIRS)

install:		$(TXTS) $(FONTOUTS) $(FONTNAMES) \
			$(ROOTDEVPOST) $(ROOTFONTFILES) $(ROOTFONTNAMES) \
			$(ROOTFONTOUTS) $(SUBDIRS)
		
$(FONTOUTS) :		$$(@:%.out=%) $(NATIVEMAKEDEV)
			@$(RM) $@; $(NATIVEMAKEDEV) $(@:%.out=%)

$(ROOTDEVPOST) :
			$(INS.dir)
	
$(NATIVEMAKEDEV) :
			@cd ..; $(MAKE) native/makedev

$(FONTLNK1OUTS) :	H.out
			@$(RM) $@; $(LN) H.out $@

$(FONTLNK2OUTS) :	HI.out
			@$(RM) $@; $(LN) HI.out $@

$(ROOTDEVPOST)/% :	%
			$(INS.file)

clean strip lint :	$(SUBDIRS)

clobber:		$(SUBDIRS)
			$(RM) $(FONTOUTS) $(FONTLNK1OUTS) $(FONTLNK2OUTS)

$(SUBDIRS):		FRC
			@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
