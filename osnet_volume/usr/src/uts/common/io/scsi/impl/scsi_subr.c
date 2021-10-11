/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsi_subr.c	1.78	99/07/30 SMI"

#include <sys/scsi/scsi.h>

/*
 * Utility SCSI routines
 */

/*
 * Polling support routines
 */

extern uintptr_t scsi_callback_id;

/*
 * Common buffer for scsi_log
 */

extern kmutex_t scsi_log_mutex;
static char scsi_log_buffer[MAXPATHLEN + 1];


#define	A_TO_TRAN(ap)	(ap->a_hba_tran)
#define	P_TO_TRAN(pkt)	((pkt)->pkt_address.a_hba_tran)
#define	P_TO_ADDR(pkt)	(&((pkt)->pkt_address))

#define	CSEC		10000			/* usecs */
#define	SEC_TO_CSEC	(1000000/CSEC)


/*PRINTFLIKE4*/
static void impl_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, ...);
static void v_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, va_list ap);

int
scsi_poll(struct scsi_pkt *pkt)
{
	register int busy_count, rval = -1, savef;
	long savet;
	void (*savec)();
	extern int do_polled_io;

	/*
	 * save old flags..
	 */
	savef = pkt->pkt_flags;
	savec = pkt->pkt_comp;
	savet = pkt->pkt_time;

	pkt->pkt_flags |= FLAG_NOINTR;

	/*
	 * XXX there is nothing in the SCSA spec that states that we should not
	 * do a callback for polled cmds; however, removing this will break sd
	 * and probably other target drivers
	 */
	pkt->pkt_comp = 0;

	/*
	 * we don't like a polled command without timeout.
	 * 60 seconds seems long enough.
	 */
	if (pkt->pkt_time == 0)
		pkt->pkt_time = SCSI_POLL_TIMEOUT;

	/*
	 * Send polled cmd.
	 *
	 * We do some error recovery for various errors.  Tran_busy,
	 * queue full, and non-dispatched commands are retried every 10 msec.
	 * as they are typically transient failures.  Busy status is retried
	 * every second as this status takes a while to change.
	 */
	for (busy_count = 0; busy_count < (pkt->pkt_time * SEC_TO_CSEC);
		busy_count++) {
		int rc;
		int poll_delay;

		/*
		 * Initialize pkt status variables.
		 */
		*pkt->pkt_scbp = pkt->pkt_reason = pkt->pkt_state = 0;

		if ((rc = scsi_transport(pkt)) != TRAN_ACCEPT) {
			if (rc != TRAN_BUSY) {
				/* Transport failed - give up. */
				break;
			} else {
				/* Transport busy - try again. */
				poll_delay = 1 *CSEC;		/* 10 msec. */
			}
		} else {
			/*
			 * Transport accepted - check pkt status.
			 */
			rc = (*pkt->pkt_scbp) & STATUS_MASK;

			if (pkt->pkt_reason == CMD_CMPLT &&
			    rc == STATUS_GOOD) {
				/* No error - we're done */
				rval = 0;
				break;

			} else if (pkt->pkt_reason == CMD_INCOMPLETE &&
			    pkt->pkt_state == 0) {
				/* Pkt not dispatched - try again. */
				poll_delay = 1 *CSEC;		/* 10 msec. */

			} else if (pkt->pkt_reason == CMD_CMPLT &&
			    rc == STATUS_QFULL) {
				/* Queue full - try again. */
				poll_delay = 1 *CSEC;		/* 10 msec. */

			} else if (pkt->pkt_reason == CMD_CMPLT &&
			    rc == STATUS_BUSY) {
				/* Busy - try again. */
				poll_delay = 100 *CSEC;		/* 1 sec. */
				busy_count += (SEC_TO_CSEC - 1);

			} else {
				/* BAD status - give up. */
				break;
			}
		}

		if ((curthread->t_flag & T_INTR_THREAD) == 0 &&
		    !do_polled_io) {
			delay(drv_usectohz(poll_delay));
		} else {
			/* we busy wait during cpr_dump or interrupt threads */
			drv_usecwait(poll_delay);
		}
	}

	pkt->pkt_flags = savef;
	pkt->pkt_comp = savec;
	pkt->pkt_time = savet;
	return (rval);
}

