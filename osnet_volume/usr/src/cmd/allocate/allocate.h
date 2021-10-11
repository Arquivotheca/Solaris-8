/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if !defined(lint) && defined(SCCSIDS)
static char	*bsm_sccsid = "@(#)allocate.h 1.10 99/06/21 SMI; SunOS BSM";
#endif

#ifndef __cmw_allocate_h
#define	__cmw_allocate_h

#undef DEBUG
/* #define	CMW Not for Kestrel */

/* Option Flags */
#define	SILENT		0001	/* -s */
#define	USER		0002	/* -U */
#define	LIST		0004	/* -l */
#define	FREE		0010	/* -n */
#define	CURRENT 	0020	/* -u */
#define	FORCE		0040	/* -F */
#define	FORCE_ALL 	0100	/* -I */
#define	LABEL		0200	/* -l */
#define	TYPE		0400	/* -g */

#define	ALLOC_OPTS	(SILENT | USER | FORCE | TYPE)
#define	DEALLOC_OPTS	(SILENT | FORCE | FORCE_ALL)
#define	LIST_OPTS	(SILENT | USER | LIST | FREE | CURRENT)

/* Misc. */

#define	ALL	-1

/* Error returns start at 4 */
#define	SYSERROR	4
#define	NODGENT		5
#define	DEVRNG		6
#define	DACLCK		7
#define	DACACC		8
#define	DEVLST		9
#define	NDEALLOC	10
#define	NALLOCU		11
#define	HASOPNS		12
#define	NOMACEX		13
#define	NOTROOT		14
#define	NOTEROOT	15
#define	CNTFRC		16
#define	CNTDEXEC	17
#define	NO_DEVICE	18
#define	DSPMISS		19
#define	ALLOCERR	20
#define	FALLERR		21
#define	IMPORT_ERR	22
#define	NODAENT		23
#define	NODMAPENT	24
#define	CHMOD_PERR	25
#define	CHOWN_PERR	26
#define	SETLABEL_PERR	27
#define	ALLOC		28
#define	ALLOC_OTHER	29
#define	NALLOC		30
#define	AUTHERR		31
#define	GETLABEL_PERR	32
#define	ALLOC_LABEL	33
#define	MACERR		34
#define	CLEAN_ERR	35

/* Tunable Parameters */
#define	DEV_DIR		"/dev"
#define	DAC_DIR		"/etc/security/dev"
#define	SECLIB		"/etc/security/lib"
#define	ALLOC_MODE	0600
#define	DEALLOC_MODE    0000
#define	ALLOC_ERR_MODE  0100
#define	ALLOC_USER	"root"
#define	DEALLOC_LABEL	"system_low [ system_high ]"

/* Functions */
int	allocate(), deallocate(), list_devices(), aud_rec();
int	aud_accume(), aud_write();
/* Audit stuff */
#define	AUDIT

#define	D_LABEL		0
#define	DEVICE		1
#define	DEV_LIST	2
#define	EVENT		3
#define	D_TYPE		4
#define	AUTHORIZ	5

#endif /*  __cmw_allocate_h */
