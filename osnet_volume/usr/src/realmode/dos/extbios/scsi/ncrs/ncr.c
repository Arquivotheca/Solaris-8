/*
 * Copyright (c) 1998, Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: NCR 710/810 EISA SCSI HBA       (ncr.c)
 */

#pragma ident	"@(#)ncr.c	1.27	98/06/17 SMI"

/* #define	DEBUG /* */

#include <types.h>
#include <dev_info.h>
#include <bef.h>
#include <befext.h>
#include "..\scsi.h"
#include "ncr.h"
#include "ncr_sni.h"

#ifdef DEBUG
#define	INIT_CMD	0x0001
#define	DEV_READ	0x0002
#define	DEV_READ_Q	0x0004
#define	MIL_SEC		0x0008
#define	DEV_INQ		0x0010
#define	DEV_RCAP	0x0020
#define	INITB		0x0040
#define	DEV_FIND	0x0080
#define	DEV_MOTOR	0x0100
#define	DEV_LOCK 	0x0200
#define	DEV_INIT	0x0400
#define	EISA_C		0x0800
#define DEV_RESET	0x1000
ushort debuglevel=DEV_FIND;

#define putstr(s) printf(s)
#define puthex(n) printf("%x", n)
#endif

#ifdef DEBUG
    #pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
    #pragma comment (user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#endif

#pragma comment (compiler)
#pragma comment (user, "$Id: @(#)ncr.c	1.1	97/07/17")


char ident[8] = "NCRS";

/*
 * These definitions provide the stack for resident code.
 * They are used by low.s.  They are placed here so that the
 * stack size can be optimized for this module.
 */
#define	STACKSIZE	1000	/* Resident stack size in words */
ushort stack[STACKSIZE];
ushort stacksize = STACKSIZE;

#define	SENSE_BYTES	14

#define	MAXADAPTERS	16

int num_adapters = 0;
struct	adapter_info {
	ushort	ioaddr;
	ushort	devid;
	unchar	pci;
	unchar	pci_bus;
	ushort	pci_ven_id;
	ushort	pci_vdev_id;
	unchar	pci_dev;
	unchar	pci_func;
	ncr_t	ncr;
} adapter_info[MAXADAPTERS] = { 0 };

static ulong transfer_address = 0;
static ulong transfer_length = 0;
static u_char npt_arena[sizeof (npt_t) + 4] = { 0 };
static npt_t *nptp = 0;
static struct scsi_pkt pkt = { 0 };
static unchar sense_data[SENSE_BYTES];
/* 
 * This simple change avoids the need for a seperate scsi.c
 * To have 2.5.1/2.6 share the common source, move this flag from 
 * installonly() here as an extern global variable.
 */
int init_dev_before;

#define	CMDLEN	nptp->nt_cmd.count
#define	CMDBYTE	nptp->nt_cdb.cdb_opaque
#define SENSE_NOT_READY		0x2
#define SENSE_UNIT_ATTENTION	0x6

STATIC int gethex(ushort *num);
STATIC int run_cmd(ushort base_port, int target, unchar rqst_type);
STATIC void bzero(unchar *p, ushort n);
STATIC int pci_dev_found(ushort id, ushort vdevid);
STATIC void add_siemens_ncr710(ushort ioaddr);
STATIC void check_siemens_motherboard();
STATIC void find_pci_ctlrs();
STATIC void find_eisa_ctlrs();
STATIC void find_isa_ctlrs();
STATIC int read_capacity(ushort base_port, ushort target, ushort lun);
STATIC int dev_reset(ushort base_port, ushort target, ushort lun);
STATIC int dev_probe(ushort base_port, ushort target, ushort lun);

struct eisa_board_info {
	long	id;		/* EISA compressed ID */
	ushort	offset;		/* offset of chips registers from slot base */
	ushort	flags;
};
#define	EBIFL_SIEMENS	1	/* Siemens/Nixdorf motherboard */

static struct eisa_board_info eisa_board_table[] = {
    {	0x1144110e,	0,	0    },	/* Compaq part #1089, rev 1 */
    {	0xC1AAC94D,	0,	EBIFL_SIEMENS	} /* Siemens PCE-5S */
};

static ushort known_eisa_boards =
    sizeof (eisa_board_table) / sizeof (struct eisa_board_info);


#ifdef DEBUG
STATIC int
gethex(ushort *num)
{
	int any = 0;
	int ch;

	*num = 0;
	for (; ; ) {
		ch = kbchar();
		switch (ch) {
		case '\r': case '\n':
			return (any);
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			putchar(ch);
			*num <<= 4;
			*num += ch - '0';
			any = 1;
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			putchar(ch);
			*num <<= 4;
			*num += ch - 'a' + 10;
			any = 1;
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			putchar(ch);
			*num <<= 4;
			*num += ch - 'A' + 10;
			any = 1;
			break;
		default:
			putchar(7);	/* bell */
			break;
		}
	}
}
#endif /* DEBUG */

