#
# Copyright (c) 1995,1997,1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.5	99/07/31 SMI"
#
# lib/gss_mechs/mech_krb5/Makefile
#
#
# This make file will build mech_krb5.so.1. This shared object
# contains all the functionality needed to support the Kereros V5 GSS-API
# mechanism. No other Kerberos libraries are needed.
#

LIBRARY= mech_krb5.a
VERS = .1
MAPFILE= ../mapfile-vers

OWNER=	root
GROUP=	sys
FILEMODE=	755

# objects are listed by source directory

REL_PATH= ../..
MAPFILE= $(REL_PATH)/mapfile-vers

to_all:	all

# crypto
CRYPTO= cryptoconf.o encrypt_data.o decrypt_data.o \
	des_crc.o des_md5.o raw_des.o

# crypto/des
CRYPTO_DES= cbc_cksum.o	finish_key.o fin_rndkey.o init_rkey.o afsstring2key.o \
	process_ky.o random_key.o string2key.o key_sched.o u_rn_key.o u_nfold.o \
	weak_key.o f_cbc.o f_cksum.o f_sched.o f_ecb.o f_parity.o f_tables.o


# crypto/md5
CRYPTO_MD5= md5.o md5glue.o md5crypto.o

# crypto/crc32
CRYPTO_CRC32= crc.o

# crypto/os
CRYPTO_OS= rnd_confoun.o c_localaddr.o 	c_ustime.o

# et error_tables
ET=	adb_err.o adm_err.o asn1_err.o chpass_util_strings.o error_message.o \
	com_err.o gssapi_err_generic.o import_err.o \
	gssapi_err_krb5.o kadm_err.o kdb5_err.o kpasswd_strings.o kdc5_err.o \
	krb5_err.o kv5m_err.o pty_err.o ss_err.o

# krb5/asn.1
K5_ASN1= asn1_decode.o asn1_k_decode.o asn1_encode.o asn1_get.o asn1_make.o \
	asn1buf.o krb5_decode.o krb5_encode.o asn1_k_encode.o asn1_misc.o

# krb5/ccache 
K5_CC= ccbase.o ccdefault.o ccdefops.o ser_cc.o

# krb5/ccache/file
K5_CC_FILE= \
	fcc_close.o fcc_destry.o fcc_eseq.o fcc_gennew.o fcc_getnam.o \
	fcc_gprin.o fcc_init.o fcc_nseq.o fcc_read.o fcc_reslv.o \
	fcc_retrv.o fcc_sseq.o fcc_store.o fcc_skip.o fcc_ops.o \
	fcc_write.o fcc_sflags.o fcc_defops.o fcc_errs.o fcc_maybe.o

# krb5/ccache/memory
#K5_CC_MEM= \
#	mcc_close.o mcc_destry.o mcc_eseq.o mcc_gennew.o \
#	mcc_getnam.o mcc_gprin.o mcc_init.o mcc_nseq.o \
#	mcc_reslv.o mcc_retrv.o mcc_sseq.o mcc_store.o mcc_ops.o \
#	mcc_sflags.o

# krb5/ccache/stdio
K5_CC_STD= \
	scc_close.o scc_destry.o scc_eseq.o \
	scc_gennew.o scc_getnam.o scc_gprin.o scc_init.o \
	scc_nseq.o scc_read.o scc_reslv.o scc_retrv.o \
	scc_sseq.o scc_store.o scc_skip.o scc_ops.o scc_write.o \
	scc_sflags.o scc_defops.o scc_errs.o scc_maybe.o

# krb5/free
K5_FREE=f_addr.o f_address.o f_ap_rep.o	f_ap_req.o f_arep_enc.o	f_authdata.o \
	f_authent.o f_auth_cnt.o f_chksum.o f_creds.o f_cred_cnt.o \
	f_enc_kdc.o f_enc_tkt.o	f_einfo.o f_error.o f_kdc_rp.o f_kdc_rq.o \
	f_keyblock.o f_last_req.o f_padata.o f_princ.o f_priv.o	f_priv_enc.o \
	f_safe.o f_tckt.o f_tckts.o f_tgt_cred.o f_tkt_auth.o f_pwd_data.o \
	f_pwd_seq.o f_cred.o f_cred_enc.o

# krb5/keytab
K5_KT=	ktadd.o ktbase.o ktdefault.o ktfr_entry.o ktremove.o read_servi.o

K5_KT_FILE=ktf_add.o ktf_close.o ktf_endget.o ktf_g_ent.o ktf_g_name.o \
	ktf_next.o ktf_resolv.o	ktf_remove.o ktf_ssget.o ktf_util.o \
	ktf_ops.o ktf_wops.o ktf_wreslv.o ktf_defops.o ser_ktf.o

