/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)chs.c	1.15	99/11/08 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 * ===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name:   Viper
 *
 *   This driver supports:
 *		PCI	IBM PCI RAID
 *
 */

/*
 * NOTE :
 * The Solaris driver assumes system-drives on logical channel 0xFF
 * and non-system drives on logical channels 0 to N-1 (for N physical
 * channel controller which has 0 to N-1 numbered physical channels).
 * This is the information passed in the dev_info structures for
 * system and non-system devices to construct the boot-path ...
 *	e.g.  /eisa/chs@20ff,0/cmdk@1,0:a means boot-device is
 *			system drive 1 at ioaddress 0x20ff
 *	e.g.  /eisa/chs@2001,0/cmdk@3,0:a means boot-device is
 *			non-system device at channel 1, target 3
 */

#ifdef lint
#define	__far
#endif

#include <types.h>
#include <common.h>
#include <dev_info.h>
#include <bef.h>
#include <befext.h>
#ifdef lint
#include "../scsi.h"
#else
#include "..\scsi.h"
#endif
#include "chs.h"

#ifdef DEBUG
#ifdef PAUSE
/*
 * XXX chs has grown large enough that kbchar() can no longer
 * be linked in.
 */
#define	kbchar()	milliseconds(1000)
#endif
#endif

/*
 * These definitions provide the stack for resident code.
 * They are used by low.s.
 */
#define	STACKSIZE	1000	/* Resident stack size in words */
ushort stack[STACKSIZE] = { 0 };

ushort stacksize = STACKSIZE;

#define	MAXDEVS 	224

char		ident[] = "CHS";
char		vendidvpr[] = "IBM     ";	/* dont disturb blanks	*/
char		prodid[] = "SYSTEM  DRIVE   ";	/* dont disturb blanks	*/



extern DEV_INFO	dev_info[];
extern 		devs;
extern ushort 	endloc;
extern long	longaddr(ushort, ushort);

unchar cmdid = 0;
struct daccdb cdbcmdstr = { 0 };

union CONFIG config_str = { 0 };
struct ENQ_CONFIG	enq_config;

struct CHN chan[MAX_POSS_CHNS] = { 0 };

struct chs {
	int	hba_type;	/* bus adapter type 		*/
	int	iobase;		/* io base address 		*/
	ushort	slot;		/* slot number 			*/
	unchar 	__far *membase;	/* shared memory base address 	*/
	unchar	pci;		/* PCI-type flag		*/
	unchar	pci_bus;	/* PCI bus number		*/
	ushort	pci_ven_id;	/* PCI vendor ID		*/
	ushort	pci_vdev_id;	/* PCI device ID		*/
	unchar	pci_dev;	/* PCI device number		*/
	unchar	pci_func;	/* PCI function code		*/
	ushort	cds;
	ushort	was_reset;	/* to indicate the card was reset */
	unchar	statusqassigned;
	struct viper_statusq_element  *sqsr;	/*
						 * Viper statusq start
						 * virtual address
						 */
	struct viper_statusq_element  *sqtr; 	/*
						 * Viper statusq tail
						 * virtual address
						 */

} chs[MAX_PCI_SLOTS+1] = { 0 };

int	chs_inx = -1;

void	setup_max_chns_tgts(ushort, ushort *, ushort *, ushort);
void	setup_system_devices(ushort, unchar *, ushort);
void	setup_vpr_system_devices(ushort, unchar *, ushort);
void	setup_non_system_devices(ushort, unchar *, ushort, ushort, ushort);
void	drv_to_dev(ushort, ushort, ushort, ushort, unchar *, ushort, ushort);
void	set_byte_zero(unchar *, ushort);
void	handle_config_status(ushort, ushort);
void	viper_errmsg(ushort);
ushort	check_status(ushort, ushort);
ushort	convert_inq_status(ushort hba_type, ushort status);
ushort  dac_init_cmd(ushort, unchar, unchar, ulong, unchar, ulong, unchar,
		ushort, ushort *);
ushort	pci_vpr_init_cmd(ushort, unchar, unchar, ulong, unchar, ulong, unchar,
			ushort *);
void	pci_disable_intr(ushort, ushort);
int	card_init(ushort, ushort);
int	viper_card_init(ushort, ushort);
int	checkreinit(ushort, ushort);
int	pci_card_reset(ushort, ushort);
ushort	send_dcdb_inquire(ushort, ushort, ushort, unchar *, ushort, ushort *);
ushort  send_dcdb_read(ushort, ushort, ulong, ushort, ulong, ulong, ushort,
			ushort *);
ushort  send_dcdb_readcap(ushort, ushort, unchar *, ushort, ushort *);
int	dev_motor(DEV_INFO *, int);
int	dev_lock(DEV_INFO *, int);
void	pci_dev_find(void);
void	pci_dev_found(ushort, ushort, ushort, ushort, ushort);
int	Check_regbit(ushort, unchar, unchar, long, int);
#ifdef	MSCSI_FEATURE
void	make_bootpath_name(char *, char *, ushort, char *, ushort);
#endif

#ifdef MSCSI_FEATURE
#define	DEV_CHAN(i)	((i)->junk[1] & 0xFF) 		/* channel */
#define	DEV_SYS(i)	(DEV_CHAN(i) == 0xFF) 		/* is system-drive */
#else
#define	DEV_CHAN(i)	((i)->base_port & 0xFF)		/* channel */
#define	DEV_SYS(i)	(DEV_CHAN(i) == 0xFF)		/* is system-drive */
#endif


/* generic dev_find() determines bustype */
void
dev_find()
{
#ifdef DEBUG
	putstr("dev_find: ENTER\r\n");
#endif
	/*
	 * Attempt to find all HBAs configured into
	 * the system.
	 */
	pci_dev_find();
}



void
setup_max_chns_tgts(ushort slot, ushort *m_chns, ushort *m_tgts,
	ushort hba_type)
{
	ushort 			chn = 0, tagt, stat, inx;
	struct inquiry_data 	inqd;

	for (tagt = 0; tagt < MAX_POSS_TGTS; tagt++) {
		if (chan[chn].tgt[tagt].target)  /* if sys-drive */
			continue;

		if (send_dcdb_inquire(slot, chn, tagt, (unchar *) &inqd,
			hba_type, &stat) == FAIL) {
			/* I am not sure if we should continue or return */
			continue;
		}
		if (convert_inq_status(hba_type, stat) == CHS_INVALDEV) {
						/* invalid target */
			*m_tgts = tagt;
			break;
		}
	}

	if (tagt >= MAX_POSS_TGTS)
		*m_tgts = tagt - 1;
#ifdef DEBUG
	putstr(" In setup_max_chns_tgts...TGTS = ");
	put2hex((unchar)*m_tgts);
#endif

	tagt = 0;
	for (chn = 1; chn < MAX_POSS_CHNS; chn++) {
		if (chan[chn].tgt[tagt].target)  /* if sys-drive */
			continue;
		if (send_dcdb_inquire(slot, chn, tagt, (unchar *) & inqd,
			hba_type, &stat) == FAIL) {
			/* I am not sure if we should continue or return */
			continue;
		}

		if (convert_inq_status(hba_type, stat) == CHS_INVALDEV) {
			*m_chns = chn;
			break;
		}
	}

	if (chn == MAX_POSS_CHNS)
		*m_chns = chn - 1;

#ifdef DEBUG
	putstr(" In setup_max_chns_tgts...CHNS = ");
	put2hex((unchar)*m_chns);
#ifdef PAUSE
	kbchar();
#endif
#endif
}

void
setup_system_devices(ushort slot, unchar *buffer, ushort hba_type)
{
	if (hba_type == CHS_PCIVPR) {
		setup_vpr_system_devices(slot, buffer, hba_type);
	}
}

void
setup_vpr_system_devices(ushort slot, unchar *buffer, ushort hba_type)
{
	unchar  		chnid, tgtid;
	unchar  		num_arms, num_disks, drv_stat;
	ushort  		drv, armno, diskno;
	ushort			inx;
	unchar			num_chunks, chunkno;
	struct CHUNK *chunk;
	struct V_CONFIG	*conf = (struct V_CONFIG *)buffer;

	if (conf->n_sysdrives == 0) {		/* no. of system-drives */
#ifdef DEBUG
		putstr(" No system drives configured ...\n");
#ifdef PAUSE
		{
			int i;
			putchar('|');
			for (i = 0; i < sizeof (struct V_CONFIG); i++) {
				put2hex(buffer[i]);
				putchar(':');
			}
			putchar('|');
		}
		kbchar();
#endif
#endif
		return;
	}

#ifdef DEBUG
	putstr("\r\nNo. of system drives are = ");
	put2hex((unchar)conf->n_sysdrives);
	putstr("\r\n");
#ifdef PAUSE
	kbchar();
#endif
#endif

	for (drv = 0; drv < conf->n_sysdrives; drv++) {
		num_chunks = conf->log_drv[drv].NoChunkUnits;
#ifdef DEBUG
		putstr("\r\n On system-drive ");
		puthex((ushort)drv);
		putstr(" No. of chunks are : ");
		puthex((ushort)num_chunks);
		putstr("\n");
#ifdef PAUSE
		kbchar();
#endif
#endif
		if (num_chunks == 0) {
#ifdef DEBUG
			putstr(" System-drive ");
			puthex((ushort)drv);
			putstr(" Has no chunks...not configured \n");
#ifdef PAUSE
			kbchar();
#endif
#endif
			continue;
		} else {
			for (chunkno = 0; chunkno < num_chunks; chunkno++) {

				chunk = &(conf->log_drv[drv].chunk[chunkno]);
				chnid = chunk->chn;
				tgtid = chunk->tgt;
#ifdef DEBUG
				putstr("\r\nSD: chn :: tgt=");
				puthex((ushort)chnid);
				putstr(" ");
				puthex((ushort)tgtid);
				putstr("\r\n");
#ifdef PAUSE
				kbchar();
#endif
#endif
				chan[chnid].tgt[tgtid].target = 0x1;
			} /* end of for */
		} /* end of else */


		drv_stat = conf->log_drv[drv].state;
#ifdef DEBUG
		putstr(" \r\n Status of drive = ");
		put2hex((unchar)drv_stat);
		putstr("\r\n");
#endif

		if ((drv_stat == ONLINE) || (drv_stat == CRITICAL)) {
			drv_to_dev(slot, 0xFF, drv, INQD_PDT_DA, (unchar *)0,
			    SYS_DRV, hba_type);
		}

	/*
	 * fake target as drive no., chan=0xFF for system drives
	 */
	}
} /* end of setup_vpr_system_devices() */
/*ARGSUSED*/

void
setup_non_system_devices(ushort slot, unchar *buffer, ushort chans,
ushort targets, ushort hba_type)
{
	ushort 				chn, tagt, stat, dev_type;
	ushort 				arr_chn = 0, arr_tgt = 0;
	extern struct inquiry_data 	inqd;
	struct V_CONFIG	*conf = (struct V_CONFIG *)buffer;

#ifdef DEBUG
	putstr("\n In setup_non_system devices PRINTING STATE OF DEVICES\r\n");
#endif

	for (chn = 0; chn < chans; chn++) {
		for (tagt = 0; tagt < targets; tagt++) {

			if (chan[chn].tgt[tagt].target) { /* if sys-drive */
				/* clear the field for another controller */
				chan[chn].tgt[tagt].target = 0x0;
				SET_ARR_CHN_TGT(arr_chn, arr_tgt);
				continue;
			}

			if (!(CHS_DAC_TGT_STANDBY(conf->dev[chn][tagt].state))){
				/* Non logical drives need to be in */
				/*  standby modeonly standby devices */
				continue;
			}
			if (CHS_DAC_TGT_DISK(conf->dev[chn][tagt].Params)) {
				/* Standby disk drives should not be */
				/* accessible */
				continue;
			}

#ifdef DEBUG
			putstr(" Sending DCDB INQUIRY to non-system drive :");
			putstr(" Channel no. : ");
			puthex((ushort)chn);
			putstr(" Target no. : ");
			puthex((ushort)tagt);
			putstr("\r\n");
#endif

			if (send_dcdb_inquire(slot, chn, tagt,
				(unchar *)&inqd, hba_type, &stat) == FAIL) {
				putstr("setup_non-system_devices, cmd fail\n");
				SET_ARR_CHN_TGT(arr_chn, arr_tgt);
				continue;
			}


#ifdef DEBUG
			putstr(" Status returned from DCDB INQUIRY \n");
			puthex((ushort)stat);
#ifdef PAUSE
			kbchar();
#endif
#endif
			switch (convert_inq_status(hba_type, stat)) {
			case CHS_SUCCESS:
				break;
			case CHS_RETRY:
				milliseconds(100);	/* some delay */
				if (send_dcdb_inquire(slot, chn, tagt,
				    (unchar *) & inqd, hba_type, &stat)
								== FAIL){
					SET_ARR_CHN_TGT(arr_chn, arr_tgt);
					continue;
				}

				if (check_status(hba_type, stat) !=
								CHS_SUCCESS){
					SET_ARR_CHN_TGT(arr_chn, arr_tgt);
					continue;
				}
				break;
			default :
				SET_ARR_CHN_TGT(arr_chn, arr_tgt);
				continue;
			}

#ifdef DEBUG
			putstr(" Printing the peripheral device-type byte : ");
			put2hex((unchar)inqd.inqd_pdt);
#endif
			/*
			 * Improper qualifier
			 */
			if (PERI_QUAL(inqd.inqd_pdt)) {
				SET_ARR_CHN_TGT(arr_chn, arr_tgt);
				continue;
			}

			dev_type = DEV_TYPE(inqd.inqd_pdt);
#ifdef DEBUG
			putstr("\r\n Non-system drive type is :");
			puthex((ushort)dev_type);
#endif
#ifdef DEBUG
			putstr("\r\n  finfo. about non-system drive :");
			putstr(" Channel no. : ");
			puthex((ushort)chn);
			putstr(" Target no. : ");
			puthex((ushort)tagt);
			putstr("\r\n");
#ifdef PAUSE
			kbchar();
#endif
#endif

			drv_to_dev(slot, chn, tagt, dev_type,
			    (unchar *) & inqd, NONSYS_DRV, hba_type);

			SET_ARR_CHN_TGT(arr_chn, arr_tgt);
		} /* end of for for targets */
#ifdef DEBUG
		putstr("\r\n");
#endif
	} /* end of for for chans */
#ifdef PAUSE
	kbchar();
#endif
} /* end of setup_non_system_devices() */




ushort
send_dcdb_inquire(ushort slot, ushort chn, ushort tagt, unchar *inq,
	ushort hba_type, ushort *status)
{
	struct inquiry_data	*loc_inq = (struct inquiry_data *)inq;
	struct daccdb *cdbcmd = &cdbcmdstr;
	unchar 			*cdbdata;
	ulong  			address, cdb_addr;
	int 			i, ret;

	for (i = 0; i < sizeof (struct inquiry_data); i++)
		((char *)loc_inq)[i] = 0;

	for (i = 0; i < sizeof (struct daccdb); i++)
		((char *)cdbcmd)[i] = 0;
	/*
	 * Channel upper 4 bits, target  lower nibble
	 */
	cdbcmd->cdb_unit = (chn << 4) | (tagt);
#ifdef DEBUG
	putstr(" IN DCDB INQUIRE...cdb_unit is chn(4 bits)|tagt(4 bits) : ");
	puthex((ushort)cdbcmd->cdb_unit);
	putstr("\r\n");
#endif
	/*
	 * disconnect, auto request sense, 10 second timeout,
	 * normal command completion and data-in
	 */
	cdbcmd->cdb_cmdctl = 0x91;

	cdbcmd->cdb_xfersz = sizeof (struct inquiry_data);

	/*
	 * 24 bit address to inquiry-data
	 */
	address = (ulong)longaddr((ushort)loc_inq, myds());
	cdbcmd->cdb_databuf = address;

#ifdef DEBUG
	putstr(" check on physical address of databuf is :");
	put2hex((unchar)(address >> 24));
	put2hex((unchar)(address >> 16));
	put2hex((unchar)((ushort)address >> 8));
	put2hex((unchar)address);
	putstr("\r\n");
#endif
	cdbcmd->cdb_cdblen  = CDB_SCINQ_LEN;
#ifdef DEBUG
	putstr(" cdb_cdblen is : ");
	puthex((ushort)cdbcmd->cdb_cdblen);
	putstr("\r\n");
#endif

	cdbcmd->cdb_senselen = MAX_SENSE_LEN;

	if (hba_type == CHS_PCIVPR)
		cdbdata = (unchar *) cdbcmd->fmt.v.cdb_data;

	*(cdbdata+0) = SC_INQUIRY;
	set_byte_zero((cdbdata + 1), 3);
	*(cdbdata+4) = sizeof (struct inquiry_data);
	set_byte_zero((cdbdata + 5), 2);


	cdb_addr = (ulong)longaddr((ushort)cdbcmd, myds());

	if ((ret = dac_init_cmd(slot, TYPE_3, DAC_DCDB, cdb_addr,
	    DUMMY, DUMMY, DUMMY, hba_type, status)) == FAIL) {
		putstr(" send_dcdb_inquire: sending DAC_DCDB failed\n");
		return (ret);
	}

	if (cdbcmd->cdb_senselen) {
#ifdef DEBUG
		putstr(" \r\n Sense-length returned is :");
		put2hex((unchar)cdbcmd->cdb_senselen);

		if (hba_type == CHS_PCIVPR) {
			putstr("\r\n Zeroth byte of sense-data : ERROR CODE: ");
			put2hex((unchar)cdbcmd->fmt.v.cdb_sensebuf[0]);
			putstr("\r\n Second byte of sense-data : SENSE KEY : ");
			put2hex((unchar)cdbcmd->fmt.v.cdb_sensebuf[2]);
		}
#endif
;
	}

#ifdef DEBUG
	putstr(" \r\n Actual no. of bytes transferred IN is : ");
	puthex((ushort)cdbcmd->cdb_xfersz);
	putstr("\r\n");
	putstr(" \r\n SCSI status returned is ");
	if (hba_type == CHS_PCIVPR) {
		put2hex((unchar)cdbcmd->fmt.v.cdb_status);
	}

#endif

#ifdef DEBUG
	putstr("\r\n in send_dcdb_inquire STAT is : ");
	puthex((ushort)status);
	putstr("\r\nInquiry Data: hex/ASCII\r\n");

	for (i = 0; i < cdbcmd->cdb_xfersz; i++) {
		put2hex((unchar)((char *)loc_inq)[i]);
		putchar(':');
		putchar((unchar)((char *)loc_inq)[i]);
		putchar('|');
	}

	putstr("\r\nInquiry Data: ASCII\r\n");

	for (i = 0; i < cdbcmd->cdb_xfersz; i++)
		putchar((unchar)((char *)loc_inq)[i]);

	putstr("\r\nDCDB INQUIRE... chn(4 bits)|tagt(4 bits) : ");
	puthex((ushort)cdbcmd->cdb_unit);
	putstr("\r\n");
#ifdef PAUSE
	kbchar();
#endif
#endif
	return (ret);

} /* end of send_dcdb_inquire() */


/* return 1 if pci hba type, else returns 0 */
int
is_pci_type(ushort hba_type)
{
	if (hba_type == CHS_PCIVPR)
		return (1);
	return (0);
}



void
drv_to_dev(ushort slot, ushort chan, ushort tgt, ushort devtype, unchar *inq,
		ushort drvtype, ushort hba_type)
{
	char			bootpath[80];
	ushort 			i, slot_save = slot;
	struct inquiry_data 	*loc_inq = (struct inquiry_data *)inq;

	dev_info[devs].version = MDB_VERS_MISC_FLAGS;

	dev_info[devs].MDBdev.scsi.bsize = UNKNOWN_BSIZE;
	dev_info[devs].MDBdev.scsi.targ = tgt;  /* drive no. for sys-drives */

	dev_info[devs].MDBdev.scsi.lun = 0x0;		/*  lun = 0x0  */
	dev_info[devs].MDBdev.scsi.pdt = devtype;

	/*
	 * We save this slot's HBA type into this
	 * field for lack of a named field.
	 */
	dev_info[devs].junk[0]  = hba_type;

	if (drvtype == SYS_DRV) {		/* system-drives */

		/*
		 * Distinguish direct attach devices (system drives)
		 * so that different target drivers can be assigned
		 * if the Solaris driver is dual-mode.
		 */
		dev_info[devs].misc_flags |= MDB_MFL_DIRECT;

		if (is_pci_type(hba_type))
			slot <<= 3;

		dev_info[devs].dev_type = MDB_SCSI_HBA;

		/*
		 * Channel 0xFF is the virtual channel for system drives
		 */
		dev_info[devs].base_port = slot + 0xFF;
#ifdef MSCSI_FEATURE
		dev_info[devs].junk[1]  = 0xFF;
#endif

		if (hba_type == CHS_PCIVPR) {
			for (i = 0; i < 8; i++) {	/* vendor id */
				dev_info[devs].vid[i] = vendidvpr[i];
			}
		}

		for (i = 0; i < 16; i++) {		/* product id */
			dev_info[devs].pid[i] = prodid[i];
		}

		dev_info[devs].prl[0] = 0x30 + tgt;	/* sys-drive no. */
		for (i = 0; i < 4; i++) {
			dev_info[devs].prl[i] = ' ';
		}
	} else {					/* non-system-drives */
		if (is_pci_type(hba_type))
			slot <<= 3;

		dev_info[devs].dev_type = MDB_SCSI_HBA;

#ifdef MSCSI_FEATURE
		dev_info[devs].base_port = slot + 0xFF;
		dev_info[devs].junk[1]  = chan;
#else
		dev_info[devs].base_port = slot + chan;
#endif
		dev_info[devs].MDBdev.scsi.pdt = loc_inq->inqd_pdt;
		for (i = 0; i < 8; i++) {		/* vendor id */
			dev_info[devs].vid[i] = loc_inq->inqd_vid[i];
		}

		for (i = 0; i < 16; i++) {		/* product id */
			dev_info[devs].pid[i] = loc_inq->inqd_pid[i];
		}

		for (i = 0; i < 4; i++) {	/* product revision level */
			dev_info[devs].prl[i] = loc_inq->inqd_prl[i];
		}
	}

	dev_info[devs].blank1 = ' ';
	dev_info[devs].blank2 = ' ';
	dev_info[devs].term = 0;

#ifdef DEBUG
	putstr("devs = ");
	puthex(devs);
	putstr("\r\n");
	putstr(" \r\n BASE_PORT is =");
	put2hex((unchar)(dev_info[devs].base_port >> 8));
	put2hex((unchar)dev_info[devs].base_port);
	putstr(" \r\n VENDID is =");
	putstr((char *)dev_info[devs].vid);
	putstr(" \r\n PRODID is =");
	putstr((char *)dev_info[devs].pid);
	putstr("\r\n");
#ifdef PAUSE
	kbchar();
#endif
#endif

	for (i = 0; i < 8; i++) { 		/* hba-identifier */
		dev_info[devs].hba_id[i] = ident[i];
	}

	dev_info[devs].bios_dev = next_MDB_code();

	if (dev_info[devs].bios_dev == 0) {
		putstr("No BIOS device code available.\r\n");
		return;
	}

	/*
	 * Advance next device pointer
	 */
	if (devs < MAXDEVS) {
		ushort	inx;

		if (((inx = get_chs_inx(hba_type, slot_save)) != -1) &&
		    chs[inx].pci) {
			dev_info[devs].pci_valid   = 1;
			dev_info[devs].pci_bus	   = chs[inx].pci_bus;
			dev_info[devs].pci_ven_id  = chs[inx].pci_ven_id;
			dev_info[devs].pci_vdev_id = chs[inx].pci_vdev_id;
			dev_info[devs].pci_dev	   = chs[inx].pci_dev,
			dev_info[devs].pci_func	   = chs[inx].pci_func;
		}

#ifdef MSCSI_FEATURE
		dev_info[devs].user_bootpath[0] = '\0';
		/*
		 * Build the bootpath.
		 */

		make_bootpath_name(bootpath,
			"pci1014,2e@",
			slot_save >> 8,
			"mscsi@",
			(drvtype == SYS_DRV) ? 0xFF : chan);
#endif
		devs++;
	}

	/*
	 * Expand resident portion of data segment
	 * to include new device.
	 */
	endloc = (ushort)(dev_info + devs);
} /* end of drv_to_dev() */

ushort
check_status(ushort hba_type, ushort status)
{
	if (hba_type == CHS_PCIVPR) {
		if (((status & VIPER_GSC_MASK) == 0) ||
				((status & VIPER_GSC_MASK) == 0x1)) {
			return (CHS_SUCCESS);
		} else {
			return (CHS_ERROR);
		}
	}
}

/*
 * All we care about statuses are in DCDB inquiry, and from all those
 * error messages we are really interested on INVALDEV and RETRYING
 * the command. For all the reset of statuses, we only care if it failed
 * or not and about debugging messages for failures
 */
ushort
convert_inq_status(ushort hba_type, ushort status)
{
	if (check_status(hba_type, status) == CHS_SUCCESS)
		return (CHS_SUCCESS);

	if (hba_type == CHS_PCIVPR) {
		switch (status  & VIPER_STATUS_MASK) {
			case 0xFC0F:		/* recovered error sense key */
			case 0xF20F:		/* Data Over run/under run */
			case 0xFF0F:		/* Check Condition */

				return (CHS_SUCCESS);
			case VIPER_TGT_BUSY:
			case VIPER_SEL_TIMEOUT:
				return (CHS_RETRY);
			case VIPER_INV_PARAMS:
			case VIPER_INV_DEV:
				return (CHS_INVALDEV);
			default:
				return (CHS_ERROR);
		}
	}
}

void
viper_errmsg(ushort status)
{
#ifdef DEBUG
	if (check_status(CHS_PCIVPR, status) != CHS_SUCCESS)
	putstr(viper_gsc_errmsg[status & VIPER_GSC_MASK]);
	putstr("\n");
#endif
}

void
handle_config_status(ushort hba_type, ushort status)
{


	if (hba_type == CHS_PCIVPR) {
		viper_errmsg(status);
	}
}

ushort
dac_init_cmd(ushort slot, unchar cmd_type, unchar opcode, ulong address,
unchar drive, ulong start_block, unchar count, ushort hba_type, ushort *status)
{
	ushort (*fp)() = pci_vpr_init_cmd;

	switch (hba_type) {
	case CHS_PCIVPR:
		fp = pci_vpr_init_cmd;
	}

	return (fp(slot, cmd_type, opcode, address, drive,
		start_block, count, status));
}




void
handle_dacread_status(ushort hba_type, ushort status)
{
	if (hba_type == CHS_PCIVPR){
		viper_errmsg(status);
	}
}


int
dev_read(register DEV_INFO *info, long start_block, ushort count, ushort bufo,
	ushort bufs)
{
	int	hba_type;
	unchar	drive;
	ushort	slot, chan, target, unit, status;
	ulong	address;

#ifdef DEBUG
	putstr(" \r\n IN DEV_READ...new era ");
	putstr("\r\n BUFFERsegment::offset = ");
	puthex((ushort)bufs);
	putstr("::");
	puthex((ushort)bufo);
#endif
	address = longaddr(bufo, bufs);
#ifdef DEBUG
	putstr("\r\n CONFIRM check on physical address is :");
	put2hex((unchar)(address >> 24));
	put2hex((unchar)(address >> 16));
	put2hex((unchar)((ushort)address >> 8));
	put2hex((unchar)address);

	putstr("\r\n VALUE OF start_block =");
	put2hex((unchar)(start_block >> 24));
	put2hex((unchar)(start_block >> 16));
	put2hex((unchar)((ushort)start_block >> 8));
	put2hex((unchar)start_block);

	putstr("\r\n COUNT no. of blocks =");
	puthex((ushort)count);
#endif

	slot = info->base_port & 0xFF00;

	/*
	 * Determine this slot's HBA type secretly
	 * tucked away in this misnamed field for
	 * lack of an appropriate named entry.
	 */
	hba_type = info->junk[0];

	/*
	 * Need to do this only for PCI devices.
	 */
	if (is_pci_type(hba_type))
		slot = (slot >> 3);

	if (checkreinit(slot, hba_type) == FAIL) {
		putstr("Initialization Failed!");
	}

#ifdef DEBUG
	putstr("\r\n BASE_PORT OF DEVICE = ");
	puthex((ushort)slot);
	putstr("\r\n IN READ....first stopover ");
#ifdef PAUSE
	kbchar();
#endif
#endif

	if (DEV_SYS(info)) {		/* system-drives */
		drive   = info->MDBdev.scsi.targ; /* faked sys drive number */
		if (dac_init_cmd(slot, TYPE_1, DAC_READ, address,
		    drive, start_block, (unchar)count, hba_type, &status) ==
									FAIL) {
			return (1);
		}
#ifdef DEBUG
		putstr(" \r\n rtrnd status in dev_read from dac_init_cmd \r\n");
		puthex((ushort)status);
		putstr("\r\n");
#endif
		if (check_status (hba_type, status) == CHS_SUCCESS)
			return (0);
		handle_dacread_status(hba_type, status);
		return (1);
	} else {					/* non-system drives */
		ulong totbytes;
		register int	i;

		chan   = DEV_CHAN(info);
		target = info->MDBdev.scsi.targ;

		for (totbytes = 0, i = 0; i < count; i++) {
			totbytes += info->MDBdev.scsi.bsize;
		}

		/*
		 * Channel upper 4 bits, target  lower nibble
		 */
		unit = (chan << 4) | target;
		if (send_dcdb_read(slot, unit, start_block, count,
		    totbytes, address, hba_type, &status) == FAIL) {
			return (1);
		}
#ifdef DEBUG
		putstr(" \r\n For read on non-system drives STAT = ");
		puthex((ushort)status);
		putstr("\r\n");
#endif
		if (check_status(hba_type, status) != CHS_SUCCESS)
			return (1);

		return (0);

	}
} /* end of dev_read() */

ushort
send_dcdb_read(ushort slot, ushort unit, ulong start_block,
ushort count, ulong totbytes, ulong address, ushort hba_type, ushort *status)
{
	ulong 		cdb_addr;
	unchar 		*cdbdata;
	struct daccdb	*cdbcmd = &cdbcmdstr;

	cdbcmd->cdb_unit = unit;

	/*
	 * disconnect, auto request sense, 10 second timeout,
	 * normal command completion and data-in
	 */
	cdbcmd->cdb_cmdctl   = 0x91;
	cdbcmd->cdb_databuf  = address;
	cdbcmd->cdb_xfersz   = totbytes;
	cdbcmd->cdb_cdblen   = CDB_SCREAD_LEN;
	cdbcmd->cdb_senselen = MAX_SENSE_LEN;


	if (hba_type == CHS_PCIVPR)
		cdbdata = (unchar *) cdbcmd->fmt.v.cdb_data;

	*cdbdata  = SX_READ;
	*(cdbdata + 1) = 0x0;	/* lun=0, DPO=1, FUA=1, RelAddr=0 */
	*(cdbdata + 2) = (unchar)(start_block >> 24);
	*(cdbdata + 3) = (unchar)(start_block >> 16);
	*(cdbdata + 4) = (unchar)((ushort)start_block >> 0x8);
	*(cdbdata + 5) = (unchar)start_block;
	*(cdbdata + 7) = (unchar)(count >> 8);
	*(cdbdata + 8) = (unchar)(count);
	*(cdbdata + 9) = 0x0;

	cdb_addr = (ulong)longaddr((ushort)cdbcmd, myds());
	if (dac_init_cmd(slot, TYPE_3, DAC_DCDB, cdb_addr,
	    DUMMY, DUMMY, DUMMY, hba_type, status) == FAIL) {
		return (FAIL);
	}

#ifdef DEBUG
	putstr(" \r\n Sense-length returned is :");
	put2hex((unchar)cdbcmd->cdb_senselen);
	if (hba_type == CHS_PCIVPR) {
		putstr("\r\n Zeroth byte of sense-data : ERROR CODE : ");
		put2hex((unchar)cdbcmd->fmt.v.cdb_sensebuf[0]);
		putstr("\r\n Second byte of sense-data : SENSE KEY : ");
		put2hex((unchar)cdbcmd->fmt.v.cdb_sensebuf[2]);
	}

	putstr(" \r\n Actual no. of bytes transferred IN is : ");
	puthex((ushort)cdbcmd->cdb_xfersz);
#endif
	return (SUCCESS);
} /* end of send_dcdb_read() */

int
dev_readcap(register DEV_INFO *info)
{
	ushort	hba_type;
	ushort	slot, chan, target, unit;
	extern	struct readcap_data readcap_data;
	ushort	status;
	int	succeed;
	ulong 	buffer, size;
	struct	ENQ_CONFIG *enq_config_ptr = &enq_config;


#ifdef DEBUG
	putstr(" \r\n IN dev_readcap...printing info str");
	putstr(" \r\n VENDOR id = ");
	putstr((char *)info->vid);
	putstr(" \r\n PRODUCT REV id = ");
	putstr((char *)info->prl);
	putstr(" \r\n BASE_PORT = ");
	puthex((ushort)info->base_port);
	putstr(" \r\n DEVICE-TYPE = ");
	put2hex((unchar)info->MDBdev.scsi.pdt);
	putstr(" \r\n DEVICE-TYPE-QUALIFIER = ");
	put2hex((unchar)info->MDBdev.scsi.dtq);
#endif

	slot   = info->base_port & 0xFF00;
	hba_type = info->junk[0];

	/*
	 * Need to do this only for PCI devices.
	 */
	if (is_pci_type(hba_type))
		slot = (slot >> 3);

	if (checkreinit(slot, hba_type) == FAIL) {
		putstr("Initialization Failed!");
	}

	if (DEV_SYS(info)) {		/* system-drives */
		readcap_data.rdcd_bl[3] = 0x0;		/* byte value 0 */
		readcap_data.rdcd_bl[2] = 0x2;		/* byte shifted */
		readcap_data.rdcd_bl[1] = 0x0;
		readcap_data.rdcd_bl[0] = 0x0;


		buffer = (ulong)longaddr((ushort)enq_config_ptr, myds());

		/* read ROM configuration */
		if (dac_init_cmd(slot, TYPE_5, DAC_ENQ_CONFIG,
			buffer, DUMMY, DUMMY, DUMMY, hba_type, &status)
								== FAIL) {
			handle_config_status(hba_type, status);
			putstr("dev_readcap, DAC_ENQ_CONFIG cmd fail\n");
			return (UNDEF_ERROR);
		}
		size = enq_config_ptr->sd_sz[info->MDBdev.scsi.targ];
		if (size > 0) {
			/*
			 * readcap_data.readcd_lba is
			 * last block number.
			 */
			size--;
		}
		readcap_data.rdcd_lba[0] = size >> 24;
		readcap_data.rdcd_lba[1] = (size >> 16) & 0xff;
		readcap_data.rdcd_lba[2] = (size >> 8)  & 0xff;
		readcap_data.rdcd_lba[3] = size & 0xff;

		return (0);
	} else {					/* non-system drives */
#ifdef DEBUG
		putstr(" \r\n CHANNEL = ");
		put2hex((unchar)info->MDBdev.scsi.targ);
		putstr(" \r\n TARGET = ");
		put2hex((unchar)info->MDBdev.scsi.lun);
#endif


		chan   = DEV_CHAN(info);
		target = info->MDBdev.scsi.targ;
		unit   = (chan << 4) | target;

		if (send_dcdb_readcap(slot, unit, (unchar *)&readcap_data,
				hba_type, &status) == FAIL) {
			return (1);
		}
#ifdef DEBUG
		putstr(" \r\n RETURNED status from dev_readcap = ");
		puthex((ushort)status);
		putstr("\r\n");
#ifdef PAUSE
		kbchar();
#endif
#endif
		if (check_status(hba_type, status) != CHS_SUCCESS)
			return (1);

		return (0);

	}
} /* end of dev_read() */

ushort
send_dcdb_readcap(ushort slot, ushort unit, unchar *address, ushort hba_type,
ushort *status)
{
	ulong 		cdb_addr;
	struct daccdb 	*cdbcmd = &cdbcmdstr;
	unchar		*cdbdata;

	/* channel upper 4 bits, target lower nibble */
	cdbcmd->cdb_unit = unit;

	/*
	 * disconnect, auto request sense, 10 second timeout,
	 * normal command completion and data-in
	 */
	cdbcmd->cdb_cmdctl = 0x91;
	cdbcmd->cdb_databuf = (ulong)longaddr((ushort)address, myds());
	cdbcmd->cdb_xfersz  = sizeof (struct readcap_data);
	cdbcmd->cdb_cdblen  = CDB_SCREAD_LEN;


	if (hba_type == CHS_PCIVPR)
		cdbdata = (unchar *) cdbcmd->fmt.v.cdb_data;

	*cdbdata = SX_READCAP;
	set_byte_zero((cdbdata + 1), 9);

	cdb_addr = (ulong)longaddr((ushort)cdbcmd, myds());

	if (dac_init_cmd(slot, TYPE_3, DAC_DCDB, cdb_addr,
	    DUMMY, DUMMY, DUMMY, hba_type, status) == FAIL) {
		return (FAIL);
	}

#ifdef DEBUG
	putstr(" \r\n Returned status from send_dcdb_readcap = ");
	puthex((ushort)status);
	putstr("\r\n");
#endif

	return (SUCCESS);
} /* end of send_dcdb_readcap() */

int
dev_motor(DEV_INFO *info, int start)
{
	int		hba_type;
	ushort 		slot, chan, target;
	ulong 		cdb_addr;
	unchar		*cdbdata;
	struct daccdb 	*cdbcmd = &cdbcmdstr;
	ushort		status;

	slot   = info->base_port & 0xFF00;
	hba_type = info->junk[0];

	/*
	 * Need to do this only for PCI devices.
	 */
	if (is_pci_type(hba_type))
		slot = (slot >> 3);

	if (checkreinit(slot, hba_type) == FAIL) {
		putstr("Initialization Failed!");
	}

	chan   = DEV_CHAN(info);
	target = info->MDBdev.scsi.targ;

	/* channel upper 4 bits, target  lower nibble */
	cdbcmd->cdb_unit = (chan << 4) | target;

	/*
	 * disconnect, auto request sense, 10 second timeout,
	 * normal command completion and no data xfr
	 */
	cdbcmd->cdb_cmdctl = 0x90;
	cdbcmd->cdb_cdblen  = CDB_SCSTRT_STP;


	if (hba_type == CHS_PCIVPR)
		cdbdata = (unchar *) cdbcmd->fmt.v.cdb_data;

	*cdbdata = SC_STRT_STOP;

	set_byte_zero((cdbdata + 1), 3);
	*(cdbdata + 4) = start;	/* start = 1 => unit ready to use */

	/* start = 0 => unit is stopped */
	set_byte_zero((cdbdata + 5), 1);

	cdb_addr = (ulong)longaddr((ushort)cdbcmd, myds());

	if (dac_init_cmd(slot, TYPE_3, DAC_DCDB, cdb_addr,
	    DUMMY, DUMMY, DUMMY, hba_type, &status) == FAIL) {
		return (1);
	}
	return (0);
} /* end of dev_motor() */

int
dev_lock(DEV_INFO *info, int lock)
{
	ushort		hba_type;
	ushort 		slot, chan, target;
	ulong 		cdb_addr;
	unchar 		*cdbdata;
	struct daccdb 	*cdbcmd = &cdbcmdstr;
	ushort		status;

	slot   = info->base_port & 0xFF00;
	hba_type = info->junk[0];

	/*
	 * Need to do this only for PCI devices.
	 */
	if (is_pci_type (hba_type))
		slot = (slot >> 3);

	if (checkreinit(slot, hba_type) == FAIL) {
		putstr("Initialization Failed!");
	}

	chan   = DEV_CHAN(info);
	target = info->MDBdev.scsi.targ;

	/* channel upper 4 bits, target  lower nibble */
	cdbcmd->cdb_unit = (chan << 4) | target;

	/*
	 * disconnect, auto request sense, 10 second timeout,
	 * normal command completion and no data xfr
	 */
	cdbcmd->cdb_cmdctl = 0x90;
	cdbcmd->cdb_cdblen  = CDB_SCREMV_MED;


	if (hba_type == CHS_PCIVPR)
		cdbdata = (unchar *) cdbcmd->fmt.v.cdb_data;

	*cdbdata = SC_REMOV;
	set_byte_zero((cdbdata + 1), 3);
	*(cdbdata + 4) = lock;		/* lock = 1, unlock = 0 */
	set_byte_zero((cdbdata + 5), 1);

	cdb_addr = (ulong)longaddr((ushort)cdbcmd, myds());

	if (dac_init_cmd(slot, TYPE_3, DAC_DCDB, cdb_addr,
	    DUMMY, DUMMY, DUMMY, hba_type, &status) == FAIL)  {
		return (1);
	}
	return (0);

} /* end of dev_lock() */

void
set_byte_zero(unchar *start_addr, ushort count)
{
	register ushort i;

	for (i = 0; i < count; i++) {
		*(start_addr + i) = 0x0;
	}
}

/*ARGSUSED*/
init_dev(ushort base_port, ushort dataseg)
{
}


/*ARGSUSED*/
int
dev_init(DEV_INFO *info)
{
	return (0);
}

/*ARGSUSED*/
int
dev_sense(ushort base_port, ushort targid, ushort lun)
{
	return (0);
}

/*ARGSUSED*/
int
dev_inquire(ushort base_port, ushort targid, ushort lun)
{
	return (0);
}


void
pci_disable_intr(ushort hba_type, ushort iobase)
{
	unchar old;
	switch (hba_type) {
		case CHS_PCIVPR:
			old = (unchar) inb((ushort) iobase | HIST_REG);
			outb((ushort) iobase | HIST_REG, (unchar) (old & ~0x8));
			break;
		default:
			putstr("unknown type\n");
	}
}

int
Check_regbit(ushort slot, unchar regoffset, unchar whichbit,  long timeframe,
		int check)
{
	long i = 0;

	for (i = 0; i < timeframe; i++) {
		if (check) {
			if (inb(slot | regoffset) &
				whichbit) {
				return (TRUE);
			}
		} else {
			if ((inb(slot | regoffset) &
				whichbit) == 0) {
				return (TRUE);
			}
		}
		milliseconds(1000);
	}
	return (FALSE);
}

int
pci_card_reset(ushort slot, ushort hba_type)
{
	int reset_counter = 0;
	int reset_pass = FALSE;
	int inx;

	switch (hba_type) {

	case CHS_PCIVPR:
		if ((inx = get_chs_inx(CHS_PCIVPR, slot)) < 0)
			return (FAIL);

		slot = chs[inx].iobase;

		while ((reset_counter < 2) && (reset_pass != TRUE)) {
			reset_pass = TRUE;
			reset_counter++;
			outb(slot | SCPR_REG, (unchar)(inb(slot | SCPR_REG) |
				VIPER_RESET_ADAPTER));
			milliseconds(1);

			outb(slot | SCPR_REG, (unchar)(inb(slot | SCPR_REG) &
						~VIPER_RESET_ADAPTER));
			if (Check_regbit(slot, HIST_REG, VIPER_HIST_GHI, 45, 1)
						== FALSE) {
				reset_pass = FALSE;
				continue;
			}
		}
		chs[inx].was_reset = 1;
		if (reset_pass == FALSE) {
			putstr ("pci_card_reset not successful \r\n");
			return (FAIL);
		} else
			return (SUCCESS);

	}
}

int
checkreinit(ushort slot, ushort hba_type)
{
	int inx;
	if ((inx = get_chs_inx(hba_type, slot)) < 0)
			return (FAIL);

	if ((chs[inx].cds != myds()) || (chs[inx].was_reset == 1)) {
		return (card_init(slot, hba_type));
	}
	return (SUCCESS);
}

int
card_init(ushort slot, ushort hba_type)
{
	int inx;
	int status = SUCCESS;

	if ((inx = get_chs_inx(hba_type, slot)) < 0)
		return (FAIL);

	if (hba_type ==  CHS_PCIVPR) {
		status = viper_card_init (slot, hba_type);
	}

	chs[inx].was_reset = (status == SUCCESS) ? 0:1;

	return (status);
}

int
viper_card_init(ushort slot, ushort hba_type)
{
	int inx, i;
	ulong statusq_phys;
	struct viper_statusq_element *statusq;
	int reset_counter = 0;
	unchar postmajor, postminor, bcs, ecs;
	int reset_pass = FALSE;


	if ((inx = get_chs_inx(CHS_PCIVPR, slot)) < 0)
		return (FAIL);

	slot = chs[inx].iobase;
	chs[inx].cds = myds();

	/* Initialization for I/O Processing */

	reset_counter = 0;
	/* step3: */
step3:

	while ((reset_counter < 2) && (reset_pass != TRUE)) {
		reset_pass = TRUE;
		reset_counter++;

		outb(slot | SCPR_REG, (unchar)(inb(slot | SCPR_REG) |
				VIPER_RESET_ADAPTER));
		milliseconds(1);

		outb(slot | SCPR_REG, (unchar)(inb(slot | SCPR_REG) &
						~VIPER_RESET_ADAPTER));
		/* step4 */
		if (Check_regbit(slot, HIST_REG, VIPER_HIST_GHI, 45, 1) ==
									FALSE) {

#ifdef DEBUG
putstr ("viper_init, step3 fail\n");
#endif
			reset_pass = FALSE;
			continue;
		}

		/* step5: */
		postmajor = inb(slot | ISPR_REG);

		/* step6: */
		outb(slot | HIST_REG, (unchar)(inb(slot | HIST_REG) |
					VIPER_HIST_GHI));

		/* step7: */

		if (Check_regbit(slot, HIST_REG, VIPER_HIST_GHI, 45, 1) ==
									FALSE) {
#ifdef DEBUG
putstr ("viper_init, step7 fail\n");
#endif
			reset_pass = FALSE;
			continue;
		}

		/* step8: */
		postminor = inb(slot | ISPR_REG);

		/* step9: */
		outb(slot | HIST_REG, (unchar)(inb(slot | HIST_REG) |
						VIPER_HIST_GHI));

		/* step10: */
		if (!(postmajor & 0x80)) {
			reset_pass = FALSE;
#ifdef DEBUG
putstr ("viper_init, step10 fail\n");
#endif
			break;
		}
		/* step11: */
		if (!(postmajor & 0x40)) {
			putstr("viper_init: SCSI Bus Problem\r\n");
			reset_pass = FALSE;
			break;
		}
		/* step12: */
		if (Check_regbit(slot, HIST_REG, VIPER_HIST_GHI, 240, 1) ==
									FALSE) {
#ifdef DEBUG
putstr ("viper_init, step12 fail\n");
#endif
			reset_pass = FALSE;
			continue;
		}
		/* step13: */
		bcs = inb(slot | ISPR_REG);

		/* step14: */
		outb(slot | HIST_REG, (unchar)(inb(slot | HIST_REG) |
				VIPER_HIST_GHI));
		/* step15: */

		if (Check_regbit(slot, HIST_REG, VIPER_HIST_GHI, 240, 1) ==
									FALSE) {
#ifdef DEBUG
putstr ("viper_init, step15 fail\n");
#endif
			reset_pass = FALSE;
			continue;
		}
		/* step16: */
		ecs = inb(slot | ISPR_REG);

		/* step17: */
		outb(slot | HIST_REG, (unchar)(inb(slot | HIST_REG) |
				VIPER_HIST_GHI | VIPER_HIST_EI));

		/* step18: */
		if (Check_regbit(slot, CBSP_REG, VIPER_OP_PENDING, 240, 0) ==
									FALSE) {
#ifdef DEBUG
putstr ("viper_init, step18 fail\n");
#endif
			reset_pass = FALSE;
			continue;
		}
	}

	if (reset_pass == FALSE) {
		putstr("viper_init: Defective Board\n");
		putstr("viper_init: Initialization failed\n");
		return (FAIL);
	}

	if (!(((bcs == 0x0F) || (bcs == 0x09)) && (ecs == 0))) {
		putstr("viper_init: Raid Configuration Problem\r\n");
	}

	/* Set up CCCR, enable bus master */
	outl(slot | CCCR_REG, 0x10);

	/* step21: */
	outb(slot | SCPR_REG, (unchar)(inb(slot | SCPR_REG) |
				VIPER_ENABLE_BUS_MASTER));

	/* step22: */
	/* Setup status queue */

	if (chs[inx].statusqassigned == 0) {
		statusq = nextavailstatusq;
		nextavailstatusq = nextavailstatusq + VIPER_STATUS_QUEUE_ELEMS;
		chs[inx].statusqassigned = 1;
	} else
		statusq = chs[inx].sqsr;

	for (i = 0; i < 8; i++)
		* ((unchar *)statusq + i) = 0;

	statusq_phys = (ulong)longaddr((ushort)statusq, myds());

	outl(slot | SQSR_REG, statusq_phys);
	outl(slot | SQHR_REG, statusq_phys + 4);
	outl(slot | SQER_REG, statusq_phys + 8);
	outl(slot | SQTR_REG, statusq_phys);


	chs[inx].sqsr = (struct viper_statusq_element *)statusq;
	chs[inx].sqtr = (struct viper_statusq_element *)statusq;

	outb (slot | HIST_REG, 0x04);

	return (SUCCESS);
}



/*
 * pci_dev_find() -- finds which pci slots have this adapter
 */
void
pci_dev_find()
{
	int 		num;
	int 		dev;
	ushort 		id, vid, devid, type;
	unchar 		iline_reg;
	ushort 		cmd_reg;
	unchar		id3_mask;
	ulong 		address;


	struct card_spec {
		ushort vendorid;
		ushort hbatype;
		ushort deviceid;
	} cardid[] = {
		{ DACVPR_VID, CHS_PCIVPR, DACVPR_DID1 },
	};
	static int 	devtab_items = sizeof (cardid) /
					sizeof (struct card_spec);



	if (!is_pci()) {
#ifdef DEBUG
		putstr(ident);
		putstr(" pci_dev_find(): Not a PCI machine\r\n");
#endif
		return;
	}

#ifdef DEBUG
putstr("pci_dev_find(): Its PCI machine\r\n");
#endif


	nextavailstatusq = (struct viper_statusq_element *)
				(((ulong) totalstatusq + 3) & ~0x3);

	for (dev = 0; dev < devtab_items; dev++) {
		vid = cardid[dev].vendorid;
		devid = cardid[dev].deviceid;
		type = cardid[dev].hbatype;
		for (num = 0; ; num++) {

			if (!pci_find_device(vid, devid, num, &id)) {
				break;
			}

			if (!pci_read_config_byte(id, PCI_ILINEREG, &iline_reg))
				continue;

			/*
			 * Assume device is disabled if IRQ is 0
			 */
			if (iline_reg < 1)
				continue;

			if (!pci_read_config_word(id, PCI_CMDREG, &cmd_reg))
				continue;

			/*
			 * Get the iobase address
			 */
			if (!pci_read_config_dword(id, PCI_BASEAD, &address))
				continue;

			pci_dev_found(id, vid, devid, (ushort)(address & ~1), type);
		}
	}
}


#ifdef DEBUG
# pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
# pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
# define Dprintf(f, x) if (debug_chs & f) printf x ; else
# define Pause() {							\
	printf("<Press any key to continue>"); kbchar(); printf("\n");	\
}
#else
# define Dprintf(f, x)
# define Pause()
#endif
#define	DBG_ERRS	0x0001	/* ... error messages */
#define	DBG_GEN		0x0002	/* ... general info */
#define	DBG_INIT	0x0004	/* ... initialization code */
#define	DBG_ALL		0xffff
#define	DBG_OFF		0x0000
int debug_chs = DBG_ALL;


legacyprobe()
{
}

