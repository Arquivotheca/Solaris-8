/*
 * Copyright (c) 1988 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)token.c	1.28	99/12/06 SMI"

/*
 * Token processing for auditreduce.
 */

#include <locale.h>
#include "auditr.h"

/*
 * These tokens are the same in CMW 1.0, 4.1.X C2-BSM and 5.X C2-BSM
 */
static int	arbitrary_data_token();
static int	argument32_token();
static int	argument64_token();
static int	file_token();
static int	group_token();
static int	header_token();
static int	ip_addr_token();
static int	ip_addr_token_ex();
static int	ip_token();
static int	iport_token();
static int	opaque_token();
static int	return_value32_token();
static int	return_value64_token();
static int	s5_IPC_token();
static int	sequence_token();
static int	server_token();
static int	text_token();
static int	trailer_token();
static int	attribute_token();
static int	attribute32_token();
static int	attribute64_token();
static int	acl_token();
static int	cmd_token();
static int	exit_token();
static int	liaison_token();
static int	path_token();
static int	process32_token();
static int	process32_token_ex();
static int	process64_token_ex();
static int	process64_token();
static int	s5_IPC_perm_token();
static int	socket_token();
static int	socket_ex_token();
static int	subject32_token();
static int	subject32_token_ex();
static int	subject64_token();
static int	subject64_token_ex();
static int	xatom_token();
static int	xobj_token();
static int	xproto_token();
static int	xselect_token();
static int	old_header_token();

static int	newgroup_token();
static int	exec_args_token();
static int	exec_env_token();


static void	anchor_path();
static char	*collapse_path();
static int	ipc_type_match();
static void	skip_string();
static void	get_string();

struct token_desc {
	char	*t_name;	/* name of the token */
	int	tokenid;	/* token type */
	int	t_fieldcount;	/* number of fields in this token */
	int	(*func)();	/* token processing function */
	char	*t_fields[16];	/* The fields to watch */
};
typedef struct token_desc token_desc_t;

static token_desc_t tokentable[] = {
	{ "argument32",	AUT_ARG32, 	3, 	argument32_token, 	},
	{ "argument64",	AUT_ARG64, 	3, 	argument64_token, 	},
	{ "acl",	AUT_ACL,	3,	acl_token,		},
	{ "attr", 	AUT_ATTR, 	6, 	attribute_token, 	},
	{ "attr32", 	AUT_ATTR32, 	6, 	attribute32_token, 	},
	{ "attr64", 	AUT_ATTR64, 	6, 	attribute64_token, 	},
	{ "cmd", 	AUT_CMD, 	2, 	cmd_token, 		},
	{ "data", 	AUT_DATA, 	4, 	arbitrary_data_token, 	},
	{ "exec_args", 	AUT_EXEC_ARGS, 	-1, 	exec_args_token, 	},
	{ "exec_env", 	AUT_EXEC_ENV, 	-1, 	exec_env_token, 	},
	{ "exit", 	AUT_EXIT, 	2, 	exit_token, 		},
	{ "groups", 	AUT_GROUPS, 	16, 	group_token, 		},
	{ "header32", 	AUT_HEADER32, 	4, 	header_token,		},
	{ "header32_ex", AUT_HEADER32_EX, 6,	header_token,		},
	{ "header64", 	AUT_HEADER64, 	4, 	header_token,		},
	{ "header64_ex", AUT_HEADER64_EX, 6, 	header_token,		},
	{ "in_addr", 	AUT_IN_ADDR, 	1, 	ip_addr_token, 		},
	{ "in_addr_ex", AUT_IN_ADDR_EX, 1, 	ip_addr_token_ex,	},
	{ "ip", 	AUT_IP,		10, 	ip_token,		},
	{ "ipc", 	AUT_IPC, 	1, 	s5_IPC_token, 		},
	{ "ipc_perm", 	AUT_IPC_PERM, 	8, 	s5_IPC_perm_token, 	},
	{ "iport", 	AUT_IPORT, 	1, 	iport_token, 		},
	{ "liaison", 	AUT_LIAISON, 	1, 	liaison_token, 		},
	{ "newgroups", 	AUT_NEWGROUPS, 	-1, 	newgroup_token, 	},
	{ "old_header",	AUT_OHEADER, 	1, 	old_header_token, 	},
	{ "opaque", 	AUT_OPAQUE, 	2, 	opaque_token, 		},
	{ "other_file32", AUT_OTHER_FILE32,	3, 	file_token, 	},
	{ "other_file64", AUT_OTHER_FILE64,	3, 	file_token, 	},
	{ "path", 	AUT_PATH, 	3, 	path_token, 		},
	{ "process32", 	AUT_PROCESS32, 	5, 	process32_token,	},
	{ "process32_ex", AUT_PROCESS32_EX, 5, 	process32_token_ex,	},
	{ "process64", 	AUT_PROCESS64, 	5, 	process64_token,	},
	{ "process64_ex", AUT_PROCESS64_EX, 5, 	process64_token_ex,	},
	{ "return32", 	AUT_RETURN32, 	2, 	return_value32_token, 	},
	{ "return64", 	AUT_RETURN64, 	2, 	return_value64_token, 	},
	{ "sequence", 	AUT_SEQ, 	1, 	sequence_token, 	},
	{ "server32", 	AUT_SERVER32, 	5, 	server_token, 		},
	{ "server64", 	AUT_SERVER64, 	5, 	server_token, 		},
	{ "socket", 	AUT_SOCKET, 	3, 	socket_token, 		},
	{ "socket", 	AUT_SOCKET_EX, 	8, 	socket_ex_token,	},
	{ "subject32", 	AUT_SUBJECT32, 	5, 	subject32_token,	},
	{ "subject32_ex", AUT_SUBJECT32_EX, 5, 	subject32_token_ex,	},
	{ "subject64", 	AUT_SUBJECT64, 	5, 	subject64_token,	},
	{ "subject64_ex", AUT_SUBJECT64_EX, 5, 	subject64_token_ex,	},
	{ "text", 	AUT_TEXT, 	1, 	text_token, 		},
	{ "trailer", 	AUT_TRAILER, 	2, 	trailer_token, 		},
	{ "xatom", 	AUT_XATOM, 	2, 	xatom_token, 		},
	{ "xobj", 	AUT_XOBJ, 	4, 	xobj_token, 		},
	{ "xproto", 	AUT_XPROTO, 	1, 	xproto_token, 		},
	{ "xselect", 	AUT_XSELECT, 	4, 	xselect_token, 		},
};

