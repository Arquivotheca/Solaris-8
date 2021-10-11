/*
 * Copyright 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * PHOTON CONFIGURATION MANAGER
 * Common definitions
 */

/*
 * I18N message number ranges
 *  This file: 12500 - 12999
 *  Shared common messages: 1 - 1999
 */

#ifndef	_COMMON
#define	_COMMON

#pragma ident	"@(#)common.h	1.12	99/07/29 SMI"


/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

extern	char	*p_error_msg_ptr;

/* Defines */
#define	USEAGE()	{(void) fprintf(stderr,  MSGSTR(12500, \
			"Usage: %s [-v] subcommand [option...]" \
			" {enclosure[,dev]... | pathname...}\n"), \
			whoami); \
			(void) fflush(stderr); }

#define	E_USEAGE()	{(void) fprintf(stderr,  MSGSTR(12501, \
			"Usage: %s [-v] -e subcommand [option...]" \
			" {enclosure[,dev]... | pathname...}\n"), \
			whoami); \
			(void) fflush(stderr); }

#define	VERBPRINT	 if (Options & PVERBOSE) (void) printf

#define	L_ERR_PRINT	\
			if (p_error_msg_ptr == NULL) {  \
				perror(MSGSTR(12502, "Error"));	 \
			} else {	\
	(void) fprintf(stderr, MSGSTR(12503, "Error: %s"), p_error_msg_ptr); \
			} \
			p_error_msg_ptr = NULL;

#define	P_ERR_PRINT	 if (p_error_msg_ptr == NULL) {  \
					perror(whoami);	 \
			} else {	\
	(void) fprintf(stderr, MSGSTR(12504, "Error: %s"), p_error_msg_ptr); \
			} \
			p_error_msg_ptr = NULL;

/* Display extended mode page information. */
#ifndef	MODEPAGE_CACHING
#undef	MODEPAGE_CACHING
#define	MODEPAGE_CACHING	0x08
#endif


/* Primary commands */
#define	ENCLOSURE_NAMES 100
#define	DISPLAY	 101
#define	DOWNLOAD	102
#define	FAST_WRITE	400	 /* SSA */
#define	FC_UPDATE	401	 /* SSA */
#define	FCAL_UPDATE	103	 /* Update the Fcode on Sbus soc card */
#define	FCODE_UPDATE	117	 /* Update the Fcode on all cards */
#define	QLGC_UPDATE	116	 /* Update the Fcode on PCI card(s) */
#define	INQUIRY		105
#define	LED		107
#define	LED_ON		108
#define	LED_OFF		109
#define	LED_BLINK	110
#define	NVRAM_DATA	402	 /* SSA */
#define	POWER_OFF	403	 /* SSA */
#define	POWER_ON	111
#define	PASSWORD	112
#define	PURGE		404	 /* SSA */
#define	PERF_STATISTICS 405	 /* SSA */
#define	PROBE		113
#define	RELEASE		210
#define	RESERVE		211
#define	START		213
#define	STOP		214
#define	SYNC_CACHE	406	 /* SSA */
#define	SET_BOOT_DEV	115	 /* Set the boot-device variable in nvram */

/* Enclosure Specific */
#define	ALARM		407	 /* SSA */
#define	ALARM_OFF	408	 /* SSA */
#define	ALARM_ON	409	 /* SSA */
#define	ALARM_SET	410	 /* SSA */
#define	ENV_DISPLAY	411	 /* SSA */

/* Expert commands */
#define	RDLS		215
#define	P_BYPASS	218
#define	P_ENABLE	219
#define	BYPASS		220
#define	ENABLE		221
#define	FORCELIP	222
#define	P_OFFLINE	223
#define	P_ONLINE	224

/* Undocumented commands */
#define	DUMP		300
#define	CHECK_FILE	301	/* Undocumented - Check download file */
#define	DUMP_MAP	302	/* Dump map of loop */
#define	VERSION		303	/* undocumented */
#define	AU		304	/* undocumented */

/* Undocumented diagnostic subcommands */
#define	SYSDUMP	 350


/* SSA - for adm_download */
#define	SSAFIRMWARE_FILE	"/usr/lib/firmware/ssa/ssafirmware"

/* Default FCODE dir - for adm_fcode */
#define	FCODE_DIR	"/usr/lib/firmware/fc_s"

/*	Global variables	*/
char			*whoami;
int	Options;
const	OPTION_A	= 0x00000001;
const	OPTION_B	= 0x00000002;
const	OPTION_C	= 0x00000004;
const	OPTION_D	= 0x00000008;
const	OPTION_E	= 0x00000010;
const	OPTION_F	= 0x00000020;
const	OPTION_L	= 0x00000040;
const	OPTION_P	= 0x00000080;
const	OPTION_R	= 0x00000100;
const	OPTION_T	= 0x00000200;
const	OPTION_V	= 0x00000400;
const	OPTION_W	= 0x00000800;
const	OPTION_Z	= 0x00001000;
const	OPTION_Y	= 0x00002000;
const	OPTION_CAPF	= 0x00004000;
const	PVERBOSE	= 0x00008000;
const	SAVE		= 0x00010000;
const	EXPERT		= 0x00020000;


#define		TARGET_ID(box_id, f_r, slot)	\
		((box_id | ((f_r == 'f' ? 0 : 1) << 4)) | (slot + 2))

#define		NEWER(time1, time2) 	(time1.tv_sec > time2.tv_sec)


#ifdef	__cplusplus
}
#endif

#endif	/* _COMMON */
