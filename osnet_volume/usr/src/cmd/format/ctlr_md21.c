
#ifndef lint
#ident	"@(#)ctlr_md21.c	1.9	98/06/05 SMI"
#endif	lint

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains the ctlr dependent routines for the md21 ctlr.
 */
#include "global.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/scsi/scsi.h>
#include <sys/scsi/targets/sddef.h>

#include "ctlr_scsi.h"


#include "startup.h"
#include "scsi_com.h"
#include "misc.h"
#include "ctlr_md21.h"
#include "ctlr_scsi.h"
#include "param.h"
#include "analyze.h"

#define	OLD_DEFECT_MAGIC    0xdefe

struct	ctlr_ops md21ops = {
	md_rdwr,
	md_ck_format,
	md_format,
	md_ex_man,
	md_ex_cur,
	md_repair,
	0,
};

/*
 * Read or write the disk.
 */
int
md_rdwr(dir, file, blkno, secnt, bufaddr, flags, xfercntp)
	int	dir;
	int	file;
	daddr_t blkno;
	int	secnt;
	caddr_t bufaddr;
	int	flags;
	int	*xfercntp;

{
	return (scsi_rdwr(dir, file, blkno, secnt, bufaddr, flags, xfercntp));
}

/*
 * Check to see if the disk has been formatted.
 * If we are able to read the first track, we conclude that
 * the disk has been formatted.
 */
int
md_ck_format()
{
	return (scsi_ck_format());
}


/*
 * Format the disk, the whole disk, and nothing but the disk.
 */
/*ARGSUSED*/
int
md_format(start, end, list)
	int start, end;				/* irrelevant for us */
	struct defect_list *list;
{
	struct scsi_format_params	*format_params;
	struct defect_entry		*defect_entry;
	struct uscsi_cmd		ucmd;
	union scsi_cdb			cdb;
	int				status;
	char				rawbuf[255];
	int				num_defects;
	int				bn;
	int				i;
	int				nbytes;


	/*
	 * Issue an inquiry, for debugging purposes
	 */
	if (uscsi_inquiry(cur_file, rawbuf, sizeof (rawbuf))) {
		err_print("Inquiry failed\n");
	}

	/*
	 * Set up the various SCSI parameters specified before
	 * formatting the disk.  Each routine handles the
	 * parameters relevent to a particular page.
	 * If no parameters are specified for a page, there's
	 * no need to do anything.  Otherwise, issue a mode
	 * sense for that page.  If a specified parameter
	 * differs from the drive's default value, and that
	 * parameter is not fixed, then issue a mode select to
	 * set the default value for the disk as specified
	 * in format.dat.
	 */
	if (scsi_ms_page1(0) || scsi_ms_page2(0) || scsi_ms_page4(0) ||
			scsi_ms_page3(0)) {
		return (-1);
	}

	/*
	 * If we're debugging the drive, dump every page
	 * the device supports, for thorough analysis.
	 */
	if (option_msg && diag_msg) {
		(void) scsi_dump_mode_sense_pages(MODE_SENSE_PC_DEFAULT);
		(void) scsi_dump_mode_sense_pages(MODE_SENSE_PC_CURRENT);
		(void) scsi_dump_mode_sense_pages(MODE_SENSE_PC_SAVED);
		(void) scsi_dump_mode_sense_pages(MODE_SENSE_PC_CHANGEABLE);
		err_print("\n");
	}

	/*
	 * Mash the defect list into the proper format.
	 */

	if (cur_list.list != NULL) {
		format_params = (struct scsi_format_params *)
					zalloc((list->header.count * 8) + 4);
		defect_entry = list->list;
		for (num_defects = 0, i = 0; i < list->header.count; i++) {
			if (defect_entry->bfi != -1) {
				format_params->list[i].cyl =
						defect_entry->cyl;
				format_params->list[i].head =
						defect_entry->head;
				format_params->list[i].bytes_from_index =
						defect_entry->bfi;
				num_defects++;
			}
			defect_entry++;
		}
		format_params->length = num_defects * 8;
		nbytes = (num_defects * 8) + 4;
	} else {
		format_params = (struct scsi_format_params *)zalloc(4);
		nbytes = 4;
		if (option_msg) {
			err_print(
"Warning: Using internal drive defect list only.\n");
		}
	}