/*
 * Command packaging routines.
 *
 * makecom_g*() are original routines and scsi_setup_cdb()
 * is the new and preferred routine.
 */

/*
 * These routines put LUN information in CDB byte 1 bits 7-5.
 * This was required in SCSI-1. SCSI-2 allowed it but it preferred
 * sending LUN information as part of IDENTIFY message.
 * This is not allowed in SCSI-3.
 */

void
makecom_g0(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G0(pkt, devp, flag, cmd, addr, (uchar_t)cnt);
}

void
makecom_g0_s(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int cnt, int fixbit)
{
	MAKECOM_G0_S(pkt, devp, flag, cmd, cnt, (uchar_t)fixbit);
}

void
makecom_g1(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G1(pkt, devp, flag, cmd, addr, cnt);
}

void
makecom_g5(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G5(pkt, devp, flag, cmd, addr, cnt);
}

/*
 * Following routine does not put LUN information in CDB.
 * This interface must be used for SCSI-2 targets having
 * more than 8 LUNs or a SCSI-3 target.
 */
int
scsi_setup_cdb(union scsi_cdb *cdbp, uchar_t cmd, uint_t addr, uint_t cnt,
    uint_t addtl_cdb_data)
{
	uint_t	addr_cnt;

	cdbp->scc_cmd = cmd;

	switch (CDB_GROUPID(cmd)) {
		case CDB_GROUPID_0:
			/*
			 * The following calculation is to take care of
			 * the fact that format of some 6 bytes tape
			 * command is different (compare 6 bytes disk and
			 * tape read commands).
			 */
			addr_cnt = (addr << 8) + cnt;
			addr = (addr_cnt & 0x1fffff00) >> 8;
			cnt = addr_cnt & 0xff;
			FORMG0ADDR(cdbp, addr);
			FORMG0COUNT(cdbp, cnt);
			break;

		case CDB_GROUPID_1:
		case CDB_GROUPID_2:
			FORMG1ADDR(cdbp, addr);
			FORMG1COUNT(cdbp, cnt);
			break;

		case CDB_GROUPID_4:
			FORMG4ADDR(cdbp, addr);
			FORMG4COUNT(cdbp, cnt);
			FORMG4ADDTL(cdbp, addtl_cdb_data);
			break;

		case CDB_GROUPID_5:
			FORMG5ADDR(cdbp, addr);
			FORMG5COUNT(cdbp, cnt);
			break;

		default:
			return (0);
	}

	return (1);
}


/*
 * Common iopbmap data area packet allocation routines
 */

struct scsi_pkt *
get_pktiopb(struct scsi_address *ap, caddr_t *datap, int cdblen, int statuslen,
    int datalen, int readflag, int (*func)())
{
	scsi_hba_tran_t	*tran = A_TO_TRAN(ap);
	dev_info_t	*pdip = tran->tran_hba_dip;
	struct scsi_pkt	*pkt = NULL;
	struct buf	local;

	if (!datap)
		return (pkt);
	*datap = (caddr_t)0;
	bzero((caddr_t)&local, sizeof (struct buf));
	if (ddi_iopb_alloc(pdip, (ddi_dma_lim_t *)0,
	    (uint_t)datalen, &local.b_un.b_addr)) {
		return (pkt);
	}
	if (readflag)
		local.b_flags = B_READ;
	local.b_bcount = datalen;
	pkt = (*tran->tran_init_pkt) (ap, NULL, &local,
		cdblen, statuslen, 0, PKT_CONSISTENT,
		(func == SLEEP_FUNC) ? SLEEP_FUNC : NULL_FUNC,
		NULL);
	if (!pkt) {
		ddi_iopb_free(local.b_un.b_addr);
		if (func != NULL_FUNC) {
			ddi_set_callback(func, NULL, &scsi_callback_id);
		}
	} else {
		*datap = local.b_un.b_addr;
	}
	return (pkt);
}

/*
 *  Equivalent deallocation wrapper
 */

