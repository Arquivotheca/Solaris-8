#
#pragma ident	"@(#)Makefile	1.113	99/07/21 SMI"
#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sun/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

# NOTE: pm.h is not shipped.  It should be shipped when power management
# is a real feature of Solaris.

HDRS=  \
aflt.h \
audio_79C30.h		audio_4231.h		audio_4231_dma.h \
avintr.h \
bmac.h \
bootconf.h		bpp_reg.h \
bpp_var.h \
bw2reg.h		bw2var.h \
cg3var.h		\
cg6fbc.h		cg6reg.h		cg6tec.h \
cg6thc.h		cg6var.h \
cg8reg.h		cg8var.h \
cg14io.h		cg14reg.h \
cms.h			comvec.h \
cursor_impl.h \
dbriio.h		dbrireg.h		dbrivar.h \
dkmpio.h		dmaga.h	\
ecppio.h		ecppreg.h		ecppvar.h \
fdreg.h			fdvar.h \
hme.h			hme_mac.h \
hme_phy.h		i82586.h \
isdnio.h \
lance.h			le.h \
mace.h \
memfb.h			memreg.h		memvar.h \
mmcodecreg.h \
obpdefs.h		openprom.h \
pixrect.h  \
pr_impl_util.h		pr_planegroups.h \
promif.h		promimpl.h \
qe.h			qec.h \
ramdac.h 		ser_async.h		ser_zscc.h \
soc_cq_defs.h		socmap.h		socreg.h \
socvar.h 		stcconf.h \
socalvar.h		socalreg.h		socalmap.h \
socalio.h 		socal_cq_defs.h \
stcio.h 		stcreg.h 		stcvar.h \
stp4020_reg.h		stp4020_var.h \
zsdev.h

DBHDRS=			debugger.h

SCSIADHDRS= \
espcmd.h	espreg.h	espvar.h	fascmd.h	fasdma.h \
fasreg.h	fasvar.h	sfvar.h	\
ifpcmd.h	ifpvar.h	ifpio.h		ifpmail.h	ifpreg.h \
ispcmd.h	ispmail.h	ispreg.h	ispvar.h	\
plndef.h	plnvar.h	ssaisp.h

SCSITARGHDRS= \
sddef.h		pln_ctlr.h	ssddef.h

FCHDRS= \
fc_transport.h	linkapp.h	fc.h		fcp.h \
fcal_transport.h fcal.h fcal_linkapp.h fcio.h

FCTL_HDRS= \
fc_error.h      fc_fcaif.h      fc_fla.h        fc_linkapp.h    fc_portif.h \
fc_ulpif.h      fcal.h          fcgs2.h         fcph.h          fctl.h \
fctl_private.h  fp.h

FCA_HDRS= \
usoc_cq_defs.h	usocio.h	usocmap.h	usocreg.h	usocvar.h

ULP_HDRS= \
fcp.h		fcpvar.h	fcp_util.h

FC_COMMON_HDRS= \
fc.h		fc_appif.h	fc_types.h	fcio.h

ROOTDIR=	$(ROOT)/usr/include/sys
ROOTDIRS=	$(ROOTDIR) \
		$(ROOTDIR)/debug \
		$(ROOTDIR)/scsi/adapters \
		$(ROOTDIR)/scsi/targets \
		$(ROOTDIR)/fc4 \
		$(ROOTDIR)/fibre-channel \
		$(ROOTDIR)/fibre-channel/impl \
		$(ROOTDIR)/fibre-channel/ulp \
		$(ROOTDIR)/fibre-channel/fca

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%)
ROOTDBHDRS= $(DBHDRS:%=$(ROOTDIR)/debug/%)
ROOTFCHDRS= $(FCHDRS:%=$(ROOTDIR)/fc4/%)
ROOTSCSIADHDRS= $(SCSIADHDRS:%=$(ROOTDIR)/scsi/adapters/%)
ROOTSCSITARGHDRS= $(SCSITARGHDRS:%=$(ROOTDIR)/scsi/targets/%)
ROOTFCCOMHDRS= $(FC_COMMON_HDRS:%=$(ROOTDIR)/fibre-channel/%)
ROOTFCTLHDRS= $(FCTL_HDRS:%=$(ROOTDIR)/fibre-channel/impl/%)
ROOTFCAHDRS= $(FCA_HDRS:%=$(ROOTDIR)/fibre-channel/fca/%)
ROOTULPHDRS= $(ULP_HDRS:%=$(ROOTDIR)/fibre-channel/ulp/%)


