/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_GENERIC_SENSE_H
#define	_SYS_SCSI_GENERIC_SENSE_H

#pragma ident	"@(#)sense.h	1.15	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Standard (Non-Extended) SCSI Sense.
 *
 * For Error Classe 0-6. This is all
 * Vendor Unique sense information.
 *
 * Note: This is pre-SCSI-2.
 */

struct scsi_sense {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	ns_code		: 4,	/* Vendor Uniqe error code 	*/
		ns_class	: 3,	/* Error class 			*/
		ns_valid	: 1;	/* Logical Block Address is val */
	uchar_t	ns_lba_hi	: 5,	/* High Logical Block Address */
		ns_vu		: 3;	/* Vendor Unique value */
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t	ns_valid	: 1,	/* Logical Block Address is valid */
		ns_class	: 3,	/* Error class */
		ns_code		: 4;	/* Vendor Uniqe error code */
	uchar_t	ns_vu		: 3,	/* Vendor Unique value */
		ns_lba_hi	: 5;	/* High Logical Block Address */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
	uchar_t	ns_lba_mid;		/* Middle Logical Block Address */
	uchar_t	ns_lba_lo;		/* Low part of Logical Block Address */
};

/*
 * SCSI Extended Sense structure
 *
 * For Error Class 7, the Extended Sense Structure is applicable.
 *
 */

#define	CLASS_EXTENDED_SENSE	0x7	/* indicates extended sense */

struct scsi_extended_sense {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	es_code		: 4,	/* Vendor Unique error code 	*/
		es_class	: 3,	/* Error Class- fixed at 0x7 	*/
		es_valid	: 1;	/* sense data is valid 		*/

	uchar_t	es_segnum;		/* segment number: for COPY cmd */

	uchar_t	es_key		: 4,	/* Sense key (see below) 	*/
				: 1,	/* reserved 			*/
		es_ili		: 1,	/* Incorrect Length Indicator 	*/
		es_eom		: 1,	/* End of Media 		*/
		es_filmk	: 1;	/* File Mark Detected 		*/
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t	es_valid	: 1,	/* sense data is valid */
		es_class	: 3,	/* Error Class- fixed at 0x7 */
		es_code		: 4;	/* Vendor Unique error code */

	uchar_t	es_segnum;		/* segment number: for COPY cmd */

	uchar_t	es_filmk	: 1,	/* File Mark Detected */
		es_eom		: 1,	/* End of Media */
		es_ili		: 1,	/* Incorrect Length Indicator */
				: 1,	/* reserved */
		es_key		: 4;	/* Sense key (see below) */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */

	uchar_t	es_info_1;		/* information byte 1 */
	uchar_t	es_info_2;		/* information byte 2 */
	uchar_t	es_info_3;		/* information byte 3 */
	uchar_t	es_info_4;		/* information byte 4 */
	uchar_t	es_add_len;		/* number of additional bytes */

	uchar_t	es_cmd_info[4];		/* command specific information */
	uchar_t	es_add_code;		/* Additional Sense Code */
	uchar_t	es_qual_code;		/* Additional Sense Code Qualifier */
	uchar_t	es_fru_code;		/* Field Replaceable Unit Code */
	uchar_t	es_skey_specific[3];	/* Sense Key Specific information */

	/*
	 * Additional bytes may be defined in each implementation.
	 * The actual amount of space allocated for Sense Information
	 * is also implementation dependent.
	 *
	 * Modulo that, the declaration of an array two bytes in size
	 * nicely rounds this entire structure to a size of 20 bytes.
	 */

	uchar_t	es_add_info[2];		/* additional information */

};


/*
 * Sense Key values for Extended Sense.
 */

#define	KEY_NO_SENSE		0x00
#define	KEY_RECOVERABLE_ERROR	0x01
#define	KEY_NOT_READY		0x02
#define	KEY_MEDIUM_ERROR	0x03
#define	KEY_HARDWARE_ERROR	0x04
#define	KEY_ILLEGAL_REQUEST	0x05
#define	KEY_UNIT_ATTENTION	0x06
#define	KEY_WRITE_PROTECT	0x07
#define	KEY_DATA_PROTECT	KEY_WRITE_PROTECT
#define	KEY_BLANK_CHECK		0x08
#define	KEY_VENDOR_UNIQUE	0x09
#define	KEY_COPY_ABORTED	0x0A
#define	KEY_ABORTED_COMMAND	0x0B
#define	KEY_EQUAL		0x0C
#define	KEY_VOLUME_OVERFLOW	0x0D
#define	KEY_MISCOMPARE		0x0E
#define	KEY_RESERVED		0x0F

#ifdef	__cplusplus
}
#endif

/*
 * Each implementation will have specific mappings to what
 * Sense Information means
 */

#include <sys/scsi/impl/sense.h>

#endif	/* _SYS_SCSI_GENERIC_SENSE_H */
