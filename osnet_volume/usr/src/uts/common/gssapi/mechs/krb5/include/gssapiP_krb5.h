#pragma ident	"@(#)gssapiP_krb5.h	1.1	99/07/18 SMI"	/* SVr4.0 2.34.1.2 */
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _GSSAPIP_KRB5_H_
#define _GSSAPIP_KRB5_H_

/*
 * $Id: gssapiP_krb5.h,v 1.31.2.1 1996/11/21 02:19:35 marc Exp $
 */

#include <krb5.h>
#ifndef _KERNEL
#include <memory.h>
#endif

/* work around sunos braindamage */
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

#ifdef	_KERNEL
#ifdef	DEBUG
extern	void prom_printf();
#endif
#if !defined(KRB5DEBUG)
#define KRB5DEBUG
#endif
#else
#define prom_printf printf 
#endif


#ifdef _KERNEL
#ifdef KRB5DEBUG
extern u_int krb5_log;

#define KRB5LOG(A, B, C) \
        ((void)((krb5_log & (A)) && (printf((B), (C)), TRUE)))
#define KRB5LOG0(A, B)   \
        ((void)((krb5_log & (A)) && (printf(B), TRUE)))
#else
#define KRB5LOG(A, B, C)
#define KRB5LOG0(A, B)
#endif /* KRB5_DEBUG */
#endif /* _KERNEL */

#ifndef	_KERNEL
#ifdef DEBUG_ON

#define	dprintf(a) printf(a)
#define	dprintf1(a, b) printf(a, b)

#else

#define	dprintf(a)
#define	dprintf1(a, b)
#define	DUMMY_STATIC

#endif	/* DEBUG_ON */

#else	/* _KERNEL */

#define	dprintf(a)	KRB5LOG0(8, a)
#define	dprintf1(a, b)	KRB5LOG(8, a, b)
#define	DUMMY_STATIC	static

#endif	/* _KERNEL */

/* The include of gssapi_krb5.h will dtrt with the above #defines in
 * effect.
 */
#include <gssapi_krb5.h>
#include <gssapi_err_krb5.h>
#include <sys/systm.h>

/** constants **/

#define CKSUMTYPE_KG_CB		0x8003

#define KG_TOK_CTX_AP_REQ	0x0100
#define KG_TOK_CTX_AP_REP	0x0200
#define KG_TOK_CTX_ERROR	0x0300
#define KG_TOK_SIGN_MSG		0x0101
#define KG_TOK_SEAL_MSG		0x0201
#define	KG_TOK_MIC_MSG		0x0101
#define	KG_TOK_WRAP_MSG		0x0201
#define KG_TOK_DEL_CTX		0x0102

#define KG_IMPLFLAGS(x) (GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG | \
			 ((x) & (GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | \
				 GSS_C_SEQUENCE_FLAG | GSS_C_DELEG_FLAG)))

#define KRB5_GSS_FOR_CREDS_OPTION 1

/** internal types **/

typedef krb5_principal krb5_gss_name_t;

typedef struct _krb5_gss_cred_id_rec {
   /* name/type of credential */
   gss_cred_usage_t usage;
   krb5_principal princ;	/* this is not interned as a gss_name_t */
   const gss_OID_set_desc *actual_mechs;
   int prerfc_mech;		/* these are a cache of the set above */
   int rfc_mech;

   /* keytab (accept) data */
   krb5_keytab keytab;

   /* ccache (init) data */
   krb5_ccache ccache;
   krb5_timestamp tgt_expire;
} krb5_gss_cred_id_rec, *krb5_gss_cred_id_t; 

typedef struct _krb5_gss_enc_desc {
   int processed;
   krb5_keyblock *key;
   krb5_encrypt_block eblock;
} krb5_gss_enc_desc;

typedef struct _krb5_gss_ctx_id_rec {
   int initiate;	/* nonzero if initiating, zero if accepting */
   OM_uint32 gss_flags;
   int seed_init;
   unsigned char seed[16];
   krb5_principal here;
   krb5_principal there;
   krb5_keyblock *subkey;
   int signalg;
   int cksum_size;
   int sealalg;
   krb5_gss_enc_desc enc;
   krb5_gss_enc_desc seq;
   krb5_timestamp endtime;
   krb5_flags krb_flags;
   krb5_int32 seq_send;
   krb5_int32 seq_recv;
   void *seqstate;
   int established;
   int big_endian;
   krb5_auth_context auth_context;
   /* In MIT Distibution the following field was
    * defined as gss_OID_desc *. This has been
    * changed to gss_OID_desc 
   */ 
   gss_OID_desc mech_used;
} krb5_gss_ctx_id_rec, *krb5_gss_ctx_id_t;

