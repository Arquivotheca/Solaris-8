
/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_OPTCOM_H
#define	_INET_OPTCOM_H

#pragma ident	"@(#)optcom.h	1.15	99/03/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && defined(__STDC__)

/* Options Description Structure */
typedef struct opdes_s {
	t_uscalar_t	opdes_name;	/* option name */
	t_uscalar_t	opdes_level;	/* option "level" */
	int	opdes_access_nopriv; /* permissions for non-privileged */
	int	opdes_access_priv; /* permissions for privilged */
	int	opdes_props;	/* properties of associated with option */
	t_uscalar_t	opdes_size;	/* length of option */
					/* [ or maxlen if variable */
			/* length(OP_VARLEN) property set for option] */
	union {
		/*
		 *
		 * Note: C semantics:
		 * static initializer of "union" type assume
		 * the constant on RHS is of the type of the
		 * first member of the union. So what comes first
		 * is important.
		 */
#define	OPDES_DEFSZ_MAX		64
		int64_t  opdes_def_int64;
		char	opdes_def_charbuf[OPDES_DEFSZ_MAX];
	} opdes_def;
} opdes_t;

#define	opdes_default	opdes_def.opdes_def_int64
#define	opdes_defbuf	opdes_def.opdes_def_charbuf
/*
 * Flags to set in opdes_acces_{all,priv} fields in opdes_t
 *
 *	OA_R	read access
 *	OA_W	write access
 *	OA_RW	read-write access
 *	OA_X	execute access
 *
 * Note: - semantics "execute" access used for operations excuted using
 *		option management interface
 *	- no bits set means this option is not visible. Some options may not
 *	  even be visible to all but priviliged users.
 */
#define	OA_R	0x1
#define	OA_W	0x2
#define	OA_X	0x4

/*
 * Utility macros to test permissions needed to compose more
 * complex ones. (Only a few really used directly in code).
 */
#define	OA_RW	(OA_R|OA_W)
#define	OA_WX	(OA_W|OA_X)
#define	OA_RX	(OA_R|OA_X)
#define	OA_RWX	(OA_R|OA_W|OA_X)

#define	OA_ANY_ACCESS(x) ((x)->opdes_access_nopriv|(x)->opdes_access_priv)
#define	OA_R_NOPRIV(x)	((x)->opdes_access_nopriv & OA_R)
#define	OA_R_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_R)
#define	OA_W_NOPRIV(x)	((x)->opdes_access_nopriv & OA_W)
#define	OA_X_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_X)
#define	OA_X_NOPRIV(x)	((x)->opdes_access_nopriv & OA_X)
#define	OA_W_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_W)
#define	OA_WX_NOPRIV(x)	((x)->opdes_access_nopriv & OA_WX)
#define	OA_WX_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_WX)
#define	OA_RWX_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_RWX)
#define	OA_RONLY_NOPRIV(x)	(((x)->opdes_access_nopriv & OA_RWX) == OA_R)
#define	OA_RONLY_ANYPRIV(x)	((OA_ANY_ACCESS(x) & OA_RWX) == OA_R)

/*
 * Following macros supply the option and their privelege and
 * are used to determine permissions.
 */
#define	OA_READ_PERMISSION(x, p)	((!(p) && OA_R_NOPRIV(x)) ||\
						((p) && OA_R_ANYPRIV(x)))
#define	OA_WRITE_OR_EXECUTE(x, p)	((!(p) && OA_WX_NOPRIV(x)) ||\
						((p) && OA_WX_ANYPRIV(x)))
#define	OA_READONLY_PERMISSION(x, p)	((!(p) && OA_RONLY_NOPRIV(x)) ||\
						((p) && OA_RONLY_ANYPRIV(x)))
#define	OA_WRITE_PERMISSION(x, p)	((!(p) && OA_W_NOPRIV(x)) ||\
					    (p && OA_W_ANYPRIV(x)))
#define	OA_EXECUTE_PERMISSION(x, p)	((!(p) && OA_X_NOPRIV(x)) ||\
					    ((p) && OA_X_ANYPRIV(x)))
#define	OA_NO_PERMISSION(x, p)	((!(p) && (x)->opdes_access_nopriv == 0) || \
				    ((p) && (x)->opdes_access_priv == 0))

/*
 * Other properties set in opdes_props field.
 */
#define	OP_PASSNEXT	0x1	/* to pass option to next module or not */
#define	OP_VARLEN	0x2	/* option is varible length  */
#define	OP_NOT_ABSREQ	0x4	/* option is not a "absolute requirement" */
				/* i.e. failure to negotiate does not */
				/* abort primitive ("ignore" semantics ok) */
#define	OP_NODEFAULT	0x8	/* no concept of "default value"  */
#define	OP_DEF_FN	0x10	/* call a "default function" to get default */
				/* value, not from static table  */


/*
 * Structure to represent attributed of option management specific
 * to one particular layer of "transport".
 */

typedef	t_uscalar_t optlevel_t;

typedef struct optdb_obj {
	pfi_t		odb_deffn;	/* default value function */
	pfi_t		odb_getfn;	/* get function */
	pfi_t		odb_setfn;	/* set function */
	boolean_t	odb_topmost_tpiprovider; /* whether topmost tpi */
					/* provider or downstream */
	uint_t		odb_opt_arr_cnt; /* count of number of options in db */
	opdes_t		*odb_opt_des_arr; /* option descriptors in db */
	uint_t		odb_valid_levels_arr_cnt;
					/* count of option levels supported */
	optlevel_t	*odb_valid_levels_arr;
					/* array of option levels supported */
} optdb_obj_t;

/*
 * Values for "optset_context" parameter passed to
 * transport specific "setfn()" routines
 */
#define	SETFN_OPTCOM_CHECKONLY		1 /* "checkonly" semantics T_CHECK */
#define	SETFN_OPTCOM_NEGOTIATE		2 /* semantics for T_*_OPTCOM_REQ */
#define	SETFN_UD_NEGOTIATE		3 /* semantics for T_UNITDATA_REQ */
#define	SETFN_CONN_NEGOTIATE		4 /* semantics for T_CONN_*_REQ */

/*
 * Function prototypes
 */
extern void optcom_err_ack(queue_t *, mblk_t *, t_scalar_t, int);
extern void svr4_optcom_req(queue_t *, mblk_t *, int, optdb_obj_t *);
extern void tpi_optcom_req(queue_t *, mblk_t *, int, optdb_obj_t *);
extern int  tpi_optcom_buf(queue_t *, mblk_t *, t_scalar_t *, t_scalar_t, int,
    optdb_obj_t *, void *, int *);
extern t_uscalar_t optcom_max_optbuf_len(opdes_t *, uint_t);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_OPTCOM_H */
