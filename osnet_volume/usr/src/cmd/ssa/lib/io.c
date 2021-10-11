/*LINTLIBRARY*/

#pragma	ident	"@(#)io.c 1.4     97/02/05 SMI"

/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

/*
 *
 * 	This module is part of the Command Interface Library
 *	for the Pluto User Interface.
 *
 */

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<sys/file.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<assert.h>
/* SVR4 */
#include	<fcntl.h>
#include	<sys/sunddi.h>
#include	<sys/systm.h>
#include	<sys/scsi/scsi.h>

#include	"common.h"
#include	"scsi.h"

#define	MIN(a, b) (a < b ? a : b)
static	int	uscsi_count;		/* internal variable */

/* external functions */
extern	uscsi_cmd(int, struct uscsi_cmd *, int);

/*
 *		Write buffer command set up to download firmware
 */
int
scsi_writebuffer_cmd(int fd, u_char *buf_ptr, int buf_len, u_char sp, int bid)
{
struct uscsi_cmd	ucmd;
my_cdb_g1	cdb = {SCMD_WRITE_BUFFER, 0x4, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.byte1 |= sp;		/* set the save bit */
	cdb.byte2 = (char)(bid & 0xff);
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

	return (uscsi_cmd(fd, &ucmd, USCSI_WRITE));
}
/*
 *		Write buffer command set up to download firmware
 */
int
scsi_download_code_cmd(int fd, u_char *buf_ptr, int buf_len, u_char sp)
{
	int	status, sz;
	int bid;

	if (status = scsi_writebuffer_cmd(fd, buf_ptr, 0, sp, 0xff))
		return (status);

	bid = 0;
	while (buf_len) {
		sz = MIN(32*1024, buf_len);
		status = scsi_writebuffer_cmd(fd, buf_ptr, sz, sp, bid);
		if (status)
			return (status);
		buf_len -= sz;
		buf_ptr += sz;
		bid ++;
	}

	return (scsi_writebuffer_cmd(fd, NULL, 0, sp, 0xfe));
}

/*
 *		Read buffer command set up to upload firmware
 *	Reads from PLUTO code image (in 3 readable proms) starting at offset
 * "code_off" for "buf_len" bytes.
 */
int
scsi_readbuffer_cmd(int fd, u_char *buf_ptr, int buf_len, int code_off)
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

	return (uscsi_cmd(fd, &ucmd, USCSI_READ));
}

/*
 *		Read buffer command set up to upload firmware
 *	Reads from PLUTO code image (in 3 readable proms) starting at offset
 * "code_off" for "buf_len" bytes.
 */
int
scsi_upload_code_cmd(int fd, u_char *buf_ptr, int buf_len, int code_off)
{
	int	sz, status;

	while (buf_len) {
		sz = MIN(32*1024, buf_len);

		status = scsi_readbuffer_cmd(fd, buf_ptr, sz, code_off);
		if (status)
			return (status);
		buf_len -= sz;
		buf_ptr += sz;
		code_off += sz;
	}
	return (status);
}

int
scsi_inquiry_cmd(int fd, u_char *buf_ptr, int buf_len)
{
struct uscsi_cmd	ucmd;
my_cdb_g0	cdb = {SCMD_INQUIRY, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	cdb.count = (u_char)buf_len;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = (caddr_t)buf_ptr;
	ucmd.uscsi_buflen = buf_len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (uscsi_cmd(fd, &ucmd, USCSI_READ | USCSI_SILENT));
}

int
scsi_log_sense_cmd(int fd, u_char *buf_ptr, int buf_len, u_char page_code)
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
	return (uscsi_cmd(fd, &ucmd, USCSI_READ));
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
scsi_mode_select_cmd(int fd, u_char *buf_ptr, int buf_len, u_char sp)
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

	return (uscsi_cmd(fd, &ucmd, USCSI_WRITE));
}


/*
 *		MODE SENSE USCSI command
 *
 *
 *		pc = page control field
 *		page_code = Pages to return
 */
int
scsi_mode_sense_cmd(int fd,
	u_char *buf_ptr,
	int buf_len,
	u_char pc,
	u_char page_code)
{
struct uscsi_cmd	ucmd;
/* 10 byte Mode Select cmd */
my_cdb_g1	cdb =  {SCMD_MODE_SENSE_G1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;
int			status;

	(void) memset(buf_ptr, 0, buf_len);
	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	/* Just for me  - a sanity check */
	if ((page_code > MODEPAGE_ALLPAGES) || (pc > 3) ||
		(buf_len > MAX_MODE_SENSE_LEN)) {
		(void) fprintf(stderr,
			"Programming error - illegal Mode Sense parameter\n");
		exit(0x10);
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

	status = uscsi_cmd(fd, &ucmd, USCSI_READ);
	/* Bytes actually transfered */
	uscsi_count = buf_len - ucmd.uscsi_resid;
	S_DPRINTF("Number of bytes read on Mode Sense 0x%x\n", uscsi_count);
	return (status);
}

int
scsi_read_capacity_cmd(int fd, u_char *buf_ptr, int buf_len)
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
	return (uscsi_cmd(fd, &ucmd, USCSI_READ));
}

int
scsi_release_cmd(int fd)
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
	return (uscsi_cmd(fd, &ucmd, 0));
}

int
scsi_reserve_cmd(int fd)
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
	return (uscsi_cmd(fd, &ucmd, 0));
}

int
scsi_start_cmd(int fd)
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
	return (uscsi_cmd(fd, &ucmd, 0));
}

#define	IMMED	1
int
scsi_stop_cmd(int fd)
{
struct uscsi_cmd	ucmd;
const my_cdb_g0	cdb = {SCMD_START_STOP, IMMED, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = 0;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 120;
	return (uscsi_cmd(fd, &ucmd, 0));
}

int
scsi_sync_cache_cmd(int fd)
{
struct uscsi_cmd	ucmd;
const my_cdb_g1	cdb = {SCMD_SYNC_CACHE, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = 0;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 90;
	return (uscsi_cmd(fd, &ucmd, 0));
}

int
scsi_tur(int fd)
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
	return (uscsi_cmd(fd, &ucmd, 0));
}
/*
 * uscsi commands to talk to envsen driver
 */
scsi_get_envsen_data(int fd, char *buf, int len)
{
struct uscsi_cmd	ucmd;
char	cdb[6] = {SCMD_GDIAG, 0x10, 0x4, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	cdb[3] = (len & 0xFF00) >> 8;
	cdb[4] = len & 0xFF;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = buf;
	ucmd.uscsi_buflen = len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (uscsi_cmd(fd, &ucmd, USCSI_READ));
}
scsi_put_envsen_data(int fd, char *buf, int len)
{
struct uscsi_cmd	ucmd;
char	cdb[6] = {SCMD_SDIAG, 0x10, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	cdb[3] = (len & 0xFF00) >> 8;
	cdb[4] = len & 0xFF;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = buf;
	ucmd.uscsi_buflen = len;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 60;
	return (uscsi_cmd(fd, &ucmd, USCSI_WRITE));
}
