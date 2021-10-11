/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_ALTSCTR_H
#define	_SYS_DKTP_ALTSCTR_H

#pragma ident	"@(#)altsctr.h	1.5	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*	alternate sector partition information table			*/
struct	alts_parttbl {
	long	alts_sanity;	/* to validate correctness		*/
	long  	alts_version;	/* version number			*/
	daddr_t	alts_map_base;	/* disk offset of alts_partmap		*/
	long	alts_map_len;	/* byte length of alts_partmap		*/
	daddr_t	alts_ent_base;	/* disk offset of alts_entry		*/
	long	alts_ent_used;	/* number of alternate entries used	*/
	long	alts_ent_end;	/* disk offset of top of alts_entry	*/
	daddr_t	alts_resv_base;	/* disk offset of alts_reserved		*/
	long 	alts_pad[5];	/* reserved fields			*/
};

/*	alternate sector remap entry table				*/
struct	alts_ent {
	daddr_t	bad_start;	/* starting bad sector number		*/
	daddr_t	bad_end;	/* ending bad sector number		*/
	daddr_t	good_start;	/* starting alternate sector to use	*/
};

/*	size of alternate partition table structure			*/
#define	ALTS_PARTTBL_SIZE	sizeof (struct alts_parttbl)
/*	size of alternate entry table structure				*/
#define	ALTS_ENT_SIZE	sizeof (struct alts_ent)

/*	definition for alternate sector partition sector map		*/
#define	ALTS_GOOD	0	/* good alternate sectors		*/
#define	ALTS_BAD	1	/* bad alternate sectors		*/

/*	definition for alternate sector partition id			*/
#define	ALTS_SANITY	0xaabbccdd /* magic number to validate alts_part */
#define	ALTS_VERSION1	0x01	/* version of alts_parttbl		*/

#define	ALTS_ENT_EMPTY	-1	/* empty alternate entry		*/
#define	ALTS_MAP_UP	1	/* search forward with increasing sect# */
#define	ALTS_MAP_DOWN	-1	/* search backward with decreasing sect# */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_ALTSCTR_H */