static int	numtokenentries = sizeof (tokentable) / sizeof (token_desc_t);


/*
 *  Process a token in a record to determine whether the record
 *  is interesting.
 */

int
token_processing(adr, tokenid)
adr_t	*adr;
int	tokenid;
{
	int	i;
	token_desc_t *k;
	int	rc;

	for (i = 0; i < numtokenentries; i++) {
		k = &(tokentable[i]);
		if ((k->tokenid) == tokenid) {
#if AUDIT_REC
			(void) fprintf(stderr, "token_processing: %s\n",
			    tokentable[i].t_name);
#endif
			rc = (*tokentable[i].func)(adr);
			return (rc);
		}
	}
	/* here if token id is not in table */
#if AUDIT_REC
	(void) fprintf(stderr, "token_processing: token %d not found\n",
	    tokenid);
#endif
	return (-2);
}


/*
 * There should not be any file or header tokens in the middle of
 * a record
 */

/*ARGSUSED*/
int
file_token(adr)
adr_t	*adr;
{
	return (-2);
}


/*ARGSUSED*/
int
header_token(adr)
adr_t	*adr;
{
	return (-2);
}


/*ARGSUSED*/
int
old_header_token(adr)
adr_t	*adr;
{
	return (-2);
}


/*
 * ======================================================
 *  The following token processing routines return
 *  -1: if the record is not interesting
 *  -2: if an error is found
 * ======================================================
 */

int
trailer_token(adr)
adr_t	*adr;
{
	short	magic_number;
	uint32_t bytes;

	adrm_u_short(adr, (ushort_t *)&magic_number, 1);
	if (magic_number != AUT_TRAILER_MAGIC) {
		(void) fprintf(stderr, "%s\n",
			gettext("auditreduce: Bad trailer token"));
		return (-2);
	}
	adrm_u_int32(adr, &bytes, 1);

	return (-1);
}


/*
 * Format of arbitrary data token:
 *	arbitrary data token id	adr char
 * 	how to print		adr_char
 *	basic unit		adr_char
 *	unit count		adr_char, specifying number of units of
 *	data items		depends on basic unit
 *
 */
int
arbitrary_data_token(adr)
adr_t	*adr;
{
	int	i;
	char	c1;
	short	c2;
	int32_t	c3;
	int64_t c4;
	char	how_to_print, basic_unit, unit_count;

	/* get how_to_print, basic_unit, and unit_count */
	adrm_char(adr, &how_to_print, 1);
	adrm_char(adr, &basic_unit, 1);
	adrm_char(adr, &unit_count, 1);
	for (i = 0; i < unit_count; i++) {
		switch (basic_unit) {
			/* case AUR_BYTE: has same value as AUR_CHAR */
		case AUR_CHAR:
			adrm_char(adr, &c1, 1);
			break;
		case AUR_SHORT:
			adrm_short(adr, &c2, 1);
			break;
		case AUR_INT32:
			adrm_int32(adr, (int32_t *)&c3, 1);
			break;
		case AUR_INT64:
			adrm_int64(adr, (int64_t *)&c4, 1);
			break;
		default:
			return (-2);
			break;
		}
	}
	return (-1);
}


/*
 * Format of opaque token:
 *	opaque token id		adr_char
 *	size			adr_short
 *	data			adr_char, size times
 *
 */
int
opaque_token(adr)
adr_t	*adr;
{
	skip_string(adr);
	return (-1);
}