extern void *kg_vdb;

extern krb5_context kg_context;
#ifdef _KERNEL
extern kmutex_t	krb5_mutex;
#else
extern mutex_t	krb5_mutex;
#endif

/* helper macros */

#define kg_save_name(name)		g_save_name(&kg_vdb,name)
#define kg_save_cred_id(cred)		g_save_cred_id(&kg_vdb,cred)
#define kg_save_ctx_id(ctx)		g_save_ctx_id(&kg_vdb,ctx)

#define kg_validate_name(name)		g_validate_name(&kg_vdb,name)
#define kg_validate_cred_id(cred)	g_validate_cred_id(&kg_vdb,cred)
#define kg_validate_ctx_id(ctx)		g_validate_ctx_id(&kg_vdb,ctx)

#define kg_delete_name(name)		g_delete_name(&kg_vdb,name)
#define kg_delete_cred_id(cred)		g_delete_cred_id(&kg_vdb,cred)
#define kg_delete_ctx_id(ctx)		g_delete_ctx_id(&kg_vdb,ctx)

/** helper functions **/

OM_uint32 kg_get_defcred 
	PROTOTYPE((OM_uint32 *minor_status, 
		   gss_cred_id_t *cred));

OM_uint32 kg_release_defcred PROTOTYPE((OM_uint32 *minor_status));

krb5_error_code kg_checksum_channel_bindings
         PROTOTYPE((krb5_context context, gss_channel_bindings_t cb,
					     krb5_checksum *cksum,
					     int bigend));

krb5_error_code kg_make_seq_num PROTOTYPE((krb5_context context,
					   krb5_gss_enc_desc *ed,
            int direction, krb5_int32 seqnum, unsigned char *cksum,
				unsigned char *buf));

krb5_error_code kg_get_seq_num PROTOTYPE((krb5_context context,
					  krb5_gss_enc_desc *ed,
            unsigned char *cksum, unsigned char *buf, int *direction,
					  krb5_int32 *seqnum));

krb5_error_code kg_make_seed PROTOTYPE((krb5_context context,
					krb5_keyblock *key,
					unsigned char *seed));

int kg_confounder_size PROTOTYPE((krb5_gss_enc_desc *ed));

krb5_error_code kg_make_confounder PROTOTYPE((krb5_gss_enc_desc *ed,
            unsigned char *buf));

int kg_encrypt_size PROTOTYPE((krb5_gss_enc_desc *ed, int n));

krb5_error_code kg_encrypt PROTOTYPE((krb5_context context, 
            krb5_gss_enc_desc *ed,
            krb5_pointer iv, krb5_pointer in, krb5_pointer out, int length));

krb5_error_code kg_decrypt PROTOTYPE((krb5_context context,
                           krb5_gss_enc_desc *ed, 
			   krb5_pointer iv, krb5_pointer in, krb5_pointer out, int length));

OM_uint32 kg_seal PROTOTYPE((krb5_context context,
		  OM_uint32 *minor_status,
		  gss_ctx_id_t context_handle,
		  int conf_req_flag,
		  int qop_req,
		  gss_buffer_t input_message_buffer,
		  int *conf_state,
		  gss_buffer_t output_message_buffer,
		  int toktype));

OM_uint32 kg_unseal PROTOTYPE((krb5_context context,
		    OM_uint32 *minor_status,
		    gss_ctx_id_t context_handle,
		    gss_buffer_t input_token_buffer,
		    gss_buffer_t message_buffer,
		    int *conf_state,
		    int *qop_state,
		    int toktype));

OM_uint32 kg_seal_size PROTOTYPE((krb5_context context,
				  OM_uint32 *minor_status,
				  gss_ctx_id_t context_handle,
				  int conf_req_flag,
				  gss_qop_t qop_req,
				  OM_uint32 output_size,
				  OM_uint32 *input_size));

krb5_error_code kg_ctx_size PROTOTYPE((krb5_context kcontext,
				       krb5_pointer arg,
				       size_t *sizep));

krb5_error_code kg_ctx_externalize PROTOTYPE((krb5_context kcontext,
					      krb5_pointer arg,
					      krb5_octet **buffer,
					      size_t *lenremain));

krb5_error_code kg_ctx_internalize PROTOTYPE((krb5_context kcontext,
					      krb5_pointer *argp,
					      krb5_octet **buffer,
					      size_t *lenremain));

OM_uint32 kg_get_context PROTOTYPE((OM_uint32 *minor_status,
				    krb5_context *context));
	
/** declarations of internal name mechanism functions **/

