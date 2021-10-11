#ifndef lint
static char sccsid[] = "@(#)au_to.c 1.21 99/10/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <unistd.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/libbsm.h>
#include <sys/ipc.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <malloc.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <string.h>

#define	NGROUPS		16	/* XXX - temporary */

#ifdef __STDC__
static token_t *au_to_exec(char **, char);
#else
static token_t *au_to_exec();
#endif

static token_t *
get_token(int s)
{
	token_t *token;	/* Resultant token */

	if ((token = (token_t *)malloc(sizeof (token_t))) == (token_t *)0)
		return ((token_t *)0);
	if ((token->tt_data = malloc(s)) == (char *)0) {
		free(token);
		return ((token_t *)0);
	}
	token->tt_size = s;
	token->tt_next = (token_t *)0;
	return  (token);
}

/*
 * au_to_header
 * return s:
 *	pointer to header token.
 */
token_t *
#ifdef __STDC__
au_to_header(au_event_t e_type, au_emod_t e_mod)
#else
au_to_header(e_type, e_mod)
	au_event_t e_type;
	au_emod_t e_mod;
#endif
{
	adr_t adr;			/* adr memory stream header */
	token_t *token;			/* token pointer */
	char version = TOKEN_VERSION;	/* version of token family */
	int32_t byte_count;
	struct timeval tv;
#ifdef _LP64
	char data_header = AUT_HEADER64;	/* header for this token */

	token = get_token(2 * sizeof (char) +
			  sizeof (int32_t) +
			  2 * sizeof (int64_t) +
			  2 * sizeof (short));
#else
	char data_header = AUT_HEADER32;

	token = get_token(2 * sizeof (char) +
			  3 * sizeof (int32_t) +
			  2 * sizeof (short));
#endif

	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);	/* token ID */
	adr_int32(&adr, &byte_count, 1);	/* length of audit record */
	adr_char(&adr, &version, 1);		/* version of audit tokens */
	adr_short(&adr, &e_type, 1);		/* event ID */
	adr_short(&adr, &e_mod, 1);		/* event ID modifier */
#ifdef _LP64
	adr_int64(&adr, (int64_t *)&tv, 2);	/* time & date */
#else
	adr_int32(&adr, (int32_t *)&tv, 2);	/* time & date */
#endif
	return (token);
}

/*
 * au_to_trailer
 * return s:
 *	pointer to a trailer token.
 */
token_t *
au_to_trailer()
{
	adr_t adr;				/* adr memory stream header */
	token_t *token;				/* token pointer */
	char data_header = AUT_TRAILER;		/* header for this token */
	short magic = (short)AUT_TRAILER_MAGIC;	/* trailer magic number */
	int32_t byte_count;

	token = get_token(sizeof (char) + sizeof (int32_t) + sizeof (short));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);	/* token ID */
	adr_short(&adr, &magic, 1);		/* magic number */
	adr_int32(&adr, &byte_count, 1);	/* length of audit record */

	return (token);
}

/*
 * au_to_arg32
 * return s:
 *	pointer to an argument token.
 */
#ifdef __STDC__
token_t *au_to_arg(char n, char *text, uint32_t v);
token_t *
au_to_arg32(char n, char *text, uint32_t v)
#else
token_t *au_to_arg();
token_t *
au_to_arg32(n, text, v)
	char n;		/* argument # being used */
	char *text;	/* optional text describing argument */
	uint32_t v;	/* argument value */
#endif
#pragma weak au_to_arg = au_to_arg32
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_ARG32;	/* header for this token */
	short bytes;			/* length of string */

	bytes = strlen(text) + 1;

	token = get_token((int)(2 * sizeof (char) +
				sizeof (int32_t) +
				sizeof (short) + bytes));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);	/* token type */
	adr_char(&adr, &n, 1);			/* argument id */
	adr_int32(&adr, (int32_t *)&v, 1);	/* argument value */
	adr_short(&adr, &bytes, 1);
	adr_char(&adr, text, bytes);

	return (token);
}

/*
 * au_to_arg64
 * return s:
 *	pointer to an argument token.
 */
token_t *
#ifdef __STDC__
au_to_arg64(char n, char *text, uint64_t v)
#else
au_to_arg64(n, text, v)
	char n;		/* argument # being used */
	char *text;	/* optional text describing argument */
	uint64_t v;	/* argument value */
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_ARG64;	/* header for this token */
	short bytes;			/* length of string */

	bytes = strlen(text) + 1;

	token = get_token((int)(2 * sizeof (char) +
				sizeof (int64_t) +
				sizeof (short) + bytes));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);	/* token type */
	adr_char(&adr, &n, 1);			/* argument id */
	adr_int64(&adr, (int64_t *)&v, 1);	/* argument value */
	adr_short(&adr, &bytes, 1);
	adr_char(&adr, text, bytes);

	return (token);
}


/*
 * au_to_attr
 * return s:
 *	pointer to an attribute token.
 */
token_t *
#ifdef __STDC__
au_to_attr(struct vattr *attr)
#else
au_to_attr(attr)
	struct vattr *attr;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	int32_t value;
#ifdef _LP64
	char data_header = AUT_ATTR64;	/* header for this token */

	token = get_token(sizeof (char) +
			  sizeof (int32_t) * 4 +
			  sizeof (int64_t) * 2);
#else
	char data_header = AUT_ATTR32;

	token = get_token(sizeof (char) +
			  sizeof (int32_t) * 5 +
			  sizeof (int64_t));
#endif

	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	value = (int32_t)attr->va_mode;
	adr_int32(&adr, &value, 1);
	value = (int32_t)attr->va_uid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)attr->va_gid;
	adr_int32(&adr, &value, 1);
	adr_int32(&adr, (int32_t *)&(attr->va_fsid), 1);
	adr_int64(&adr, (int64_t *)&(attr->va_nodeid), 1);
#ifdef _LP64
	adr_int64(&adr, (int64_t *)&(attr->va_rdev), 1);
#else
	adr_int32(&adr, (int32_t *)&(attr->va_rdev), 1);
#endif

	return (token);
}

/*
 * au_to_data
 * return s:
 *	pointer a data token.
 */
token_t *
#ifdef __STDC__
au_to_data(char unit_print, char unit_type, char unit_count, char *p)
#else
au_to_data(unit_print, unit_type, unit_count, p)
	char unit_print;
	char unit_type;
	char unit_count;
	char *p;
#endif
{
	adr_t adr;			/* adr memory stream header */
	token_t *token;			/* token pointer */
	char data_header = AUT_DATA;	/* header for this token */
	int byte_count;			/* number of bytes */

	if (p == (char *)0 || unit_count < 1)
		return  ((token_t *)0);

	/*
	 * Check validity of print type
	 */
	if (unit_print < AUP_BINARY || unit_print > AUP_STRING)
		return  ((token_t *)0);

	switch (unit_type) {
	case AUR_SHORT:
		byte_count = unit_count * sizeof (short);
		break;
	case AUR_INT32:
		byte_count = unit_count * sizeof (int32_t);
		break;
	case AUR_INT64:
		byte_count = unit_count * sizeof (int64_t);
		break;
	/* case AUR_CHAR: */
	case AUR_BYTE:
		byte_count = unit_count * sizeof (char);
		break;
	default:
		return  ((token_t *)0);
	}

	token = get_token((int)(4 * sizeof (char) + byte_count));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, &unit_print, 1);
	adr_char(&adr, &unit_type, 1);
	adr_char(&adr, &unit_count, 1);

	switch (unit_type) {
	case AUR_SHORT:
		adr_short(&adr, (short *)p, unit_count);
		break;
	case AUR_INT32:
		adr_int32(&adr, (int32_t *)p, unit_count);
		break;
	case AUR_INT64:
		adr_int64(&adr, (int64_t *)p, unit_count);
		break;
	/* case AUR_CHAR: */
	case AUR_BYTE:
		adr_char(&adr, p, unit_count);
		break;
	}

	return  (token);
}

/*
 * au_to_process
 * return s:
 *	pointer to a process token.
 */

token_t *
#ifdef __STDC__
au_to_process(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
		pid_t pid, au_asid_t sid, au_tid_t *tid)
#else
au_to_process(auid, euid, egid, ruid, rgid, pid, sid, tid)
	au_id_t auid;
	uid_t euid;
	gid_t egid;
	uid_t ruid;
	gid_t rgid;
	pid_t pid;
	au_asid_t sid;
	au_tid_t *tid;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
#ifdef _LP64
	char data_header = AUT_PROCESS64;	/* header for this token */

	token = get_token(sizeof (char) + 8 * sizeof (int32_t) +
					sizeof (int64_t));
#else
	char data_header = AUT_PROCESS32;

	token = get_token(sizeof (char) + 9 * sizeof (int32_t));
#endif

	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&auid, 1);
	adr_int32(&adr, (int32_t *)&euid, 1);
	adr_int32(&adr, (int32_t *)&egid, 1);
	adr_int32(&adr, (int32_t *)&ruid, 1);
	adr_int32(&adr, (int32_t *)&rgid, 1);
	adr_int32(&adr, (int32_t *)&pid, 1);
	adr_int32(&adr, (int32_t *)&sid, 1);
#ifdef _LP64
	adr_int64(&adr, (int64_t *)&tid->port, 1);
#else
	adr_int32(&adr, (int32_t *)&tid->port, 1);
#endif
	adr_int32(&adr, (int32_t *)&tid->machine, 1);

	return  (token);
}

/* au_to_seq
 * return s:
 *	pointer to token chain containing a sequence token
 */
token_t *
#ifdef __STDC__
au_to_seq(int audit_count)
#else
au_to_seq(int audit_count)
	int audit_count;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_SEQ;	/* header for this token */

	token = get_token(sizeof (char) + sizeof (int32_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&audit_count, 1);

	return (token);
}

/*
 * au_to_socket
 * return s:
 *	pointer to mbuf chain containing a socket token.
 */
token_t *
#ifdef __STDC__
au_to_socket(struct oldsocket *so)
#else
au_to_socket(so)
	struct oldsocket *so;
#endif
{
	adr_t adr;
	token_t *token;
	char data_header = AUT_SOCKET;
	struct inpcb *inp = (struct inpcb *)so->so_pcb;

	token = get_token(sizeof (char) + 
			  sizeof (short) * 3 +
			  sizeof (int32_t) * 2);
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, (short *)&so->so_type, 1);
	adr_short(&adr, (short *)&inp->inp_lport, 1);
	adr_int32(&adr, (int32_t *)&inp->inp_laddr, 1);
	adr_short(&adr, (short *)&inp->inp_fport, 1);
	adr_int32(&adr, (int32_t *)&inp->inp_faddr, 1);

	return  (token);
}

/* au_to_subject
 * return s:
 *	pointer to a process token.
 */

token_t *
#ifdef __STDC__
au_to_subject(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
		pid_t pid, au_asid_t sid, au_tid_t *tid)
#else
au_to_subject(auid, euid, egid, ruid, rgid, pid, sid, tid)
	au_id_t auid;
	uid_t euid;
	gid_t egid;
	uid_t ruid;
	gid_t rgid;
	pid_t pid;
	au_asid_t sid;
	au_tid_t *tid;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
#ifdef _LP64
	char data_header = AUT_SUBJECT64;	/* header for this token */

	token = get_token(sizeof (char) + sizeof (int64_t) +
			  8 * sizeof (int32_t));
#else
	char data_header = AUT_SUBJECT32;

	token = get_token(sizeof (char) +
			  9 * sizeof (int32_t));
#endif

	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&auid, 1);
	adr_int32(&adr, (int32_t *)&euid, 1);
	adr_int32(&adr, (int32_t *)&egid, 1);
	adr_int32(&adr, (int32_t *)&ruid, 1);
	adr_int32(&adr, (int32_t *)&rgid, 1);
	adr_int32(&adr, (int32_t *)&pid, 1);
	adr_int32(&adr, (int32_t *)&sid, 1);
#ifdef _LP64
	adr_int64(&adr, (int64_t *) &tid->port, 1);
#else
	adr_int32(&adr, (int32_t *) &tid->port, 1);
#endif
	adr_int32(&adr, (int32_t *) &tid->machine, 1);

	return  (token);
}

/* au_to_subject_ex
 * return s:
 *	pointer to a process token.
 */

token_t *
#ifdef __STDC__
au_to_subject_ex(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
		pid_t pid, au_asid_t sid, au_tid_addr_t *tid)
#else
au_to_subject_ex(auid, euid, egid, ruid, rgid, pid, sid, tid)
	au_id_t auid;
	uid_t euid;
	gid_t egid;
	uid_t ruid;
	gid_t rgid;
	pid_t pid;
	au_asid_t sid;
	au_tid_addr_t *tid;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
#ifdef _LP64
	char data_header;		/* header for this token */

	if (tid->at_type == AU_IPv6) {
		data_header = AUT_SUBJECT64_EX;
		token = get_token(sizeof (char) + sizeof (int64_t) +
			  12 * sizeof (int32_t));
	} else {
		data_header = AUT_SUBJECT64;
		token = get_token(sizeof (char) + sizeof (int64_t) +
			  8 * sizeof (int32_t));
	}
#else
	char data_header;		/* header for this token */

	if (tid->at_type == AU_IPv6) {
		data_header = AUT_SUBJECT32_EX;
		token = get_token(sizeof (char) +
			  13 * sizeof (int32_t));
	} else {
		data_header = AUT_SUBJECT32;
		token = get_token(sizeof (char) +
			  9 * sizeof (int32_t));
	}
#endif

	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&auid, 1);
	adr_int32(&adr, (int32_t *)&euid, 1);
	adr_int32(&adr, (int32_t *)&egid, 1);
	adr_int32(&adr, (int32_t *)&ruid, 1);
	adr_int32(&adr, (int32_t *)&rgid, 1);
	adr_int32(&adr, (int32_t *)&pid, 1);
	adr_int32(&adr, (int32_t *)&sid, 1);
#ifdef _LP64
	adr_int64(&adr, (int64_t *) &tid->at_port, 1);
#else
	adr_int32(&adr, (int32_t *) &tid->at_port, 1);
#endif
	if (tid->at_type == AU_IPv6) {
		adr_int32(&adr, (int32_t *) &tid->at_type, 1);
		adr_char(&adr, (char *) &tid->at_addr[0], 16);
	} else {
		adr_char(&adr, (char *) &tid->at_addr[0], 4);
	}

	return  (token);
}

/*
 * au_to_me
 * return s:
 *	pointer to a process token.
 */

token_t *
au_to_me()
{
	auditinfo_addr_t info;

	if (getaudit_addr(&info, sizeof(info)))
		return ((token_t *)0);
	return  (au_to_subject_ex(info.ai_auid, geteuid(), getegid(), getuid(),
			    getgid(), getpid(), info.ai_asid, &info.ai_termid));
}
/*
 * au_to_text
 * return s:
 *	pointer to a text token.
 */
token_t *
#ifdef __STDC__
au_to_text(char *text)
#else
au_to_text(text)
	char *text;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_TEXT;	/* header for this token */
	short bytes;			/* length of string */

	bytes = strlen(text) + 1;
	token = get_token((int)(sizeof (char) + sizeof (short) + bytes));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bytes, 1);
	adr_char(&adr, text, bytes);

	return  (token);
}

/*
 * au_to_path
 * return s:
 *	pointer to a path token.
 */
token_t *
#ifdef __STDC__
au_to_path(char *path)
#else
au_to_path(path)
	char *path;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_PATH;	/* header for this token */
	short bytes;			/* length of string */

	bytes = (short) strlen(path) + 1;

	token = get_token((int)(sizeof (char) +  sizeof (short) + bytes));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bytes, 1);
	adr_char(&adr, path, bytes);

	return  (token);
}

/*
 * au_to_cmd
 * return s:
 *	pointer to an command line argument token
 */
token_t *
#ifdef __STDC__
au_to_cmd(u_int argc, char **argv, char **envp)
#else
au_to_cmd(argc, argv, envp)
	u_int argc;
	char **argv;
	char **envp;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_CMD;	/* header for this token */
	short len = 0;
	short cnt = 0;
	short envc = 0;
	short largc = (short)argc;

	/*
	 * one char for the header, one short for argc,
	 * one short for # envp strings.
	 */
	len = sizeof (char) + sizeof (short) + sizeof (short);

	/* get sizes of strings */

	for (cnt = 0; cnt < argc; cnt++) {
		len += (short) sizeof (short);
		len += (short) (strlen(argv[cnt]) + 1);
	}

	if (envp != (char **)0) {
		for (envc = 0; envp[envc] != (char *)0; envc++) {
			len += (short) sizeof (short);
			len += (short) (strlen(envp[envc]) + 1);
		}
	}

	token = get_token(len);
	if (token == (token_t *)0)
		return ((token_t *)0);

	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);

	adr_short(&adr, &largc, 1);

	for (cnt = 0; cnt < argc; cnt++) {
		len = (short) (strlen(argv[cnt]) + 1);
		adr_short(&adr, &len, 1);
		adr_char(&adr, argv[cnt], len);
	}

	adr_short(&adr, &envc, 1);

	for (cnt = 0; cnt < envc; cnt++) {
		len = (short) (strlen(envp[cnt]) + 1);
		adr_short(&adr, &len, 1);
		adr_char(&adr, envp[cnt], len);
	}

	return  (token);
}

/*
 * au_to_exit
 * return s:
 *	pointer to a exit value token.
 */
token_t *
#ifdef __STDC__
au_to_exit(int retval, int err)
#else
au_to_exit(retval, err)
	int retval;
	int err;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_EXIT;	/* header for this token */

	token = get_token(sizeof (char) + (2 * sizeof (int32_t)));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&retval, 1);
	adr_int32(&adr, (int32_t *)&err, 1);

	return  (token);
}

/*
 * au_to_return
 * return s:
 *	pointer to a return  value token.
 */
#ifdef __STDC__
token_t *au_to_return (char number, uint32_t value);
token_t *
au_to_return32 (char number, uint32_t value)
#else
token_t *au_to_return ();
token_t *
au_to_return32 (number, value)
	char number;
	uint32_t value;
#endif
#pragma weak au_to_return = au_to_return32
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_RETURN32;/* header for this token */

	token = get_token(2 * sizeof (char) + sizeof (int32_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, &number, 1);
	adr_int32(&adr, (int32_t *)&value, 1);

	return (token);
}

/*
 * au_to_return
 * return s:
 *	pointer to a return  value token.
 */
token_t *
#ifdef __STDC__
au_to_return64 (char number, uint64_t value)
#else
au_to_return64 (number, value)
	char number;
	uint64_t value;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_RETURN64;/* header for this token */

	token = get_token(2 * sizeof (char) + sizeof (int64_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, &number, 1);
	adr_int64(&adr, (int64_t *)&value, 1);

	return (token);
}


/*
 * au_to_opaque
 * return s:
 *	pointer to a opaque token.
 */
token_t *
#ifdef __STDC__
au_to_opaque(char *opaque, short bytes)
#else
au_to_opaque(opaque, bytes)
	char *opaque;
	short bytes;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_OPAQUE;	/* header for this token */

	if (bytes < 1)
		return  ((token_t *)0);

	token = get_token((int)(sizeof (char) + sizeof (short) + bytes));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bytes, 1);
	adr_char(&adr, opaque, bytes);

	return  (token);
}

/*
 * au_to_in_addr
 * return s:
 *	pointer to a internet address token
 */
token_t *
#ifdef __STDC__
au_to_in_addr(struct in_addr *internet_addr)
#else
au_to_in_addr(internet_addr)
	struct in_addr *internet_addr;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_IN_ADDR;	/* header for this token */

	token = get_token(sizeof (char) + sizeof (uint32_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)internet_addr, 1);

	return (token);
}

/*
 * au_to_iport
 * return s:
 *	pointer to token chain containing a ip port address token
 */
token_t *
#ifdef __STDC__
au_to_iport(u_short iport)
#else
au_to_iport(iport)
	u_short iport;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_IPORT;	/* header for this token */

	token = get_token(sizeof (char) + sizeof (short));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, (short *)&iport, 1);

	return  (token);
}

token_t *
#ifdef __STDC__
au_to_ipc(int id)
#else
au_to_ipc(id)
	int id;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_IPC;	/* header for this token */

	token = get_token(sizeof (char) + sizeof (int32_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&id, 1);

	return (token);
}


/*
 * The Modifier tokens
 */

/*
 * au_to_groups
 * return s:
 *	pointer to a group list token.
 *
 * This function is obsolete.  Please use au_to_newgroups.
 */
token_t *
#ifdef __STDC__
au_to_groups(int *groups)
#else
au_to_groups(groups)
	int *groups;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_GROUPS;	/* header for this token */

	token = get_token(sizeof (char) + NGROUPS * sizeof (int32_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)groups, NGROUPS);

	return  (token);
}

/*
 * au_to_newgroups
 * return s:
 *	pointer to a group list token.
 */
token_t *
#ifdef __STDC__
au_to_newgroups(int n, gid_t *groups)
#else
au_to_newgroups(n, groups)
	int n;
	gid_t *groups;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_NEWGROUPS;	/* header for this token */
	short n_groups;

	if (n < NGROUPS_UMIN || n > NGROUPS_UMAX || groups == (gid_t *)0)
		return ((token_t *)0);
	token = get_token(sizeof (char) + sizeof (short) + n * sizeof (gid_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	n_groups = (short)n;
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &n_groups, 1);
	adr_int32(&adr, (int32_t *)groups, n_groups);

	return (token);
}

/*
 * au_to_exec_args
 * returns:
 *	pointer to an exec args token.
 */
token_t *
#ifdef __STDC__
au_to_exec_args(char **argv)
#else
au_to_exec_args(argv)
	char **argv;
#endif
{
	return (au_to_exec(argv, AUT_EXEC_ARGS));
}

/*
 * au_to_exec_env
 * returns:
 *	pointer to an exec args token.
 */
token_t *
#ifdef __STDC__
au_to_exec_env(char **envp)
#else
au_to_exec_env(envp)
	char **envp;
#endif
{
	return (au_to_exec(envp, AUT_EXEC_ENV));
}

/*
 * au_to_exec
 * returns:
 *	pointer to an exec args token.
 */
static token_t *
#ifdef __STDC__
au_to_exec(char **v, char data_header)
#else
au_to_exec(v, data_header)
	char **v;
	char data_header;
#endif
{
	token_t *token;
	adr_t adr;
	char **p;
	int32_t n = 0;
	int len = 0;

	for (p = v; *p != NULL; p++) {
		len += strlen(*p) + 1;
		n++;
	}
	token = get_token(sizeof (char) + sizeof (int32_t) + len);
	if (token == (token_t *)NULL)
		return ((token_t *)NULL);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, &n, 1);
	for (p = v; *p != NULL; p++) {
		adr_char(&adr, *p, strlen(*p) + 1);
	}
	return (token);
}

/*
 * au_to_xatom
 * return s:
 *	pointer to a xatom token.
 */
token_t *
#ifdef __STDC__
au_to_xatom(u_short len, char *atom)
#else
au_to_xatom(len, atom)
	u_short len;
	char *atom;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_XATOM;	/* header for this token */

	token = get_token((int)(sizeof (char) + sizeof (u_short) + len));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, (short *)&len, 1);
	adr_char(&adr, atom, len);

	return  (token);
}

/*
 * au_to_xproto
 * return s:
 *	pointer to a X protocol token.
 */
token_t *
#ifdef __STDC__
au_to_xproto(pid_t pid)
#else
au_to_xproto(pid)
	pid_t pid;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_XPROTO;	/* header for this token */
	int32_t v = pid;

	token = get_token(sizeof (char) + sizeof (int32_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, &v, 1);

	return  (token);
}

/*
 * au_to_xobj
 * return s:
 *	pointer to a X object token.
 */
token_t *
#ifdef __STDC__
au_to_xobj(int oid, int xid, int cuid)
#else
au_to_xobj(oid, xid, cuid)
	int oid;
	int xid;
	int cuid;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_XOBJ;	/* header for this token */

	token = get_token(sizeof (char) + 3 * sizeof (int32_t));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&oid, 1);
	adr_int32(&adr, (int32_t *)&xid, 1);
	adr_int32(&adr, (int32_t *)&cuid, 1);

	return (token);
}

/*
 * au_to_xselect
 * return s:
 *	pointer to a X select token.
 */
token_t *
#ifdef __STDC__
au_to_xselect(char *pstring, char *type, short dlen, char *data)
#else
au_to_xselect(pstring, type, dlen, data)
	char  *pstring;
	char  *type;
	short  dlen;
	char  *data;
#endif
{
	token_t *token;			/* local token */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_XSELECT;	/* header for this token */
	short bytes;

	bytes = strlen(pstring) + strlen(type) + 2 + dlen;
	token = get_token((int)(sizeof (char) + 
				sizeof (short) * 3 +
				bytes));
	if (token == (token_t *)0)
		return ((token_t *)0);
	adr_start(&adr, token->tt_data);
	adr_char(&adr, &data_header, 1);
	bytes = strlen(pstring) + 1;
	adr_short(&adr, &bytes, 1);
	adr_char(&adr, pstring, bytes);
	bytes = strlen(type) + 1;
	adr_short(&adr, &bytes, 1);
	adr_char(&adr, type, bytes);
	adr_short(&adr, &dlen, 1);
	adr_char(&adr, data, dlen);
	return  (token);
}