/*
 * Format of return32 value token:
 * 	return value token id	adr_char
 *	error number		adr_char
 *	return value		adr_u_int32
 *
 */
int
return_value32_token(adr)
adr_t	*adr;
{
	char		errnum;
	uint32_t	value;

	adrm_char(adr, &errnum, 1);
	adrm_u_int32(adr, &value, 1);
	if ((flags & M_SORF) &&
		((global_class & mask.am_success) && (errnum == 0)) ||
		((global_class & mask.am_failure) && (errnum != 0))) {
			checkflags |= M_SORF;
	}
	return (-1);
}

/*
 * Format of return64 value token:
 * 	return value token id	adr_char
 *	error number		adr_char
 *	return value		adr_u_int64
 *
 */
int
return_value64_token(adr)
adr_t	*adr;
{
	char		errnum;
	uint64_t	value;

	adrm_char(adr, &errnum, 1);
	adrm_u_int64(adr, &value, 1);
	if ((flags & M_SORF) &&
		((global_class & mask.am_success) && (errnum == 0)) ||
		((global_class & mask.am_failure) && (errnum != 0))) {
			checkflags |= M_SORF;
	}
	return (-1);
}


/*
 * Format of sequence token:
 *	sequence token id	adr_char
 *	audit_count		int32_t
 *
 */
int
sequence_token(adr)
adr_t	*adr;
{
	int32_t	audit_count;

	adrm_int32(adr, &audit_count, 1);
	return (-1);
}


/*
 * Format of server token:
 *	server token id		adr_char
 *	auid			adr_u_short
 *	euid			adr_u_short
 *	ruid			adr_u_short
 *	egid			adr_u_short
 *	pid			adr_u_short
 *
 */
int
server_token(adr)
adr_t	*adr;
{
	unsigned short	auid, euid, ruid, egid, pid;

	adrm_u_short(adr, &auid, 1);
	adrm_u_short(adr, &euid, 1);
	adrm_u_short(adr, &ruid, 1);
	adrm_u_short(adr, &egid, 1);
	adrm_u_short(adr, &pid, 1);

	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags |= M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags |= M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags |= M_USERR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags |= M_GROUPE;
	}
	return (-1);
}


/*
 * Format of text token:
 *	text token id		adr_char
 * 	text			adr_string
 *
 */
int
text_token(adr)
adr_t	*adr;
{
	skip_string(adr);
	return (-1);
}


/*
 * Format of ip_addr token:
 *	ip token id	adr_char
 *	address		adr_int32
 *
 */
int
ip_addr_token(adr)
adr_t	*adr;
{
	int32_t	address;

	adrm_char(adr, (char *)&address, 4);

	return (-1);
}

/*
 * Format of ip_addr_ex token:
 *	ip token id	adr_char
 *	ip type		adr_int32
 *	address		4*adr_int32
 *
 */
int
ip_addr_token_ex(adr)
adr_t	*adr;
{
	int32_t	address[4];
	int32_t type;

	adrm_int32(adr, (int32_t *)&type, 1);
	adrm_int32(adr, (int32_t *)&address, 4);

	return (-1);
}

/*
 * Format of ip token:
 *	ip header token id	adr_char
 *	version			adr_char
 *	type of service		adr_char
 *	length			adr_short
 *	id			adr_u_short
 *	offset			adr_u_short
 *	ttl			adr_char
 *	protocol		adr_char
 *	checksum		adr_u_short
 *	source address		adr_int32
 *	destination address	adr_int32
 *
 */
int
ip_token(adr)
adr_t	*adr;
{
	char	version;
	char	type;
	short	len;
	unsigned short	id, offset, checksum;
	char	ttl, protocol;
	int32_t	src, dest;

	adrm_char(adr, &version, 1);
	adrm_char(adr, &type, 1);
	adrm_short(adr, &len, 1);
	adrm_u_short(adr, &id, 1);
	adrm_u_short(adr, &offset, 1);
	adrm_char(adr, &ttl, 1);
	adrm_char(adr, &protocol, 1);
	adrm_u_short(adr, &checksum, 1);
	adrm_char(adr, (char *)&src, 4);
	adrm_char(adr, (char *)&dest, 4);

	return (-1);
}


/*
 * Format of iport token:
 *	ip port address token id	adr_char
 *	port address			adr_short
 *
 */
int
iport_token(adr)
adr_t	*adr;
{
	short	address;

	adrm_short(adr, &address, 1);

	return (-1);
}


/*
 * Format of groups token:
 *	group token id		adr_char
 *	group list		adr_int32, 16 times
 *
 */
int
group_token(adr)
adr_t	*adr;
{
	int	gid[16];
	int	i;
	int	flag = 0;

	for (i = 0; i < 16; i++) {
		adrm_int32(adr, (int32_t *)&gid[i], 1);
		if (flags & M_GROUPR) {
			if ((unsigned short)m_groupr == gid[i])
				flag = 1;
		}
	}

	if (flags & M_GROUPR) {
		if (flag)
			checkflags |= M_GROUPR;
	}
	return (-1);
}