void
free_pktiopb(struct scsi_pkt *pkt, caddr_t datap, int datalen)
{
	register struct scsi_address	*ap = P_TO_ADDR(pkt);
	register scsi_hba_tran_t	*tran = A_TO_TRAN(ap);

	(*tran->tran_destroy_pkt)(ap, pkt);
	if (datap && datalen) {
		ddi_iopb_free(datap);
	}
	if (scsi_callback_id != 0) {
		ddi_run_callback(&scsi_callback_id);
	}
}

/*
 * Common naming functions
 */

static char scsi_tmpname[64];

char *
scsi_dname(int dtyp)
{
	static char *dnames[] = {
		"Direct Access",
		"Sequential Access",
		"Printer",
		"Processor",
		"Write-Once/Read-Many",
		"Read-Only Direct Access",
		"Scanner",
		"Optical",
		"Changer",
		"Communications",
		"Array Controller"
	};

	if ((dtyp & DTYPE_MASK) <= DTYPE_COMM) {
		return (dnames[dtyp&DTYPE_MASK]);
	} else if (dtyp == DTYPE_NOTPRESENT) {
		return ("Not Present");
	}
	return ("<unknown device type>");

}

char *
scsi_rname(uchar_t reason)
{
	static char *rnames[] = {
		"cmplt",
		"incomplete",
		"dma_derr",
		"tran_err",
		"reset",
		"aborted",
		"timeout",
		"data_ovr",
		"cmd_ovr",
		"sts_ovr",
		"badmsg",
		"nomsgout",
		"xid_fail",
		"ide_fail",
		"abort_fail",
		"reject_fail",
		"nop_fail",
		"per_fail",
		"bdr_fail",
		"id_fail",
		"unexpected_bus_free",
		"tag reject",
		"terminated"
	};
	if (reason > CMD_TAG_REJECT) {
		return ("<unknown reason>");
	} else {
		return (rnames[reason]);
	}
}

char *
scsi_mname(uchar_t msg)
{
	static char *imsgs[23] = {
		"COMMAND COMPLETE",
		"EXTENDED",
		"SAVE DATA POINTER",
		"RESTORE POINTERS",
		"DISCONNECT",
		"INITIATOR DETECTED ERROR",
		"ABORT",
		"REJECT",
		"NO-OP",
		"MESSAGE PARITY",
		"LINKED COMMAND COMPLETE",
		"LINKED COMMAND COMPLETE (W/FLAG)",
		"BUS DEVICE RESET",
		"ABORT TAG",
		"CLEAR QUEUE",
		"INITIATE RECOVERY",
		"RELEASE RECOVERY",
		"TERMINATE PROCESS",
		"CONTINUE TASK",
		"TARGET TRANSFER DISABLE",
		"RESERVED (0x14)",
		"RESERVED (0x15)",
		"CLEAR ACA"
	};
	static char *imsgs_2[6] = {
		"SIMPLE QUEUE TAG",
		"HEAD OF QUEUE TAG",
		"ORDERED QUEUE TAG",
		"IGNORE WIDE RESIDUE",
		"ACA",
		"LOGICAL UNIT RESET"
	};

	if (msg < 23) {
		return (imsgs[msg]);
	} else if (IS_IDENTIFY_MSG(msg)) {
		return ("IDENTIFY");
	} else if (IS_2BYTE_MSG(msg) &&
	    (int)((msg) & 0xF) < (sizeof (imsgs_2) / sizeof (char *))) {
		return (imsgs_2[msg & 0xF]);
	} else {
		return ("<unknown msg>");
	}

}

char *
scsi_cname(uchar_t cmd, register char **cmdvec)
{
	while (*cmdvec != (char *)0) {
		if (cmd == **cmdvec) {
			return ((char *)((long)(*cmdvec)+1));
		}
		cmdvec++;
	}
	return (sprintf(scsi_tmpname, "<undecoded cmd 0x%x>", cmd));
}

