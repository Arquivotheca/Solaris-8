/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ix_alts.c	1.9	99/01/31 SMI\n"

/*
 * IX_ALTS.C:
 *
 * Alternate sector handling routines.
 *
 */

#define _FAR_ _far         /* for peaceful coexistence...... */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/dktp/altsctr.h>
#include "bootblk.h"

#define physaddr(x)  (paddr_t)(x)   /* from stand/sys/boot.h */

/* #define DEBUG */
/* #define PAUSE */
extern BootDbg;               /* global debug output switch */

#ifdef DEBUG
    #pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
    #pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "ix_alts.c	1.9	99/01/31" )


/*
 * Stand-alone filesystem alternate sector handling routines.
 * NOTE: This set of routines is shared between the primary and secondary
 * boot subsystems (primary boot: real mode, small model; secondary
 * boot: protected mode)
 *
 * The primary-to-secondary boot interface structure is one entity common
 * to both worlds.  This should be used as much as possible to eliminate
 * extra work that would otherwise have to be performed in twice.
 */

struct	d_blk {
	long	sec;     /* cannot be unsigned - initialized to -1! */
	ulong	mem;
	ulong	cnt;
};

struct	alts_part {
	ulong			ap_flag;
	struct alts_parttbl	ap_tbl;
	struct alts_ent  _FAR_ 	*ap_entp;
} alts_part;
struct  alts_part _FAR_ *ap;

struct	dpb {
	ulong			dpb_secsiz;
} dpb;
struct  dpb _FAR_ *dpbp;

struct pri_to_secboot *realp;
char buffer[512];

void bd_getalts ();
void bd_get_altsctr ();
void dsk_sort_altsctr ();

/*
 *	get the alternate sector entry table
 */
void
bd_getalts ( daddr_t alts_start )
{
	ushort	i;

        ap = (struct alts_part _FAR_ *)&alts_part;
        dpbp = (struct dpb _FAR_ *)&dpb;
/*	get disk sector size						*/
	dpbp->dpb_secsiz = NBPSCTR;

        if ( alts_start == 0 )  /* SCSI or disk w/no alts partition */
	{
		ap->ap_tbl.alts_ent_used = 0;
		return;
	}

/*	check for alternate sector/track partition			*/
	bd_get_altsctr ( alts_start );

#ifdef DEBUG
	if (BootDbg & DBG_ALTS) {
		putstr ("bd_getalts:\r\n");
		for (i=0; i<ap->ap_tbl.alts_ent_used; i++) {
			printf ( (char _FAR_ *)"[%d]: badsec= %d altsec= %d count= %d\n",
				i, ap->ap_entp[i].bad_start, ap->ap_entp[i].good_start,
				(ap->ap_entp[i].bad_end - ap->ap_entp[i].bad_start + 1));
		}
	}
#endif
}

#define	byte_to_dsksec(APSIZE, DPBPTR)	(daddr_t) \
					((((APSIZE) +(DPBPTR)->dpb_secsiz - 1) \
					 / (DPBPTR)->dpb_secsiz) \
					 * (DPBPTR)->dpb_secsiz)

/*
 *	get the alternate sector entry table based on the
 *	INTERACTIVE alternate partition mapping scheme
 */
void
bd_get_altsctr ( daddr_t alts_start )
{
	long	dsk_bytes;
	short	dsk_blk;
	char	_FAR_ *buf;
	long	i;

	dsk_bytes = byte_to_dsksec(ALTS_PARTTBL_SIZE, dpbp);
	dsk_blk = dsk_bytes / dpbp->dpb_secsiz;

        buf = (char _FAR_ *)buffer;
        ap = (struct alts_part _FAR_ *)&alts_part;
        dpbp = (struct dpb _FAR_ *)&dpb;

/*	read alternate partition table					*/
	devread ( alts_start, physaddr(buf), dsk_blk );
	if (((struct alts_parttbl _FAR_ *)buf)->alts_sanity != ALTS_SANITY) {
		ap->ap_tbl.alts_ent_used = 0;
		return;
	}

/*	initialize the alternate partition table			*/
	/* structure copy */
	ap->ap_tbl = *(struct alts_parttbl _FAR_ *)buf;
	if (!ap->ap_tbl.alts_ent_used)
		return;

	dsk_blk = ap->ap_tbl.alts_ent_end - ap->ap_tbl.alts_ent_base + 1;
/*	allocate incore alternate entry table				*/
   /* use a static buffer for now */
   ap->ap_entp = (struct alts_ent _FAR_ *)buffer;

/*	read alternate entry table					*/
	devread ( alts_start+ap->ap_tbl.alts_ent_base, physaddr(ap->ap_entp),
		dsk_blk );
}


