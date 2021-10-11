#
# Copyright (c) 1989,1993,1994-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.107	99/08/06 SMI"
#
# head/Makefile
#
# include global definitions
include ../Makefile.master

sparc_HDRS= 
i386_HDRS= floatingpoint.h math.h

KRB5HDRS= mit_copyright.h mit-sipb-copyright.h

ATTRDB_HDRS=	secdb.h auth_attr.h exec_attr.h prof_attr.h user_attr.h \
	auth_list.h

HDRS=   $($(MACH)_HDRS)  $(ATTRDB_HDRS) \
aio.h        alloca.h     apptrace.h	apptrace_impl.h\
ar.h         archives.h   assert.h     \
config_admin.h cpio.h     crypt.h      ctype.h      \
deflt.h      devid.h      devmgmt.h    devpoll.h    \
dial.h       dirent.h     dlfcn.h      door.h \
elf.h        errno.h      euc.h        fatal.h      fcntl.h      \
float.h      fmtmsg.h     fnmatch.h    ftw.h        \
gelf.h       getwidth.h   glob.h       \
grp.h        iconv.h      ieeefp.h     inttypes.h   iso646.h	\
langinfo.h   lastlog.h    ldap.h       lber.h       libelf.h     libgen.h \
libgenIO.h   libintl.h	  libw.h       limits.h     linenum.h    \
link.h       listen.h     locale.h     macros.h     malloc.h     \
md5.h        memory.h     \
mon.h        monetary.h   mp.h         mqueue.h     nan.h        ndbm.h       \
netconfig.h  netdb.h      netdir.h     nl_types.h   nlist.h      note.h       \
nsswitch.h   nss_common.h nss_dbdefs.h nss_netdir.h pcsample.h   pfmt.h	      \
pkgdev.h     \
pkginfo.h    pkglocs.h    pkgstrct.h   pkgtrans.h   poll.h       \
proc_service.h procfs.h   prof.h       pthread.h    pw.h         pwd.h        \
re_comp.h    regex.h      regexp.h     resolv.h     rje.h        \
rtld_db.h \
rmmount.h    sac.h        sched.h      schedctl.h   search.h     \
semaphore.h  setjmp.h     \
sgtty.h      shadow.h     siginfo.h    \
signal.h     std.h        stdarg.h     stddef.h     stdio.h      \
stdio_ext.h  stdio_tag.h \
stdio_impl.h stdlib.h     storclass.h  string.h     \
strings.h    stropts.h    sum.h        synch.h      \
sysexits.h   syslog.h     syms.h       table.h      \
tar.h        termio.h     termios.h    thread.h     thread_db.h  \
time.h       tiuser.h     tzfile.h     \
ucontext.h   ulimit.h     unistd.h     userdefs.h   ustat.h      utime.h      \
utmp.h       utmpx.h      valtools.h   values.h     varargs.h    wait.h       \
wchar.h      wchar_impl.h wctype.h     widec.h      wordexp.h    xti.h \
xti_inet.h

ISOHDRS = \
ctype_iso.h	limits_iso.h	locale_iso.h	setjmp_iso.h  	\
signal_iso.h	stdarg_iso.h	stddef_iso.h	stdio_iso.h	\
stdlib_iso.h	string_iso.h	time_iso.h	wchar_iso.h	\
wctype_iso.h

ARPAHDRS= ftp.h	 inet.h		nameser.h	telnet.h	tftp.h \
		nameser_compat.h

# rpcsvc headers which are just headers (not derived from a .x file)
RPCSVC_SRC_HDRS = \
bootparam.h      dbm.h             nis_db.h          nis_tags.h \
nislib.h         yp_prot.h         ypclnt.h          yppasswd.h \
ypupd.h          rpc_sztypes.h

# rpcsvc headers which are generated from .x files
RPCSVC_GEN_HDRS = \
bootparam_prot.h mount.h           nfs_prot.h \
nis.h            nis_cache.h       nis_callback.h    nlm_prot.h \
rex.h            rquota.h          rstat.h           rusers.h \
rwall.h          spray.h           \
nispasswd.h      ufs_prot.h        nfs_acl.h

SYMHDRASSERT = $(ROOT)/usr/include/iso/assert_iso.h
SYMHDRERRNO = $(ROOT)/usr/include/iso/errno_iso.h
SYMHDRFLOAT = $(ROOT)/usr/include/iso/float_iso.h
SYMHDRISO646 = $(ROOT)/usr/include/iso/iso646_iso.h

RPCGENFLAGS = -C -h
rpcsvc/nispasswd.h :=	RPCGENFLAGS += -M
rpcsvc/rwall.h :=	RPCGENFLAGS += -M

# rpcsvc rpcgen source (.x files)
#
# yp.x is an attempt at codifying what was hand coded in RPCL. 
# Unfortunately it doesn't quite work. (The handcoded stuff isn't
# expressable in RPCL) this is due to the fact that YP was written
# before rpcgen existed.  Hence, yp_prot.h cannot be derived from yp.x
#
# There is no `.h' for nis_object.x because it is included by nis.x and
# the resulting .h is nis.h.

