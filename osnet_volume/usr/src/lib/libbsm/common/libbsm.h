/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _BSM_LIBBSM_H
#define	_BSM_LIBBSM_H

#pragma ident	"@(#)libbsm.h	1.31	99/10/14 SMI"

#include <stdio.h>
#include <errno.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef TEXT_DOMAIN
#define	TEXT_DOMAIN	"SUNW_OST_OSLIB"
#endif

#ifdef	__STDC__
extern const char *bsm_dom;
#else
extern char *bsm_dom;
#endif

/*
 * For audit_event(5)
 */
struct au_event_ent {
	au_event_t ae_number;
	char	*ae_name;
	char	*ae_desc;
	au_class_t ae_class;
};
typedef struct au_event_ent au_event_ent_t;

/*
 * For audit_class(5)
 */
struct au_class_ent {
	char	*ac_name;
	au_class_t ac_class;
	char	*ac_desc;
};
typedef struct au_class_ent au_class_ent_t;

/*
 * For audit_user(5)
 */
struct au_user_ent {
	char	*au_name;
	au_mask_t au_always;
	au_mask_t au_never;
};
typedef struct au_user_ent au_user_ent_t;

/*
 * Internal representation of audit user in libnsl
 */
typedef struct au_user_str_s {
	char	*au_name;
	char	*au_always;
	char	*au_never;
} au_user_str_t;

/*
 * Functions that manipulate bytes from an audit file
 */

#ifdef	__STDC__

extern void	adr_char(adr_t *, char *, int);
extern int	adr_count(adr_t *);
extern void	adr_int32(adr_t *, int32_t *, int);
extern void	adr_int64(adr_t *, int64_t *, int);
extern void	adr_short(adr_t *, short *, int);
extern void	adr_start(adr_t *, char *);
extern int	adrf_char(adr_t *, char *, int);
extern int	adrf_int32(adr_t *, int32_t *, int);
extern int	adrf_int64(adr_t *, int64_t *, int);
extern int	adrf_short(adr_t *, short *, int);
extern void	adrf_start(adr_t *, FILE *);
extern int	adrf_string(adr_t *, char *);
extern int	adrf_opaque(adr_t *, char *);
extern int	adrf_u_char(adr_t *, uchar_t *, int);
extern int	adrf_u_int32(adr_t *, uint32_t *, int);
extern int	adrf_u_int64(adr_t *, uint64_t *, int);
extern int	adrf_u_short(adr_t *, ushort_t *, int);

#else /* not __STDC__ */

extern void	adr_char();
extern int	adr_count();
extern void	adr_int32();
extern void	adr_int64();
extern void	adr_short();
extern void	adr_start();
extern int	adrf_char();
extern int	adrf_int32();
extern int	adrf_int64();
extern int	adrf_short();
extern void	adrf_start();
extern int	adrf_string();
extern int	adrf_opaque();
extern int	adrf_u_char();
extern int	adrf_u_int32();
extern int	adrf_u_int64();

#endif

/*
 * Functions that manipulate bytes from an audit character stream.
 */

#ifdef	__STDC__

extern void	adrm_start(adr_t *, char *);
extern void	adrm_char(adr_t *, char *, int);
extern void	adrm_short(adr_t *, short *, int);
extern void	adrm_int64(adr_t *, int64_t *, int);
extern void	adrm_string(adr_t *, char *);
extern void	adrm_int32(adr_t *, int32_t *, int);
extern void	adrm_u_int32(adr_t *, uint32_t *, int);
extern void	adrm_u_opaque(adr_t *, char *);
extern void	adrm_u_char(adr_t *, uchar_t *, int);
extern void	adrm_u_int64(adr_t *, uint64_t *, int);
extern void	adrm_u_short(adr_t *, ushort_t *, int);

#else

extern void	adrm_start();
extern void	adrm_char();
extern void	adrm_short();
extern void	adrm_int64();
extern void	adrm_string();
extern void	adrm_int32();
extern void	adrm_u_int32();
extern void	adrm_u_opaque();
extern void	adrm_u_char();
extern void	adrm_u_int64();
extern void	adrm_u_short();

#endif

/*
 * Functions that do I/O for audit files
 */