/*
 * 	bubble sort the entry table into ascending order
 */
void
dsk_sort_altsctr ( struct alts_ent buf[], long cnt )
{
struct	alts_ent temp;
long	flag;
register long	i,j;

	for ( i = 0; i < cnt - 1; i++ ) {
	    temp = buf[cnt-1];
	    flag = 1;

	    for ( j = cnt - 1; j > i; j-- ) {
		   if (buf[j-1].bad_start < temp.bad_start) {
		       buf[j] = temp;
		       temp = buf[j-1];
	   	} else {
		       buf[j] = buf[j-1];
		       flag = 0;
	   	}
	    }
	    buf[i] = temp;
	    if (flag) break;
	}
}


/*
 *	read all disk blocks of the given logical block number
 */
long
disk ( daddr_t secno, paddr_t addr, ulong totcnt )
{

 register short i;
 register long nread, rc;

   struct d_blk d_entry[16];

      Dprintf(DBG_READ, ((char _FAR_ *)"disk: sector %ld, totcnt %ld, addr %lx\n",
               secno, totcnt, (long)addr));

/*	reset the disk block entry array				*/
	for ( i = 0; i < 16; i++ )
		d_entry[i].sec = -1;
/*	initialize the first entry of disk block to request block	*/
	d_entry[0].cnt = totcnt;
	d_entry[0].sec = secno;
	d_entry[0].mem = addr;
/*	perform bad sector remap					*/
	bd_alt_badsec ( (struct d_blk _FAR_ *)d_entry );

#ifdef DEBUG
	if ( BootDbg & DBG_ALTS ) {
		printf ( (char _FAR_ *)"disk: remapped read requests: (cnt/sec/addr)\n" );
		for ( i = 0; i < 16; i++ ) {
			if ( d_entry[i].sec == -1 )
				break;
			printf ( (char _FAR_ *)"d_entry[%d]: %ld   %ld   %lx\n", d_entry[i].cnt,
				d_entry[i].sec, (long)d_entry[i].mem );
		}
	}
#endif

/*	get all disk blocks						*/
	for ( i = 0, rc = nread = 0L; d_entry[i].sec != -1; i++ )
	{
		rc = devread ( d_entry[i].sec, d_entry[i].mem, (ushort) d_entry[i].cnt );
      if ( rc > 0 )
         nread += rc;
	}
   return nread;
}


/*
 *	dsk_alt_badsec remaps the bad sectors to alternates.
 *	There are 7 different cases when the comparison is made
 *	between the bad sector cluster and the disk section.
 *
 *	bad sector cluster	gggggggggggbbbbbbbggggggggggg
 *	case 1:			   ddddd
 *	case 2:				   -d-----
 *	case 3:					     ddddd
 *	case 4:			         dddddddddddd
 *	case 5:			      ddddddd-----
 *	case 6:			           ---ddddddd
 *	case 7:			           ddddddd
 *
 *	where: g = good sector,      b = bad sector
 *	       d = sector in disk section
 *             - = disk section may be extended to cover those disk area
 */
