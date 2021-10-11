#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)private.spec	1.2	99/10/14 SMI"
#
# lib/libbsm/spec/private.spec

function	adr_char
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adr_char(adr_t *adr, char *cp, int count);
version		SUNWprivate_1.1
end		

function	adr_count
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adr_count(adr_t *adr)
version		SUNWprivate_1.1
end		

function	adr_int32
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adr_int32(adr_t *adr, int32_t *lp, int count)
version		SUNWprivate_1.1
end		

function	adr_int64
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adr_int64(adr_t *adr, int64_t *lp, int count)
version		SUNWprivate_1.1
end		

function	adr_short
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adr_short(adr_t *adr, short *sp, int count)
version		SUNWprivate_1.1
end		

function	adr_start
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adr_start(adr_t *adr, char *p)
version		SUNWprivate_1.1
end		

function	adrf_char
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_char(adr_t *adr, char *cp, int count)
version		SUNWprivate_1.1
end		

function	adrf_int32
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_int32(adr_t *adr, int32_t *cp, int count)
version		SUNWprivate_1.1
end		

function	adrf_int64
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_int64(adr_t *adr, int64_t *lp, int count)
version		SUNWprivate_1.1
end		

function	adrf_opaque
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_opaque(adr_t *adr, char *p)
version		SUNWprivate_1.1
end		

function	adrf_peek
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_peek(adr_t *adr)
version		SUNWprivate_1.1
end		

function	adrf_short
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_short(adr_t *adr, short *sp, int count)
version		SUNWprivate_1.1
end		

function	adrf_start
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrf_start(adr_t *adr, FILE *fp)
version		SUNWprivate_1.1
end		

function	adrf_string
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_string(adr_t *adr, char *p)
version		SUNWprivate_1.1
end		

function	adrf_u_char
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_u_char(adr_t *adr, uchar_t *cp, int count)
version		SUNWprivate_1.1
end		

function	adrf_u_int32
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_u_int32(adr_t *adr, uint32_t *cp, int count)
version		SUNWprivate_1.1
end		

function	adrf_u_int64
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_u_int64(adr_t *adr, uint64_t *lp, int count)
version		SUNWprivate_1.1
end		

function	adrf_u_short
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int adrf_u_short(adr_t *adr, ushort_t *sp, int count)
version		SUNWprivate_1.1
end		

function	adrm_char
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_char(adr_t *adr, char *cp, int count)
version		SUNWprivate_1.1
end		

function	adrm_int32
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_int32(adr_t *adr, int32_t *cp, int count)
version		SUNWprivate_1.1
end		

function	adrm_int64
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_int64(adr_t *adr, int64_t *lp, int count)
version		SUNWprivate_1.1
end		

function	adrm_opaque
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_opaque(adr_t *adr, char *p)
version		SUNWprivate_1.1
end		

function	adrm_short
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_short(adr_t *adr, short *sp, int count)
version		SUNWprivate_1.1
end		

function	adrm_start
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_start(adr_t *adr, char *p)
version		SUNWprivate_1.1
end		

function	adrm_string
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_string(adr_t *adr, char *p)
version		SUNWprivate_1.1
end		

function	adrm_u_char
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_u_char(adr_t *adr, uchar_t *cp, int count)
version		SUNWprivate_1.1
end		

function	adrm_u_int32
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_u_int32(adr_t *adr, uint32_t *cp, int count)
version		SUNWprivate_1.1
end		

function	adrm_u_int64
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_u_int64(adr_t *adr, uint64_t *cp, int count)
version		SUNWprivate_1.1
end		

function	adrm_u_short
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void adrm_u_short(adr_t *adr, ushort_t *sp, int count)
version		SUNWprivate_1.1
end		

function	au_to_exec_args
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_exec_args(char **argv)
version		SUNWprivate_1.1
end		

function	au_to_exec_env
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_exec_env(char **envp)
version		SUNWprivate_1.1
end		

function	au_to_exit
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_exit(int retval, int err)
version		SUNWprivate_1.1
end		

function	au_to_header
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_header(au_event_t e_type, au_emod_t e_mod)
version		SUNWprivate_1.1
end		

function	au_to_seq
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_seq(int audit_count)
version		SUNWprivate_1.1
end		

function	au_to_trailer
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_trailer(void)
version		SUNWprivate_1.1
end		