#ifdef	__STDC__
extern int	au_close(int, int, short);
extern int	au_open(void);
extern int	au_write(int, token_t *);
extern int	au_read_rec(FILE *, char **);
extern int	au_fetch_tok(au_token_t *, char *, int);
extern int	au_print_tok(FILE *, au_token_t *, char *, char *, char *, int);

#else /* not __STDC__ */

extern int	au_close();
extern int	au_open();
extern int	au_write();
extern int	au_read_rec();
extern int	au_fetch_tok();
extern int	au_print_tok();

#endif

/*
 * Functions than manipulate audit events
 */

#ifdef	__STDC__

extern void	setauevent(void);
extern void	endauevent(void);
extern int	setaueventfile(char *);

extern au_event_ent_t	*getauevent(void);
extern au_event_ent_t	*getauevent_r(au_event_ent_t *);
extern au_event_ent_t	*getauevnam(char *);
extern au_event_ent_t	*getauevnam_r(au_event_ent_t *, char *);
extern au_event_ent_t	*getauevnum(au_event_t);
extern au_event_ent_t	*getauevnum_r(au_event_ent_t *, au_event_t);
extern au_event_t	getauevnonam(char *);
extern int		au_preselect(au_event_t, au_mask_t *, int, int);
extern int		cacheauevent(au_event_ent_t **, au_event_t);

#else /* not __STDC__ */

extern void	setauevent();
extern void	endauevent();
extern int	setaueventfile();

extern au_event_ent_t	*getauevent();
extern au_event_ent_t	*getauevent_r();
extern au_event_ent_t	*getauevnam();
extern au_event_ent_t	*getauevnam_r();
extern au_event_ent_t	*getauevnum();
extern au_event_ent_t	*getauevnum_r();
extern au_event_t	getauevnonam();
extern int		au_preselect();
extern int		cacheauevent();

#endif

/*
 * Functions that manipulate audit classes
 */

#ifdef	__STDC__

extern void	setauclass(void);
extern void	endauclass(void);
extern int	setauclassfile(char *);

extern int	cacheauclass(au_class_ent_t **, au_class_t);
extern int	cacheauclassnam(au_class_ent_t **, char *);
extern au_class_ent_t *getauclassent(void);
extern au_class_ent_t *getauclassent_r(au_class_ent_t *);
extern au_class_ent_t *getauclassnam(char *);
extern au_class_ent_t *getauclassnam_r(au_class_ent_t *, char *);

#else /* not __STDC__ */

extern void	setauclass();
extern void	endauclass();
extern int	setauclassfile();

extern int	cacheauclass();
extern int	cacheauclassnam();
extern au_class_ent_t *getauclassent();
extern au_class_ent_t *getauclassent_r();
extern au_class_ent_t *getauclassnam();
extern au_class_ent_t *getauclassnam_r();

#endif

/*
 * Functions that manipulate audit attributes of users
 */

#ifdef	__STDC__

void	setauuser(void);
void	endauuser(void);
int	setauuserfile(char *);

au_user_ent_t *getauuserent(void);
au_user_ent_t *getauuserent_r(au_user_ent_t *);
au_user_ent_t *getauusernam(char *);
au_user_ent_t *getauusernam_r(au_user_ent_t *, char *);

#else

void	setauuser();
void	endauuser();
int	setauuserfile();

au_user_ent_t *getauuserent();
au_user_ent_t *getauusernam();

#endif

/*
 * Functions that manipulate the audit control file
 */

#ifdef	__STDC__

void	endac(void);
void	setac(void);
int	testac(void);

int	getacdir(char *, int);
int	getacmin(int *);
int	getacna(char *, int);
int	getacflg(char *, int);

#else

void	endac();
void	setac();
int	testac();

int	getacdir();
int	getacmin();
int	getacna();
int	getacflg();

#endif

/*
 * Functions that manipulate audit masks
 */

#ifdef	__STDC__

int	au_user_mask(char *, au_mask_t *);
int	getauditflags();
int	getauditflagsbin();
int	getauditflagschar();

#else

int	au_user_mask();
int	getauditflags();
int	getauditflagsbin();
int	getauditflagschar();

#endif

/*
 * Functions that do system calls
 */

#ifdef	__STDC__

