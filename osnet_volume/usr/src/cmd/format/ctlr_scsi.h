
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_CTLR_SCSI_H
#define	_CTLR_SCSI_H

#pragma ident	"@(#)ctlr_scsi.h	1.15	98/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/buf.h>
#include <sys/scsi/scsi.h>
#ifdef sparc
#include <sys/scsi/targets/sddef.h>
#endif


/*
 * Rounded parameter, as returned in Extended Sense information
 */
#define	ROUNDED_PARAMETER	0x37



/*
 * Mode sense/select page header information
 */
struct scsi_ms_header {
	struct mode_header	mode_header;
	struct block_descriptor	block_descriptor;
};

/*
 * Mode Sense Page Control
 */
#define	MODE_SENSE_PC_CURRENT		(0 << 6)
#define	MODE_SENSE_PC_CHANGEABLE	(1 << 6)
#define	MODE_SENSE_PC_DEFAULT		(2 << 6)
#define	MODE_SENSE_PC_SAVED		(3 << 6)

/*
 * Mode Select options
 */
#define	MODE_SELECT_SP			0x01
#define	MODE_SELECT_PF			0x10



/*
 * Minimum length of Request Sense data that we can accept
 */
#define	MIN_REQUEST_SENSE_LEN		18

/*
 * "impossible" status value
 */
#define	IMPOSSIBLE_SCSI_STATUS		0xff

/*
 * Convert a three-byte triplet into an int
 */
#define	TRIPLET(u, m, l)	((int)((((u))&0xff<<16) + \
				(((m)&0xff)<<8) + (l&0xff)))

/*
 * Define the amount of slop we can tolerate on a SCSI-2 mode sense.
 * Usually we try to deal with just the common subset between the
 * the SCSI-2 structure and the CCS structure.  The length of the
 * data returned can vary between targets, so being tolerant gives
 * gives us a higher chance of success.
 */
#define	PAGE1_SLOP		5
#define	PAGE2_SLOP		6
#define	PAGE3_SLOP		3
#define	PAGE4_SLOP		8
#define	PAGE8_SLOP		8
#define	PAGE38_SLOP		8

/*
 * Minimum lengths of a particular SCSI-2 mode sense page that
 * we can deal with.  We must reject anything less than this.
 */
#define	MIN_PAGE1_LEN		(sizeof (struct mode_err_recov)-PAGE1_SLOP)
#define	MIN_PAGE2_LEN		(sizeof (struct mode_disco_reco)-PAGE2_SLOP)
#define	MIN_PAGE3_LEN		(sizeof (struct mode_format)-PAGE3_SLOP)
#define	MIN_PAGE4_LEN		(sizeof (struct mode_geometry)-PAGE4_SLOP)
#define	MIN_PAGE8_LEN		(sizeof (struct mode_cache)-PAGE8_SLOP)
#define	MIN_PAGE38_LEN		(sizeof (struct mode_cache_ccs)-PAGE38_SLOP)

/*
 * Macro to extract the length of a mode sense page
 * as returned by a target.
 */
#define	MODESENSE_PAGE_LEN(p)	(((int)((struct mode_page *)p)->length) + \
					sizeof (struct mode_page))

/*
 * Request this number of bytes for all mode senses.  Since the
 * data returned is self-defining, we can accept anywhere from
 * the minimum for a particular page, up to this maximum.
 * Whatever the drive gives us, we return to the drive, delta'ed
 * by whatever we want to change.
 */
#define	MAX_MODE_SENSE_SIZE		255


#ifdef	__STDC__
/*
 *	Local prototypes for ANSI C compilers
 */
int	scsi_rdwr(int, int, daddr_t, int, caddr_t, int, int *);
int	scsi_ck_format(void);
int	scsi_ms_page1(int);
int	scsi_ms_page2(int);
int	scsi_ms_page3(int);
int	scsi_ms_page4(int);
int	scsi_ex_man(struct defect_list *);
int	scsi_ex_cur(struct defect_list *);
int	scsi_ex_grown(struct defect_list *);
int	scsi_read_defect_data(struct defect_list *, int);
int	scsi_repair(int, int);
int	uscsi_cmd(int, struct uscsi_cmd *, int);
int	uscsi_mode_sense(int, int, int, caddr_t, int,
		struct scsi_ms_header *);
int	uscsi_mode_select(int, int, int, caddr_t, int,
		struct scsi_ms_header *);
int	uscsi_inquiry(int, caddr_t, int);
int	uscsi_read_capacity(int, struct scsi_capacity *);
int	uscsi_reserve_release(int, int);
int	scsi_translate(int, struct scsi_bfi_defect *);
int	scsi_dump_mode_sense_pages(int);
int	scsi_supported_page(int);
int	apply_chg_list(int, int, u_char *, u_char *, struct chg_list *);
int	scsi_format_time(void);
int	scsi_format_without_defects(void);
int	check_support_for_defects(void);

#else

int	scsi_rdwr();
int	scsi_ck_format();
int	scsi_ms_page1();
int	scsi_ms_page2();
int	scsi_ms_page3();
int	scsi_ms_page4();
int	scsi_ex_man();
int	scsi_ex_cur();
int	scsi_ex_grown();
int	scsi_read_defect_data();
int	scsi_repair();
int	uscsi_cmd();
int	uscsi_mode_sense();
int	uscsi_mode_select();
int	uscsi_inquiry();
int	uscsi_read_capacity();
int	scsi_translate();
int	scsi_dump_mode_sense_pages();
int	scsi_supported_page();
int	apply_chg_list();
int	scsi_format_time();
int	uscsi_reserve_release();
int	scsi_format_without_defects(void);
int	check_support_for_defects(void);

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _CTLR_SCSI_H */
