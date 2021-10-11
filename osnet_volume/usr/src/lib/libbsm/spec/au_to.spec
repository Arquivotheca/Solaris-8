#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)au_to.spec	1.4	99/10/14 SMI"
#
# lib/libbsm/spec/au_to.spec

function	au_to_arg32
include		<bsm/libbsm.h>
declaration	token_t *au_to_arg32(char n, char *text, uint32_t v)
version		SUNW_1.2
exception	($return == 0)
end

function	au_to_arg64
include		<bsm/libbsm.h>
declaration	token_t *au_to_arg64(char n, char *text, uint64_t v)
version		SUNW_1.2
end

function	au_to_arg
weak		au_to_arg32
version		SUNW_0.7
end

function	au_to_attr
include		<bsm/libbsm.h>
declaration	token_t *au_to_attr(struct vattr *attr)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_cmd
include		<bsm/libbsm.h>
declaration	token_t *au_to_cmd(uint_t Argc, char **Argv, char **envp)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_data
include		<bsm/libbsm.h>
declaration	token_t *au_to_data(char unit_print, char unit_type, \
			char	unit_count, char *p)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_groups
include		<bsm/libbsm.h>
declaration	token_t *au_to_groups(int *groups)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_in_addr
include		<bsm/libbsm.h>
declaration	token_t *au_to_in_addr(struct in_addr *internet_addr)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_ipc
include		<bsm/libbsm.h>
declaration	token_t *au_to_ipc(int id)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_iport
include		<bsm/libbsm.h>
declaration	token_t *au_to_iport(ushort_t iport)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_me
include		<bsm/libbsm.h>
declaration	token_t *au_to_me(void)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_newgroups
include		<bsm/libbsm.h>
declaration	token_t *au_to_newgroups(int n, gid_t *groups)
version		SUNW_0.8
exception	($return == 0)
end

function	au_to_opaque
include		<bsm/libbsm.h>
declaration	token_t *au_to_opaque(char *data, short bytes)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_path
include		<bsm/libbsm.h>
declaration	token_t *au_to_path(char *path)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_process
include		<bsm/libbsm.h>
declaration	token_t *au_to_process (au_id_t auid, uid_t euid, \
			gid_t egid, uid_t ruid, gid_t rgid, pid_t pid, \
			au_asid_t sid, au_tid_t *tid)
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_return32
include		<bsm/libbsm.h>
declaration	token_t *au_to_return32(char number, uint32_t value)
version		SUNW_1.2
exception	($return == 0)
end

function	au_to_return64
include		<bsm/libbsm.h>
declaration	token_t *au_to_return64(char number, uint64_t value)
version		SUNW_1.2
exception	($return == 0)
end

function	au_to_return
weak		au_to_return32
version		SUNW_0.7
end

function	au_to_socket
include		<bsm/libbsm.h>
declaration	token_t *au_to_socket(struct oldsocket *so);
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_subject
include		<bsm/libbsm.h>
declaration	token_t *au_to_subject(au_id_t auid, uid_t euid, \
			gid_t egid, uid_t ruid, gid_t rgid,  pid_t pid, \
			au_asid_t sid, au_tid_t *tid )
version		SUNW_0.7
exception	($return == 0)
end

function	au_to_subject_ex
include		<bsm/libbsm.h>
declaration	token_t *au_to_subject_ex(au_id_t auid, uid_t euid, \
			gid_t egid, uid_t ruid, gid_t rgid,  pid_t pid, \
			au_asid_t sid, au_tid_addr_t *tid )
version		SUNW_1.2
exception	($return == 0)
end

function	au_to_text
include		<bsm/libbsm.h>
declaration	token_t *au_to_text(char *text);
version		SUNW_0.7
exception	($return == 0)
end
