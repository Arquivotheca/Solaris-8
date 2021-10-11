/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)io.c	1.8	99/08/13 SMI"

/*LINTLIBRARY*/

/*
 *
 *	This module is part of the photon Command Line
 *	Interface program.
 *
 */

/*
 * I18N message number ranges
 *  This file: 11500 - 11999
 *  Shared common messages: 1 - 1999
 */

/* #define		_POSIX_SOURCE 1 */

/*	Includes	*/
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<sys/file.h>
#include	<sys/types.h>
#include	<fcntl.h>
#include	<sys/sunddi.h>
#include	<sys/systm.h>
#include	<sys/scsi/scsi.h>
#include	<nl_types.h>
#include	<unistd.h>
#include	<l_common.h>
#include	<stgcom.h>
#include	<l_error.h>
#include	<g_state.h>


/*	Defines		*/
/* Because of a bug in Unisys Envsen card,  Bug ID:1266986. */
#define	SCSI_ESI_PCV	0x11		/* Page Code Valid */
#define	SCSI_ESI_PF	0x10		/* Page Format */
#define	ACTION_MASK	0x1f		/* Persistent Reserve In command */
#define	IMMED		1		/* make the stop immediate */



/*	Global variables	*/
extern	nl_catd l_catd;



/*	External functions	*/