STATIC int
run_cmd(ushort base_port, int target, unchar rqst_type)
{
	ushort index;

	/*
	 * Find the correct adapter structure.  Report failure
	 * for command if not found.
	 */
	for (index = 0; index < num_adapters; index++) {
		if (adapter_info[index].devid == base_port)
			break;
#define	OLDNODES
#ifdef	OLDNODES
		/*
		 * HACK ALERT - make this realmode driver work
		 * on 2.4 only!!! The 2.4 solaris driver 
		 * expects func to be shifted off base_port 
		 * Argh.
		 */
		if (adapter_info[index].pci &&
		    adapter_info[index].devid == (base_port << 3))
			break;
#define	OLDNODES
#endif
	}
	if (index == num_adapters) {
		return (1);
	}

	/*
	 * The following items are derived from TableInit() in
	 * the Solaris driver init.c file.  We have to set these
	 * up separately for each command because we share a single
	 * npt_t between all devices.
	 */
	bzero((unchar *)&pkt, sizeof (struct scsi_pkt));
	ncr_table_init(&adapter_info[index].ncr, nptp, target,
			CMDBYTE[1] >> 5, CMDLEN, &pkt);

	if (transfer_length == 0) {
		nptp->nt_savedp.nd_num = 0;
	} else {
		ushort	i;
		nptp->nt_savedp.nd_num = 1;
		i = NCR_MAX_DMA_SEGS - nptp->nt_savedp.nd_num;
		nptp->nt_savedp.nd_data[i].count = transfer_length;
		nptp->nt_savedp.nd_data[i].address = transfer_address;
	}

	/*
	 * Send the command and wait for completion.
	 */
	ncr_queue_ccb(&adapter_info[index].ncr, nptp, rqst_type);
	ncr_pollret(&adapter_info[index].ncr, &pkt);

	/*
	 * Analyze result.
	 */
	switch (pkt.pkt_reason) {
	case CMD_CMPLT:
		return (0);
	case CMD_TIMEOUT:
		return (2);
	default:
		if (pkt.pkt_statistics == STAT_DISCON &&
			 rqst_type == NRQ_DEV_RESET)
			return (0);
		else
			return (1);
	}
}

int
dev_read(DEV_INFO *info, long start_block, ushort count,
	   ushort bufo, ushort bufs)
{
	register int i;

	/*
	 * Quick and dirty hack to allow long result of multiply
	 * without involving library routines.
	 */
	for (transfer_length = 0, i = 0; i < count; i++) {
		transfer_length += info->MDBdev.scsi.bsize;
	}
	transfer_address = longaddr(bufo, bufs);

	CMDLEN = 10;
	CMDBYTE[0] = SX_READ;
	CMDBYTE[1] = info->MDBdev.scsi.lun << 5;
	CMDBYTE[2] = (start_block >> 24) & 0xff;
	CMDBYTE[3] = (start_block >> 16) & 0xff;
	CMDBYTE[4] = (start_block >> 8) & 0xff;
	CMDBYTE[5] = start_block & 0xff;
	CMDBYTE[6] = 0;
	CMDBYTE[7] = count >> 8;
	CMDBYTE[8] = count & 0xFF;
	CMDBYTE[9] = 0;

#ifdef DEBUG
	if (debuglevel & DEV_READ) {
		union {
			ushort s[2];
			char _far *p;
		} u;

		putstr("dev_read(): transfer length ");
		puthex(transfer_length);
		putstr(" info->bsize ");
		puthex(info->MDBdev.scsi.bsize);
		putstr(" start_block ");
		puthex(start_block);
		putstr(" target ");
		puthex(info->MDBdev.scsi.targ);
		putstr(" lun ");
		puthex(info->MDBdev.scsi.lun);
		putstr(" count ");
		puthex(count);
		putstr(" buffer ");
		puthex(bufs);
		putchar(':');
		puthex(bufo);
		putstr(" (");
		puthex(transfer_address >> 16);
		puthex(transfer_address);
		putstr(") ");

		u.s[0] = bufo;
		u.s[1] = bufs;
		putstr("\r\nlead guard bytes: ");
		put2hex(u.p[-4]); putchar(' ');
		put2hex(u.p[-3]); putchar(' ');
		put2hex(u.p[-2]); putchar(' ');
		put2hex(u.p[-1]); putstr("\r\n");
		putstr("first bytes: ");
		put2hex(u.p[0]); putchar(' ');
		put2hex(u.p[1]); putchar(' ');
		put2hex(u.p[2]); putchar(' ');
		put2hex(u.p[3]); putstr("\r\n");
		putstr("last bytes: ");
		put2hex(u.p[transfer_length - 4]); putchar(' ');
		put2hex(u.p[transfer_length - 3]); putchar(' ');
		put2hex(u.p[transfer_length - 2]); putchar(' ');
		put2hex(u.p[transfer_length - 1]); putstr("\r\n");
		putstr("trail guard bytes: ");
		put2hex(u.p[transfer_length]); putchar(' ');
		put2hex(u.p[transfer_length + 1]); putchar(' ');
		put2hex(u.p[transfer_length + 2]); putchar(' ');
		put2hex(u.p[transfer_length + 3]); putstr("\r\n");
		putstr("<< PRESS ANY KEY >>");
		kbchar();
		putstr("\r\n");
	}
#endif
	if ((i = run_cmd(info->base_port, info->MDBdev.scsi.targ,
		 NRQ_NORMAL_CMD)) == 0) {
		if (nptp->nt_statbuf[0] != S_GOOD)
			i = 1;
	}
#ifdef DEBUG
	if (debuglevel & DEV_READ) {
		putstr("dev_read(): ");
		if (i) {
			putstr("failed\r\n");
			putstr("<< PRESS ANY KEY >>");
			kbchar();
			putstr("\r\n");
		} else {
			union {
				ushort s[2];
				char _far *p;
			} u;

			u.s[0] = bufo;
			u.s[1] = bufs;
			putstr("lead guard bytes: ");
			put2hex(u.p[-4]); putchar(' ');
			put2hex(u.p[-3]); putchar(' ');
			put2hex(u.p[-2]); putchar(' ');
			put2hex(u.p[-1]); putstr("\r\n");
			putstr("first bytes: ");
			put2hex(u.p[0]); putchar(' ');
			put2hex(u.p[1]); putchar(' ');
			put2hex(u.p[2]); putchar(' ');
			put2hex(u.p[3]); putstr("\r\n");
			putstr("last bytes: ");
			put2hex(u.p[transfer_length - 4]); putchar(' ');
			put2hex(u.p[transfer_length - 3]); putchar(' ');
			put2hex(u.p[transfer_length - 2]); putchar(' ');
			put2hex(u.p[transfer_length - 1]); putstr("\r\n");
			putstr("trail guard bytes: ");
			put2hex(u.p[transfer_length]); putchar(' ');
			put2hex(u.p[transfer_length + 1]); putchar(' ');
			put2hex(u.p[transfer_length + 2]); putchar(' ');
			put2hex(u.p[transfer_length + 3]); putstr("\r\n");
			putstr("<< PRESS ANY KEY >>");
			kbchar();
			putstr("\r\n");
		}
	}
#endif
	return (i ? UNDEF_ERROR : 0);
}