/*
 * Format of newgroups token:
 *	group token id		adr_char
 *	number of groups	adr_short
 *	group list		adr_int32, "number" times
 *
 */
int
newgroup_token(adr)
adr_t	*adr;
{
	gid_t	gid;
	int	i;
	short int   number;

	adrm_short(adr, &number, 1);

	for (i = 0; i < number; i++) {
		adrm_int32(adr, (int32_t *)&gid, 1);
		if (flags & M_GROUPR) {
			if (m_groupr == gid)
				checkflags |= M_GROUPR;
		}
	}

	return (-1);
}

/*
 * Format of argument32 token:
 *	argument token id	adr_char
 *	argument number		adr_char
 *	argument value		adr_int32
 *	argument description	adr_string
 *
 */
int
argument32_token(adr)
adr_t	*adr;
{
	char	arg_num;
	int32_t	arg_val;

	adrm_char(adr, &arg_num, 1);
	adrm_int32(adr, &arg_val, 1);
	skip_string(adr);

	return (-1);
}

/*
 * Format of argument64 token:
 *	argument token id	adr_char
 *	argument number		adr_char
 *	argument value		adr_int64
 *	argument description	adr_string
 *
 */
int
argument64_token(adr)
adr_t	*adr;
{
	char	arg_num;
	int64_t	arg_val;

	adrm_char(adr, &arg_num, 1);
	adrm_int64(adr, &arg_val, 1);
	skip_string(adr);

	return (-1);
}

int
acl_token(adr)
adr_t	*adr;
{

	int32_t	id;
	int32_t	mode;
	int32_t	type;

	adrm_int32(adr, &type, 1);
	adrm_int32(adr, &id, 1);
	adrm_int32(adr, &mode, 1);

	return (-1);
}

/*
 * Format of attribute token: (old pre SunOS 5.7 format)
 *	attribute token id	adr_char
 * 	mode			adr_int32 (printed in octal)
 *	uid			adr_int32
 *	gid			adr_int32
 *	file system id		adr_int32
 *	node id			adr_int32
 *	device			adr_int32
 *
 */
int
attribute_token(adr)
adr_t	*adr;
{
	int32_t	dev;
	int32_t	file_sysid;
	int32_t	gid;
	int32_t	mode;
	int32_t	nodeid;
	int32_t	uid;

	adrm_int32(adr, &mode, 1);
	adrm_int32(adr, &uid, 1);
	adrm_int32(adr, &gid, 1);
	adrm_int32(adr, &file_sysid, 1);
	adrm_int32(adr, &nodeid, 1);
	adrm_int32(adr, &dev, 1);

	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == uid)
			checkflags |= M_USERE;
	}
	if (!new_mode && (flags & M_GROUPE)) {
		if (m_groupe == gid)
			checkflags |= M_GROUPE;
	}

	if (flags & M_OBJECT) {
		if ((obj_flag & OBJ_FGROUP) &&
		    (obj_group == gid))
			checkflags |= M_OBJECT;
		else if ((obj_flag & OBJ_FOWNER) &&
		    (obj_owner == uid))
			checkflags |= M_OBJECT;
	}
	return (-1);
}

/*
 * Format of attribute32 token:
 *	attribute token id	adr_char
 * 	mode			adr_int32 (printed in octal)
 *	uid			adr_int32
 *	gid			adr_int32
 *	file system id		adr_int32
 *	node id			adr_int64
 *	device			adr_int32
 *
 */
int
attribute32_token(adr)
adr_t	*adr;
{
	int32_t	dev;
	int32_t	file_sysid;
	int32_t	gid;
	int32_t	mode;
	int64_t	nodeid;
	int32_t	uid;

	adrm_int32(adr, &mode, 1);
	adrm_int32(adr, &uid, 1);
	adrm_int32(adr, &gid, 1);
	adrm_int32(adr, &file_sysid, 1);
	adrm_int64(adr, &nodeid, 1);
	adrm_int32(adr, &dev, 1);

	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == uid)
			checkflags |= M_USERE;
	}
	if (!new_mode && (flags & M_GROUPE)) {
		if (m_groupe == gid)
			checkflags |= M_GROUPE;
	}

	if (flags & M_OBJECT) {
		if ((obj_flag & OBJ_FGROUP) &&
		    (obj_group == gid))
			checkflags |= M_OBJECT;
		else if ((obj_flag & OBJ_FOWNER) &&
		    (obj_owner == uid))
			checkflags |= M_OBJECT;
	}
	return (-1);
}

/*
 * Format of attribute64 token:
 *	attribute token id	adr_char
 * 	mode			adr_int32 (printed in octal)
 *	uid			adr_int32
 *	gid			adr_int32
 *	file system id		adr_int32
 *	node id			adr_int64
 *	device			adr_int64
 *
 */
