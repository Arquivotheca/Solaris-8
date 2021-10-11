/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_OSESIO_H
#define	_SYS_OSESIO_H

#pragma ident	"@(#)osesio.h	1.13	98/03/26 SMI"

/*
 * Enclosure Services Interface Structures
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ses io control commands
 */
#define	SESIOC			('e'<<8)
#define	SES_IOCTL_GETSTATE	(SESIOC|1)	/* get esi status */
#define	SES_IOCTL_SETSTATE	(SESIOC|2)	/* set esi state */
#define	SES_IOCTL_INQUIRY	(SESIOC|3)	/* get SCSI Inquiry info */

/*
 * The following structure is used by the SES_IOCTL_GETSTATE
 * and the SES_IOCTL_SETSTATE ioctls.
 */
struct ses_ioctl {
	uint32_t page_size;	/* Size of page to be read/written */
	uint8_t  page_code;	/* Page to be read/written */
	caddr_t  page;		/* Address of page to be read/written */
};


/*
 * The following structures are used by the RSM (aka Tabasco)
 * enclosure services devices.
 */
struct rsm_es_d_stat {
#ifdef _BIT_FIELDS_HTOL
uchar_t	dp   : 1,	/* drive present; valid only for IN page */
	dl   : 1,	/* LED On */
	rsvd : 6;
#else
uchar_t	rsvd : 6,
	dl   : 1,	/* LED On */
	dp   : 1;	/* drive present; valid only for IN page */
#endif
};

struct rsm_es_mod_stat {
#ifdef _BIT_FIELDS_HTOL
uchar_t  modfail : 1,	/* module has failed */
	rsvd01	: 1,
	dual_fan_failure : 1,
	rsvd02	: 5;
#else
uchar_t	rsvd02	: 5,
	dual_fan_failure : 1,
	rsvd01	: 1,
	modfail : 1;	/* module has failed */
#endif
};

#define	ES_RSM_MAX_DRIVES		7
#define	ES_RSM_MAX_PWMS			2
#define	ES_RSM_MAX_FANS			1
#define	ES_RSM_MAX_TEMPS		1


/*
 * The GETSTATE structure. Corresponds to SCSI page 0x04h for GETSTATE.
 */
struct rsm_es_in {
#ifdef _BIT_FIELDS_HTOL
uchar_t	page_code;
uchar_t	rsvd1 : 5,
	abs   : 1,	/* abnormal state, no intervention needed */
	chk   : 1,	/* failure state, immediate attention needed */
	efw   : 1;	/* unused */
ushort_t page_len;
uchar_t	encl_gd_len;	/* enclosure global descriptor length */
uchar_t	rsvd2;
uchar_t	num_unit_types;	/* # of supported unit types with status reporting */
uchar_t	rsvd3;
ushort_t alsen : 1,	/* alarm sounding */
	alenb : 1,	/* alarm enable */
	rsvd4 : 2,
	altime : 12;	/* alarm time (secs) */
uchar_t	idsen	: 1,	/* MS bit of SCSI Id's in enclosure */
	rsvd5	: 1,
	dsdly	: 1,	/* 1=spin-up delay enabled in h/w; 0=disabled */
	rsvd6	: 5;
uchar_t	rsvd7;
uchar_t	max_drvs;	/* max no. of drives */
uchar_t	drv_dscp_len;	/* device descriptor length */
uchar_t	max_pwms;	/* max no. of power modules */
uchar_t	pwm_dscp_len;	/* power module descriptor length */
uchar_t	max_fans;	/* max no. of fans */
uchar_t	fan_dscp_len;	/* fan descriptor length */
uchar_t	max_temps;	/* max no. of temperature sensors */
uchar_t	temp_dscp_len;	/* temperature sensor descriptor length */
struct rsm_es_d_stat devstat[ES_RSM_MAX_DRIVES];
struct rsm_es_mod_stat pwm[ES_RSM_MAX_PWMS];	/* power modules */
struct rsm_es_mod_stat fan;		/* fan module */
struct rsm_es_mod_stat ovta;	/* overtemperature sensing module A */
#else
uchar_t	page_code;
uchar_t	efw   : 1,	/* unused */
	chk   : 1,	/* failure state, immediate attention needed */
	abs   : 1,	/* abnormal state, no intervention needed */
	rsvd1 : 5;
ushort_t page_len;
uchar_t	encl_gd_len;	/* enclosure global descriptor length */
uchar_t	rsvd2;
uchar_t	num_unit_types;	/* # of supported unit types with status reporting */
uchar_t	rsvd3;
ushort_t altime : 12,	/* alarm time (secs) */
	rsvd4 : 2,
	alenb : 1,	/* alarm enable */
	alsen : 1;	/* alarm sounding */
uchar_t	rsvd6	: 5,
	dsdly	: 1,	/* 1=spin-up delay enabled in h/w; 0=disabled */
	rsvd5	: 1,
	idsen	: 1;	/* MS bit of SCSI Id's in enclosure */
uchar_t	rsvd7;
uchar_t	max_drvs;	/* max no. of drives */
uchar_t	drv_dscp_len;	/* device descriptor length */
uchar_t	max_pwms;	/* max no. of power modules */
uchar_t	pwm_dscp_len;	/* power module descriptor length */
uchar_t	max_fans;	/* max no. of fans */
uchar_t	fan_dscp_len;	/* fan descriptor length */
uchar_t	max_temps;	/* max no. of temperature sensors */
uchar_t	temp_dscp_len;	/* temperature sensor descriptor length */
struct rsm_es_d_stat devstat[ES_RSM_MAX_DRIVES];
struct rsm_es_mod_stat pwm[ES_RSM_MAX_PWMS];	/* power modules */
struct rsm_es_mod_stat fan;		/* fan module */
struct rsm_es_mod_stat ovta;	/* overtemperature sensing module A */
#endif
};