# install rules
$(ROOTDIR)/%: %
	$(INS.file)

$(ROOTDIR)/debug/%: debug/%
	$(INS.file)

$(ROOTDIR)/scsi/adapters/%: scsi/adapters/%
	$(INS.file)

$(ROOTDIR)/scsi/targets/%: scsi/targets/%
	$(INS.file)

$(ROOTDIR)/fibre-channel/%: fibre-channel/%
	$(INS.file)

$(ROOTDIR)/fibre-channel/ulp/%: fibre-channel/ulp/%
	$(INS.file)

$(ROOTDIR)/fibre-channel/fca/%: fibre-channel/fca/%
	$(INS.file)

$(ROOTDIR)/fibre-channel/impl/%: fibre-channel/impl/%
	$(INS.file)

# check files really don't exist

debug/%.check:		debug/%.h
	$(DOT_H_CHECK)

scsi/adapters/%.check:	scsi/adapters/%.h
	$(DOT_H_CHECK)

scsi/targets/%.check:	scsi/targets/%.h
	$(DOT_H_CHECK)

fibre-channel/%.check: fibre-channel/%.h
	$(DOT_H_CHECK)

fibre-channel/ulp/%.check: fibre-channel/ulp/%.h
	$(DOT_H_CHECK)

fibre-channel/fca/%.check: fibre-channel/fca/%.h
	$(DOT_H_CHECK)

fibre-channel/impl/%.check: fibre-channel/impl/%.h
	$(DOT_H_CHECK)

CHECKHDRS= \
	$(HDRS:%.h=%.check) \
	$(DBHDRS:%.h=debug/%.check) \
	$(FCHDRS:%.h=fc4/%.check) \
	$(SCSIADHDRS:%.h=scsi/adapters/%.check) \
	$(SCSITARGHDRS:%.h=scsi/targets/%.check) \
	$(FCTL_HDRS:%.h=fibre-channel/impl/%.check) \
	$(ULP_HDRS:%.h=fibre-channel/ulp/%.check) \
	$(FCA_HDRS:%.h=fibre-channel/fca/%.check) \
	$(FC_COMMON_HDRS:%.h=fibre-channel/%.check)


# headers which won't quite meet the standards...
#
# devops.h has a macro where the formal parameters to the macro are greater
# than 80 characters.  cpp (or the equivalent built into acomp) does not allow
# continuation line breaks in the formal parameter list.  This could be fixed
# by giving shorter names to the formal parameters, but the right fix is to
# fix cpp.  (Also, /* CSTYLED */ doesn't seem to fix this.
#
devops.check    := CSTYLE_TAIL = | grep -v "line > 80 characters" | true

.KEEP_STATE:

.PARALLEL:	$(CHECKHDRS) $(ROOTHDRS) $(ROOTDBHDRS) $(ROOTSCSIADHDRS) \
		$(ROOTSCSITARGHDRS) $(ROOTXHDRS) $(ROOTFCHDRS) \
		$(ROOTFCCOMHDRS) $(ROOTFCTLHDRS) $(ROOTFCAHDRS) $(ROOTULPHDRS)

install_h: 	$(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTDBHDRS) $(ROOTSCSIADHDRS) \
	   	$(ROOTSCSITARGHDRS) $(ROOTFCHDRS) \
		$(ROOTFCCOMHDRS) $(ROOTFCTLHDRS) $(ROOTFCAHDRS) $(ROOTULPHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