function	au_to_xatom
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_xatom(ushort_t len, char *atom)
version		SUNWprivate_1.1
end		

function	au_to_xobj
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_xobj(int oid, int xid, int cuid)
version		SUNWprivate_1.1
end		

function	au_to_xproto
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_xproto(pid_t pid)
version		SUNWprivate_1.1
end		

function	au_to_xselect
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	token_t *au_to_xselect(char *pstring, char *type, \
			short dlen, char *data)
version		SUNWprivate_1.1
end		

function	audit_allocate_argv
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_allocate_argv(int flg, int argc, char *argv[])
version		SUNWprivate_1.1
end		

function	audit_allocate_device
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_allocate_device(char *path)
version		SUNWprivate_1.1
end		

function	audit_allocate_list
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_allocate_list(char *list)
version		SUNWprivate_1.1
end		

function	audit_allocate_record
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_allocate_record(char status)
version		SUNWprivate_1.1
end		

function	audit_cron_session
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_cron_session(char *nam, uid_t uid)
version		SUNWprivate_1.1
end		

function	audit_ftpd_bad_pw
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_ftpd_bad_pw(char *uname)
version		SUNWprivate_1.1
end		

function	audit_ftpd_excluded
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_ftpd_excluded(char *uname)
version		SUNWprivate_1.1
end		

function	audit_ftpd_failure
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_ftpd_failure(char *uname)
version		SUNWprivate_1.1
end		

function	audit_ftpd_no_anon
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_ftpd_no_anon(void)
version		SUNWprivate_1.1
end		

function	audit_ftpd_sav_data
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_ftpd_sav_data(struct sockaddr_in *sin, int port)
version		SUNWprivate_1.1
end		

function	audit_ftpd_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_ftpd_success(char *uname)
version		SUNWprivate_1.1
end		

function	audit_ftpd_unknown
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_ftpd_unknown(char *uname)
version		SUNWprivate_1.1
end		

function	audit_halt_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_halt_fail(void)
version		SUNWprivate_1.1
end		

function	audit_halt_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_halt_setup(int argc, char **argv)
version		SUNWprivate_1.1
end		

function	audit_halt_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_halt_success(void)
version		SUNWprivate_1.1
end		

function	audit_inetd_config
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_inetd_config(void)
version		SUNWprivate_1.1
end		

function	audit_inetd_termid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_inetd_termid(int)
version		SUNWprivate_1.1
end		

function	audit_inetd_service
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_inetd_service(char *service_name, struct passwd *pwd)
version		SUNWprivate_1.1
end		

function	audit_init_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_init_fail(void)
version		SUNWprivate_1.1
end		

function	audit_init_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_init_setup(int argc, char **argv)
version		SUNWprivate_1.1
end		

function	audit_init_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_init_success(void)
version		SUNWprivate_1.1
end		

function	audit_uadmin_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_uadmin_setup(int argc, char **argv)
version		SUNWprivate_1.1
end		

function	audit_uadmin_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_uadmin_success(void)
version		SUNWprivate_1.1
end		

function	audit_settid
include		<sys/socket.h>, <netinet/in.h>, <strings.h>, <bsm/libbsm.h>
declaration	int audit_settid(int fd)
version		SUNWprivate_1.1
end

function	audit_login_bad_dialup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_bad_dialup(void)
version		SUNWprivate_1.1
end		

function	audit_login_bad_pw
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_bad_pw(void)
version		SUNWprivate_1.1
end		

function	audit_login_maxtrys
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_maxtrys(void)
version		SUNWprivate_1.1
end		

function	audit_login_not_console
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_not_console(void)
version		SUNWprivate_1.1
end		

function	audit_login_save_flags
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_save_flags(int rflag, int hflag)
version		SUNWprivate_1.1
end		

function	audit_login_save_host
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_save_host(char *host)
version		SUNWprivate_1.1
end		

function	audit_login_save_machine
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_save_machine(void)
version		SUNWprivate_1.1
end		

function	audit_login_save_port
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_save_port(void)
version		SUNWprivate_1.1
end		

function	audit_login_save_pw
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_save_pw(struct passwd *pwd)
version		SUNWprivate_1.1
end		

function	audit_login_save_ttyn
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_save_ttyn(char *ttyn)
version		SUNWprivate_1.1
end		

function	audit_login_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_login_success(void)
version		SUNWprivate_1.1
end		