OM_uint32 krb5_gss_acquire_cred
PROTOTYPE( (void *,		/* krb5 context */
       	    OM_uint32*,       /* minor_status */
            gss_name_t,       /* desired_name */
            OM_uint32,        /* time_req */
            gss_OID_set,      /* desired_mechs */
            gss_cred_usage_t, /* cred_usage */
            gss_cred_id_t*,   /* output_cred_handle */
            gss_OID_set*,     /* actual_mechs */
            OM_uint32*        /* time_rec */
           ));

OM_uint32 krb5_gss_release_cred
PROTOTYPE( (void *,		/* krb5 context */
	    OM_uint32*,       /* minor_status */
            gss_cred_id_t*    /* cred_handle */
           ));

OM_uint32 krb5_gss_init_sec_context
PROTOTYPE( (void *,		/* krb5 context */
	    OM_uint32*,       /* minor_status */
            gss_cred_id_t,    /* claimant_cred_handle */
            gss_ctx_id_t*,    /* context_handle */
            gss_name_t,       /* target_name */
            gss_OID,          /* mech_type */
            OM_uint32,        /* req_flags */
            OM_uint32,        /* time_req */
            gss_channel_bindings_t,
                              /* input_chan_bindings */
            gss_buffer_t,     /* input_token */
            gss_OID*,         /* actual_mech_type */
            gss_buffer_t,     /* output_token */
            OM_uint32*,       /* ret_flags */
            OM_uint32*        /* time_rec */
           ));

OM_uint32 krb5_gss_accept_sec_context
PROTOTYPE( (void *,		/* krb5 context */
	    OM_uint32*,       /* minor_status */
            gss_ctx_id_t*,    /* context_handle */
            gss_cred_id_t,    /* verifier_cred_handle */
            gss_buffer_t,     /* input_token_buffer */
            gss_channel_bindings_t,
                              /* input_chan_bindings */
            gss_name_t*,      /* src_name */
            gss_OID*,         /* mech_type */
            gss_buffer_t,     /* output_token */
            OM_uint32*,       /* ret_flags */
            OM_uint32*,       /* time_rec */
            gss_cred_id_t*    /* delegated_cred_handle */
           ));

OM_uint32 krb5_gss_process_context_token
PROTOTYPE( (void *,		/* krb5 context */
	    OM_uint32*,       /* minor_status */
            gss_ctx_id_t,     /* context_handle */
            gss_buffer_t      /* token_buffer */
           ));

OM_uint32 krb5_gss_delete_sec_context
	 (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_ctx_id_t*,    /* context_handle */
            gss_buffer_t      /* output_token */
#ifdef	_KERNEL
	/* */, OM_uint32	/* context verifier */
#endif
	);

OM_uint32 krb5_gss_context_time
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_ctx_id_t,     /* context_handle */
            OM_uint32*        /* time_rec */
           ));


OM_uint32 krb5_gss_sign
	(void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_ctx_id_t,     /* context_handle */
            int,              /* qop_req */
            gss_buffer_t,     /* message_buffer */
            gss_buffer_t      /* message_token */
#ifdef	_KERNEL
	/* */, OM_uint32	/* context verifier */
#endif
           );

OM_uint32 krb5_gss_verify
	(void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_ctx_id_t,     /* context_handle */
            gss_buffer_t,     /* message_buffer */
            gss_buffer_t,     /* token_buffer */
            int*              /* qop_state */
#ifdef	_KERNEL
	/* */, OM_uint32	/* context verifier */
#endif
           );

OM_uint32 krb5_gss_display_status
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            OM_uint32,        /* status_value */
            int,              /* status_type */
            gss_OID,          /* mech_type */
            OM_uint32*,       /* message_context */
            gss_buffer_t      /* status_string */
           ));

OM_uint32 krb5_gss_indicate_mechs
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_OID_set*      /* mech_set */
           ));

OM_uint32 krb5_gss_compare_name
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_name_t,       /* name1 */
            gss_name_t,       /* name2 */
            int*              /* name_equal */
           ));

OM_uint32 krb5_gss_display_name
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,      /* minor_status */
            gss_name_t,      /* input_name */
            gss_buffer_t,    /* output_name_buffer */
            gss_OID*         /* output_name_type */
           ));

OM_uint32 krb5_gss_import_name
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_buffer_t,     /* input_name_buffer */
            gss_OID,          /* input_name_type */
            gss_name_t*       /* output_name */
           ));

OM_uint32 krb5_gss_release_name
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
            gss_name_t*       /* input_name */
           ));