int
dev_init(DEV_INFO *info)
{
	/* Device specific stuff goes here */

#ifdef DEBUG
	if (debuglevel & DEV_INIT) {
		putstr(ident);
		putstr(" dev_init(): number of adapter(s): ");
		puthex(num_adapters);
		putstr("\r\n");
		putstr("<< PRESS ANY KEY >>\r\n");
		kbchar();
	}
#endif
	return (!num_adapters);
}


int
dev_sense(ushort base_port, ushort target, ushort lun)
{
	register int i;

	transfer_length = SENSE_BYTES;
	transfer_address = longaddr((ushort)sense_data, myds());

	CMDLEN = 6;
	CMDBYTE[0] = SC_RSENSE;
	CMDBYTE[1] = lun << 5;
	CMDBYTE[2] = 0;
	CMDBYTE[3] = 0;
	CMDBYTE[4] = SENSE_BYTES;
	CMDBYTE[5] = 0;

	for (i = 0; ; i++) {
		if (run_cmd(base_port, target, NRQ_NORMAL_CMD) == 0)
			break;
		if (i >= 2) {
			return (1);
		}
	}
	return (0);
}

STATIC void
bzero(unchar *p, ushort n)
{
	for (; n > 0; n--)
	*p++ = 0;
}

/*
 * This routine is called by the generic SCSI code after relocation to
 * allow the driver to recalculate any addresses that changed.
 *
 * Some drivers also call it from dev_find or from routines it calls.
 */
init_dev(ushort base_port, ushort dataseg)
{
	ncr_script_init(dataseg);
}

/*
 * dev_inquire() -- return 0 for success,
 *			   1 for unspecified failure,
 *			   2 for selection timeout failure
 */
dev_inquire(ushort base_port, ushort target, ushort lun)
{
	extern struct inquiry_data inqd;
	int ret;

#ifdef DEBUG
	if (debuglevel & DEV_INQ) {
		putstr(ident);
		putstr(" dev_inquire() target ");
		puthex(target);
		putstr(" lun ");
		puthex(lun);
		putstr("\r\n");
	}
#endif

	/*
	 * Pre-initialize buffer to contain "no lun" byte.
	 * Some devices respond in non-standard ways to SC_INQUIRY
	 * on unsupported luns.  This way we tend to reject such
	 * phantom devices rather than accept them.
	 */
	bzero((unchar *)&inqd, sizeof(struct inquiry_data));
	inqd.inqd_pdt = INQD_PDT_NOLUN;

	transfer_length = sizeof (struct inquiry_data);
	transfer_address = longaddr((ushort)&inqd, myds());

	CMDLEN = 6;
	CMDBYTE[0] = SC_INQUIRY;
	CMDBYTE[1] = lun << 5;
	CMDBYTE[2] = 0;
	CMDBYTE[3] = 0;
	CMDBYTE[4] = sizeof (struct inquiry_data);
	CMDBYTE[5] = 0;

	if ((ret = run_cmd(base_port, target, NRQ_NORMAL_CMD)) != 0) {
		if (ret == 2) {
			return (2);
		}
		if (run_cmd(base_port, target, NRQ_NORMAL_CMD)) {
			return (1);
		}
	}
	if (nptp->nt_statbuf[0] != S_GOOD || inqd.inqd_pdt == INQD_PDT_NOLUN) {
		return (1);
	}
	return (0);
}