int
attribute64_token(adr)
adr_t	*adr;
{
	int64_t	dev;
	int32_t	file_sysid;
	int32_t	gid;
	int32_t	mode;
	int64_t	nodeid;
	int32_t	uid;

	adrm_int32(adr, &mode, 1);
	adrm_int32(adr, &uid, 1);
	adrm_int32(adr, &gid, 1);
	adrm_int32(adr, &file_sysid, 1);
	adrm_int64(adr, &nodeid, 1);
	adrm_int64(adr, &dev, 1);

	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == uid)
			checkflags |= M_USERE;
	}
	if (!new_mode && (flags & M_GROUPE)) {
		if (m_groupe == gid)
			checkflags |= M_GROUPE;
	}

	if (flags & M_OBJECT) {
		if ((obj_flag & OBJ_FGROUP) &&
		    (obj_group == gid))
			checkflags |= M_OBJECT;
		else if ((obj_flag & OBJ_FOWNER) &&
		    (obj_owner == uid))
			checkflags |= M_OBJECT;
	}
	return (-1);
}


/*
 * Format of command token:
 *	attribute token id	adr_char
 *	argc			adr_short
 *	argv len		adr_short	variable amount of argv len
 *	argv text		argv len	and text
 *	.
 *	.
 *	.
 *	envp count		adr_short	variable amount of envp len
 *	envp len		adr_short	and text
 *	envp text		envp		len
 *	.
 *	.
 *	.
 *
 */
int
cmd_token(adr)
adr_t	*adr;
{
	short	cnt;
	short	i;

	adrm_short(adr, &cnt, 1);

	for (i = 0; i < cnt; i++)
		skip_string(adr);

	adrm_short(adr, &cnt, 1);

	for (i = 0; i < cnt; i++)
		skip_string(adr);

	return (-1);
}


/*
 * Format of exit token:
 *	attribute token id	adr_char
 *	return value		adr_int32
 *	errno			adr_int32
 *
 */
int
exit_token(adr)
adr_t	*adr;
{
	int32_t	retval;
	int32_t	errno;

	adrm_int32(adr, &retval, 1);
	adrm_int32(adr, &errno, 1);
	return (-1);
}

/*
 * Format of exec_args token:
 *	attribute token id	adr_char
 *	count value		adr_int32
 *	strings			null terminated strings
 *
 */
int
exec_args_token(adr)
adr_t *adr;
{
	int count, i;
	char c;

	adrm_int32(adr, (int32_t *)&count, 1);
	for (i = 1; i <= count; i++) {
		adrm_char(adr, &c, 1);
		while (c != (char)0)
			adrm_char(adr, &c, 1);
	}
	/* no dump option here, since we will have variable length fields */
	return (-1);
}

/*
 * Format of exec_env token:
 *	attribute token id	adr_char
 *	count value		adr_int32
 *	strings			null terminated strings
 *
 */
int
exec_env_token(adr)
adr_t *adr;
{
	int count, i;
	char c;

	adrm_int32(adr, (int32_t *)&count, 1);
	for (i = 1; i <= count; i++) {
		adrm_char(adr, &c, 1);
		while (c != (char)0)
			adrm_char(adr, &c, 1);
	}
	/* no dump option here, since we will have variable length fields */
	return (-1);
}

/*
 * Format of liaison token:
 */
int
liaison_token(adr)
adr_t	*adr;
{
	int32_t	li;

	adrm_int32(adr, &li, 1);
	return (-1);
}


/*
 * Format of path token:
 *	path				adr_string
 */
int
path_token(adr)
adr_t 	*adr;
{
	if ((flags & M_OBJECT) && (obj_flag == OBJ_PATH)) {
		char *path;

		get_string(adr, &path);
		if (path[0] != '/')
			/*
			 * anchor the path. user apps may not do it.
			 */
			anchor_path(path);
		/*
		 * match against the collapsed path. that is what user sees.
		 */
		if (re_exec2(collapse_path(path)) == 1)
			checkflags |= M_OBJECT;
		free(path);
	} else {
		skip_string(adr);
	}
	return (-1);
}


/*
 * Format of System V IPC permission token:
 *	System V IPC permission token id	adr_char
 * 	uid					adr_int32
 *	gid					adr_int32
 *	cuid					adr_int32
 *	cgid					adr_int32
 *	mode					adr_int32
 *	seq					adr_int32
 *	key					adr_int32
 *	label					adr_opaque, sizeof (bslabel_t)
 *							    bytes
 */
