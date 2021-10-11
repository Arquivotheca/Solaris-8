#
#ident	"@(#)Makefile	1.161	00/09/14 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/Makefile
#
# include global definitions
include ../Makefile.master

#	Note that libcurses installs commands along with its library.
#	This is a minor bug which probably should be fixed.
#	Note also that a few extra libraries are kept in cmd source.
#
# Certain libraries are linked with, hence depend on, other libraries.
# These subordinate libraries must appear in the *_SUBDIRS list after
# the libraries on which they depend.
#
# Almost everything depends on libc, which depends on libdl.
# After that, many libraries depend on libnsl, which depends on libc and libmp.
# Other than that, the dependencies are indicated by .WAIT below.
#
# Build everything in parallel; use .WAIT for dependencies
.PARALLEL:

COMMON_SUBDIRS= \
	../cmd/sgs/libdl	.WAIT	\
	libc			.WAIT	\
	libmp			.WAIT	\
	libnsl			.WAIT	\
	libsecdb		.WAIT	\
	../cmd/sgs/libelf		\
	librpcsvc			\
	libsocket		.WAIT	\
	libresolv			\
	libresolv2		.WAIT	\
	libw			.WAIT	\
	libintl			.WAIT	\
	fn			.WAIT	\
	../cmd/sgs/librtld_db	\
	libadm			\
	libaio			\
	libcmd			\
	libcurses		\
	libdevinfo	.WAIT	\
	libgen			\
	libgss			\
	libpam			\
	liblwp			\
	libthread	.WAIT	\
	libslp		\
	libTL		\
	libbsdmalloc	\
	libdl_stubs	\
	libdoor		\
	libdevfsevent	\
	libeti		\
	libcrypt	\
	libgenIO	\
	libkstat	\
	libkvm		\
	liblm		\
	libmalloc	\
	libmapmalloc	\
	libmd5		\
	libmtmalloc	\
	libnls		\
	libnull		\
	libpkg		\
	libplot		\
	libldap3	\
	libldap		.WAIT	\
	libsldap	.WAIT	\
	libsys		\
	libnisdb	\
	nametoaddr	\
	librac		\
	ami		.WAIT \
	smartcard	.WAIT \
	gss_mechs/mech_krb5	.WAIT	\
	krb5	.WAIT	\
	pam_modules	\
	libbsm		\
	librt		\
	libvolmgt	\
	libdevice	\
	libdevid	\
	libc2		\
	liblwp_db	\
	libthread_db	\
	libsec		\
	libtnfprobe	\
	libtnf		\
	libtnfctl	\
	libdhcp		\
	libdhcpagent	\
	libdhcputil	\
	nsswitch	\
	print		\
	libpthread	\
	libsched	\
	libproc		.WAIT	\
	libpctx		.WAIT	\
	libcpc		\
	watchmalloc	\
	libxcurses	\
	libxcurses2	\
	libxnet		\
	gss_mechs/mech_dummy	\
	gss_mechs/mech_dh	\
	rpcsec_gss	\
	librcm		.WAIT	\
	libcfgadm	.WAIT	\
	cfgadm_plugins	\
	libmail		\
	zlib

sparc_SUBDIRS= .WAIT	\
	libc_psr	.WAIT	\
	libmd5_psr	.WAIT	\
	libprtdiag	.WAIT	\
	libprtdiag_psr	\
	storage

SUBDIRS= $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS)

#
# For message catalogue file creation
#
MSGSUBDIRS= libc libcmd libnsl libpkg libcurses libpam pam_modules \
	gss_mechs/mech_krb5 gss_mechs/mech_dh krb5 libgss rpcsec_gss \
	libcfgadm cfgadm_plugins libslp libsldap ami smartcard

BWOSDIRS= libg libm libmsg libwindows

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg

.KEEP_STATE:

all clean clobber: $(SUBDIRS)

install: $(SUBDIRS) .WAIT install_extra

# extra libraries kept in other source areas
install_extra:
	@cd ../cmd/sgs; pwd; $(MAKE) install_lib
	@pwd

install_h check:
	@cd libc; pwd; $(MAKE) $@
	@cd libcurses; pwd; $(MAKE) $@
	@cd libdhcp; pwd; $(MAKE) $@
	@cd libdhcpagent; pwd; $(MAKE) $@
	@cd libdhcputil; pwd; $(MAKE) $@
	@cd libeti; pwd; $(MAKE) $@
	@cd libgen; pwd; $(MAKE) $@
	@cd libkstat; pwd; $(MAKE) $@
	@cd libkvm; pwd; $(MAKE) $@
	@cd libproc; pwd; $(MAKE) $@
	@cd libpctx; pwd; $(MAKE) $@
	@cd libcpc; pwd; $(MAKE) $@
	@cd librpcsvc; pwd; $(MAKE) $@
	@cd libaio; pwd; $(MAKE) $@
	@cd libbsm; pwd; $(MAKE) $@
	@cd libvolmgt; pwd; $(MAKE) $@
	@cd libdevice; pwd; $(MAKE) $@
	@cd libdevid; pwd; $(MAKE) $@
	@cd libdevinfo; pwd; $(MAKE) $@
	@cd libdevfsevent; pwd; $(MAKE) $@
	@cd libplot; pwd; $(MAKE) $@
	@cd libtnfprobe; pwd; $(MAKE) $@
	@cd libtnf; pwd; $(MAKE) $@
	@cd libtnfctl; pwd; $(MAKE) $@
	@cd fn; pwd; $(MAKE) $@
	@cd libxcurses2; pwd; $(MAKE) $@
	@cd libmtmalloc; pwd; $(MAKE) $@
	@cd libmail; pwd; $(MAKE) $@
	@cd libpam; pwd; $(MAKE) $@
	@cd ami; pwd; $(MAKE) $@
	@if [ -f smartcard/Makefile  ]; then \
		cd smartcard; pwd; $(MAKE) $@; \
	 else \
		true; \
	 fi
	@cd libslp; pwd; $(MAKE) $@
	@cd librcm; pwd; $(MAKE) $@
	@cd zlib; pwd; $(MAKE) $@

_msg: $(MSGSUBDIRS)

$(SUBDIRS): FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	 else \
		true; \
	 fi

FRC:
