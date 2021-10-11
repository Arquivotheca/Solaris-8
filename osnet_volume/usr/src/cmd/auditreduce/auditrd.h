/*
 * Copyright (c) 1987, 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_AUDITRD_H
#define	_AUDITRD_H

#pragma ident	"@(#)auditrd.h	1.11	99/12/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Global data for auditreduce
 */

/*
 * Message selection options
 */
unsigned short	m_type;		/* 'm' message type */
gid_t	m_groupr;		/* 'g' group-id */
gid_t	m_groupe;		/* 'f' effective group-id */
uid_t	m_usera;		/* 'u' user id */
uid_t	m_usere;		/* 'e' effective user-id */
uid_t	m_userr;		/* 'r' real user-id */
time_t	m_after;		/* 'a' after a time */
time_t	m_before;		/* 'b' before a time */
audit_state_t mask;		/* used with m_class */
int	flags;
int	checkflags;
int	socket_flag;
int	ip_type;
int	ip_ipv6[4];		/* ipv6 type object */
int	obj_flag;		/* 'o' object type */
int	obj_id;			/* object identifier */
gid_t	obj_group;		/* object group */
uid_t	obj_owner;		/* object owner */
int	subj_id;		/* subject identifier  */
char	ipc_type;		/* 'o' object type - tell what type of IPC */

/*
 * File selection options
 */
char	*f_machine;		/* 'M' machine (suffix) type */
char	*f_root;		/* 'R' audit root */
char	*f_server;		/* 'S' server */
char	*f_outfile;		/* 'W' output file */
static char	*f_outtemp;	/* 'W' temporary file name */
int	f_all;			/* 'A' all records from a file */
int	f_complete;		/* 'C' only completed files */
int	f_delete;		/* 'D' delete when done */
int	f_quiet;		/* 'Q' sshhhh! */
int	f_verbose;		/* 'V' verbose */
int	f_stdin;		/* '-' read from stdin */
int	f_cmdline;		/* files specified on the command line */
int	new_mode;		/* 'N' new object selection mode */

/*
 * Global error reporting
 */
char	*error_str;		/* current error message */
char	errbuf[256];		/* for creating error messages with sprintf */
char	*ar = "auditreduce:";
static int	root_pid;	/* remember original process's pid */

/*
 * Global control blocks
 */
audit_pcb_t *audit_pcbs; /* ptr to array of pcbs that hold files (fcbs) */

int	pcbsize;		/* size of audit_pcb[] */
int	pcbnum;		/* number of pcbs in audit_pcb[] that are active */

/*
 * Time values
 */
time_t f_start;		/* time of first record written */
time_t f_end;		/* time of last record written */
time_t time_now;	/* time the program began */

/*
 * Global counting vars
 */
int	filenum;		/* number of files to process */

/*
 * Global variable, class of current record being processed.
 */
int global_class;

#ifdef __cplusplus
}
#endif

#endif	/* _AUDITRD_H */