STATIC int
read_capacity(ushort base_port, ushort target, ushort lun)
{
	extern struct readcap_data readcap_data;

#ifdef DEBUG
	if (debuglevel & DEV_RCAP) {
		putstr(ident);
		putstr(" read_capacity(): target ");
		put1hex(target);
		putstr("\r\n");
	}
#endif

	bzero((unchar *)&readcap_data, sizeof (struct readcap_data));
	transfer_length = sizeof (struct readcap_data);
	transfer_address = longaddr((ushort)&readcap_data, myds());

	CMDLEN = 10;
	CMDBYTE[0] = SX_READCAP;
	CMDBYTE[1] = lun << 5;
	CMDBYTE[2] = 0;
	CMDBYTE[3] = 0;
	CMDBYTE[4] = 0;
	CMDBYTE[5] = 0;
	CMDBYTE[6] = 0;
	CMDBYTE[7] = 0;
	CMDBYTE[8] = 0;
	CMDBYTE[9] = 0;

	if (run_cmd(base_port, target, NRQ_NORMAL_CMD)) {
#ifdef DEBUG
		putstr(ident);
		putstr(" read_capacity(): failed \r\n");
#endif
		return (1);
	}

#ifdef DEBUG
	if (debuglevel & DEV_RCAP) {
		putstr(ident);
		putstr(" read_capacity()\r\n");
		putstr("READCAP: # of blocks ");
		put2hex(readcap_data.rdcd_lba[0]);
		put2hex(readcap_data.rdcd_lba[1]);
		put2hex(readcap_data.rdcd_lba[2]);
		put2hex(readcap_data.rdcd_lba[3]);
		putstr(" block length ");
		put2hex(readcap_data.rdcd_bl[0]);
		put2hex(readcap_data.rdcd_bl[1]);
		put2hex(readcap_data.rdcd_bl[2]);
		put2hex(readcap_data.rdcd_bl[3]);
		putstr(" target ");
		puthex(target);
		putstr("\r\n");
	}
#endif
	return (0);
}

/*
 * dev_readcap -- entry point to read capacity
 */
dev_readcap(DEV_INFO *info)
{
	int i, ret;
	ushort base_port, target, lun;

	base_port = info->base_port;
	target = info->MDBdev.scsi.targ;
	lun = info->MDBdev.scsi.lun;

	for (i = 0; i < 30; i++) {
		if (read_capacity(base_port, target, lun)) {
#ifdef DEBUG
			putstr(ident);
			putstr(" dev_readcap(): failed \r\n");
#endif
			return (1);
		}

		switch (nptp->nt_statbuf[0]) {
		case S_GOOD:
			return (0);
		case S_CK_COND:
			/*
			 * Some devices need longer delay before readcap
			 * can be executed.  Retry again, if the device 
			 * is not ready when CHECK CONDITION status with
			 * sense key NOT READY is return.
			 */
			if (ret = dev_sense(base_port, target, lun))
				return (ret);
			if ((sense_data[2] & 0xf) == SENSE_NOT_READY) {
				milliseconds(500);
			}
			break;
		default:
			return (1);
		}
	}
	return (1);
}

/*
 * init_board() -- initialize the board
 */