	/*
	 * Construct the uscsi format ioctl.
	 */
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	(void) memset((char *)&cdb, 0, sizeof (union scsi_cdb));
	cdb.scc_cmd = SCMD_FORMAT;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	cdb.cdb_opaque[1] = FPB_DATA | FPB_CMPLT | FPB_BFI;
	cdb.cdb_opaque[4] = 1;
	ucmd.uscsi_bufaddr = (caddr_t)format_params;
	ucmd.uscsi_buflen = nbytes;

	/*
	 * Issue the format ioctl
	 */
	fmt_print("Formatting...\n");
	(void) fflush(stdout);
	status = uscsi_cmd(cur_file, &ucmd, F_NORMAL);

	free((char *)format_params);

	if (status)
		return (status);

	/*
	 * Put defect list back in place
	 */
	if (cur_list.list != NULL) {
	    defect_entry = list->list;
	    for (i = 0; i < list->header.count; i++) {
		if (defect_entry->bfi == -1) {
		    bn = chs2bn(defect_entry->cyl, defect_entry->head,
				defect_entry->sect);
		    status = md_repair(bn, 0);
		    if (status)
			return (status);
		}
		defect_entry++;
	    }
	}

	return (status);
}

/*
 * Extract the manufacturer's defect list.
 */
int
md_ex_man(list)
	struct  defect_list	*list;
{
	int	status;

	/*
	 * First try to read the grown list
	 */
	status = scsi_read_defect_data(list, DLD_GROWN_DEF_LIST);
	if (status == 0)
		return (0);

	/*
	 * Try for the manufacturer's list
	 */
	status = scsi_read_defect_data(list, DLD_MAN_DEF_LIST);
	return (status);

}

/*
 * Extract the current defect list.
 * If we find an old format defect list, we should upgrade it
 * to a new list.
 */
int
md_ex_cur(list)
	struct defect_list		*list;
{
	struct scsi_format_params	*old_defect_list;
	int				sec_offset;
	daddr_t				blkno;

	blkno = chs2bn(ncyl, 0, 0);
	old_defect_list = (struct scsi_format_params *)cur_buf;

	/*
	 * Try hard to find the old defect list.
	 */
	for (sec_offset = 0; sec_offset < nsect; sec_offset += 2) {
		blkno += 2;
		old_defect_list->reserved = 0;
		if (scsi_rdwr(DIR_READ, cur_file, blkno, 2,
			cur_buf, F_SILENT, NULL)) {
				continue;
		}
		if (old_defect_list->reserved == OLD_DEFECT_MAGIC) {
			convert_old_list_to_new(list,
				(struct scsi_format_params *)cur_buf);
			return (0);
		}
	}
	return (-1);
}


/*
 * Map a block.
 */
/*ARGSUSED*/
int
md_repair(bn, flag)
	int	bn;
	int	flag;
{
	return (scsi_repair(bn, flag));
}


/*
 * This routine converts an old-format to a new-format defect list.
 * In the process, it allocates space for the new list.
 */
void
convert_old_list_to_new(list, old_defect_list)
	struct  defect_list *list;
	struct	scsi_format_params *old_defect_list;
{
	register u_short len = old_defect_list->length / 8;
	register struct scsi_bfi_defect *old_defect = old_defect_list->list;
	register struct defect_entry *new_defect;
	register int i;
	int size;	/* size in blocks of the allocated list */

	/*
	 * Allocate space for the rest of the list.
	 */
	list->header.magicno = (u_int) DEFECT_MAGIC;
	list->header.count = len;

	size = LISTSIZE(list->header.count);
	list->list = new_defect = (struct defect_entry *)zalloc(size * SECSIZE);

	for (i = 0; i < (int)len; i++, new_defect++, old_defect++) {
		/* copy 'em in */
		new_defect->cyl = (short)old_defect->cyl;
		new_defect->head = (short)old_defect->head;
		new_defect->bfi = (int)old_defect->bytes_from_index;
		new_defect->nbits = UNKNOWN;	/* size of defect */
	}
	(void) checkdefsum(list, CK_MAKESUM);
}
