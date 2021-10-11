/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_G_STATE_H
#define	_G_STATE_H

#pragma ident	"@(#)g_state.h	1.20	99/07/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Include any headers you depend on.
 */

/*
 * I18N message number ranges
 *  This file: 19000 - 19499
 *  Shared common messages: 1 - 1999
 */

#ifdef	TWO_SIX
#include	"fcio.h"
#else
#include	<libdevice.h>
#include	<sys/fibre-channel/fcio.h>
/*
 * The following define is not to
 * include sys/fc4/fcal_linkapp.h
 * file from sys/fc4/fcio.h, since it
 * has the same structure defines as
 * in sys/fibre-channel/fcio.h.
 */
#define	_SYS_FC4_FCAL_LINKAPP_H
#include	<sys/fc4/fcio.h>
#endif /* TWO_SIX */
#include	<sys/devctl.h>
#include	<g_scsi.h>
#include 	<gfc.h>

/* hotplug defines */
#define	SENA		1
#define	NON_SENA	0
/* format parameters to dump() */
#define	HEX_ONLY	0	/* Print Hex only */
#define	HEX_ASCII	1	/* Print Hex and Ascii */
/* Persistent Reservation */
#define	ACTION_READ_KEYS	0x00
#define	ACTION_READ_RESERV	0x01
#define	ACTION_REGISTER		0x00
#define	ACTION_RESERVE		0x01
#define	ACTION_RELEASE		0x02
#define	ACTION_CLEAR		0x03
#define	ACTION_PREEMPT		0x04
#define	ACTION_PREEMPT_CLR	0x05

/* defines for FC ioctl retries */
#define	MAX_RETRIES		3
/*
 * XXX: Wait 30 secs before each
 * retry(???). Investigate and select a
 * best wait time.
 */
#define	MAX_WAIT_TIME		30
typedef struct	read_keys_struct {
	int		rk_generation;
	int		rk_length;
	int		rk_key[256];
} Read_keys;

typedef struct	read_reserv_struct {
	int		rr_generation;
	int		rr_length;
} Read_reserv;


/* Function prototyes defined for libg_fc modules */
/* genf.c */
void		*g_zalloc(int);
char		*g_alloc_string(char *);
void		g_destroy_data(void *);
void		g_dump(char *, uchar_t *, int, int);
int		g_object_open(char *, int);
char		*g_scsi_find_command_name(int);
void		g_scsi_printerr(struct uscsi_cmd *,
		struct scsi_extended_sense *, int, char msg_string[], char *);
int		g_get_machineArch(int *);

/* hot.c */
void		g_ll_to_str(uchar_t *, char *);
void		g_free_hotplug_dlist(struct hotplug_disk_list **);

/* map.c */
int		g_string_to_wwn(uchar_t *, uchar_t *);
int		g_get_perf_statistics(char *, uchar_t *);
int		g_get_port_multipath(char *, struct dlist **, int);
int		g_device_in_map(sf_al_map_t *, int);
int		g_start(char *);
int		g_stop(char *, int);
int		g_reserve(char *);
int		g_release(char *);
int		g_issue_fcio_ioctl(int, fcio_t *, int);

/* cmd.c */
int		cmd(int, struct uscsi_cmd *, int);

/* io.c */
int		g_scsi_persistent_reserve_in_cmd(int, uchar_t *, int, uchar_t);
int		g_scsi_send_diag_cmd(int, uchar_t *, int);
int		g_scsi_rec_diag_cmd(int, uchar_t *, int, uchar_t);
int		g_scsi_writebuffer_cmd(int, int, uchar_t *, int, int, int);
int		g_scsi_readbuffer_cmd(int, uchar_t *, int, int);
int		g_scsi_inquiry_cmd(int, uchar_t *, int);
int		g_scsi_log_sense_cmd(int, uchar_t *, int, uchar_t);
int		g_scsi_mode_select_cmd(int, uchar_t *, int, uchar_t);
int		g_scsi_mode_sense_cmd(int, uchar_t *, int, uchar_t, uchar_t);
int		g_scsi_read_capacity_cmd(int, uchar_t *, int);
int		g_scsi_release_cmd(int);
int		g_scsi_reserve_cmd(int);
int		g_scsi_start_cmd(int);
int		g_scsi_stop_cmd(int, int);
int		g_scsi_tur(int);
int		g_scsi_reset(int);


#ifdef	__cplusplus
}
#endif

#endif	/* _G_STATE_H */