STATIC void
init_board(int boardnum)
{
	int	target;
	int	lun;
	int	ha_id = -1;
	int	ret;
	ushort	ntargets = 16;

	init_dev(adapter_info[boardnum].devid, myds());

	/*
	 * Device-specific initialization goes here.
	 */
	adapter_info[boardnum].ncr.n_ioaddr = adapter_info[boardnum].ioaddr;
	adapter_info[boardnum].ncr.n_initiatorid = 7;
	adapter_info[boardnum].ncr.n_idmask =
		1 << adapter_info[boardnum].ncr.n_initiatorid;
	adapter_info[boardnum].ncr.n_state = NSTATE_IDLE;
	adapter_info[boardnum].ncr.n_syncstate = NSYNC_SDTR_REJECT;
	ha_id = adapter_info[boardnum].ncr.n_initiatorid;
	NCR_INIT(&adapter_info[boardnum].ncr);

  	switch (adapter_info[boardnum].pci_vdev_id) {
  		case NCR_53c825:
  		case NCR_53c875:
  		case NCR_53c875_95:
  		case NCR_53c895:
  			ntargets = NTARGETS_WIDE;
  			break;
  		default:
  			ntargets = NTARGETS;
  	}

	/*
	 * Determine adapter target ID and store in ha_id.
	 */

	for (target = 0;  target < ntargets;  target++)  {
		if (target == ha_id)  {		/* Skip controller's SCSI ID */
			continue;
		}
		if (dev_reset(adapter_info[boardnum].devid, target, 0))
			continue;

		for (lun = 0; lun < 8; lun++) {   /* Check luns on target */
			ret = dev_probe(adapter_info[boardnum].devid,
			    target, lun);
			/*
			 * If lun 0 fails with a selection timeout, there
			 * can be no luns on this target.  Stop now.
			 */
			if (ret == 2 && lun == 0) {
#ifdef DEBUG
				if (debuglevel & INITB) {
					putstr("init_board(): target:");
					puthex(target);
					putstr(" lun:");
					puthex(lun);
					putstr(" selection timeout\r\n");
				}
#endif /* DEBUG */
				break;
			}

			/*
			 * Any other failure means that this lun is not
			 * present.
			 */
			if (ret)
				continue;

			/*
			 * Call the generic or pci SCSI code to register
			 * this device.
			 */
			if (adapter_info[boardnum].pci) {
#ifdef	LATER
				/*
				 * HACK ALERT - make this realmode driver load
				 * a different Solaris driver if it's
				 * a PCI device.  Don't use strcpy()
				 * because it's not in the lib.  Argh.
				 */
				char *src, *dst;
				src = "SYMBIOS";
				dst = ident;
				do {
					*dst++ = *src++;
				} while (*src != '\0');
#endif

				scsi_dev_pci(adapter_info[boardnum].devid,
				    target,
				    lun,
				    adapter_info[boardnum].pci_bus,
				    adapter_info[boardnum].pci_ven_id,
				    adapter_info[boardnum].pci_vdev_id,
				    adapter_info[boardnum].pci_dev,
				    adapter_info[boardnum].pci_func);
			} else {
				scsi_dev(adapter_info[boardnum].devid,
				    target,
				    lun);
			}

#ifdef DEBUG
			if (debuglevel & INITB) {
				putstr("init_board(): target:");
				puthex(target);
				putstr(" lun:");
				puthex(lun);
				putstr(" found << PRESS ANY KEY >> \r\n");
				kbchar();
			}
#endif

		}   /* For LUN */
	}	/* For ID  */
}

int
legacyprobe()
{
	/* ---- no legacy cards ---- */
}

int
installonly()
{
	DWORD	val[2], len;
	int	Rtn = BEF_FAIL, b, i;
	DWORD id;
	ushort vdev; /* Vendors device id */

	/* ---- Arrange for DMA-visible items to be 4-byte aligned. ---- */
	nptp = (npt_t *)(((int)npt_arena + 3) & ~3);

	/*
	 * This simple change avoids the need for a seperate scsi.c
	 * which was also always getting out of sync with the parent
	 */
	init_dev_before = 1;

	do {
		if (node_op(NODE_START) != NODE_OK)
			return (Rtn);

		len = 2;
		if (get_res("name", val, &len) != RES_OK) {
			node_op(NODE_FREE);
			return (BEF_FAIL);
		}
		switch (val[1]) { /* bustype */

		case RES_BUS_PCI:
			len = 1;
			vdev = val[0] & 0xffff;
			if (get_res("addr", &id, &len) != RES_OK) {
				node_op(NODE_FREE);
				return (BEF_FAIL);
			}
			/*
			printf("VVVVDDDD 0x%lx, id 0x%x\n",
				 val[0], (ushort) id);
			*/
			if ((b = pci_dev_found((ushort) id, vdev)) != -1) {
				Rtn = BEF_OK;
				init_board(b);
			}
			break;
		case RES_BUS_EISA:
			len = 1;
			if (get_res("slot", val, &len) != RES_OK) {
				node_op(NODE_FREE);
				return (BEF_FAIL);
			}
			if ((b = probe_slot((ushort)val[0])) != -1) {
				Rtn = BEF_OK;
				init_board(b);
			}
			break;
		default:
			/* * Invalid bustype */
			node_op(NODE_FREE);
			return (BEF_FAIL);
			
		}
	} while (1);
}

/*
 * dev_find() -- Finds all adapters of this type, looks for devices
 *		 and returns the number of adapters found.
 */
int
dev_find()
{
	int boardnum;

#ifdef DEBUG
	{	ushort level;

		putstr("Compiled debug level is ");
		puthex(debuglevel);
		putstr("\r\nEnter a new level in hex or type <RETURN>: ");
		if (gethex(&level)) {
			debuglevel = level;
		}
		putstr("\r\n");
	}
#endif /* DEBUG */

	/*
	 * Arrange for DMA-visible items to be 4-byte aligned.
	 */
	nptp = (npt_t *)(((int)npt_arena + 3) & ~3);

	/*
	 * Find any adapters of each type and enters their base
	 * addresses in adapter_info.
	 */
	find_pci_ctlrs();
	find_eisa_ctlrs();
	find_isa_ctlrs();

	if (num_adapters == 0)
		return (0);
	for (boardnum = 0; boardnum < num_adapters; boardnum++)  {
#ifdef DEBUG
		if (debuglevel & DEV_FIND) {
			putstr(ident);
			putstr(" dev_find(): adapter found with base address ");
			puthex(adapter_info[boardnum].ioaddr);
			putstr("\r\n");
		}
#endif
		init_board(boardnum);
	}
#ifdef DEBUG
	if (debuglevel & DEV_FIND) {
		putstr(ident);
		putstr(" dev_find(): returning ");
		puthex(num_adapters);
		putstr("\r\n");
		putstr("<< PRESS ANY KEY >>\r\n");
		kbchar();
	}
#endif
	return (num_adapters);
}