K5_KRB=	addr_comp.o addr_order.o addr_srch.o auth_con.o bld_pr_ext.o \
	bld_princ.o chk_trans.o conv_princ.o copy_addrs.o copy_auth.o \
	copy_athctr.o copy_cksum.o copy_creds.o copy_data.o copy_key.o \
	copy_princ.o copy_tick.o cp_key_cnt.o decode_kdc.o decrypt_tk.o \
	encode_kdc.o encrypt_tk.o free_rtree.o fwd_tgt.o gc_frm_kdc.o \
	gc_via_tkt.o gen_seqnum.o gen_subkey.o get_creds.o get_in_tkt.o \
	in_tkt_ktb.o in_tkt_pwd.o in_tkt_sky.o init_ctx.o kdc_rep_dc.o \
	mk_cred.o mk_error.o mk_priv.o mk_rep.o mk_req.o mk_req_ext.o \
	mk_safe.o parse.o pr_to_salt.o preauth.o princ_comp.o rd_cred.o \
	rd_error.o rd_priv.o rd_rep.o rd_req.o rd_req_dec.o rd_safe.o \
	recvauth.o sendauth.o send_tgs.o ser_actx.o ser_adata.o ser_addr.o \
	ser_auth.o ser_cksum.o ser_ctx.o ser_eblk.o ser_key.o ser_princ.o \
	serialize.o srv_rcache.o str_conv.o tgtname.o unparse.o	valid_times.o \
	walk_rtree.o privacy_allowed.o

K5_OS=	an_to_ln.o def_realm.o ccdefname.o free_krbhs.o free_hstrl.o \
	full_ipadr.o get_krbhst.o gen_port.o genaddrs.o gen_rname.o \
	gmt_mktime.o hostaddr.o hst_realm.o init_os_ctx.o krbfileio.o \
	ktdefname.o kuserok.o mk_faddr.o localaddr.o locate_kdc.o lock_file.o \
	net_read.o net_write.o osconfig.o port2ip.o promptusr.o \
	read_msg.o read_pwd.o realm_dom.o sendto_kdc.o sn2princ.o timeofday.o \
	toffset.o unlck_file.o ustime.o	write_msg.o safechown.o

K5_POSIX= setenv.o daemon.o

K5_RCACHE=rc_base.o rc_dfl.o rc_io.o rcdef.o rc_conv.o ser_rc.o
SEAL = 

MECH=	accept_sec_context.o acquire_cred.o compare_name.o context_time.o \
	delete_sec_context.o disp_name.o disp_status.o export_sec_context.o \
	get_tkt_flags.o gssapi_krb5.o import_name.o import_sec_context.o \
	indicate_mechs.o init_sec_context.o inq_context.o inq_cred.o \
	inq_names.o k5seal.o k5unseal.o k5mech.o \
	process_context_token.o rel_cred.o rel_name.o rel_oid.o ser_sctx.o \
	sign.o util_cksum.o util_crypt.o util_ordering.o util_seed.o \
	util_seqnum.o util_set.o wrap_size_limit.o verify.o \
	pname_to_uid.o

MECH_GEN= disp_major_status.o disp_com_err_status.o \
	  util_buffer.o util_canonhost.o util_dup.o \
	  util_token.o util_validate.o
	  
PROFILE= prof_tree.o prof_file.o prof_parse.o prof_err.o prof_init.o

OBJECTS= \
	$(MECH) $(SEAL) $(MECH_GEN) $(PROFILE) \
	$(CRYPTO) $(CRYPTO_DES) \
	$(CRYPTO_MD5) $(CRYPTO_CRC32) \
	$(CRYPTO_OS) \
	$(ET) \
	$(K5_ASN1) \
	$(K5_CC) $(K5_CC_FILE) $(K5_CC_MEM) $(K5_CC_STD) $(K5_FREE) \
	$(K5_KT) $(K5_KT_FILE) $(K5_KRB) $(K5_OS) $(K5_POSIX) $(K5_RCACHE)

# include library definitions
include $(REL_PATH)/../../Makefile.lib

sparcv9_C_PICFLAGS =  -K PIC

# override default text domain
TEXT_DOMAIN= SUNW_OST_NETRPC
#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@
INS.file=       -$(RM) $@; $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) \
	-f $(@D) $<
INS.dir=        $(INS) -s -d -m $(DIRMODE) -u $(OWNER) -g $(GROUP) $@

CPPFLAGS += -I$(REL_PATH)/../libgss -I$(REL_PATH)/include  \
		-I$(SRC)/uts/common/gssapi/include \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include/krb5 \
		-I$(REL_PATH)/include/krb5 \
		-I$(REL_PATH)/crypto/crc32 \
		-I$(REL_PATH)/crypto/des -I$(REL_PATH)/crypto/md5 \
		-I$(REL_PATH)/crypto/os  \
		-I$(SRC)/uts/common/gssapi/include \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
		-DHAVE_STDLIB_H -DHAVE_STDARG_H  -DHAVE_SYS_TIME_H -DHAVE_SYS_TYPES_H


