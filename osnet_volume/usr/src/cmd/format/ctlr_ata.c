
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)ctlr_ata.c	1.19	98/06/05 SMI"


/*
 * This file contains the routines for the IDE drive interface
 */
#include "global.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/byteorder.h>
#include <errno.h>
#if defined(i386)
#include <sys/dktp/altsctr.h>
#endif
#include <sys/dktp/dadkio.h>


#include "startup.h"
#include "misc.h"
#include "ctlr_ata.h"
#include "analyze.h"
#include "param.h"
#include "io.h"
#include "badsec.h"

#include "menu_fdisk.h"

int	wr_altsctr();
int	read_altsctr();
int	updatebadsec();


struct  ctlr_ops ataops = {
#if defined(sparc)
	ata_rdwr,
	ata_ck_format,
	0,
	0,
	0,
	0,
	0,
	0,
#else
	ata_rdwr,
	ata_ck_format,
	0,
	0,
	ata_ex_cur,
	ata_repair,
	0,
	ata_wr_cur,
#endif	/* defined(sparc) */
};


struct	partition	*dpart = NULL;
extern	struct	badsec_lst	*badsl_chain;
extern	int	badsl_chain_cnt;
extern	struct	badsec_lst	*gbadsl_chain;
extern	int	gbadsl_chain_cnt;
extern	struct	alts_mempart	*ap;

char *dadkrawioerrs[] = {
	"cmd was successful",		/* DADKIO_STAT_NO_ERROR */
	"device not ready",		/* DADKIO_STAT_NOT_READY */
	"error on medium blkno: %d",	/* DADKIO_STAT_MEDIUM_ERROR */
	"other hardware error",		/* DADKIO_STAT_HARDWARE_ERROR */
	"illegal request",		/* DADKIO_STAT_ILLEGAL_REQUEST */
	"illegal block address: %d",	/* DADKIO_STAT_ILLEGAL_ADDRESS */
	"device write-protected",	/* DADKIO_STAT_WRITE_PROTECTED	*/
	"no response from device",	/* DADKIO_STAT_TIMED_OUT */
	"parity error in data",		/* DADKIO_STAT_PARITY */
	"error on bus",			/* DADKIO_STAT_BUS_ERROR */
	"data recovered via ECC",	/* DADKIO_STAT_SOFT_ERROR */
	"no resources for cmd",		/* DADKIO_STAT_NO_RESOURCES */
	"device is not formatted",	/* DADKIO_STAT_NOT_FORMATTED */
	"device is reserved",		/* DADKIO_STAT_RESERVED */
	"feature not supported",	/* DADKIO_STAT_NOT_SUPPORTED */
	};

int
ata_rdwr(dir, fd, blkno, secnt, bufaddr, flags, xfercntp)
	int	dir;
	int	fd;
	daddr_t blkno;
	int	secnt;
	caddr_t bufaddr;
	int	flags;
	int	*xfercntp;


{
	int	tmpsec;
	struct dadkio_rwcmd	dadkio_rwcmd;

	bzero((caddr_t)&dadkio_rwcmd, sizeof (struct dadkio_rwcmd));

	tmpsec = secnt * 512;

	/* Doing raw read */
	dadkio_rwcmd.cmd = (dir == DIR_READ) ? DADKIO_RWCMD_READ :
					DADKIO_RWCMD_WRITE;
	dadkio_rwcmd.blkaddr = blkno;
	dadkio_rwcmd.buflen  = tmpsec;
	dadkio_rwcmd.flags   = flags;
	dadkio_rwcmd.bufaddr = bufaddr;

	media_error = 0;
	if (cur_ctype->ctype_ctype == DKC_PCMCIA_ATA) {
		/*
		 * PCATA requires to use "p0" when calling
		 *	DIOCTL_RWCMD ioctl() to read/write the label
		 */
		close(fd);
		(void) open_cur_file(FD_USE_P0_PATH);
		fd = cur_file;
	}

	if (ioctl(fd, DIOCTL_RWCMD, &dadkio_rwcmd) == -1) {
		err_print("DIOCTL_RWCMD: %s\n", strerror(errno));
		return (1);
	}

	if (cur_ctype->ctype_ctype == DKC_PCMCIA_ATA) {
		/* Restore cur_file with cur_disk->disk_path */
		(void) open_cur_file(FD_USE_CUR_DISK_PATH);
	}

	switch (dadkio_rwcmd.status.status) {
	case  DADKIO_STAT_NOT_READY:
			disk_error = DISK_STAT_NOTREADY;
			break;
	case  DADKIO_STAT_RESERVED:
			disk_error = DISK_STAT_RESERVED;
			break;
	case DADKIO_STAT_MEDIUM_ERROR:
			media_error = 1;
			break;
	}

	if (dadkio_rwcmd.status.status) {
		if ((flags & F_SILENT) == 0)
			err_print(dadkrawioerrs[dadkio_rwcmd.status.status],
				dadkio_rwcmd.status.failed_blk);
		return (1);
	}
	return (0);
}

