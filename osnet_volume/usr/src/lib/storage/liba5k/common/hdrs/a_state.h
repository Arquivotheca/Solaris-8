/*
 * Copyright 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * SES State definitions
 */

/*
 * I18N message number ranges
 *  This file: 16500 - 16999
 *  Shared common messages: 1 - 1999
 */

#ifndef	_A_STATE_H
#define	_A_STATE_H

#pragma ident	"@(#)a_state.h	1.16	99/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Include any headers you depend on.
 */
#ifdef	TWO_SIX
#include	<sys/devctl.h>
#include	"fcio.h"
#else
#include	<sys/fibre-channel/fcio.h>
#define	_SYS_FC4_FCAL_LINKAPP_H
#include	<sys/fc4/fcio.h>
#endif /* TWO_SIX */
#include <gfc.h>
#include <g_state.h>
#include <a5k.h>

/*
 * Definitions for send/receive diagnostic command
 */
#define	HEADER_LEN		4
#define	MAX_REC_DIAG_LENGTH	0xfffe



typedef struct	rec_diag_hdr {
	uchar_t		page_code;
	uchar_t		sub_enclosures;
	ushort_t	page_len;
} Rec_diag_hdr;

/*
 * We should use the scsi_capacity structure in impl/commands.h
 * but it uses u_long's to define 32 bit values.
 */
typedef	struct	capacity_data_struct {
	uint_t	last_block_addr;
	uint_t	block_size;
} Read_capacity_data;

/* Function prototypes defined for liba5k modules */
/* diag.c */
int		d_dev_bypass_enable(struct  path_struct *, int, int,
		int, int);
int		d_bp_bypass_enable(char *, int, int, int, int, int);
int		d_p_enable(char *, int);
int		d_p_bypass(char *, int);

/* mon.c */
int		l_ex_open_test(struct dlist *, char *, int);
int		l_get_conflict(char *, char **, int);
int		l_new_password(char *, char *);
int		l_get_mode_pg(char *, uchar_t **, int);
void		l_element_msg_string(uchar_t, char *);
int		l_check_file(char *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _A_STATE_H */