/*
 * The SETSTATE structure. Corresponds to SCSI page 0x04h for SETSTATE.
 */
struct rsm_es_out {
#ifdef _BIT_FIELDS_HTOL
uchar_t	page_code;
uchar_t	rsvd1;
ushort_t page_len;
uchar_t	encl_gd_len;	/* enclosure global descriptor length */
uchar_t	rsvd2;
uchar_t	num_unit_types;	/* # of supported unit types with status reporting */
uchar_t	rsvd3;
ushort_t rsvd4 : 1,
	alenb : 1,	/* alarm enable */
	rsvd5 : 2,
	altime : 12;	/* alarm time (secs) */
uchar_t rsvd6 : 1,
	rpoff : 1,	/* power-off the enclosure */
	rsvd7 : 6;
uchar_t	rsvd8;
uchar_t	max_drvs;
uchar_t	drv_dscp_len;	/* device descriptor length */
struct rsm_es_d_stat devstat[ES_RSM_MAX_DRIVES];
uchar_t	rsvd9;
#else
uchar_t	page_code;
uchar_t	rsvd1;
ushort_t page_len;
uchar_t	encl_gd_len;	/* enclosure global descriptor length */
uchar_t	rsvd2;
uchar_t	num_unit_types;	/* # of supported unit types with status reporting */
uchar_t	rsvd3;
ushort_t altime : 12,	/* alarm time (secs) */
	rsvd5 : 2,
	alenb : 1,	/* alarm enable */
	rsvd4 : 1;
uchar_t	rsvd7 : 6,
	rpoff : 1,	/* power-off the enclosure */
	rsvd6 : 1;
uchar_t	rsvd8;
uchar_t	max_drvs;
uchar_t	drv_dscp_len;	/* device descriptor length */
struct rsm_es_d_stat devstat[ES_RSM_MAX_DRIVES];
uchar_t	rsvd9;
#endif
};

/* For page_len field of struct rsm_es_out */
#define		SCSI_RSM_ES_OUT_PAGE_LEN	0x0012

/* For encl_gd_len field of struct rsm_es_out */
#define		SCSI_RSM_ENCL_GD_LEN	0x04

/* For num_unit_types field of struct rsm_es_out and rsm_es_in */
#define		SCSI_RSM_NUM_UNIT_TYPES	0x01

/* For dev_dscp_len field of struct rsm_es_out */
#define		SCSI_RSM_DEV_DSCP_LEN	0x01

/* For page_len field of struct rsm_es_in */
#define		SCSI_RSM_ES_IN_PAGE_LEN	0x001C

/* Total Page Sizes */
#define		SCSI_RSM_ES_IN_PAGE_SIZE	0x0020
#define		SCSI_RSM_ES_OUT_PAGE_SIZE	0x0016


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_OSESIO_H */