/*
 * find_pci_ctlrs() -- finds which pci slots have this adapter
 */
STATIC void
find_pci_ctlrs()
{
/* NCR 53c8xx device IDs */
#define	N810_VID	0x1000		/* NCR810 Vendor ID */
#define	N810_DID	0x0001		/* NCR810 Device ID */
#define	N820_DID	0x0002		/* NCR820 Device ID */
#define	N825_DID	0x0003		/* NCR825 Device ID */
#define	N815_DID	0x0004		/* NCR815 Device ID */
#define	PCI_SCRATCHB	(0x5C+0x80)	/* register 5C; +0x80 for cfg space */

	int num;
	int dev;
	ushort id;
	static ushort devtab[] = { N810_DID, N820_DID, N825_DID, N815_DID };
	static int devtab_items = sizeof devtab / sizeof (ushort);
	extern nops_t ncr53c810_nops;
	static int is_compaq = 0;
	static int slot0id;


	if (!is_pci()) {
#ifdef DEBUG
		if (debuglevel & DEV_FIND) {
			putstr(ident);
			putstr(" find_pci_ctlrs(): Not a PCI machine\r\n");
		}
#endif
		return;
	}

 	_asm {
 		mov cl, 0		/* slot 0 */
 		mov ax, 0xD800		/* function D800 - get slot info */
 		int 15h			/* call BIOS */
 		jc	not_compaq	/* if error */
 		mov	word ptr [slot0id], di
 		jmp	compaq_exit
not_compaq:
 		mov	word ptr [slot0id], 0xFFFF 
compaq_exit:
 	}

 	if (slot0id == 0x110e) {
 		is_compaq = 1;
 	}


	for (dev = 0; dev < devtab_items; dev++) {
		for (num = 0; ; num++) {
			if (!pci_find_device(N810_VID, devtab[dev], num, &id)) {
				break;
			}
			/* 
			 * special checks for Compaq: if an 825, and a Compaq
			 * machine, and SCRATCHB sums to 0x5A, then refuse
			 * this device
			 */
			
			if (devtab[dev] == N825_DID && is_compaq) {
				unsigned long scratchb;
				unsigned char *p;
				unsigned char sum;
				int i;
		
				if (!pci_read_config_dword(id, PCI_SCRATCHB,
					&scratchb))
					continue;
				sum = 0; 
				p = (unsigned char *)&scratchb;
				for (i = 0; i < 4; i++) {
					sum += *p++;
				}
				if (sum == 0x5A) {
					/* skip this device */
					continue;
				}
			}
		
			(void) pci_dev_found(id, devtab[dev]);
		}
	}
}

STATIC int
pci_dev_found(ushort id, ushort vdevid)
{
/* PCI configuration space offsets */
#define	PCI_CMDREG	0x04
#define	PCI_BASEAD	0x10

/* Bits in PCI_CMDREG */
#define PCI_CMD_IOEN	1

	ushort cmd_reg;
	ulong base_addr;
	ushort address;
	extern nops_t ncr53c810_nops;

	if (!pci_read_config_word(id, PCI_CMDREG, &cmd_reg))
		return (-1);

	/*
	 * Assume device is disabled if I/O space is
	 * disabled.
	 */
	if ((cmd_reg & PCI_CMD_IOEN) == 0) {
		return (-1);
	}

	if (!pci_read_config_dword(id, PCI_BASEAD, &base_addr))
		return (-1);
	address = (ushort) base_addr & ~1;
#ifdef DEBUG
	if (debuglevel & DEV_FIND) {
		putstr(ident);
		putstr(" pci_dev_found(): found device ");
		puthex(vdevid);
		putstr(" at base address ");
		puthex(address);
		putstr("\r\n");
		putstr("<< PRESS ANY KEY >>");
		kbchar();
		putstr("\r\n");
	}
#endif
	if (num_adapters < MAXADAPTERS) {
		adapter_info[num_adapters].ioaddr = address;
		adapter_info[num_adapters].devid = id;
		adapter_info[num_adapters].ncr.n_ops = &ncr53c810_nops;
		adapter_info[num_adapters].pci = 1;
		adapter_info[num_adapters].pci_ven_id =  N810_VID;
		adapter_info[num_adapters].pci_vdev_id = vdevid;
		adapter_info[num_adapters].pci_bus = (id >> 8);
		adapter_info[num_adapters].pci_dev = ((id >> 3) & 0x1f);
		adapter_info[num_adapters].pci_func = (id & 0x7);

		return (num_adapters++);
	}
}


/*
 * find_eisa_ctlrs() -- finds which eisa slots have this adapter
 */
STATIC void
find_eisa_ctlrs()
{
	ushort slotnum;

	if (!is_eisa()) {
#ifdef DEBUG
		if (debuglevel & DEV_FIND) {
			putstr(ident);
			putstr(" find_eisa_ctlrs(): Not an EISA machine\r\n");
		}
#endif
		return;
	}


	/* scan thru all EISA slots for valid adapter signature */
	for (slotnum = 0; slotnum <= 0xF; slotnum++) {
		probe_slot(slotnum);
	}
}

