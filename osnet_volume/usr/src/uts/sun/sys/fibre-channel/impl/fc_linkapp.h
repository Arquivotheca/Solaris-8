/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FC_LINKAPP_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FC_LINKAPP_H

#pragma ident	"@(#)fc_linkapp.h	1.1	99/07/21 SMI"

#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_BIT_FIELDS_LTOH) && !defined(_BIT_FIELDS_HTOL)
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */

/*
 * Link Application Opcodes.
 */
#define	LA_ELS_RJT		0x01
#define	LA_ELS_ACC		0x02
#define	LA_ELS_PLOGI		0x03
#define	LA_ELS_FLOGI		0x04
#define	LA_ELS_LOGO		0x05
#define	LA_ELS_ABTX		0x06
#define	LA_ELS_RCS		0x07
#define	LA_ELS_RES		0x08
#define	LA_ELS_RSS		0x09
#define	LA_ELS_RSI		0x0a
#define	LA_ELS_ESTS		0x0b
#define	LA_ELS_ESTC		0x0c
#define	LA_ELS_ADVC		0x0d
#define	LA_ELS_RTV		0x0e
#define	LA_ELS_RLS		0x0f
#define	LA_ELS_ECHO		0x10
#define	LA_ELS_RRQ		0x12
#define	LA_ELS_PRLI		0x20
#define	LA_ELS_PRLO		0x21
#define	LA_ELS_SCN		0x22
#define	LA_ELS_TPLS		0x23
#define	LA_ELS_GPRLO		0x24
#define	LA_ELS_GAID		0x30
#define	LA_ELS_FACT		0x31
#define	LA_ELS_FDACT		0x32
#define	LA_ELS_NACT		0x33
#define	LA_ELS_NDACT		0x34
#define	LA_ELS_QoSR		0x40
#define	LA_ELS_RVCS		0x41
#define	LA_ELS_PDISC		0x50
#define	LA_ELS_FDISC		0x51
#define	LA_ELS_ADISC		0x52
#define	LA_ELS_RSCN		0x61
#define	LA_ELS_SCR		0x62
#define	LA_ELS_LINIT		0x70

/*
 * LINIT status codes in the ACC
 */
#define	FC_LINIT_SUCCESS	0x01
#define	FC_LINIT_FAILURE	0x02

/* Basic Accept Payload. */
typedef struct la_ba_acc {

#if defined(_BIT_FIELDS_LTOH)
	uint32_t	org_sid : 24,
			seq_id : 8;

#else
	uint32_t	seq_id : 8,
			org_sid : 24;

#endif	/* _BIT_FIELDS_LTOH */

	uint16_t	ox_id;
	uint16_t	rx_id;
} la_ba_acc_t;


/* Basic Reject. */
typedef struct la_ba_rjt {
	uchar_t		reserved;
	uchar_t		reason_code;
	uchar_t		explanation;
	uchar_t		vendor;
} la_ba_rjt_t;


/* Logout payload. */
typedef struct la_els_logo {
	ls_code_t	ls_code;
	fc_portid_t	nport_id;
	la_wwn_t	nport_ww_name;
} la_els_logo_t;

/* Address discovery */
typedef	struct la_els_adisc {
	ls_code_t	ls_code;
	fc_hardaddr_t	hard_addr;
	la_wwn_t	port_wwn;
	la_wwn_t	node_wwn;
	fc_portid_t	nport_id;
} la_els_adisc_t;


/* Link Application Reject */
typedef struct la_els_rjt {
	ls_code_t	ls_code;
	uchar_t		action;
	uchar_t		reason;
	uchar_t		reserved;
	uchar_t		vu;
} la_els_rjt_t;

/* Process login */
typedef struct la_els_prli {
	uchar_t		ls_code;
	uchar_t		page_length;
	uint16_t	payload_length;
	uchar_t		service_params[16];
} la_els_prli_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per request", la_ba_rjt))
_NOTE(SCHEME_PROTECTS_DATA("unique per request", la_els_logo))
_NOTE(SCHEME_PROTECTS_DATA("unique per request", la_els_adisc))
_NOTE(SCHEME_PROTECTS_DATA("unique per request", la_els_rjt))
_NOTE(SCHEME_PROTECTS_DATA("unique per request", la_els_prli_t))
_NOTE(SCHEME_PROTECTS_DATA("unique per request", la_ba_acc))
#endif /* lint */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FC_LINKAPP_H */