int
s5_IPC_perm_token(adr)
adr_t	*adr;
{
	int32_t	uid, gid, cuid, cgid, mode, seq;
	int32_t	key;

	adrm_int32(adr, &uid, 1);
	adrm_int32(adr, &gid, 1);
	adrm_int32(adr, &cuid, 1);
	adrm_int32(adr, &cgid, 1);
	adrm_int32(adr, &mode, 1);
	adrm_int32(adr, &seq, 1);
	adrm_int32(adr, &key, 1);

	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == uid)
			checkflags |= M_USERE;
	}

	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == cuid)
			checkflags |= M_USERE;
	}

	if (!new_mode && (flags & M_GROUPR)) {
		if (m_groupr == gid)
			checkflags |= M_GROUPR;
	}

	if (!new_mode && (flags & M_GROUPR)) {
		if (m_groupr == cgid)
			checkflags |= M_GROUPR;
	}

	if ((flags & M_OBJECT) &&
	    ((obj_owner == uid) ||
	    (obj_owner == cuid) ||
	    (obj_group == gid) ||
	    (obj_group == cgid))) {

		switch (obj_flag) {
		case OBJ_MSGGROUP:
		case OBJ_MSGOWNER:
			if (ipc_type_match(OBJ_MSG, ipc_type))
				checkflags |= M_OBJECT;
			break;
		case OBJ_SEMGROUP:
		case OBJ_SEMOWNER:
			if (ipc_type_match(OBJ_SEM, ipc_type))
				checkflags |= M_OBJECT;
			break;
		case OBJ_SHMGROUP:
		case OBJ_SHMOWNER:
			if (ipc_type_match(OBJ_SHM, ipc_type))
				checkflags |= M_OBJECT;
			break;
		}
	}
	return (-1);
}


/*
 * Format of process32 token:
 *	process token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int32*2
 *
 */
int
process32_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int32_t port, machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int32(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (!new_mode && (flags & M_USERA)) {
		if (m_usera == auid)
			checkflags |= M_USERA;
	}
	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == euid)
			checkflags |= M_USERE;
	}
	if (!new_mode && (flags & M_USERR)) {
		if (m_userr == ruid)
			checkflags |= M_USERR;
	}
	if (!new_mode && (flags & M_GROUPR)) {
		if (m_groupr == rgid)
			checkflags |= M_GROUPR;
	}
	if (!new_mode && (flags & M_GROUPE)) {
		if (m_groupe == egid)
			checkflags |= M_GROUPE;
	}

	if (flags & M_OBJECT) {
		if ((obj_flag & OBJ_PROC) &&
		    (obj_id == pid)) {
			checkflags |= M_OBJECT;
		} else if ((obj_flag & OBJ_PGROUP) &&
		    ((obj_group == egid) ||
		    (obj_group == rgid))) {
			checkflags |= M_OBJECT;
		} else if ((obj_flag & OBJ_POWNER) &&
		    ((obj_owner == euid) ||
		    (obj_group == ruid))) {
			checkflags |= M_OBJECT;
		}
	}
	return (-1);
}

/*
 * Format of process32 token:
 *	process token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int32*6
 *
 */
int
process32_token_ex(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int32_t port, type, addr[4];

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int32(adr, &port, 1);
	adrm_int32(adr, &type, 1);
	adrm_int32(adr, &addr[0], 4);

	if (!new_mode && (flags & M_USERA)) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (!new_mode && (flags & M_USERR)) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (!new_mode && (flags & M_GROUPR)) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (!new_mode && (flags & M_GROUPE)) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}

	if (flags & M_OBJECT) {
		if ((obj_flag & OBJ_PROC) &&
		    (obj_id == pid)) {
			checkflags = checkflags | M_OBJECT;
		} else if ((obj_flag & OBJ_PGROUP) &&
		    ((obj_group == egid) ||
		    (obj_group == rgid))) {
			checkflags = checkflags | M_OBJECT;
		} else if ((obj_flag & OBJ_POWNER) &&
		    ((obj_owner == euid) ||
		    (obj_group == ruid))) {
			checkflags = checkflags | M_OBJECT;
		}
	}
	return (-1);
}

/*
 * Format of process64 token:
 *	process token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int64+adr_int32
 *
 */
int
process64_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int64_t port;
	int32_t machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int64(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (!new_mode && (flags & M_USERA)) {
		if (m_usera == auid)
			checkflags |= M_USERA;
	}
	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == euid)
			checkflags |= M_USERE;
	}
	if (!new_mode && (flags & M_USERR)) {
		if (m_userr == ruid)
			checkflags |= M_USERR;
	}
	if (!new_mode && (flags & M_GROUPR)) {
		if (m_groupr == rgid)
			checkflags |= M_GROUPR;
	}
	if (!new_mode && (flags & M_GROUPE)) {
		if (m_groupe == egid)
			checkflags |= M_GROUPE;
	}

	if (flags & M_OBJECT) {
		if ((obj_flag & OBJ_PROC) &&
		    (obj_id == pid)) {
			checkflags |= M_OBJECT;
		} else if ((obj_flag & OBJ_PGROUP) &&
		    ((obj_group == egid) ||
		    (obj_group == rgid))) {
			checkflags |= M_OBJECT;
		} else if ((obj_flag & OBJ_POWNER) &&
		    ((obj_owner == euid) ||
		    (obj_group == ruid))) {
			checkflags |= M_OBJECT;
		}
	}
	return (-1);
}

/*
 * Format of process64 token:
 *	process token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int64+5*adr_int32
 *
 */