int
probe_slot(ushort slotnum)
{
	ushort index;
	union {
		unchar c[4];
		ulong l;
	} u;
	register ushort address;
	extern nops_t ncr53c710_nops;

	milliseconds(1);

	address = (slotnum << 12) + 0xC80;

	/* Slot non-empty if high bit of first ID byte not set */
	outb(address, 0xFF);		/* precharge the register */
	u.c[0] = inb(address);
	if (u.c[0] & 0x80) {
#ifdef DEBUG
		if (debuglevel & DEV_FIND) {
			putstr(ident);
			putstr(" find_eisa_ctlrs(): slot ");
			put1hex(slotnum);
			putstr(": no ID\r\n");
			putstr("<< PRESS ANY KEY >>");
			kbchar();
			putstr("\r\n");
		}
#endif
		return (-1);
	}

	/* Read remainder of product ID */
	u.c[1] = inb(address + 1);
	u.c[2] = inb(address + 2);
	u.c[3] = inb(address + 3);

#ifdef DEBUG
	if (debuglevel & DEV_FIND) {
		putstr(ident);
		putstr(" find_eisa_ctlrs(): slot ");
		put1hex(slotnum);
		putstr(" ID ");
		puthex(u.l >> 16);
		puthex(u.l);
		putstr("\r\n");
	}
#endif

	/*
	 * Search the table for an exact match with a known board
	 * ID including the revision number.
	 */
	for (index = 0; index < known_eisa_boards; index++) {
		if (u.l == eisa_board_table[index].id) {
#ifdef DEBUG
			if (debuglevel & DEV_FIND) {
				putstr(ident);
				putstr(" find_eisa_ctlrs(): slot ");
				put1hex(slotnum);
				putstr(" exact match\r\n");
				putstr("<< PRESS ANY KEY >>");
				kbchar();
				putstr("\r\n");
			}
#endif
			break;
		}
	}

	/*
	 * If an exact match was not found, try again ignoring
	 * the revision number.
	 */
#define	REV_MASK	0xF0FFFFFF
	if (index == known_eisa_boards) {
		for (index = 0; index < known_eisa_boards; index++) {
			if ((u.l & REV_MASK) ==
			    (eisa_board_table[index].id & REV_MASK)) {
#ifdef DEBUG
				if (debuglevel & DEV_FIND) {
					putstr(ident);
					putstr(" find_eisa_ctlrs():");
					putstr(" slot ");
					put1hex(slotnum);
					putstr(" ID match\r\n");
					putstr("<< PRESS ANY KEY >>");
					kbchar();
					putstr("\r\n");
				}
#endif
				break;
			}
		}
		if (index == known_eisa_boards) {
#ifdef DEBUG
			if (debuglevel & DEV_FIND) {
				putstr(ident);
				putstr(" find_eisa_ctlrs(): slot ");
				put1hex(slotnum);
				putstr(" ID not known\r\n");
				putstr("<< PRESS ANY KEY >>");
				kbchar();
				putstr("\r\n");
			}
#endif
			
			return (-1);
		}
	}
	
	if (eisa_board_table[index].flags & EBIFL_SIEMENS) {
		putstr(ident);
		putstr(": find_eisa_ctlrs: ");
		putstr("Siemens motherboard in slot ");
		put1hex(slotnum);
		putstr("\r\n");
		if (slotnum == 0)
			check_siemens_motherboard();
		return (-1);
	}

	/* We ignore slot 0 other than for Siemens motherboards */
	if (slotnum == 0)
		return (-1);

	address = (slotnum << 12) + eisa_board_table[index].offset;

	if (num_adapters < MAXADAPTERS) {
		adapter_info[num_adapters].ioaddr = address;
		adapter_info[num_adapters].devid = (slotnum << 12);
		adapter_info[num_adapters].ncr.n_ops =
			&ncr53c710_nops;
		return (num_adapters++);
	}
	else
	  return (-1);
}

STATIC void
check_siemens_motherboard()
{
	/*
	 * The Siemens motherboard contains two NCR710 chips
	 * which can be enabled or disabled with the state
	 * stored in CMOS.
	 */
	switch (GET_CMOS_BYTE(0x29) & SNI_CMOS_ENABLE_MASK) {
	case SNI_CMOS_SCSI_DISABLED:
#ifdef DEBUG
	    putstr(ident);
	    putstr(": find_eisa_ctlrs: SCSI disabled\r\n");
#endif
	    break;
	case SNI_CMOS_SCSI_1_ONLY:
#ifdef DEBUG
	    putstr(ident);
	    putstr(": find_eisa_ctlrs: SCSI 1 enabled\r\n");
#endif
	    add_siemens_ncr710(NCR53C710_SNI_CTL1_ADDR);
	    break;
	case SNI_CMOS_SCSI_1_2:
#ifdef DEBUG
	    putstr(ident);
	    putstr(": find_eisa_ctlrs: SCSI 1, 2 enabled\r\n");
#endif
	    add_siemens_ncr710(NCR53C710_SNI_CTL1_ADDR);
	    add_siemens_ncr710(NCR53C710_SNI_CTL2_ADDR);
	    break;
	case SNI_CMOS_SCSI_2_1:
#ifdef DEBUG
	    putstr(ident);
	    putstr(": find_eisa_ctlrs: SCSI 2, 1 enabled\r\n");
#endif
	    add_siemens_ncr710(NCR53C710_SNI_CTL2_ADDR);
	    add_siemens_ncr710(NCR53C710_SNI_CTL1_ADDR);
	    break;
	}
}