RPCSVCPROTS = \
$(RPCSVC_GEN_HDRS:%.h=%.x)        nis_object.x       yp.x

RPCSVCHDRS= $(RPCSVC_SRC_HDRS) $(RPCSVC_GEN_HDRS)

PROTOHDRS=   dumprestore.h routed.h ripngd.h rwhod.h timed.h

ROOTHDRS= $(HDRS:%=$(ROOT)/usr/include/%)  \
	$(KRB5HDRS:%=$(ROOT)/usr/include/kerberosv5/%)  \
	$(ISOHDRS:%=$(ROOT)/usr/include/iso/%)  \
	$(ARPAHDRS:%=$(ROOT)/usr/include/arpa/%)  \
	$(RPCSVCHDRS:%=$(ROOT)/usr/include/rpcsvc/%)  \
	$(RPCSVCPROTS:%=$(ROOT)/usr/include/rpcsvc/%)  \
	$(PROTOHDRS:%=$(ROOT)/usr/include/protocols/%)

DIRS= iso arpa rpcsvc protocols security kerberosv5
ROOTDIRS= $(DIRS:%=$(ROOT)/usr/include/%)

SED=	sed

# check files really don't exist
#
# should do something with the rpcsvc headers

iso/%.check:		iso/%.h
	$(DOT_H_CHECK)

arpa/%.check:		arpa/%.h
	$(DOT_H_CHECK)

rpcsvc/%.check:		rpcsvc/%.h
	$(DOT_H_CHECK)

rpcsvc/%.check:		rpcsvc/%.x
	$(DOT_X_CHECK)

protocols/%.check:	protocols/%.h
	$(DOT_H_CHECK)

kerberosv5/%.check:             kerberosv5/%.h
	$(DOT_H_CHECK)

# Note that the derived headers (rpcgen) are not checked at this time.  These
# need work at the source level and rpcgen itself has a bug which causes a
# cstyle violation.  Furthermore, there seems to be good reasons for the
# generated headers to not pass all of the hdrchk rules.
#
# Add the following to the CHECKHDRS list to activate the .x checks:
#	$(RPCSVCPROTS:%.x=rpcsvc/%.check) \
#
CHECKHDRS= $(HDRS:%.h=%.check)  \
	$(KRB5HDRS:%.h=kerberosv5/%.check)  \
	$(ISOHDRS:%.h=iso/%.check)  \
	$(ARPAHDRS:%.h=arpa/%.check)  \
	$(RPCSVC_SRC_HDRS:%.h=rpcsvc/%.check) \
	$(PROTOHDRS:%.h=protocols/%.check)

# headers which won't quite meet the standards...
#
# assert.h is required by ansi-c to *not* be idempotent (section 4.1.2).
# Hence the trailing guard is not the last thing in the file nor can it
# be without playing silly games.

assert.check	:= HDRCHK_TAIL = | grep -v "end guard wrong" | true

# install rules

$(ROOT)/usr/include/security/%: security/%
	$(INS.file)

$(ROOT)/usr/include/protocols/%: protocols/%
	$(INS.file)

$(ROOT)/usr/include/rpcsvc/%: rpcsvc/%
	$(INS.file)

$(ROOT)/usr/include/kerberosv5/%: kerberosv5/%
	$(INS.file)

$(ROOT)/usr/include/arpa/%: arpa/%
	$(INS.file)

$(ROOT)/usr/include/iso/%: iso/%
	$(INS.file)

$(ROOT)/usr/include/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL:	$(ROOTHDRS) $(CHECKHDRS)

install_h: 	$(ROOTDIRS) .WAIT $(ROOTHDRS) $(SYMHDRASSERT) $(SYMHDRERRNO) \
		$(SYMHDRFLOAT) $(SYMHDRISO646)

check:	$(CHECKHDRS)

clean clobber:
	cd rpcsvc ; $(RM) $(RPCSVC_GEN_HDRS)

$(ROOTDIRS):
	$(INS.dir)

$(SYMHDRASSERT): 
	-$(RM) $@; $(SYMLINK) ../assert.h $@

$(SYMHDRERRNO):  
	-$(RM) $@; $(SYMLINK) ../errno.h $@

$(SYMHDRFLOAT):  
	-$(RM) $@; $(SYMLINK) ../float.h $@

$(SYMHDRISO646): 
	-$(RM) $@; $(SYMLINK) ../iso646.h $@

rpcsvc/%.h:	rpcsvc/%.x
	$(RPCGEN) $(RPCGENFLAGS) $< -o $@

rpcsvc/nis.h:	rpcsvc/nis.x
	$(RPCGEN) $(RPCGENFLAGS) rpcsvc/nis.x |\
	$(SED) -e '/EDIT_START/,$$ d' > $@