int
ata_ck_format()
{
	unsigned char bufaddr[2048];
	int status;


	status = ata_rdwr(DIR_READ, cur_file, 1, 4, (caddr_t)bufaddr, 0, NULL);

	return (!status);

}


#if defined(i386)

int
get_alts_slice()
{

	int	i;
	int	alts_slice = -1;

	if (cur_parts == NULL) {
		fprintf(stderr, "No current partition list\n");
		return (-1);
	}

	for (i = 0; i < V_NUMPAR && alts_slice == -1; i++) {
		if (cur_parts->vtoc.v_part[i].p_tag == V_ALTSCTR) {
			alts_slice = i;
			dpart = (struct partition *)&cur_parts->vtoc.v_part[i];
		}
	}

	if (alts_slice == -1) {
		fprintf(stderr, "NO Alt slice\n");
		return (-1);
	}
	if (!solaris_offset)
		if (copy_solaris_part(&cur_disk->fdisk_part))
			return (-1);

	altsec_offset = dpart->p_start + solaris_offset;

	return (SUCCESS);
}


int
put_alts_slice()
{
	int	status;

	status = wr_altsctr();
	if (status) {
		return (status);
	}

	if (ioctl(cur_file, DKIOCADDBAD, NULL) == -1) {
		fprintf(stderr, "Warning: DKIOCADDBAD ioctl failed\n");
		sync();
		return (-1);
	}
	sync();
	return (0);
}

ata_convert_list(list, list_format)
	struct defect_list	*list;
	int	list_format;
{

	int	i;
	struct  defect_entry    *new_defect;

	switch (list_format) {

	case BFI_FORMAT:
		if (ap->ap_tblp->alts_ent_used) {
			new_defect = (struct defect_entry *)
					calloc(ap->ap_tblp->alts_ent_used,
					    sizeof (struct defect_entry));
			list->header.count = ap->ap_tblp->alts_ent_used;
			list->header.magicno = (u_int) DEFECT_MAGIC;
			list->list = new_defect;
			for (i = 0; i < ap->ap_tblp->alts_ent_used;
					    i++, new_defect++) {
				new_defect->cyl =
					    bn2c((ap->ap_entp)[i].bad_start);
				new_defect->head =
					    bn2h((ap->ap_entp)[i].bad_start);
				new_defect->bfi = UNKNOWN;
				new_defect->sect =
					    bn2s((ap->ap_entp)[i].bad_start);
				new_defect->nbits = UNKNOWN;
			}


		} else {

			list->header.count = 0;
			list->header.magicno = (u_int) DEFECT_MAGIC;
			new_defect = (struct defect_entry *)
					calloc(1,
					    sizeof (struct defect_entry));
			list->list = new_defect;
		}
		break;

	default:
		err_print("ata_convert_list: can't deal with it\n");
		exit(0);
	}
	(void) checkdefsum(list, CK_MAKESUM);
	return (0);
}


/*
 * NB - there used to be a ata_ex_man() which was identical to
 * ata_ex_cur; since it's really not a "manufacturer's list",
 * it's gone; if we ever want that exact functionality back,
 * we can add ata_ex_cur() to the ctlr_ops above.  Otherwise,
 * if this is ever modified to support formatting of IDE drives,
 * we should probably add something that issues the
 * drive Read Defect list rather than getting the s9 info
 * as ata_ex_cur() does.
 */


ata_ex_cur(list)
	struct	defect_list	*list;
{
	int	status;

	status = get_alts_slice();
	if (status)
		return (status);
	status = read_altsctr(dpart);
	if (status) {
		return (status);
	}
	ata_convert_list(list, BFI_FORMAT);
	return (status);
}