#define	GetRes(Res) {							\
	len = Res##TupleSize;						\
	if (get_res(Res##Name, val, &len) == RES_FAIL) {		\
		Dprintf(DBG_ERRS, ("Can't find %s resource!\n", Res##Name));	\
		node_op(NODE_FREE);					\
		return (BEF_FAIL);					\
	}								\
	Res = val[0];							\
}

#define	NameName "name"
#define	PortName "port"
#define	SlotName "slot"
#define	AddrName "addr"

installonly()
{
	DWORD	val[3], len;
	int	Rtn = BEF_FAIL;		/* ... default case if nothing works */
	ulong	vvvvdddd;
	ushort	Name, Port, Slot, Bus, Addr,
		cmd_reg;
	unchar	iline_reg;

	nextavailstatusq = (struct viper_statusq_element *)
				(((ulong) totalstatusq + 3) & ~0x3);
	do {
		if (node_op(NODE_START) == NODE_FAIL)
			return (Rtn);

		/*
		 * Special case for Bus type which is stored in the
		 * Name resource.
		 */
		GetRes(Name); Bus = (ushort)val[1];

		switch (Bus) {
		      case RES_BUS_PCI:
			vvvvdddd = val[0];
			GetRes(Port);
			GetRes(Addr);
			if (!pci_read_config_word(Addr, PCI_CMDREG, &cmd_reg)) {
				return (BEF_FAIL);
			}

			if (!pci_read_config_byte(Addr, PCI_ILINEREG, &iline_reg))
				return (BEF_FAIL);
			/*
			 * Assume device is disabled if IRQ is 0
                         */
			if (iline_reg < 1)
				return (BEF_FAIL);

			pci_dev_found(Addr, (ushort) (vvvvdddd >> 16),
			    (ushort) (vvvvdddd & 0xffff), Port, CHS_PCIVPR);
			break;

		}
		node_op(NODE_DONE);

		Rtn = BEF_OK;
	} while (1);
}






void
pci_dev_found(ushort id, ushort vid, ushort devid, ushort address,
								ushort type)
{
	ushort 		slotnum, tempslot, status;
	ushort 		cmd_reg;
	int		succeed;
	ulong  		buffer;
	union CONFIG 	*conf_ptr = &config_str;
	unchar 		*buf;
	ushort 		max_chns = 0, max_tgts = 0;

	if (!pci_read_config_word(id, PCI_CMDREG, &cmd_reg))
		return;
	pci_write_config_word(id, PCI_CMDREG, cmd_reg |
		PCI_COMM_IO | PCI_COMM_ME |
		PCI_COMM_PARITY_DETECT);

	slotnum  = (id >> 3) & 0x1f;
	tempslot = (slotnum << MS2_NIBBLE);

	chs[++chs_inx].hba_type = type;
	chs[chs_inx].iobase = (ushort)address;
	chs[chs_inx].slot = tempslot;
	chs[chs_inx].cds	= myds();

	chs[chs_inx].pci = 1;
	chs[chs_inx].pci_ven_id = vid;
	chs[chs_inx].pci_vdev_id = devid;
	chs[chs_inx].pci_bus = (id >> 8);
	chs[chs_inx].pci_dev = ((id >> 3) & 0x1f);
	chs[chs_inx].pci_func = (id  & 0x7);

	/*
	 * Disable Interrupts.
	 */

	succeed = card_init(tempslot, type);
	if (succeed != SUCCESS) {
		putstr("pci_dev_find:");
		putstr(" unsuccessful initializaiton\r\n");
		return;
	}

	pci_disable_intr(type, (ushort)address);
#ifdef	DEBUG
	putstr(" pci_dev_find(): iobase ");
	puthex((ushort)address);
	putstr(" slot ");
	puthex((ushort)slotnum);
	putstr(" tempslot ");
	puthex((ushort)tempslot);
	putstr("\r\n");
	putstr("<< PRESS ANY KEY >>");
#ifdef PAUSE
	kbchar();
#endif
	putstr("\r\n");
#endif


#ifdef	DEBUG
	putstr(ident);
	putstr(" pci_dev_found(): found device ");
	puthex(devid);
	putstr(" slot ");
	puthex((ushort)slotnum);
	putstr(" at base address ");
	puthex((ushort)address);
	putstr("\r\n");
	putstr("<< PRESS ANY KEY >>");
#ifdef PAUSE
	kbchar();
#endif
	putstr("\r\n");
#endif
	buffer = (ulong)longaddr((ushort)conf_ptr, myds());

	/* read ROM configuration */
	if (dac_init_cmd(tempslot, TYPE_5, DAC_CONFIG,
	    buffer, DUMMY, DUMMY, DUMMY, type, &status) == FAIL) {
		putstr("pci_dev_find, DAC_CONFIG cmd fail\n");
	} else
		handle_config_status(type, status);
#ifdef DEBUG
	putstr(" Finished reading configuration ");
	putstr("\r\n");
#ifdef PAUSE
	kbchar();
#endif
#endif
	buf = (unchar *)conf_ptr;

	/*
	 * set up system devices
	 */
	setup_system_devices(tempslot, buf, type);

	/*
	 * set up non-system devices
	 */
	setup_max_chns_tgts(tempslot, &max_chns, &max_tgts, type);

#ifdef DEBUG
	putstr(" After SETTING up MAX CHNS = ");
	put2hex((unchar)max_chns);
	putstr(" MAX TGTS = ");
	put2hex((unchar)max_tgts);
	putstr("\r\n");
#endif

	setup_non_system_devices(tempslot, buf, max_chns, max_tgts, type);
#ifdef DEBUG
	putstr(" Finished setting up non-system devices ");
	putstr("\r\n");
#ifdef PAUSE
	kbchar();
#endif
#endif

#ifdef DEBUG
	putstr("Resetting the card\r\n");
#ifdef PAUSE
	kbchar();
#endif
#endif
	succeed = pci_card_reset (tempslot, type);
	if (succeed != SUCCESS) {
		putstr("pci_dev_find:");
		putstr(" unsuccessful reset\r\n");
	}
}



struct viper_statusq_element *
next_status(struct viper_statusq_element *start,
		struct viper_statusq_element *elem)
{
	if (elem == start) {
		return (elem + 1);
	}else
		return (start);
}


ushort
pci_vpr_init_cmd(ushort slot, unchar cmd_type, unchar opcode, ulong address,
unchar drive, ulong start_block, unchar count, ushort *status)
{

	int inx, succeed;
	long sqhr;
	ushort stat, free_counter = 0, intr_counter = 0, cmd_counter = 0;
	unchar totcmdblk[28];		/*
					 * cmdblk is 24 bytes but four
					 * more bytes added if alignement
					 * necessary
					 */
	unchar 	*cmdblk;
	struct viper_statusq_element *statusq;
	int i;

	if ((inx = get_chs_inx(CHS_PCIVPR, slot)) < 0)
		return (FAIL);

	slot = chs[inx].iobase;



	cmdblk = (unchar *) (((ulong) totcmdblk + 3) & ~0x3);

#ifdef DEBUG
#ifdef PAUSE
	kbchar();
#endif
	putstr(" \r\n Viper Waiting just before check for mailbox...\n");
#endif



	while ((inl(slot | CCCR_REG) & VIPER_CCCR_SEMBIT) != 0) {
		free_counter++;
		if (free_counter > MAX_COUNTER) {
			putstr(" Command channel not getting free...\r\n");
			return (FAIL);
		}
		milliseconds(1);
	}

	for (i = 0; i < 15; i++)
		*(cmdblk + i) = 0;

	*(cmdblk + VPR_CMD_OPCODE) = opcode;
	cmdid++;
	*(cmdblk + VPR_STATID) = cmdid;
	*(ulong *)(cmdblk + VPR_DCDB_ADDR_START) = address;


	*(cmdblk + 16) = 0x0;
	*(cmdblk + 17) = 0x0;
	*(cmdblk + 18) = 0x0;
	*(cmdblk + 19) = 0x0;
	*(cmdblk + 20) = 0x10;
	*(cmdblk + 21) = 0x0;
	*(cmdblk + 22) = 0x0;
	*(cmdblk + 23) = 0x0;

	if (cmd_type == TYPE_1) {	/* read command for us */
		/* this is really type c for viper DAC_READ command */

		*(cmdblk + VPR_DRV) = (unchar)drive;
		*(cmdblk + VPR_CMD_BLOCK_0) = (unchar) start_block;
		*(cmdblk +  VPR_CMD_BLOCK_1) =
				(unchar)((ushort)start_block >> 8);
		*(cmdblk +  VPR_CMD_BLOCK_1) = (unchar)(start_block >> 8);
		*(cmdblk + VPR_CMD_BLOCK_2) = (unchar) (start_block >> 16);
		*(cmdblk + VPR_CMD_BLOCK_3) = (unchar) (start_block >> 24);
		*(cmdblk + VPR_COUNT0) = (unchar)count;
		*(cmdblk + VPR_COUNT1) = 0;
	}
#ifdef DEBUG
	putstr("Built command block\n");
	for (i=0; i<15; i++) {
		puthex(*(cmdblk + i));
		putchar(' ');
	}
	putstr("\n");
#endif

	outl ((slot | CCSAR_REG), (ulong) (longaddr((ushort)(cmdblk), myds())));

	/*
	 * Indicate new command
	 */

	outl(slot | CCCR_REG, VIPER_CCCR_SS | VIPER_CCCR_ILE | 0x1000);

	while ((inb((slot | HIST_REG)) & 0x1) != (DAC_CHKINTR)) {
		intr_counter++;
		if (intr_counter > MAX_COUNTER) {
			putstr(" Not getting status from controller ...\r\n");
			return (FAIL);
		}
		milliseconds(1);
	}
#ifdef DEBUG
	putstr("received a status\n");
#endif

	statusq = next_status (chs[inx].sqsr, chs[inx].sqtr);

	/*
	 * While status is not for this command
	 */
	while ((statusq->stat_id) != cmdid) {
		cmd_counter++;
		if (cmd_counter > MAX_COUNTER) {
			putstr(" Cmd-id does not match with status-id \r\n");
#ifdef DEBUG
			putstr(" CMDID :: STATID == ");
			put2hex((unchar)cmdid);
			putstr("::");
			put2hex((unchar)statusq->stat_id);
			putstr("\r\n");
#endif

			chs[inx].sqtr = statusq;

			return (FAIL);
		}

		outl(slot | SQTR_REG, longaddr((ushort)statusq, myds()));
		sqhr = inl(slot | SQHR_REG);
		if (sqhr == longaddr((ushort)statusq, myds())) {
			statusq = next_status (chs[inx].sqsr, statusq);
		}
		milliseconds(1);
	}

#ifdef DEBUG
	putstr(" Also stat_id matches command-id \r\n");
#ifdef PAUSE
	kbchar();
#endif
#endif
	chs[inx].sqtr = statusq;

	stat = statusq->esb;
	stat = (stat << 8) | (statusq->bsb);

	outl(slot | SQTR_REG, longaddr((ushort)(statusq), myds()));

#ifdef DEBUG
	putstr(" In vpr_init_cmd...after all POSTs, STATUS = \r\n");
	puthex((ushort)stat);
#endif

	*status = stat;
	return (SUCCESS);

} /* end of pci_vpr_init_cmd() */




/*ARGSUSED*/
int
get_chs_inx(ushort hba_type, ushort slot)
{
	register i;
	int	 rval = -1;

	/*
	 * Search the HBA table looking for a matching
	 * HBA type and slot.
	 */
	if (chs_inx >= 0) {
		for (i = 0; i < chs_inx + 1; i++) {
			if (chs[i].hba_type == hba_type &&
			    ((chs[i].slot & 0xFF00)  == (slot & 0xFF00))) {
				rval = i;
				break;
			}
		}
	}

	return (rval);
} /* end get_chs_inx */

#ifdef	MSCSI_FEATURE
/*
 * make_bootpath_name: build special device tree path for chs.
 * Note that the chs system drives attach directly to the
 * node, and thus don't have an extra component.
 */
void
make_bootpath_name(char	*bp, char *ven_dev_id, ushort slot,
	char *chn_name, ushort chn)
{
	ushort 	i;
	char	*bps = bp;

	/*
	 * The raid system drives attach directly
	 * to the chs driver node, so no mscsi component
	 * is necessary.
	 */
	if (chn == 0xFF)
		return;

	/*
	 * Creates a Solaris bootpath name:
	 *
	 *		pci1014,2e@0/chs@ff
	 *		      ^ ^	 ^
	 *		      | |	 |
	 *	 Vendor ID ---+ |	 |
	 *	 Device ID -----+	 |
	 *	 Channel   --------------+
	 */

	/*
	 * Vendor / device part.
	 */
	i = 0;
	while (ven_dev_id[i])
		*bp++ = ven_dev_id[i++];

	/*
	 * Slot number.
	 */
	if ((slot >> 4) & 0xf)
		*bp++ = ((slot >> 4) & 0xf) + '0';

	slot &= 0xF;
	*bp++ = (slot < 10) ? slot + '0' : (slot - 10) + 'a';
	*bp++ = '/';

	/*
	 * Channel component.
	 */
	i = 0;
	while (chn_name[i])
		*bp++ = chn_name[i++];

	if (chn == 0xFF) {
		*bp++ = 'f';
		*bp++ = 'f';
	} else
		*bp++ = (chn & 0xf) + '0';

	*bp++ = ',';
	*bp++ = '0';
	*bp++ = '\0';

	/*
	 * Copy bootpath into dev_info structure.
	 */
	i = 0;
	while (*bps)
		dev_info[devs].user_bootpath[i++] = *bps++;

#ifdef DEBUG
	putstr("chs bootpath: ");
	putstr(dev_info[devs].user_bootpath);
	putstr("\r\n");
#ifdef PAUSE
	putstr("Hit ENTER to continue ");
	kbchar();
	putstr("\r\n");
#endif
#endif
}
#endif