bd_alt_badsec ( struct d_blk _FAR_ *d_entry )
{
	struct	alts_ent _FAR_ *altp;
	struct	d_blk _FAR_ *d_entp;
	struct	d_blk _FAR_ *d_blkp;
	long	alts_used;
	ushort	secsiz;
	daddr_t	lastsec;
	register long	i;
	long	flag_b = 1;
	long	flag_e = 0;

	d_entp = d_entry;
	secsiz = dpbp->dpb_secsiz;
	alts_used = ap->ap_tbl.alts_ent_used;
	altp = ap->ap_entp;
	lastsec = d_entp->sec + d_entp->cnt - 1;

	for (i=0; i<alts_used; ) {
/*	CASE 1:								*/
		while (lastsec < altp->bad_start) {
			d_entp++;
			if (d_entp->sec != -1)
				lastsec = d_entp->sec + d_entp->cnt - 1;
			else
				break;
		}
		if (d_entp->sec == -1) break;

/*	CASE 3:								*/
		if (d_entp->sec > altp->bad_end) {
			i++;
			altp++;
			continue;
		}

   if (flag_b) {
      flag_b = 0;
      flag_e = 1;
      if ( BootDbg & DBG_ALTS ) {
         printf ( (char _FAR_ *)"***** Entering bd_alt_badsec:\n ");
         for ( i=0; d_entry[i].sec != -1; i++ )
            printf ( (char _FAR_ *)"[%d]: sec= %d mem= %d cnt= %d\n", i,
                   d_entry[i].sec, d_entry[i].mem, d_entry[i].cnt );
      }
	}
/*	CASE 2 and 7:							*/
		if ((d_entp->sec >=altp->bad_start) &&
		    (lastsec <= altp->bad_end)) {
      Dprintf(DBG_ALTS, ((char _FAR_ *)"bd_alt_badsec: CASE 2 and 7.\n"));
			d_entp->sec = altp->good_start + d_entp->sec -
					altp->bad_start;
			d_entp++;
			if (d_entp->sec != -1) {
				lastsec = d_entp->sec + d_entp->cnt - 1;
				continue;
			}
			else break;
		}
		d_blkp = d_entp + 1;

/*	CASE 6:								*/
		if ((d_entp->sec <= altp->bad_end) &&
		    (d_entp->sec >= altp->bad_start)) {
      Dprintf(DBG_ALTS, ((char _FAR_ *)"bd_alt_badsec: CASE 6.\n"));
			d_blkp->cnt = d_entp->cnt - (altp->bad_end -
					d_entp->sec + 1);
			d_entp->cnt -= d_blkp->cnt;
			d_blkp->sec = altp->bad_end +1;
			d_blkp->mem = d_entp->mem + d_entp->cnt * secsiz;
			d_entp->sec = altp->good_start + d_entp->sec -
					altp->bad_start;
			d_entp++;
			continue;
		}

/*	CASE 5:								*/
		if ((lastsec >= altp->bad_start) && (lastsec<=altp->bad_end)) {
      Dprintf(DBG_ALTS, ((char _FAR_ *)"bd_alt_badsec: CASE 5.\n"));
			d_blkp->cnt = lastsec - altp->bad_start + 1;
			d_entp->cnt -= d_blkp->cnt;
			d_blkp->sec = altp->good_start;
			d_blkp->mem = d_entp->mem + d_entp->cnt * secsiz;
			break;
		}

/*	CASE 4:								*/
      Dprintf(DBG_ALTS, ((char _FAR_ *)"bd_alt_badsec: CASE 4.\n"));
		d_blkp->sec = altp->good_start;
		d_blkp->cnt = altp->bad_end - altp->bad_start + 1;
		d_entp->cnt = altp->bad_start - d_entp->sec;
		d_blkp->mem = d_entp->mem + d_entp->cnt * secsiz;
		d_entp++;
		d_blkp++;
		d_blkp->cnt = lastsec - altp->bad_end;
		d_blkp->sec = altp->bad_end + 1;
		d_blkp->mem = d_entp->mem + d_entp->cnt * secsiz;
		d_entp++;
	}

	if ( flag_e && (BootDbg & DBG_ALTS)) {
		for ( i=0; d_entry[i].sec != -1; i++ )
			printf ( (char _FAR_ *)"[%d]: sec= %d mem= %d cnt= %d\n", i,
				d_entry[i].sec, d_entry[i].mem, d_entry[i].cnt );
		printf ( (char _FAR_ *)"****** Leaving bd_alt_badsec.\n\n" );
	}
}