function	audit_mountd_mount
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_mountd_mount(char *clname, char *path, int success)
version		SUNWprivate_1.1
end		

function	audit_mountd_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_mountd_setup(void)
version		SUNWprivate_1.1
end		

function	audit_mountd_umount
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_mountd_umount(char *clname, char *path)
version		SUNWprivate_1.1
end		

function	audit_passwd_attributes_sorf
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_passwd_attributes_sorf(int retval)
version		SUNWprivate_1.1
end		

function	audit_passwd_init_id
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_passwd_init_id(void)
version		SUNWprivate_1.1
end		

function	audit_passwd_sorf
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_passwd_sorf(int retval)
version		SUNWprivate_1.1
end		

function	audit_reboot_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_reboot_fail(void)
version		SUNWprivate_1.1
end		

function	audit_reboot_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_reboot_setup(void)
version		SUNWprivate_1.1
end		

function	audit_reboot_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_reboot_success(void)
version		SUNWprivate_1.1
end		

function	audit_rexd_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_rexd_fail(char *msg, char *hostname, char *user, \
			uid_t uid, gid_t gid, char *shell, char **cmdbuf)
version		SUNWprivate_1.1
end		

function	audit_rexd_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_rexd_setup(void)
version		SUNWprivate_1.1
end		

function	audit_rexd_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_rexd_success(char *hostname, char *user, \
			uid_t uid, gid_t gid, char *shell, char **cmdbuf)
version		SUNWprivate_1.1
end		

function	audit_rexecd_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_rexecd_fail(char *msg, char *hostname, char \
			*user, char *cmdbuf)
version		SUNWprivate_1.1
end		

function	audit_rexecd_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_rexecd_setup(void)
version		SUNWprivate_1.1
end		

function	audit_rexecd_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_rexecd_success(char *hostname, char *user, char \
			*cmdbuf)
version		SUNWprivate_1.1
end		

function	audit_rshd_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_rshd_fail(char *msg, char *hostname, char \
			*remuser, char *locuser, char *cmdbuf)
version		SUNWprivate_1.1
end		

function	audit_rshd_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_rshd_setup(void)
version		SUNWprivate_1.1
end		

function	audit_rshd_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_rshd_success(char *hostname, char *remuser, char \
			*locuser, char *cmdbuf)
version		SUNWprivate_1.1
end		

function	audit_shutdown_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_shutdown_fail(void)
version		SUNWprivate_1.1
end		

function	audit_shutdown_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_shutdown_setup(int argc, char **argv)
version		SUNWprivate_1.1
end		

function	audit_shutdown_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_shutdown_success(void)
version		SUNWprivate_1.1
end		

function	audit_su_bad_authentication
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_su_bad_authentication(void)
version		SUNWprivate_1.1
end		

function	audit_su_bad_uid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_su_bad_uid(void)
version		SUNWprivate_1.1
end		

function	audit_su_bad_username
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_su_bad_username(void)
version		SUNWprivate_1.1
end		

function	audit_su_init_info
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_su_init_info(char *username, char *ttyn)
version		SUNWprivate_1.1
end		

function	audit_su_reset_ai
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int audit_su_reset_ai(void)
version		SUNWprivate_1.1
end		

function	audit_su_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_su_success(void)
version		SUNWprivate_1.1
end		

function	audit_su_unknown_failure
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void audit_su_unknown_failure(void)
version		SUNWprivate_1.1
end		

function	aug_audit
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int aug_audit(void)
version		SUNWprivate_1.1
end		

function	aug_get_machine
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int aug_get_machine(char *hostname)
version		SUNWprivate_1.1
end		

function	aug_get_port
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	dev_t aug_get_port(void)
version		SUNWprivate_1.1
end		

function	aug_init
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_init(void)
version		SUNWprivate_1.1
end		

function	aug_na_selected
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int aug_na_selected(void)
version		SUNWprivate_1.1
end		

function	aug_save_afunc
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_afunc(int (*afunc)())
version		SUNWprivate_1.1
end		

function	aug_save_asid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_asid(au_asid_t id)
version		SUNWprivate_1.1
end		

function	aug_save_auid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_auid(au_id_t id)
version		SUNWprivate_1.1
end		

function	aug_save_egid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_egid(gid_t id)
version		SUNWprivate_1.1
end		