OM_uint32 krb5_gss_inquire_cred
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,      /* minor_status */
            gss_cred_id_t,    /* cred_handle */
            gss_name_t *,     /* name */
            OM_uint32 *,      /* lifetime */
            gss_cred_usage_t*,/* cred_usage */
            gss_OID_set *     /* mechanisms */
           ));

OM_uint32 krb5_gss_inquire_context
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32*,       /* minor_status */
	    gss_ctx_id_t,     /* context_handle */
	    gss_name_t*,      /* initiator_name */
	    gss_name_t*,      /* acceptor_name */
	    OM_uint32*,       /* lifetime_rec */
	    gss_OID*,         /* mech_type */
	    OM_uint32*,       /* ret_flags */
	    int*,             /* locally_initiated */
	    int*              /* open */
	   ));

/* New V2 entry points */
OM_uint32 krb5_gss_get_mic
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_ctx_id_t,		/* context_handle */
	    gss_qop_t,			/* qop_req */
	    gss_buffer_t,		/* message_buffer */
	    gss_buffer_t		/* message_token */
	   ));

OM_uint32 krb5_gss_verify_mic
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_ctx_id_t,		/* context_handle */
	    gss_buffer_t,		/* message_buffer */
	    gss_buffer_t,		/* message_token */
	    gss_qop_t *			/* qop_state */
	   ));

OM_uint32 krb5_gss_wrap
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_ctx_id_t,		/* context_handle */
	    int,			/* conf_req_flag */
	    gss_qop_t,			/* qop_req */
	    gss_buffer_t,		/* input_message_buffer */
	    int *,			/* conf_state */
	    gss_buffer_t		/* output_message_buffer */
	   ));

OM_uint32 krb5_gss_unwrap
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_ctx_id_t,		/* context_handle */
	    gss_buffer_t,		/* input_message_buffer */
	    gss_buffer_t,		/* output_message_buffer */
	    int *,			/* conf_state */
	    gss_qop_t *			/* qop_state */
	   ));

OM_uint32 krb5_gss_wrap_size_limit
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_ctx_id_t,		/* context_handle */
	    int,			/* conf_req_flag */
	    gss_qop_t,			/* qop_req */
	    OM_uint32,			/* req_output_size */
	    OM_uint32 *			/* max_input_size */
	   ));

OM_uint32 krb5_gss_add_cred
PROTOTYPE( (void *,		/* krb5 context */
	    OM_uint32 *,		/* minor_status */
	    gss_cred_id_t,		/* input_cred_handle */
	    gss_name_t,			/* desired_name */
	    gss_OID,			/* desired_mech */
	    gss_cred_usage_t,		/* cred_usage */
	    OM_uint32,			/* initiator_time_req */
	    OM_uint32,			/* acceptor_time_req */
	    gss_cred_id_t *,		/* output_cred_handle */
	    gss_OID_set *,		/* actual_mechs */
	    OM_uint32 *,		/* initiator_time_rec */
	    OM_uint32 *			/* acceptor_time_rec */
	   ));

OM_uint32 krb5_gss_inquire_cred_by_mech
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32  *,		/* minor_status */
	    gss_cred_id_t,		/* cred_handle */
	    gss_OID,			/* mech_type */
	    gss_name_t *,		/* name */
	    OM_uint32 *,		/* initiator_lifetime */
	    OM_uint32 *,		/* acceptor_lifetime */
	    gss_cred_usage_t * 		/* cred_usage */
	   ));

OM_uint32 krb5_gss_export_sec_context
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_ctx_id_t *,		/* context_handle */
	    gss_buffer_t		/* interprocess_token */
	    ));

OM_uint32 krb5_gss_import_sec_context
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_buffer_t,		/* interprocess_token */
	    gss_ctx_id_t *		/* context_handle */
	    ));

#if 0
OM_uint32 krb5_gss_release_oid
PROTOTYPE( (OM_uint32 *,		/* minor_status */
	    gss_OID *			/* oid */
	   ));
#endif

OM_uint32 krb5_gss_internal_release_oid
PROTOTYPE( (void *,			/* krb5 context  */
		OM_uint32 *,		/* minor_status */
	    gss_OID *			/* oid */
	   ));

OM_uint32 krb5_gss_inquire_names_for_mech
PROTOTYPE( (void *,             /* krb5 context */
            OM_uint32 *,		/* minor_status */
	    gss_OID,			/* mechanism */
	    gss_OID_set *		/* name_types */
	   ));

OM_uint32 krb5_proot_to_uid(
		void *,			/* krb5 context */
		OM_uint32 *,		/* minor status */
		const gss_name_t,	/* pname */
		uid_t *			/* uidOUt */
		);

#endif /* _GSSAPIP_KRB5_H_ */