ata_repair(bn, flag)
	int	bn;
	int	flag;
{

	int	status;
	struct	badsec_lst	*blc_p;
	struct	badsec_lst	*blc_p_nxt;

#ifdef lint
	flag++;
#endif

	get_alts_slice();
	if (!gbadsl_chain) {
		blc_p = (struct badsec_lst *)calloc(1, BADSLSZ);
		if (!blc_p) {
			fprintf(stderr,
		"Unable to allocate memory for additional bad sectors\n");
			return (-1);
		}
		gbadsl_chain = blc_p;
	}
	for (blc_p = gbadsl_chain; blc_p->bl_nxt; )
		blc_p = blc_p->bl_nxt;

	if (blc_p->bl_cnt == MAXBLENT) {
		blc_p->bl_nxt = (struct badsec_lst *)calloc(1, BADSLSZ);
		if (!blc_p->bl_nxt) {
			fprintf(stderr,
		"Unable to allocate memory for additional bad sectors\n");
			return (-1);
		}
		blc_p = blc_p->bl_nxt;
	}
	blc_p->bl_sec[blc_p->bl_cnt++] = bn;
	gbadsl_chain_cnt++;

	updatebadsec(dpart, 0);
	status = put_alts_slice();

	/* clear out the bad sector list chains that were generated */

	if (badsl_chain) {
		if (badsl_chain->bl_nxt == NULL) {
			free(badsl_chain);
		} else {
			for (blc_p = badsl_chain; blc_p; ) {
				blc_p_nxt = blc_p->bl_nxt;
				free(blc_p);
				blc_p = blc_p_nxt;
			}
		}
		badsl_chain = NULL;
		badsl_chain_cnt = 0;
	}

	if (gbadsl_chain) {
		if (gbadsl_chain->bl_nxt == NULL) {
			free(gbadsl_chain);
		} else {
			for (blc_p = gbadsl_chain; blc_p; ) {
				blc_p_nxt = blc_p->bl_nxt;
				free(blc_p);
				blc_p = blc_p_nxt;
			}
		}
		gbadsl_chain = NULL;
		gbadsl_chain_cnt = 0;
	}

	return (status);

}


ata_wr_cur(list)
	struct defect_list *list;
{
	int	status;
	int	sec_count;
	int	x;
	struct	badsec_lst	*blc_p;
	struct	badsec_lst	*blc_p_nxt;
	struct	defect_entry	*dlist;

	if (list->header.magicno != (u_int) DEFECT_MAGIC)
		return (-1);

	sec_count = list->header.count;
	dlist = list->list;

	get_alts_slice();
	for (x = 0; x < sec_count; x++) {

		/* test for unsupported list format */
		if ((dlist->bfi != UNKNOWN) || (dlist->nbits != UNKNOWN)) {
			fprintf(stderr,
				"BFI unsuported format for bad sectors\n");
			return (-1);
		}

		if (!gbadsl_chain) {
			blc_p = (struct badsec_lst *)calloc(1, BADSLSZ);
			if (!blc_p) {
				fprintf(stderr,
		"Unable to allocate memory for additional bad sectors\n");
				return (-1);
			}
			gbadsl_chain = blc_p;
		}

		for (blc_p = gbadsl_chain; blc_p->bl_nxt; )
			blc_p = blc_p->bl_nxt;

		if (blc_p->bl_cnt == MAXBLENT) {
			blc_p->bl_nxt = (struct badsec_lst *)calloc(1, BADSLSZ);
			if (!blc_p->bl_nxt) {
				fprintf(stderr,
		"Unable to allocate memory for additional bad sectors\n");
				return (-1);
			}
			blc_p = blc_p->bl_nxt;
		}
		blc_p->bl_sec[blc_p->bl_cnt++] =
			    chs2bn(dlist->cyl, dlist->head, dlist->sect);
		gbadsl_chain_cnt++;
		dlist++;
	}


	updatebadsec(dpart, 0);
	status = put_alts_slice();

	/* clear out the bad sector list chains that were generated */

	if (badsl_chain) {
		if (badsl_chain->bl_nxt == NULL) {
			free(badsl_chain);
		} else {
			for (blc_p = badsl_chain; blc_p; ) {
				blc_p_nxt = blc_p->bl_nxt;
				free(blc_p);
				blc_p = blc_p_nxt;
			}
		}
		badsl_chain = NULL;
		badsl_chain_cnt = 0;
	}

	if (gbadsl_chain) {
		if (gbadsl_chain->bl_nxt == NULL) {
			free(gbadsl_chain);
		} else {
			for (blc_p = gbadsl_chain; blc_p; ) {
				blc_p_nxt = blc_p->bl_nxt;
				free(blc_p);
				blc_p = blc_p_nxt;
			}
		}
		gbadsl_chain = NULL;
		gbadsl_chain_cnt = 0;
	}

	return (status);
}

#endif	/*  defined(i386)  */