function	aug_save_euid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_euid(uid_t id)
version		SUNWprivate_1.1
end		

function	aug_save_event
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_event(au_event_t id)
version		SUNWprivate_1.1
end		

function	aug_save_gid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_gid(gid_t id)
version		SUNWprivate_1.1
end		

function	aug_save_me
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int aug_save_me(void)
version		SUNWprivate_1.1
end		

function	aug_save_na
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_na(int flag)
version		SUNWprivate_1.1
end		

function	aug_save_namask
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int aug_save_namask(void)
version		SUNWprivate_1.1
end		

function	aug_save_path
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_path(char *s)
version		SUNWprivate_1.1
end		

function	aug_save_pid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_pid(pid_t id)
version		SUNWprivate_1.1
end		

function	aug_save_policy
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int aug_save_policy(void)
version		SUNWprivate_1.1
end		

function	aug_save_sorf
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_sorf(int sorf)
version		SUNWprivate_1.1
end		

function	aug_save_text
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_text(char *s)
version		SUNWprivate_1.1
end		

function	aug_save_tid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_tid(dev_t port, uint_t machine)
version		SUNWprivate_1.1
end		

function	aug_save_uid
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_uid(uid_t id)
version		SUNWprivate_1.1
end		

function	aug_save_user
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void aug_save_user(char *s)
version		SUNWprivate_1.1
end		

function	aug_selected
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int aug_selected(void)
version		SUNWprivate_1.1
end		

function	cacheauclass
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int cacheauclass(au_class_ent_t **result, au_class_t class_no)
version		SUNWprivate_1.1
end		

function	cacheauclassnam
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int cacheauclassnam(au_class_ent_t **result, char *class_name)
version		SUNWprivate_1.1
end		

function	cacheauevent
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int cacheauevent(au_event_ent_t **result, au_event_t event_number)
version		SUNWprivate_1.1
end		

function	cannot_audit
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	int cannot_audit(int force)
version		SUNWprivate_1.1
end		

function	enddaent
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void enddaent(void)
version		SUNWprivate_1.1
end		

function	enddmapent
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void enddmapent(void)
version		SUNWprivate_1.1
end		

function	getdadfield
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	char *getdadfield(char *ptr)
version		SUNWprivate_1.1
end		

function	getdaent
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	devalloc_t *getdaent(void)
version		SUNWprivate_1.1
end		

function	getdafield
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	char *getdafield(char *ptr)
version		SUNWprivate_1.1
end		

function	getdanam
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	devalloc_t *getdanam(char *name)
version		SUNWprivate_1.1
end		

function	getdatype
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	devalloc_t *getdatype(char *tp)
version		SUNWprivate_1.1
end		

function	getdmapdev
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	devmap_t *getdmapdev(char *name)
version		SUNWprivate_1.1
end		

function	getdmapdfield
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	char *getdmapdfield(char *ptr)
version		SUNWprivate_1.1
end		

function	getdmapent
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	devmap_t *getdmapent(void)
version		SUNWprivate_1.1
end		

function	getdmapfield
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	char *getdmapfield(char *ptr)
version		SUNWprivate_1.1
end		

function	getdmapnam
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	devmap_t *getdmapnam(char *name)
version		SUNWprivate_1.1
end		

function	getdmaptype
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	devmap_t *getdmaptype(char *tp)
version		SUNWprivate_1.1
end		

function	setdaent
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void setdaent(void)
version		SUNWprivate_1.1
end		

function	setdafile
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void setdafile(char *file)
version		SUNWprivate_1.1
end		

function	setdmapent
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void setdmapent(void)
version		SUNWprivate_1.1
end		

function	setdmapfile
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
declaration	void setdmapfile(char *file)
version		SUNWprivate_1.1
end		

function	audit_at_create
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_at_delete
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_bad_user
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_create_anc_file
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_delete_anc_file
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_is_anc_name
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_mode
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_new_job
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_setinfo
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_cron_user_acct_expired
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_crontab_delete
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_crontab_modify
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_delete_user_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_delete_user_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_user_create_event
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_user_dde_event_setup
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_user_modify_event
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_users_modified_by_group_fail
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

function	audit_users_modified_by_group_success
include		<sys/types.h>, <bsm/audit.h>, <bsm/libbsm.h>, <bsm/audit_record.h>, <bsm/devices.h>, <pwd.h>
version		SUNWprivate_1.1
end		