#CPPFLAGS += 	-D_REENTRANT
$(PICS) := 	CFLAGS += -xF
$(PICS) := 	CCFLAGS += -xF

pics/rnd_confoun.o := CPPFLAGS += -DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 \
	-DHAVE_SRAND48=1 -DHAVE_SRAND=1 -DHAVE_SRANDOM=1 -DHAVE_GETPID=1

pics/cryptoconf.o  := CPPFLAGS +=  -DPROVIDE_DES_CBC_MD5=1 \
	-DPROVIDE_DES_CBC_CRC=1 -DPROVIDE_DES_CBC_RAW=1 \
	-DPROVIDE_DES_CBC_CKSUM=1 -DPROVIDE_CRC32=1 \
	-DPROVIDE_RSA_MD5=1

pics/ccdefops.o := CPPFLAGS += -I$(REL_PATH)/krb5/ccache/file

pics/ktf_util.o := CPPFLAGS += -DHAVE_ERRNO

pics/str_conv.o := CPPFLAGS += -DHAVE_STRFTIME -DHAVE_STRPTIME

pics/rc_io.o := CPPFLAGS += -DHAVE_ERRNO

pics/prof_file.o := CPPFLAGS += -DHAVE_STAT

pics/prof_init.o := CPPFLAGS += -DSIZEOF_INT=4

pics/ser_sctx.o  := CPPFLAGS +=  -DPROVIDE_KERNEL_IMPORT=1 \

OS_FLAGS = -DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 -DTIME_WITH_SYS_TIME=1 \
 -DHAVE_UNISTD_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_REGEX_H=1 -DHAVE_REGEXP_H=1 \
 -DHAVE_RE_COMP=1 -DHAVE_REGCOMP=1 -DPOSIX_TYPES=1 -DNDBM=1 -DAN_TO_LN_RULES=1

pics/an_to_ln.o := CPPFLAGS += $(OS_FLAGS)
pics/gmt_mktime.o := CPPFLAGS += $(OS_FLAGS)
pics/timeofday.o := CPPFLAGS += $(OS_FLAGS) -DHAVE_ERRNO

DYNFLAGS += -M $(MAPFILE)
LIBS = $(DYNLIB)

# override ROOTLIBDIR and ROOTLINKS
ROOTLIBDIR=	$(ROOT)/usr/lib/gss/$(KSUBDIR)
ROOTLIBDIR64=	$(ROOT)/usr/lib/gss/$(KSUBDIR)/sparcv9
GSSLIBDIR64=	$(ROOT)/usr/lib/sparcv9/gss \
		$(ROOT)/usr/lib/sparcv9/gss/$(KSUBDIR)
GSSLINK64=	$(ROOT)/usr/lib/sparcv9/gss/$(KSUBDIR)/mech_krb5.so
K5MECHLIB64=	../../../gss/$(KSUBDIR)/sparcv9/mech_krb5.so
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)
ROOTLIBS64=	$(LIBS:%=$(ROOTLIBDIR64)/%)

$(ROOTLIBDIR) $(ROOTLIBDIR64) $(GSSLIBDIR64):
	$(INS.dir)

$(GSSLINK64):
	-$(RM) $@; $(SYMLINK) $(K5MECHLIB64) $@

#DYNFLAGS += -M $(MAPFILE_REORDER)

LDLIBS += -lgss -lsocket -lnsl -ldl -lc -lmp -lresolv

objs/%.o profs/%.o pics/%.o: $(SRC)/uts/common/gssapi/mechs/krb5/mech/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/mech/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(SRC)/uts/common/gssapi/mechs/krb5/crypto/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/crypto/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(SRC)/uts/common/gssapi/mechs/krb5/crypto/des/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/crypto/des/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/crypto/md5/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/crypto/crc32/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(SRC)/uts/common/gssapi/mechs/krb5/crypto/os/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/crypto/os/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)


objs/%.o profs/%.o pics/%.o: $(REL_PATH)/et/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)


objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/error_tables/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/asn.1/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/ccache/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/ccache/file/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/ccache/stdio/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/ccache/memory/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(SRC)/uts/common/gssapi/mechs/krb5/krb5/free/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/free/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/keytab/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/keytab/file/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(SRC)/uts/common/gssapi/mechs/krb5/krb5/krb/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/krb/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(SRC)/uts/common/gssapi/mechs/krb5/krb5/os/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/os/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/posix/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/krb5/rcache/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: $(REL_PATH)/profile/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include $(REL_PATH)/../../Makefile.targ

lint: $(SRCS:.c=.ln) $(LINTLIB)