int
g_scsi_persistent_reserve_in_cmd(int fd, uchar_t *buf_ptr,
	int buf_len, uchar_t action)
{
struct uscsi_cmd	ucmd;
my_cdb_g1	cdb = {SCMD_PERS_RESERV_IN, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.byte1 = action & ACTION_MASK;
	cdb.byte7 = (buf_len>>8) & 0xff;
	cdb.byte8 = buf_len & 0xff;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;

	if (buf_len & 0x03) {
		return (L_PR_INVLD_TRNSFR_LEN);
	}
	/* Do in SILENT mode as cmd may not be supported. */
	return (cmd(fd, &ucmd, USCSI_READ | USCSI_SILENT));
}
/*
 *	Send Diagnostic command
 *
 *	NOTE: This function includes a delay.
 */
int
g_scsi_send_diag_cmd(int fd, uchar_t *buf_ptr, int buf_len)
{
struct uscsi_cmd	ucmd;
uchar_t	cdb[] = {SCMD_SDIAG, SCSI_ESI_PF, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;
int		err;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb[3] = (buf_len>>8) & 0xff;
	cdb[4] = buf_len & 0xff;
	ucmd.uscsi_cdb = (caddr_t)cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;

	if (err = cmd(fd, &ucmd, USCSI_WRITE)) {
		return (err);
	}
	/*
	 * Allow time for things to stabilize.
	 */
	sleep(5);
	return (0);
}

/*
 *	Receive Diagnostic command
 */
int
g_scsi_rec_diag_cmd(int fd, uchar_t *buf_ptr, int buf_len, uchar_t page_code)
{
struct uscsi_cmd	ucmd;
uchar_t	cdb[] = {SCMD_GDIAG, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	/*
	 * XXX
	 *
	 * The cdb[1] field is reserved according to the SCSI spec but our
	 * envsen cards will error out if we do not set this field to 0x10.
	 *
	 * XXX
	 */
	cdb[1] = SCSI_ESI_PF;
	cdb[2] = page_code;
	cdb[3] = (buf_len>>8) & 0xff;
	cdb[4] = buf_len & 0xff;
	ucmd.uscsi_cdb = (caddr_t)cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;

	if (buf_len & 0x03) {
		return (L_RD_INVLD_TRNSFR_LEN);
	}
	return (cmd(fd, &ucmd, USCSI_READ));
}



/*
 *		Write buffer command set up to download firmware
 */
int
g_scsi_writebuffer_cmd(int fd, int off, uchar_t *buf_ptr, int buf_len,
				int sp, int bid)
{
struct uscsi_cmd	ucmd;
my_cdb_g1	cdb = {SCMD_WRITE_BUFFER, 0x4, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.byte1 |= sp;		/* set the save bit */
	cdb.byte2 = (char)(bid & 0xff);
	cdb.byte3 = off>>16;	/* bytes 3-5 contain file offset */
	cdb.byte4 = (off>>8) & 0xff;
	cdb.byte5 = off & 0xff;
	cdb.byte6 = buf_len>>16;	/* bytes 6-8 contain file length */
	cdb.byte7 = (buf_len>>8) & 0xff;
	cdb.byte8 = buf_len & 0xff;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 240;	/* long timeout required */

	return (cmd(fd, &ucmd, USCSI_WRITE));
}

/*
 *	Read buffer command set up to upload firmware
 *	Reads from code image starting at offset
 *	"code_off" for "buf_len" bytes.
 */
int
g_scsi_readbuffer_cmd(int fd, uchar_t *buf_ptr, int buf_len, int code_off)
{
struct uscsi_cmd	ucmd;
my_cdb_g1	cdb = {SCMD_READ_BUFFER, 0x5, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.byte3 = (code_off >> 16) & 0xff;
	cdb.byte4 = (code_off >> 8) & 0xff;
	cdb.byte5 = code_off & 0xff;
	cdb.byte6 = buf_len>>16;	/* bytes 6-8 contain file length */
	cdb.byte7 = (buf_len>>8) & 0xff;
	cdb.byte8 = buf_len & 0xff;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 120;

	return (cmd(fd, &ucmd, USCSI_READ));
}

int
g_scsi_inquiry_cmd(int fd, uchar_t *buf_ptr, int buf_len)
{
struct uscsi_cmd	ucmd;
my_cdb_g0	cdb = {SCMD_INQUIRY, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.count = (uchar_t)buf_len;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (cmd(fd, &ucmd, USCSI_READ | USCSI_SILENT));
}

int
g_scsi_log_sense_cmd(int fd, uchar_t *buf_ptr, int buf_len, uchar_t page_code)
{
struct uscsi_cmd	ucmd;
my_cdb_g1	cdb =  {SCMD_LOG_SENSE, 0, 0x40, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	/* clear buffers on cmds that read data */
	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.byte2 |= page_code;			/* requested page */
	cdb.byte7 = buf_len>>8;
	cdb.byte8 = buf_len & 0xff;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 120;
	return (cmd(fd, &ucmd, USCSI_READ));
}

/*
 *		MODE SELECT
 *
 *		MODE SELECT USCSI command
 *
 *		sp is the save pages bit  - Must be bit 0 -
 *
 */
int
g_scsi_mode_select_cmd(int fd, uchar_t *buf_ptr, int buf_len, uchar_t sp)
{
struct uscsi_cmd	ucmd;
/* 10 byte Mode Select cmd */
my_cdb_g1	cdb =  {SCMD_MODE_SELECT_G1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.byte1 = (sp & 1) | 0x10;		/* 0x10 is the PF bit  */
	cdb.byte7 = buf_len>>8;
	cdb.byte8 = buf_len & 0xff;

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 120;

	return (cmd(fd, &ucmd, USCSI_WRITE));
}


/*
 *		MODE SENSE USCSI command
 *
 *
 *		pc = page control field
 *		page_code = Pages to return
 */
int
g_scsi_mode_sense_cmd(int fd,
	uchar_t *buf_ptr,
	int buf_len,
	uchar_t pc,
	uchar_t page_code)
{
struct uscsi_cmd	ucmd;
/* 10 byte Mode Select cmd */
my_cdb_g1	cdb =  {SCMD_MODE_SENSE_G1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;
int		status;
static	int	uscsi_count;

	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	/* Just for me  - a sanity check */
	if ((page_code > MODEPAGE_ALLPAGES) || (pc > 3) ||
		(buf_len > MAX_MODE_SENSE_LEN)) {
		return (L_ILLEGAL_MODE_SENSE_PAGE);
	}
	cdb.byte2 = (pc << 6) + page_code;
	cdb.byte7 = buf_len>>8;
	cdb.byte8 = buf_len & 0xff;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 120;

	status = cmd(fd, &ucmd, USCSI_READ);
	/* Bytes actually transfered */
	if (status == 0) {
		uscsi_count = buf_len - ucmd.uscsi_resid;
		S_DPRINTF("  Number of bytes read on "
			"Mode Sense 0x%x\n", uscsi_count);
		if (getenv("_LUX_D_DEBUG") != NULL) {
			(void) g_dump("  Mode Sense data: ", buf_ptr,
			uscsi_count, HEX_ASCII);
		}
	}
	return (status);
}

int
g_scsi_read_capacity_cmd(int fd, uchar_t *buf_ptr, int buf_len)
{
struct uscsi_cmd	ucmd;
my_cdb_g1	cdb = {SCMD_READ_CAPACITY, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	/* clear buffers on on cmds that read data */
	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (cmd(fd, &ucmd, USCSI_READ));
}

int
g_scsi_release_cmd(int fd)
{
struct uscsi_cmd	ucmd;
const my_cdb_g0	cdb = {SCMD_RELEASE, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = 0;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (cmd(fd, &ucmd, 0));
}

int
g_scsi_reserve_cmd(int fd)
{
struct uscsi_cmd	ucmd;
const my_cdb_g0	cdb = {SCMD_RESERVE, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = 0;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (cmd(fd, &ucmd, 0));
}

int
g_scsi_start_cmd(int fd)
{
struct uscsi_cmd	ucmd;
/*
 * Use this to induce a SCSI error
 *	const my_cdb_g0	cdb = {SCMD_START_STOP, 0, 0xff, 0, 1, 0};
 */
const my_cdb_g0	cdb = {SCMD_START_STOP, 0, 0, 0, 1, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = 0;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 240;	/* takes a while to start all */
	return (cmd(fd, &ucmd, 0));
}

int
g_scsi_stop_cmd(int fd, int immediate_flag)
{
struct uscsi_cmd	ucmd;
my_cdb_g0	cdb = {SCMD_START_STOP, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	if (immediate_flag) {
		cdb.lba_msb = IMMED;
	}
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = 0;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 120;
	return (cmd(fd, &ucmd, 0));
}

int
g_scsi_tur(int fd)
{
struct uscsi_cmd	ucmd;
const my_cdb_g0	cdb = {SCMD_TEST_UNIT_READY, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = NULL;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (cmd(fd, &ucmd, 0));
}

/*
 * NOTE: This function includes a delay.
 */
int
g_scsi_reset(int fd)
{
struct uscsi_cmd	ucmd;
struct	scsi_extended_sense	sense;
int	err;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	ucmd.uscsi_cdb = NULL;
	ucmd.uscsi_cdblen = NULL;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = NULL;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	if (err = cmd(fd, &ucmd, USCSI_RESET)) {
		return (err);
	}
	/*
	 * Allow time for things to stabilize.
	 */
	sleep(20);
	return (0);
}