int
process64_token_ex(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int64_t port;
	int32_t type, addr[4];

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int64(adr, &port, 1);
	adrm_int32(adr, &type, 1);
	adrm_int32(adr, &addr[0], 4);

	if (!new_mode && (flags & M_USERA)) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (!new_mode && (flags & M_USERE)) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (!new_mode && (flags & M_USERR)) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (!new_mode && (flags & M_GROUPR)) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (!new_mode && (flags & M_GROUPE)) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}

	if (flags & M_OBJECT) {
		if ((obj_flag & OBJ_PROC) &&
		    (obj_id == pid)) {
			checkflags = checkflags | M_OBJECT;
		} else if ((obj_flag & OBJ_PGROUP) &&
		    ((obj_group == egid) ||
		    (obj_group == rgid))) {
			checkflags = checkflags | M_OBJECT;
		} else if ((obj_flag & OBJ_POWNER) &&
		    ((obj_owner == euid) ||
		    (obj_group == ruid))) {
			checkflags = checkflags | M_OBJECT;
		}
	}
	return (-1);
}

/*
 * Format of System V IPC token:
 *	System V IPC token id	adr_char
 *	object id		adr_int32
 *
 */
int
s5_IPC_token(adr)
adr_t	*adr;
{
	int32_t	ipc_id;

	adrm_char(adr, &ipc_type, 1);	/* Global */
	adrm_int32(adr, &ipc_id, 1);

	if ((flags & M_OBJECT) &&
	    ipc_type_match(obj_flag, ipc_type) &&
	    (obj_id == ipc_id))
		checkflags |= M_OBJECT;

	return (-1);
}


/*
 * Format of socket token:
 *	socket_type		adrm_short
 *	remote_port		adrm_short
 *	remote_inaddr		adrm_int32
 *
 */
int
socket_token(adr)
adr_t	*adr;
{
	short	socket_type;
	short	remote_port;
	int32_t	remote_inaddr;

	adrm_short(adr, &socket_type, 1);
	adrm_short(adr, &remote_port, 1);
	adrm_char(adr, (char *)&remote_inaddr, 4);

	if ((flags & M_OBJECT) && (obj_flag == OBJ_SOCK)) {
		if (socket_flag == SOCKFLG_MACHINE) {
			if (remote_inaddr == obj_id)
				checkflags |= M_OBJECT;
		} else if (socket_flag == SOCKFLG_PORT) {
			if (remote_port == obj_id)
				checkflags |= M_OBJECT;
		}
	}
	return (-1);
}

/*
 * Format of socket token:
 *	socket_type		adrm_short
 *	remote_port		adrm_short
 *	remote_inaddr		adrm_int32
 *
 */
int
socket_ex_token(adr)
adr_t	*adr;
{
	short	socket_domain;
	short	socket_type;
	short	ip_size;
	short	local_port;
	int32_t	local_inaddr[4];
	short	remote_port;
	int32_t	remote_inaddr[4];

	adrm_short(adr, &socket_domain, 1);
	adrm_short(adr, &socket_type, 1);
	adrm_short(adr, &ip_size, 1);

	/* validate ip size */
	if ((ip_size != AU_IPv6) && (ip_size != AU_IPv4))
		return (0);

	adrm_short(adr, &local_port, 1);
	adrm_char(adr, (char *)local_inaddr, ip_size);

	adrm_short(adr, &remote_port, 1);
	adrm_char(adr, (char *)remote_inaddr, ip_size);

	/* if IP type mis-match, then nothing to do */
	if (ip_size != ip_type)
		return (-1);

	if ((flags & M_OBJECT) && (obj_flag == OBJ_SOCK)) {
		if (socket_flag == SOCKFLG_MACHINE) {
			if (ip_type == AU_IPv4) {
				if (local_inaddr[0] == obj_id)
					checkflags |= M_OBJECT;
				else if (remote_inaddr[0] == obj_id)
					checkflags |= M_OBJECT;
			} else {
				if ((local_inaddr[0] == ip_ipv6[0]) &&
				    (local_inaddr[1] == ip_ipv6[1]) &&
				    (local_inaddr[2] == ip_ipv6[2]) &&
				    (local_inaddr[3] == ip_ipv6[3]))
					checkflags |= M_OBJECT;
				else if ((remote_inaddr[0] == ip_ipv6[0]) &&
					(remote_inaddr[1] == ip_ipv6[1]) &&
					(remote_inaddr[2] == ip_ipv6[2]) &&
					(remote_inaddr[3] == ip_ipv6[3]))
					checkflags |= M_OBJECT;
			}
		} else if (socket_flag == SOCKFLG_PORT) {
			if ((local_port == obj_id) || (remote_port == obj_id))
				checkflags |= M_OBJECT;
		}
	}
	return (-1);
}

/*
 * Format of subject32 token:
 *	subject token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int32*2
 *
 */