STATIC void
add_siemens_ncr710(ushort ioaddr)
{
	extern nops_t ncr53c710_nops;

	if (num_adapters < MAXADAPTERS) {
		adapter_info[num_adapters].ioaddr = ioaddr;
		adapter_info[num_adapters].devid = ioaddr;
		adapter_info[num_adapters].ncr.n_ops = &ncr53c710_nops;
		num_adapters++;
	}
}

/*
 * find_isa_ctlrs() -- finds any versions of this adapter
 */
STATIC void
find_isa_ctlrs()
{
	/* This driver does not support any ISA adapters */
}

/*
 * dev_motor -- start/stop motor
 */
int
dev_motor(DEV_INFO *info, int on)
{
	register int i;

#ifdef DEBUG
	if (debuglevel & DEV_MOTOR) {
		putstr(ident);
		putstr(" dev_motor()\r\n");
	}
#endif

	transfer_length = 0;

	CMDLEN = 6;
	CMDBYTE[0] = SC_STRT_STOP;
	CMDBYTE[1] = info->MDBdev.scsi.lun << 5;
	CMDBYTE[2] = 0;
	CMDBYTE[3] = 0;
	CMDBYTE[4] = on;
	CMDBYTE[5] = 0;

	if (run_cmd(info->base_port, info->MDBdev.scsi.targ, NRQ_NORMAL_CMD) ||
 		nptp->nt_statbuf[0] != S_GOOD)  {
#ifdef DEBUG
		if (debuglevel & DEV_MOTOR) {
			putstr(ident);
			putstr(" dev_motor() failed\r\n");
		}
#endif
		milliseconds(100);
		return (1);
	}

#ifdef DEBUG
	if (debuglevel & DEV_MOTOR) {
		putstr(ident);
		putstr("dev_motor() succeed\r\n");
	}
#endif
	return (0);
}


/*
 * dev_lock -- lock / unlock medium
 */
int
dev_lock(DEV_INFO *info, int lock)
{
	register int i;

#ifdef DEBUG
	if (debuglevel & DEV_LOCK) {
		putstr(ident);
		putstr(" dev_lock()\r\n");
	}
#endif

	transfer_length = 0;

	CMDLEN = 6;
	CMDBYTE[0] = SC_REMOV;
	CMDBYTE[1] = info->MDBdev.scsi.lun << 5;
	CMDBYTE[2] = 0;
	CMDBYTE[3] = 0;
	CMDBYTE[4] = lock;
	CMDBYTE[5] = 0;

	if (run_cmd(info->base_port, info->MDBdev.scsi.targ, NRQ_NORMAL_CMD) ||
		nptp->nt_statbuf[0] != S_GOOD)  {
#ifdef DEBUG
		if (debuglevel & DEV_LOCK) {
			putstr(ident);
			putstr(" dev_lock(): failed \r\n");
		}
#endif
		return (1);
	}

#ifdef DEBUG
	if (debuglevel & DEV_LOCK) {
		putstr("dev_lock(): succeed \r\n");
	}
#endif
	return (0);
}

STATIC int
dev_reset(ushort base_port, ushort target, ushort lun)
{
	int ret;
#ifdef DEBUG
	if (debuglevel & DEV_RESET) {
		putstr(ident);
		putstr(" dev_reset()\r\n");
	}
#endif

	transfer_length = 0;
	transfer_address = 0L;

	CMDLEN = 6;
	CMDBYTE[0] = SC_NOOP;
	CMDBYTE[1] = lun << 5;
	CMDBYTE[4] = 0;

	if (ret = run_cmd(base_port, target, NRQ_DEV_RESET))  {
#ifdef DEBUG
		if (debuglevel & DEV_RESET) {
			putstr(ident);
			putstr(" dev_reset(): failed \r\n");
		}
#endif
		return (ret);
	}
	milliseconds(250);
	return (0);
}

STATIC int
dev_probe(ushort base_port, ushort target, ushort lun)
{
	int ret, i;
	unchar	status;
	bool_t	not_ready;

	for (i = 0, not_ready = 1; i < 15 && not_ready; i++) {
		status = 0;
		ret =  dev_inquire(base_port, target, lun);
		status = nptp->nt_statbuf[0];
		if (ret && status != S_BUSY)
			return (ret);
		switch (status) {
		case S_BUSY:
			milliseconds(250);
			break;
		case S_CK_COND:
			if (ret = dev_sense(base_port, target, lun))
				return (ret);
			if ((sense_data[2] & 0xf) == SENSE_UNIT_ATTENTION)
				break;
		case S_GOOD:
			not_ready = 0;
		}
	};
	return (not_ready);
}