extern int	audit(char *, int);
extern int	auditon(int, caddr_t, int);
extern int	auditstat(au_stat_t *);
extern int	auditsvc(int, int);
extern int	audituser(char *, int);
extern int	getaudit(auditinfo_t *);
extern int	getaudit_addr(auditinfo_addr_t *, int);
extern int	getauid(au_id_t *);
extern int	getkernstate(au_mask_t *);
extern int	getuseraudit(au_id_t, au_mask_t *);
extern int	setaudit(auditinfo_t *);
extern int	setaudit_addr(auditinfo_addr_t *, int);
extern int	setauid(au_id_t *);
extern int	setkernstate(au_mask_t *);
extern int	setuseraudit(au_id_t, au_mask_t *);

#else

extern int	audit(char *, int);
extern int	auditon(int, caddr_t, int);
extern int	auditstat(au_stat_t *);
extern int	auditsvc(int, int);
extern int	audituser(char *);
extern int	getaudit(auditinfo_t *);
extern int	getaudit_addr(auditinfo_addr_t *, int);
extern int	getauid(au_id_t *);
extern int	getkernstate(au_mask_t *);
extern int	getuseraudit(au_id_t, au_mask_t *);
extern int	setaudit(auditinfo_t *);
extern int	setaudit_addr(auditinfo_addr_t *, int);
extern int	setauid(au_id_t *);
extern int	setkernstate(au_mask_t *);
extern int	setuseraudit(au_id_t, au_mask_t *);

#endif

#define	BSM_TEXTBUFSZ	256 /* size of string for generic text token */

/*
 * Defines for au_preselect(3)
 */
#define	AU_PRS_SUCCESS	1
#define	AU_PRS_FAILURE	2
#define	AU_PRS_BOTH	(AU_PRS_SUCCESS|AU_PRS_FAILURE)

#define	AU_PRS_USECACHE	0
#define	AU_PRS_REREAD	1

/*
 * Defines for cacheauclass and cacheauevent
 */
#define	AU_CACHE_FREE	0x0000
#define	AU_CACHE_NAME	0x0001
#define	AU_CACHE_NUMBER	0x0002

/* Flags for user-level audit routines: au_open, au_close, au_to_ */
#define	AU_TO_NO_WRITE	0
#define	AU_TO_WRITE	1

/* Flags for user-level audit routine: au_fetch_tok */
#define	AUF_NOOP	0x0000
#define	AUF_POINT	0x0001
#define	AUF_DUP		0x0002
#define	AUF_COPY_IN	0x0004
#define	AUF_SKIP	0x0008

/* system audit files for auditd */
#define	AUDITCLASSFILE		"/etc/security/audit_class"
#define	AUDITCONTROLFILE	"/etc/security/audit_control"
#define	AUDITDATAFILE		"/etc/security/audit_data"
#define	AUDITEVENTFILE		"/etc/security/audit_event"
#define	AUDITUSERFILE		"/etc/security/audit_user"

/* array sizes for audit library structures */
#define	AU_CLASS_NAME_MAX	8
#define	AU_CLASS_DESC_MAX	72
#define	AU_EVENT_NAME_MAX	30
#define	AU_EVENT_DESC_MAX	50
#define	AU_EVENT_LINE_MAX	256

/*
 * Some macros used internally by the nsswitch code
*/
#define	AUDITUSER_FILENAME		"/etc/security/audit_user"
#define	AUDITUSER_DB_NAME		"audit_user.org_dir"
#define	AUDITUSER_DB_NCOL		3	/* total columns */
#define	AUDITUSER_DB_NKEYCOL		1	/* total searchable columns */
#define	AUDITUSER_DB_TBLT		"audit_user_tbl"
#define	AUDITUSER_SUCCESS		0
#define	AUDITUSER_PARSE_ERANGE		1
#define	AUDITUSER_NOT_FOUND		2

#define	AUDITUSER_COL0_KW		"name"
#define	AUDITUSER_COL1_KW		"always"
#define	AUDITUSER_COL2_KW		"never"

/*
 * indices of searchable columns
 */
#define	AUDITUSER_KEYCOL0		0	/* name */


#ifdef	__cplusplus
}
#endif

#endif	/* _BSM_LIBBSM_H */