int
subject32_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int32_t port, machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int32(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (flags & M_SUBJECT) {
		if (subj_id == pid)
			checkflags |= M_SUBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags |= M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags |= M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags |= M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == rgid)
			checkflags |= M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags |= M_GROUPE;
	}
	return (-1);
}

/*
 * Format of subject32_ex token:
 *	subject token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid_addr		adr_int32*6
 *
 */
int
subject32_token_ex(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int32_t port, type, addr[4];

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int32(adr, &port, 1);
	adrm_int32(adr, &type, 1);
	adrm_int32(adr, &addr[0], 4);

	if (flags & M_SUBJECT) {
		if (subj_id == pid)
			checkflags = checkflags | M_SUBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}
	return (-1);
}

/*
 * Format of subject64 token:
 *	subject token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int64+adr_int32
 *
 */
int
subject64_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int64_t port;
	int32_t machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int64(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (flags & M_SUBJECT) {
		if (subj_id == pid)
			checkflags |= M_SUBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags |= M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags |= M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags |= M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == rgid)
			checkflags |= M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags |= M_GROUPE;
	}
	return (-1);
}

/*
 * Format of subject64 token:
 *	subject token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int64+5*adr_int32
 *
 */
int
subject64_token_ex(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int64_t port;
	int32_t type, addr[4];

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int64(adr, &port, 1);
	adrm_int32(adr, &type, 1);
	adrm_int32(adr, &addr[0], 4);

	if (flags & M_SUBJECT) {
		if (subj_id == pid)
			checkflags = checkflags | M_SUBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}
	return (-1);
}

/*
 * Format of xatom token:
 */
int
xatom_token(adr)
adr_t	*adr;
{
	skip_string(adr);

	return (-1);
}


/*
 * Format of xobj token:
 */
int
xobj_token(adr)
adr_t	*adr;
{
	int32_t	oid, xid, cuid;

	adrm_int32(adr, &oid, 1);
	adrm_int32(adr, &xid, 1);
	adrm_int32(adr, &cuid, 1);

	return (-1);
}


/*
 * Format of xproto token:
 */
int
xproto_token(adr)
adr_t	*adr;
{
	int32_t	pid;
	adrm_int32(adr, &pid, 1);
	return (-1);
}


/*
 * Format of xselect token:
 */
int
xselect_token(adr)
adr_t	*adr;
{
	skip_string(adr);
	skip_string(adr);
	skip_string(adr);

	return (-1);
}

/*
 * anchor a path name with a slash
 * assume we have enough space
 */
void
anchor_path(path)
char	*path;
{
	(void) memmove((void *)(path + 1), (void *)path, strlen(path) + 1);
	*path = '/';
}


/*
 * copy path to collapsed path.
 * collapsed path does not contain:
 *	successive slashes
 *	instances of dot-slash
 *	instances of dot-dot-slash
 * passed path must be anchored with a '/'
 */
char	*
collapse_path(s)
char	*s; /* source path */
{
	int	id;	/* index of where we are in destination string */
	int	is;	/* index of where we are in source string */
	int	slashseen;	/* have we seen a slash */
	int	ls;		/* length of source string */

	ls = strlen(s) + 1;

	slashseen = 0;
	for (is = 0, id = 0; is < ls; is++) {
		/* thats all folks, we've reached the end of input */
		if (s[is] == '\0') {
			if (id > 1 && s[id-1] == '/') {
				--id;
			}
			s[id++] = '\0';
			break;
		}
		/* previous character was a / */
		if (slashseen) {
			if (s[is] == '/')
				continue;	/* another slash, ignore it */
		} else if (s[is] == '/') {
			/* we see a /, just copy it and try again */
			slashseen = 1;
			s[id++] = '/';
			continue;
		}
		/* /./ seen */
		if (s[is] == '.' && s[is+1] == '/') {
			is += 1;
			continue;
		}
		/* XXX/. seen */
		if (s[is] == '.' && s[is+1] == '\0') {
			if (id > 1)
				id--;
			continue;
		}
		/* XXX/.. seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '\0') {
			is += 1;
			if (id > 0)
				id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
		/* XXX/../ seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '/') {
			is += 2;
			if (id > 0)
				id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
		while (is < ls && (s[id++] = s[is++]) != '/');
		is--;
	}
	return (s);
}


int
ipc_type_match(flag, type)
int	flag;
char	type;
{
	if (flag == OBJ_SEM && type == AT_IPC_SEM)
		return (1);

	if (flag == OBJ_MSG && type == AT_IPC_MSG)
		return (1);

	if (flag == OBJ_SHM && type == AT_IPC_SHM)
		return (1);

	return (0);
}


void
skip_string(adr_t *adr)
{
	ushort_t	c;

	adrm_u_short(adr, &c, 1);
	adr->adr_now += c;
}


void
get_string(adr_t *adr, char **p)
{
	ushort_t	c;

	adrm_u_short(adr, &c, 1);
	*p = a_calloc(1, (size_t)c);
	adrm_char(adr, *p, c);
}