char *
scsi_cmd_name(uchar_t cmd, struct scsi_key_strings *cmdlist, char *tmpstr)
{
	int i = 0;

	while (cmdlist[i].key !=  -1) {
		if (cmd == cmdlist[i].key) {
			return ((char *)cmdlist[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<undecoded cmd 0x%x>", cmd));
}

static struct scsi_asq_key_strings extended_sense_list[] = {
		0x00, 0, "no additional sense info",
		0x01, 0, "no index/sector signal",
		0x02, 0, "no seek complete",
		0x03, 0, "peripheral device write fault",
		0x04, 0, "LUN not ready",
		0x05, 0, "LUN does not respond to selection",
		0x06, 0, "reference position found",
		0x07, 0, "multiple peripheral devices selected",
		0x08, 0, "LUN communication failure",
		0x09, 0, "track following error",
		0x0a, 0, "error log overflow",
		0x0c, 0, "write error",
		0x10, 0, "ID CRC or ECC error",
		0x11, 0, "unrecovered read error",
		0x12, 0, "address mark not found for ID field",
		0x13, 0, "address mark not found for data field",
		0x14, 0, "recorded entity not found",
		0x15, 0, "random positioning error",
		0x16, 0, "data sync mark error",
		0x17, 0, "recovered data with no error correction",
		0x18, 0, "recovered data with error correction",
		0x19, 0, "defect list error",
		0x1a, 0, "parameter list length error",
		0x1b, 0, "synchronous data xfer error",
		0x1c, 0, "defect list not found",
		0x1d, 0, "miscompare during verify",
		0x1e, 0, "recovered ID with ECC",
		0x1f, 0, "partial defect list transfer",
		0x20, 0, "invalid command operation code",
		0x21, 0, "logical block address out of range",
		0x22, 0, "illegal function",
		0x24, 0, "invalid field in cdb",
		0x25, 0, "LUN not supported",
		0x26, 0, "invalid field in param list",
		0x27, 0, "write protected",
		0x28, 0, "medium may have changed",
		0x29, 0, "power on, reset, or bus reset occurred",
		0x2a, 0, "parameters changed",
		0x2b, 0, "copy cannot execute since host cannot disconnect",
		0x2c, 0, "command sequence error",
		0x2d, 0, "overwrite error on update in place",
		0x2f, 0, "commands cleared by another initiator",
		0x30, 0, "incompatible medium installed",
		0x31, 0, "medium format corrupted",
		0x32, 0, "no defect spare location available",
		0x33, 0, "tape length error",
		0x36, 0, "ribbon, ink, or toner failure",
		0x37, 0, "rounded parameter",
		0x39, 0, "saving parameters not supported",
		0x3a, 0, "medium not present",
		0x3b, 0, "sequential positioning error",
		0x3d, 0, "invalid bits in indentify message",
		0x3e, 0, "LUN has not self-configured yet",
		0x3f, 0, "target operating conditions have changed",
		0x40, 0, "ram failure",
		0x41, 0, "data path failure",
		0x42, 0, "power-on or self-test failure",
		0x43, 0, "message error",
		0x44, 0, "internal target failure",
		0x45, 0, "select or reselect failure",
		0x46, 0, "unsuccessful soft reset",
		0x47, 0, "scsi parity error",
		0x48, 0, "initiator detected error message received",
		0x49, 0, "invalid message error",
		0x4a, 0, "command phase error",
		0x4b, 0, "data phase error",
		0x4c, 0, "logical unit failed self-configuration",
		0x4d, 0, "tagged overlapped commands (ASCQ = queue tag)",
		0x4e, 0, "overlapped commands attempted",
		0x50, 0, "write append error",
		0x51, 0, "erase failure",
		0x52, 0, "cartridge fault",
		0x53, 0, "media load or eject failed",
		0x54, 0, "scsi to host system interface failure",
		0x55, 0, "system resource failure",
		0x57, 0, "unable to recover TOC",
		0x58, 0, "generation does not exist",
		0x59, 0, "updated block read",
		0x5a, 0, "operator request or state change input",
		0x5b, 0, "log exception",
		0x5c, 0, "RPL status change",
		0x5d, 0, "drive operation marginal, service immediately"
			" (failure prediction threshold exceeded)",
		0x5d, 0xff, "failure prediction threshold exceeded (false)",
		0x5e, 0, "low power condition active",
		0x60, 0, "lamp failure",
		0x61, 0, "video aquisition error",
		0x62, 0, "scan head positioning error",
		0x63, 0, "end of user area encountered on this track",
		0x64, 0, "illegal mode for this track",
		0x65, 0, "voltage fault",
		0x66, 0, "automatic document feeder cover up",
		0x67, 0, "configuration failure",
		0x68, 0, "logical unit not configured",
		0x69, 0, "data loss on logical unit",
		0x6a, 0, "informational, refer to log",
		0x6b, 0, "state change has occured",
		0x6c, 0, "rebuild failure occured",
		0x6d, 0, "recalculate failure occured",
		0x6e, 0, "command to logical unit failed",
		0x70, 0xffff,
			"decompression exception short algorithm id of ASCQ",
		0x71, 0, "decompression exception long algorithm id",

		0xffff, NULL,
};

char *
scsi_esname(uint_t key, char *tmpstr)
{
	int i = 0;

	while (extended_sense_list[i].asc != 0xffff) {
		if (key == extended_sense_list[i].asc) {
			return ((char *)extended_sense_list[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<vendor unique code 0x%x>", key));
}

char *
scsi_asc_name(uint_t asc, uint_t ascq, char *tmpstr)
{
	int i = 0;

	while (extended_sense_list[i].asc != 0xffff) {
		if ((asc == extended_sense_list[i].asc) &&
		    ((ascq == extended_sense_list[i].ascq) ||
		    (extended_sense_list[i].ascq == 0xffff))) {
			return ((char *)extended_sense_list[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<vendor unique code 0x%x>", asc));
}

char *
scsi_sname(uchar_t sense_key)
{
	if (sense_key >= (uchar_t)(NUM_SENSE_KEYS+NUM_IMPL_SENSE_KEYS)) {
		return ("<unknown sense key>");
	} else {
		return (sense_keys[sense_key]);
	}
}


/*
 * Print a piece of inquiry data- cleaned up for non-printable characters.
 */

static void
inq_fill(char *p, int l, char *s)
{
	register unsigned i = 0;
	char c;

	if (!p)
		return;

	while (i++ < l) {
		/* clean string of non-printing chars */
		if ((c = *p++) < ' ' || c >= 0177) {
			c = ' ';
		}
		*s++ = c;
	}
	*s++ = 0;
}

static char *
scsi_asc_search(uint_t asc, uint_t ascq,
    struct scsi_asq_key_strings *list)
{
	int i = 0;

	while (list[i].asc != 0xffff) {
		if ((asc == list[i].asc) &&
		    ((ascq == list[i].ascq) ||
		    (list[i].ascq == 0xffff))) {
			return ((char *)list[i].message);
		}
		i++;
	}
	return (NULL);
}

static char *
scsi_asc_ascq_name(uint_t asc, uint_t ascq, char *tmpstr,
	struct scsi_asq_key_strings *list)
{
	char *message;

	if (list) {
		if (message = scsi_asc_search(asc, ascq, list)) {
			return (message);
		}
	}
	if (message = scsi_asc_search(asc, ascq, extended_sense_list)) {
		return (message);
	}

	return (sprintf(tmpstr, "<vendor unique code 0x%x>", asc));
}

/*
 * The first part/column of the error message will be at least this length.
 * This number has been calculated so that each line fits in 80 chars.
 */
#define	SCSI_ERRMSG_COLUMN_LEN	42
#define	SCSI_ERRMSG_BUF_LEN	256

void
scsi_vu_errmsg(struct scsi_device *devp, struct scsi_pkt *pkt, char *label,
    int severity, daddr_t blkno, daddr_t err_blkno,
    struct scsi_key_strings *cmdlist, struct scsi_extended_sense *sensep,
    struct scsi_asq_key_strings *asc_list,
    char *(*decode_fru)(struct scsi_device *, char *, int, uchar_t))
{
	uchar_t com;
	static char buf[SCSI_ERRMSG_BUF_LEN];
	static char buf1[SCSI_ERRMSG_BUF_LEN];
	static char tmpbuf[64];
	static char pad[SCSI_ERRMSG_COLUMN_LEN];
	dev_info_t *dev = devp->sd_dev;
	static char *error_classes[] = {
		"All", "Unknown", "Informational",
		"Recovered", "Retryable", "Fatal"
	};
	int i, buflen;

	mutex_enter(&scsi_log_mutex);

	/*
	 * We need to put our space padding code because kernel version
	 * of sprintf(9F) doesn't support %-<number>s type of left alignment.
	 */
	for (i = 0; i < SCSI_ERRMSG_COLUMN_LEN; i++) {
		pad[i] = ' ';
	}

	bzero(buf, 256);
	com = ((union scsi_cdb *)pkt->pkt_cdbp)->scc_cmd;
	(void) sprintf(buf, "Error for Command: %s",
	    scsi_cmd_name(com, cmdlist, tmpbuf));
	buflen = strlen(buf);
	if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
		(void) sprintf(&buf[buflen], "%s Error Level: %s",
		    pad, error_classes[severity]);
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
	} else {
		(void) sprintf(&buf[buflen], " Error Level: %s",
		    error_classes[severity]);
	}
	impl_scsi_log(dev, label, CE_WARN, buf);

	if (blkno != -1 || err_blkno != -1 &&
	    ((com & 0xf) == SCMD_READ) || ((com & 0xf) == SCMD_WRITE)) {
		bzero(buf, 256);
		(void) sprintf(buf, "Requested Block: %ld", blkno);
		buflen = strlen(buf);
		if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
			pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
			(void) sprintf(&buf[buflen], "%s Error Block: %ld\n",
			    pad, err_blkno);
			pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
		} else {
			(void) sprintf(&buf[buflen], " Error Block: %ld\n",
			    err_blkno);
		}
		impl_scsi_log(dev, label, CE_CONT, buf);
	}

	bzero(buf, 256);
	(void) strcpy(buf, "Vendor: ");
	inq_fill(devp->sd_inq->inq_vid, 8, &buf[strlen(buf)]);
	buflen = strlen(buf);
	if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
		(void) sprintf(&buf[strlen(buf)], "%s Serial Number: ", pad);
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
	} else {
		(void) sprintf(&buf[strlen(buf)], " Serial Number: ");
	}
	inq_fill(devp->sd_inq->inq_serial, 12, &buf[strlen(buf)]);
	impl_scsi_log(dev, label, CE_CONT, "%s\n", buf);

	if (sensep) {
		bzero(buf, 256);
		(void) sprintf(buf, "Sense Key: %s\n",
		    sense_keys[sensep->es_key]);
		impl_scsi_log(dev, label, CE_CONT, buf);

		bzero(buf, 256);
		if ((sensep->es_fru_code != 0) &&
		    (decode_fru != NULL)) {
			(*decode_fru)(devp, buf, SCSI_ERRMSG_BUF_LEN,
			    sensep->es_fru_code);
			if (buf[0] != NULL) {
				bzero(buf1, 256);
				(void) sprintf(&buf1[strlen(buf1)],
				    "ASC: 0x%x (%s)", sensep->es_add_code,
				    scsi_asc_ascq_name(sensep->es_add_code,
				    sensep->es_qual_code, tmpbuf, asc_list));
				buflen = strlen(buf1);
				if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
				    pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
				    (void) sprintf(&buf1[buflen],
				    "%s ASCQ: 0x%x", pad, sensep->es_qual_code);
				} else {
				    (void) sprintf(&buf1[buflen], " ASCQ: 0x%x",
					sensep->es_qual_code);
				}
				impl_scsi_log(dev,
					label, CE_CONT, "%s\n", buf1);
				impl_scsi_log(dev, label, CE_CONT,
					"FRU: 0x%x (%s)\n",
						sensep->es_fru_code, buf);
				mutex_exit(&scsi_log_mutex);
				return;
			}
		}
		(void) sprintf(&buf[strlen(buf)],
		    "ASC: 0x%x (%s), ASCQ: 0x%x, FRU: 0x%x",
		    sensep->es_add_code,
		    scsi_asc_ascq_name(sensep->es_add_code,
			sensep->es_qual_code, tmpbuf, asc_list),
		    sensep->es_qual_code, sensep->es_fru_code);
		impl_scsi_log(dev, label, CE_CONT, "%s\n", buf);
	}
	mutex_exit(&scsi_log_mutex);
}

void
scsi_errmsg(struct scsi_device *devp, struct scsi_pkt *pkt, char *label,
    int severity, daddr_t blkno, daddr_t err_blkno,
    struct scsi_key_strings *cmdlist, struct scsi_extended_sense *sensep)
{
	scsi_vu_errmsg(devp, pkt, label, severity, blkno,
		err_blkno, cmdlist, sensep, NULL, NULL);
}

/*ARGSUSED*/
void
scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	mutex_enter(&scsi_log_mutex);
	v_scsi_log(dev, label, level, fmt, ap);
	mutex_exit(&scsi_log_mutex);
	va_end(ap);
}



static void
impl_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, ...)
{
	va_list ap;

	ASSERT(mutex_owned(&scsi_log_mutex));

	va_start(ap, fmt);
	v_scsi_log(dev, label, level, fmt, ap);
	va_end(ap);
}


char *ddi_pathname(dev_info_t *dip, char *path);

static void
v_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, va_list ap)
{
	static char name[256];
	int log_only = 0;
	int boot_only = 0;
	int console_only = 0;

	ASSERT(mutex_owned(&scsi_log_mutex));

	if (dev) {
		if (level == CE_PANIC || level == CE_WARN ||
		    level == CE_NOTE) {
			(void) sprintf(name, "%s (%s%d):\n",
				ddi_pathname(dev, scsi_log_buffer),
				label, ddi_get_instance(dev));
		} else if (level >= (uint_t)SCSI_DEBUG) {
			(void) sprintf(name,
			    "%s%d:", label, ddi_get_instance(dev));
		} else {
			name[0] = '\0';
		}
	} else {
		(void) sprintf(name, "%s:", label);
	}

	(void) vsprintf(scsi_log_buffer, fmt, ap);

	switch (scsi_log_buffer[0]) {
	case '!':
		log_only = 1;
		break;
	case '?':
		boot_only = 1;
		break;
	case '^':
		console_only = 1;
		break;
	}

	switch (level) {
	case CE_NOTE:
		level = CE_CONT;
		/* FALLTHROUGH */
	case CE_CONT:
	case CE_WARN:
	case CE_PANIC:
		if (boot_only) {
			cmn_err(level, "?%s\t%s", name,
				&scsi_log_buffer[1]);
		} else if (console_only) {
			cmn_err(level, "^%s\t%s", name,
				&scsi_log_buffer[1]);
		} else if (log_only) {
			cmn_err(level, "!%s\t%s", name,
				&scsi_log_buffer[1]);
		} else {
			cmn_err(level, "%s\t%s", name,
				scsi_log_buffer);
		}
		break;
	case (uint_t)SCSI_DEBUG:
	default:
		cmn_err(CE_CONT, "^DEBUG: %s\t%s", name,
				scsi_log_buffer);
		break;
	}
}

int
scsi_get_device_type_scsi_options(dev_info_t *dip,
    struct scsi_device *devp, int default_scsi_options)
{

	caddr_t config_list	= NULL;
	int options		= default_scsi_options;
	struct scsi_inquiry  *inq = devp->sd_inq;
	caddr_t vidptr, datanameptr;
	int	vidlen, dupletlen;
	int config_list_len, len;

	/*
	 * look up the device-type-scsi-options-list and walk thru
	 * the list
	 * compare the vendor ids of the earlier inquiry command and
	 * with those vids in the list
	 * if there is a match, lookup the scsi-options value
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "device-type-scsi-options-list",
	    (caddr_t)&config_list, &config_list_len) == DDI_PROP_SUCCESS) {

		/*
		 * Compare vids in each duplet - if it matches, get value for
		 * dataname and then lookup scsi_options
		 * dupletlen is calculated later.
		 */
		for (len = config_list_len, vidptr = config_list; len > 0;
		    vidptr += dupletlen, len -= dupletlen) {

			vidlen = strlen(vidptr);
			datanameptr = vidptr + vidlen + 1;

			if ((vidlen != 0) &&
			    bcmp(inq->inq_vid, vidptr, vidlen) == 0) {
				/*
				 * get the data list
				 */
				options = ddi_prop_get_int(DDI_DEV_T_ANY,
				    dip, 0,
				    datanameptr, default_scsi_options);
				break;
			}
			dupletlen = vidlen + strlen(datanameptr) + 2;
		}
	}

	return (options);
}
