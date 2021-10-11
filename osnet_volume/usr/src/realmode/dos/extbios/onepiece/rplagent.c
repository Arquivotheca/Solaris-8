/*
 * Copyright (c) 1997 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)rplagent.c	1.36	97/04/30 SMI\n"

/*
 * RPL protocol agent for network install/boot
 *
 * To be used with the network driver services core.
 */

typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char unchar;
typedef long daddr_t;

#define	_BIOSERV_H

#include <types.h>
#include <bef.h>
#include <dev_info.h>
#include <bootp2s.h>
#include <common.h>
#include "rplagent.h"
#include <befext.h>
#include <stdio.h>
#include <string.h>

#define	STACKSIZE	2000	/* resident stack size in words */
#define	RXBUFSIZE	1540

#ifdef MSC60
#define	ASM							_asm
#define	MSC							1
#endif
#ifdef MSC70
#define	ASM							__asm
#define	MSC							1
#endif
#ifdef TURBOC
#define	ASM							asm
#define	TC							1
#pragma inline
#endif
#ifdef BORLANDC
#define	ASM							asm
#define	TC							1
#pragma inline
#endif

#define	INIT_CALL					0
#define	OPEN_CALL					1
#define	CLOSE_CALL					2
#define	SEND_CALL					3
#define	RECEIVE_CALL				4
#define	GET_ADDR_CALL				5
#define	IDENTIFY_CALL				6
#define	GET_DRVNAME_CALL			7
#define	MAX_SERVICE					7

#define	FINDING						0
#define	DATA_XFER					1
#define	RESET						0
#define	AGAIN						1
#define	MAX_TIMEOUT					150
#define	MAGIC_NUMBER				0x8465

/* Following is the interrupt number that inetboot talks to this driver */
#define	SVR_INT_NUM					0xFB
#define	GLUEBOOT_INT_NUM			0x21

/*
 * Begin Prototype Area
 */
static void	DriverIdentify(short);
static int	send_SENDFILE_frame(long);
void far	intr_vec(void);
void		build_FIND_frame(void);
void far	transfer_execution(void);
void far	receive_callback(ushort);
int		timeout(int, ushort);
void far	glueboot(void);
void far	bios_intercept(void);
int		DriverInit(ushort, ushort, ushort, ushort);
/*
 * End Prototype Area
 */

/* external globals defined in the driver source file */
extern char	driver_name[];
extern unchar	MAC_Addr[];
extern ushort	Inetboot_IO_Addr;
extern ushort	Inetboot_Mem_Base;
extern int	DebugFlag;

/* Global variables */
/*
 * current_ variables represent the card we've chosen to boot from,
 * either through being called at dev_netboot with a specific
 * set, or from DriverIdentify identifying the one with a boot PROM
 * (exactly one allowed).  They are set from DriverInit(), which
 * is only called for the selected instance of the card.
 */
static ushort	current_ramsize;	/* RAM size in kbytes */
static ushort	current_rambase;	/* RAM base in paragraphs */
static ushort	current_IRQ;		/* IRQ */
static ushort	current_portbase;	/* I/O port base */
static short	old_driver_interface;	/* loaded by MDB */
ushort		stack[STACKSIZE];
ushort		stacksize = STACKSIZE * sizeof (ushort);
void		(_far *xfer_addr)();
int		xfer_seg;
int		xfer_off;
int		server_not_found = 1;
int		last_frame;
ulong		expectedRxSeq;
int		state = FINDING;
unchar		serverSAP = 0xFC;
int		indicator;
unchar		serverAddr[6];
ushort		packetCount;		/* # of received packets to be read */

#define	NUMRXBUFS	4
ushort		bufferWriteIndex;		/* with these */
ushort		bufferReadIndex;		/* 2 read and write pointers */
static unchar	receiveBuffer0[RXBUFSIZE];	/* and these buffers, */
static unchar	receiveBuffer1[RXBUFSIZE];	/* implement ring buffering */
static unchar	receiveBuffer2[RXBUFSIZE];
static unchar	receiveBuffer3[RXBUFSIZE];
unchar		*receiveBufPtrs[NUMRXBUFS] = {	/* with this array of ptrs   */
			receiveBuffer0,
			receiveBuffer1,
			receiveBuffer2,
			receiveBuffer3,
		};
ushort		receiveLengths[NUMRXBUFS];
ushort		Traffic_State;

uint		bufferLen = RXBUFSIZE;
int		Rx_Timeout;
char		Rx_Flag;
int		Network_Traffic = RPL_TRAFFIC;
int		Media_Type = MEDIA_ETHERNET;
unchar		TokenRingMultiAddr[] = { 0xc0, 0x00, 0x40, 0x00, 0x00, 0x00 };
unchar		FINDframeBuf[] = {
	0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* our own address */
	0x00, 0x56,
	0xFC, 0xFC, 0x03,
	0x00, 0x53, 0x00, 0x01, 0x00, 0x08, 0x40,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	0x08, 0x00, 0x06, 0x40, 0x09, 0x08, 0x00, 0x00,
	0x06, 0x40, 0x0A, 0x00, 0x01, 0x00, 0x0A, 0x40,
	0x06,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* our own address */
	0x00,
	0x05, 0x40, 0x07, 0xFC, 0x00, 0x28, 0x00, 0x04,
	0x00, 0x24, 0xC0, 0x05, 0xFC, 0x01, 0x00, 0x74,
	0x00, 0x00, 0x00, 0x00, 0x44, 0x63, 0x02, 0x66
};

unchar		SENDFILEframeBuf[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0: server address */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 6: our own adress */
	0x00, 0x56,				/* 12: packet length */
	0x00, 0x00, 0x03,			/* 14: DSAP, SSAP */
	0x00, 0x53,				/* 17: program len. */
	0x00, 0x10,				/* 19: SEND.FILE cmd */
	0x00, 0x08, 0x40, 0x11,			/* 21: */
	0x00, 0x00, 0x00, 0x00,			/* 25: sequence number */
	0x00, 0x10, 0x00, 0x08,			/* 29: */
	0x00, 0x06, 0x40, 0x09,			/* 33: */
	0x02, 0x00,				/* 37: max frame */
	0x00, 0x06, 0x40, 0x0A,			/* 39: */
	0x00, 0x00,				/* 43: connection class */
	0x00, 0x0A, 0x40, 0x06,			/* 45: */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 49: our own address */
	0x00, 0x05, 0x40, 0x07,			/* 55: */
	0x00,					/* 59: server SAP */
	0x00, 0x28, 0x00, 0x00,			/* 60: */
	0x00, 0x24, 0xC0, 0x05,			/* 64: */
	0, 0, 0, 0, 0, 0, 0, 0,			/* 68: from INT 15 */
	0x00, 0x00,				/* 76: from INT 11 */
	0x00, 0x00,				/* 78: from INT 12 */
	0, 0, 0, 0, 0, 0, 0, 0,			/* 80: RPL version */
	0x00, 0x00,				/* 88: adapter ID */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 90: adapter version */
	0x00, 0x00, 0x00, 0x00			/* 100: file header */
};
struct pri_to_secboot Boot_Link;

void
dev_netboot(short index, ushort base_port, ushort irq_level, ushort rambase,
	    ushort ramsize, unchar boot_dev, struct bdev_info *info)
{
	int		i;
	void far	*israddr;

	old_driver_interface = 1;	/* loaded by MDB */
	printf("\nBooting from network: I/O Address 0x%x, IRQ %d,", base_port,
		irq_level);
	printf(" Memory Base 0x%x0, Size %dK\n", rambase, ramsize);

	/*
	 * Prepare the pri-to-secboot structure
	 */
	Boot_Link.F8 = *info;
	Boot_Link.F8.hba_id[0] = driver_name[0];
	Boot_Link.F8.hba_id[1] = driver_name[1];
	Boot_Link.F8.hba_id[2] = driver_name[2];
	Boot_Link.F8.hba_id[3] = driver_name[3];
	Boot_Link.F8.hba_id[4] = driver_name[4];
	Boot_Link.F8.hba_id[5] = driver_name[5];
	Boot_Link.F8.hba_id[6] = driver_name[6];
	Boot_Link.F8.hba_id[7] = driver_name[7];
	Boot_Link.bootfrom.ufs.boot_dev = boot_dev;
	Boot_Link.bootfrom.nfs.irq = irq_level;
	Boot_Link.bootfrom.nfs.ioaddr = Inetboot_IO_Addr;
	Boot_Link.bootfrom.nfs.membase = Inetboot_Mem_Base;
	Boot_Link.bootfrom.nfs.memsize = ramsize;

	/* set up the service linkage with inetboot */
	israddr = MK_FP(mycs(), FP_OFF(bios_intercept));
	setvector(SVR_INT_NUM, israddr);

	/* set up the boot linkage with gluecode */
	israddr = MK_FP(mycs(), FP_OFF(glueboot));
	setvector(GLUEBOOT_INT_NUM, israddr);

	/*
	 * Initialize the hardware.  This will be used for RPL and then
	 * after inetboot is downloaded, used by inetboot through the
	 * serviceEntry.
	 */
	if (DriverInit(base_port, irq_level, rambase, ramsize) < 0)
		return;

	printf("Node Address");
	for (i = 0; i < 6; i++) {
		printf(":%2.2x", MAC_Addr[i]);
	}
	printf("\n");

	/* Adjust for Token Ring bit-reversal in destination address */
	if (Media_Type == MEDIA_IEEE8025) {
		int	j;

		for (j = 0; j < 6; j++) FINDframeBuf[j] = TokenRingMultiAddr[j];
	}

	/* Start the RPL protocol */
	state = FINDING;
	timeout(RESET, (ushort)MAX_TIMEOUT);
	AdapterOpen();
	listen_packet();
	send_FIND_frame();
	while (server_not_found) {
		if (packetCount) {
			process_frame();
			/*
			 * here, if FOUND frame is being processed
			 * server_not_found will be set to FALSE
			 * and while loop will exit
			 */
			if (!server_not_found)
				break;
		}
		if (timeout(AGAIN, 0)) {
			putstr("Still trying to find a RPL boot server ...\n");
			timeout(RESET, (ushort)MAX_TIMEOUT);
			send_FIND_frame();
		}
	}

	last_frame = 0;
	state = DATA_XFER;
	timeout(RESET, (ushort)MAX_TIMEOUT);
	send_SENDFILE_frame((long)0);
	while (!last_frame) {
		if (packetCount) {
			process_frame();	/* will set last_frame */
			if (last_frame)
				break;
		}
		if (timeout(AGAIN, 0)) {
			putstr("Request to send file again ...\r\n");
			timeout(RESET, (ushort)MAX_TIMEOUT);
			send_SENDFILE_frame(expectedRxSeq);
		}
	}
	transfer_execution();
	/* will not return */
}

dev_init(DEV_INFO *info)
{
	ushort base_port, irq_level, rambase, ramsize, boot_dev;
	int	i;
	void __far *israddr;

	base_port = info->base_port;
	irq_level = info->MDBdev.net.irq_level;
	rambase = info->MDBdev.net.mem_base;
	ramsize = info->MDBdev.net.mem_size;
	boot_dev = info->bios_dev;

	/*
	 * Prepare the pri-to-secboot structure
	 */
	Boot_Link.F8.dev_type = MDB_NET_CARD;
	Boot_Link.F8.hba_id[0] = driver_name[0];
	Boot_Link.F8.hba_id[1] = driver_name[1];
	Boot_Link.F8.hba_id[2] = driver_name[2];
	Boot_Link.F8.hba_id[3] = driver_name[3];
	Boot_Link.F8.hba_id[4] = driver_name[4];
	Boot_Link.F8.hba_id[5] = driver_name[5];
	Boot_Link.F8.hba_id[6] = driver_name[6];
	Boot_Link.F8.hba_id[7] = driver_name[7];
	Boot_Link.bootfrom.ufs.boot_dev = boot_dev;
	Boot_Link.bootfrom.nfs.irq = irq_level;
	Boot_Link.bootfrom.nfs.ioaddr = Inetboot_IO_Addr;
	Boot_Link.bootfrom.nfs.membase = Inetboot_Mem_Base;
	Boot_Link.bootfrom.nfs.memsize = ramsize;

	/* set up the service linkage with inetboot */
	israddr = MK_FP(mycs(), FP_OFF(bios_intercept));
	setvector(SVR_INT_NUM, israddr);

	/*
	 * Initialize the hardware.  Further access available via
	 * the serviceEntry.
	 */
	if (DriverInit(base_port, irq_level, rambase, ramsize) < 0)
		return (1);

	printf("Node Address");
	for (i = 0; i < 6; i++)
		printf(":%2.2x", MAC_Addr[i]);
	printf("\n");

	/* Adjust for Token Ring bit-reversal in destination address */
	if (Media_Type == MEDIA_IEEE8025) {
		int	j;

		for (j = 0; j < 6; j++) FINDframeBuf[j] = TokenRingMultiAddr[j];
	}

	AdapterOpen();
	Network_Traffic = INETBOOT_TRAFFIC;
	return (0);
}

int
dev_find()
{
	int	ret;
	short i;
	static ushort ioaddr, irq, rambase, ramsize;

	for (i = 0; (ret = AdapterProbe(i)) != ADAPTER_TABLE_END; i++) {
		if (ret != ADAPTER_NOT_FOUND) {
			AdapterIdentify(i, &ioaddr, &irq, &rambase, &ramsize);
			net_dev(i, ioaddr, irq, rambase, ramsize);
		}
	}
}

void
legacyprobe()
{
	short	i;
	ushort	Bus, Port, PortSize, Irq, rambase, ramsize;
	DWORD	val[3], len, Mem, MemSize;
	int	flag;

	for (i = 0; ISAAddr(i++, &Port, &PortSize) != ADAPTER_TABLE_END; ) {
		/*
		 * Notify framework that we're going to need space to store
		 * some information.
		 */
		if (node_op(NODE_START) != NODE_OK)
			return;

		/*
		 * Check to see if we can use the io addr. If so find
		 * the following values: ioaddr, irq, rambase and size.
		 * NOTE: Because the framework always used 'short' args
		 * rambase is in intel paragraphs and ramsize is in K.
		 */
		val[0] = Port;
		val[1] = PortSize;
		val[2] = 0;
		flag = 0;
		len = PortTupleSize;
		/*
		 * Special case for pe device, it's i/o ports can usurp the
		 * Parallel port device.
		 */
		if (strcmp(driver_name, "pe") == 0)
			flag = RES_USURP;
		if (set_res(PortName, val, &len, flag) != RES_OK) {
			node_op(NODE_FREE);
			continue;
		}
		if (ISAProbe(Port) != ADAPTER_NOT_FOUND) {
			ISAIdentify(Port, &Irq, &rambase, &ramsize);
		} else {
			/*
			 * Couldn't find the device at this I/O address
			 * continue on.
			 */
			node_op(NODE_FREE);
			continue;
		}

		/* ---- Set the irq property ---- */
		val[0] = Irq;
		val[1] = 0;
		val[2] = 0;
		flag = 0;
		len = IrqTupleSize;
		/*
		 * Special cases:  All net cards requesting IRQ 3 can usurp,
		 * and pe can usurp Parallel port IRQ
		 */
		if (Irq == 3 || strcmp(driver_name, "pe") == 0)
			flag = RES_USURP;
		if (Irq) {
			if (set_res(IrqName, val, &len, flag) != RES_OK) {
				/*
				 * Bad problem here, a driver has found it's
				 * device but we couldn't get the necessary
				 * IRQ because of a conflict.  Indicate this
				 * failure to the framework and bail out.
				 */
				node_op(NODE_INCOMPLETE);
				continue;
			}
		}

		/* ---- Set the memory property converting to normal sizes - */
		if (rambase) {
			Mem = (DWORD)rambase * 16L;
			MemSize = (DWORD)ramsize * 1024L;
			val[0] = Mem;
			val[1] = MemSize;
			val[2] = 0;
			flag = 0;
			len = MemTupleSize;
			/*
			 * Special cases: tr and smc drivers can usurp bios
			 * memory Pseudo-devices.
			 */
			if (strcmp(driver_name, "tr") == 0 ||
				strcmp(driver_name, "smc") == 0)
				flag = RES_USURP;
			if (set_res(MemName, val, &len, flag) != RES_OK) {
				/*
				 * Same problem here, a driver has found it's
				 * device but we couldn't get the necessary
				 * memory because of a conflict.  Indicate this
				 * failure to the framework and bail out.
				 */
				node_op(NODE_INCOMPLETE);
				continue;
			}
		}

		/* ---- XXX need to deal with dma ---- */
		node_op(NODE_DONE);
	}
}

#define	GetRes(Res) {							\
	/*								\
	 * Get the resource list for "Res". The driver has allocated	\
	 * an array large enough to hold its resources. The first 	\
	 * element of the array is initialized with the size so that	\
	 * we can just pass it along to bootconf.			\
	 */								\
	len = Res##_Space[0];						\
	(void) get_res(Res##Name, Res##_Space, &len);			\
}

installonly()
{
	DWORD	len;		/* ... length of each giving resource */
	ushort	Dev,
		Rtn = BEF_FAIL;	/* ... must process one correctly */
	extern DWORD	Name_Space[],
			Slot_Space[],
			Port_Space[],
			Irq_Space[],
			Mem_Space[],
			Dma_Space[];

	Dev = 0;
	do {
		if (node_op(NODE_START) != NODE_OK)
			return (Rtn);

		/* ---- Get all possible props ---- */
		GetRes(Name);	GetRes(Slot);	GetRes(Port);
		GetRes(Irq);	GetRes(Mem); GetRes(Dma);

		Mem_Space[0] /= 16L;
		Mem_Space[1] /= 1024L;

		/* ---- mem / 16 size / 1024 ---- */
		if (InstallConfig()) {
			printf("Failed to configure board\n");
			return (BEF_FAIL);
		}

		/* ---- add into the system ---- */
		net_dev(Dev, (ushort)Port_Space[0], (short)Irq_Space[0],
			(short)Mem_Space[0], (short)Mem_Space[1]);

		Rtn = BEF_OK;
		node_op(NODE_DONE);
	} while (++Dev);
}
#undef GetRes


int
listen_packet()
{
	/*
	 * Make a far pointer to receive_callback by hand because we're
	 * running without DOS relocation.  Casting the routine address
	 * to an unsigned gets us the offset; mycs() is used for the
	 * segment.
	 */
#define	ADAPTERRECEIVE(b, l, f) \
		AdapterReceive((unchar far *)b, l, (recv_callback_t)f)

	if (!Rx_Flag) {
		ADAPTERRECEIVE(receiveBufPtrs[bufferWriteIndex],
			bufferLen,
			MK_FP(mycs(), FP_OFF(receive_callback)));
		Rx_Flag = 1;
	}
}

static void __far
receive_callback(ushort rxlen)
{
	/*
	 * Rx buffer no longer posted
	 */
	Rx_Flag = 0;

	if (old_driver_interface) {
		/*
		 * Filter out all multicast and broadcast packets.  One
		 * implication is that the booting up client will not answer
		 * ARP request, which is okay because the system is only
		 * booting up.
		 */
		if (receiveBufPtrs[bufferWriteIndex][0] & 0x01) {
			listen_packet();
			return;
		}

#define	STATE_INITIAL		0x00
#define	STATE_RARP_RECVD	0x01
#define	STATE_SHOULD_SYNC	0x02

		/*
		 * If we have received a RARP reply, do not receive any more.
		 * This is okay because anything received by the real mode
		 * driver will definitely show up in inetboot, so we are
		 * certain that inetboot would have seen the RARP reply.
		 * However, if we do not have this filter, extraneous RARP
		 * replies will jam in and interfere with receiving of RPC
		 * replies, particularly the RPC to locate a bootparams server.
		 * Without this filter, it has been observed that the client
		 * can hang while waiting for a bootparams server to respond
		 * when actually it is the client that has ignored the
		 * response.
		 */
		if (Traffic_State >= STATE_RARP_RECVD) {
			if (receiveBufPtrs[bufferWriteIndex][12] == 0x80 &&
				receiveBufPtrs[bufferWriteIndex][13] == 0x35 &&
				receiveBufPtrs[bufferWriteIndex][21] == 0x04) {
					listen_packet();
					return;
			}
		}
	}

	/*
	 * The current buffer being filled is pointed to by the index
	 * bufferWriteIndex.
	 */
	receiveLengths[bufferWriteIndex] = rxlen;
	packetCount++;

	if (old_driver_interface && (Network_Traffic == INETBOOT_TRAFFIC)) {
		/*
		 * We have a problem that if we have multiple receive buffers,
		 * due to the way inetboot is doing client/server by polling,
		 * if there is any unsolicited packet in one of the receive
		 * buffers that is not a reply to a request, all the
		 * subsequent request/reply will be out of sync and for
		 * each out-of-sync packet, a retransmission request will
		 * be made until the correct reply comes around to be read by
		 * inetboot.  This is not acceptable.  The true fix should
		 * be in inetboot by read flushing all the outstanding
		 * received packets before issuing the request in this polling
		 * fashion, but this means that people using this new
		 * real mode driver will mandate patching their inetboot,
		 * also not very acceptable.  So a real ugly kludge is done
		 * here to __look_at__ the content of the received packet
		 * to transition through a RARP reply then a UDP reply.
		 * At this point, we are pretty sure that the client has
		 * started the polled client/server traffic and so we silently
		 * sync up the received buffers for inetboot.  Ughh...
		 * See bug 1152034 for inetboot.
		 */
		switch (Traffic_State) {
		case STATE_INITIAL:
			/* check for a RARP reply (type 0x8035, command 04) */
			if (receiveBufPtrs[bufferWriteIndex][12] == 0x80 &&
				receiveBufPtrs[bufferWriteIndex][13] == 0x35 &&
				receiveBufPtrs[bufferWriteIndex][21] == 0x04) {
				Traffic_State = STATE_RARP_RECVD;
			}
			break;
		case STATE_RARP_RECVD:
			if (receiveBufPtrs[bufferWriteIndex][12] == 0x08 &&
				receiveBufPtrs[bufferWriteIndex][13] == 0x00) {
				Traffic_State = STATE_SHOULD_SYNC;
			}
			break;
		case STATE_SHOULD_SYNC:
			if (packetCount > 1) {
				bufferReadIndex = bufferWriteIndex;
				packetCount = 1;
			}
			break;
		}
	}

	/* update to receive into the next buffer, if available */
	if (++bufferWriteIndex == NUMRXBUFS)
		bufferWriteIndex = 0;

	if (packetCount == NUMRXBUFS) {
		/*
		 * The ring buffer is full. There is no good thing to do
		 * here, but the choice is to keep the receiver going
		 * by simulating a read and dumping the oldest packet.
		 * Odds are, the upper layers are not interested in
		 * these packets and will dump them anyway. If we
		 * stop the receiver, we force the driver to deal with
		 * incoming packets and no place to put them.
		 */
		packetCount--;
		bufferReadIndex = ++bufferReadIndex % NUMRXBUFS;
	}
	listen_packet();
}

int
send_FIND_frame()
{
	bcopy((char _far *)&FINDframeBuf[6], (char _far *)MAC_Addr, 6);
	bcopy((char _far *)&FINDframeBuf[49], (char _far *)MAC_Addr, 6);
	AdapterSend((char _far *)FINDframeBuf, 100);
	return (0);
}

static int
send_SENDFILE_frame(long seqnum)
{
	bcopy((char _far *)SENDFILEframeBuf, (char _far *)serverAddr, 6);
	bcopy((char _far *)&SENDFILEframeBuf[6], (char _far *)MAC_Addr, 6);
	SENDFILEframeBuf[14] = serverSAP;
	SENDFILEframeBuf[15] = 0xFC;
	SENDFILEframeBuf[25] = (char)((seqnum & 0xFF000000) >> 24);
	SENDFILEframeBuf[26] = (char)((seqnum & 0x00FF0000) >> 16);
	SENDFILEframeBuf[27] = (char)((seqnum & 0x0000FF00) >> 8);
	SENDFILEframeBuf[28] = (char)(seqnum & 0x000000FF);
	bcopy((char _far *)&SENDFILEframeBuf[49], (char _far *)MAC_Addr, 6);
	SENDFILEframeBuf[59] = serverSAP;
	AdapterSend((char _far *)SENDFILEframeBuf, sizeof (SENDFILEframeBuf));
	return (0);
}

int
process_frame()
{
unchar frame_type;
long seq;
uint loadseg;
uint loadoff;
uint loadcnt;
char flags;
uint diff, rest;
ushort len;
int x;
unchar *receiveBuffer;

	intr_disable();

	receiveBuffer = receiveBufPtrs[bufferReadIndex];

	/*
	 * Verify that this is a LLC packet addressed to us by first
	 * checking if the length field is less than 1500 and then
	 * the destination SAP field is 0xFC
	 */
	len = (ushort)(receiveBuffer[12])*0x100 + (ushort)(receiveBuffer[13]);
	if (len > 1500) {
		goto getout;
	}
	if (receiveBuffer[14] != 0xFC) {
		goto getout;
	}

	frame_type = receiveBuffer[20];
	switch (frame_type) {
	case FOUND_FRAME:
		if (state != FINDING)
			break;
		bcopy((char _far *)serverAddr,
			(char _far *)&receiveBuffer[48], 6);
		serverSAP = receiveBuffer[74];
		printf("RPL Server Address:");
		for (x = 0; x < 6; x++)
			printf("%s%2.2x", x ? ":" : "", serverAddr[x]);
		putchar('\n');
		server_not_found = 0;
		break;
	case FILEDATA_FRAME:
		if (state != DATA_XFER)
			break;
		/* check correct sequence number */
		seq = (long)(receiveBuffer[25]*256*256*256) +
			(long)(receiveBuffer[26]*256*256) +
			(long)(receiveBuffer[27]*256) +
			(long)(receiveBuffer[28]);
		printf("\b%c", "\\|/-"[indicator++%4]);
		if ((long)seq != (long)expectedRxSeq) {
			send_SENDFILE_frame(expectedRxSeq);
			timeout(RESET, (ushort)MAX_TIMEOUT);
			goto getout;
		}
		flags = receiveBuffer[41];
#define	FINAL_BIT 	(char)0x80
		if (flags & FINAL_BIT) {
			last_frame = 1;
			putstr("Download complete\r\n");
			xfer_seg = (receiveBuffer[37]*256 +
				receiveBuffer[38])*0x1000;
			xfer_off = receiveBuffer[39]*256 + receiveBuffer[40];
			xfer_addr = (void (_far *)())((long)xfer_seg*0x10000 +
				(long)xfer_off);
		} else {
			loadseg = (uint)((receiveBuffer[33]*256 +
				receiveBuffer[34])*0x1000);
			loadoff = (uint)(receiveBuffer[35]*256 +
				receiveBuffer[36]);
			loadcnt = receiveBuffer[42]*256 + receiveBuffer[43] - 4;

			/*
			 * A very subtle problem is that if the memory copy
			 * will result in crossing into the next segment,
			 * ie, if loadseg:loadoff + loadcnt > loadseg:0xffff,
			 * then the underlying assembly bcopy() function will
			 * not have the intelligence, or the "rep movsb"
			 * instruction will not increment the es register to
			 * go to the next segment.  This will result in wrong
			 * loading and in the case of loadseg = 0, this will
			 * overwrite the beginning of the interrupt vector
			 * table and particularly the hardware timer interrupt
			 * thus hang the system.
			 */
			if (((ulong)loadoff + (ulong)loadcnt) > (ulong)0xffff) {
				diff = (ulong)0x10000 - (ulong)loadoff;
				rest = (ushort)(loadoff + loadcnt);
				bcopy(MK_FP(loadseg, loadoff),
					(char _far *)(&receiveBuffer[46]),
					diff);
				loadseg += 0x1000;
				bcopy(MK_FP(loadseg, 0),
					(char _far *)(&receiveBuffer[46+diff]),
					rest);
			} else {
				bcopy(MK_FP(loadseg, loadoff),
					(char _far *)(&receiveBuffer[46]),
					loadcnt);
			}

			expectedRxSeq++;
			timeout(RESET, (ushort)MAX_TIMEOUT);
		}
		break;
	default:
		break;
	} /* switch */

getout:
	if (++bufferReadIndex == NUMRXBUFS)
		bufferReadIndex = 0;
	if (packetCount-- == NUMRXBUFS)
	/* restart the receive side after [a ring] buffer empties */
	{
		listen_packet();
	}
	if (!last_frame)
		intr_enable();
	return (0);
}

void far
transfer_execution()
{
	/* Update the flag */
	Network_Traffic = INETBOOT_TRAFFIC;

	putstr("Transfer execution to ");
	puthex(xfer_seg);
	putstr(":");
	puthex(xfer_off);
	putstr("\r\n");

	/*
	 * Move the pri-to-secboot info structure in es:di and
	 * pass to the secondary boot
	 */
	set_ds();
	ASM {
		mov		ax, ds
		mov		es, ax
		mov		ax, offset Boot_Link
		mov		di, ax
	}
	ASM {
		mov		ax, xfer_seg
		push	ax
		mov		ax, xfer_off
		push	ax
		retf
	}
	/* NOTREACHED */
}

int
timeout(int function, ushort value)
{
static ushort i = 0;
static ushort j = 0;
static ushort k = 0;
static ushort limit = MAX_TIMEOUT;

	if (function == RESET) {
		i = 0;
		j = 0;
		k = 0;
		limit = value;
		return (0);
	}
	if (function == AGAIN) {
		k++;
		if (k > (ushort)limit) {
			k = 0;
			j++;
			if (j > (ushort)limit) {
				j = 0;
				i++;
				if (i > (ushort)limit) {
					i = 0;
					return (1);
				} /* i */
			} /* j */
		} /* k */
	}
	return (0);
}

void
PROM_boot_setup()
{
	/* Find the recognized hardware */
	DriverIdentify(-1);

	/* Initialize the hardware */
	/* sets up linkages */
	/* sets up the pri_to_secboot structure */
	/* transfer control to pre-defined address of 0:8000 */
}

/* DriverInit is called exactly once, for the selected instance of the card. */
int
DriverInit(ushort ioaddr, ushort irq, ushort rambase, ushort ramsize)
{
	int IRQ_Vectors[] = {
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77
	};
	int PIC_Masks[] = {
		0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F,
		0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F
	};
	unchar inval;
	void __far *israddr;

	current_rambase = rambase;
	current_ramsize = ramsize;
	current_IRQ = irq;
	current_portbase = ioaddr;
	save_ds();

	/* Install interrupt handler first, as AdapterInit() may use it */
	intr_disable();
	israddr = MK_FP(mycs(), FP_OFF(intr_vec));
	setvector(IRQ_Vectors[current_IRQ], israddr);
	if (irq > 7) {
		inval = inb(0xA1);
		inval &= PIC_Masks[current_IRQ];
		outb(0xA1, inval);
		outb(0xA0, 0x20);
	} else {
		inval = inb(0x21);
		inval &= PIC_Masks[current_IRQ];
		outb(0x21, inval);
		outb(0x20, 0x20);
	}

	AdapterInit(current_portbase, current_IRQ, current_rambase,
		current_ramsize);

	/* in case AdapterInit() didn't do it */
	intr_enable();
	return (0);
}

static void
DriverIdentify(short index)
{
int found;
int num_adapters;
int last_index;
int ret;
int i;

	if (DebugFlag) {
		putstr("DriverIdentify(): index=");
		puthex(index);
		putstr("\r\n");
	}
	if (index < 0) {
		if (DebugFlag)
			putstr("[Need to find the card] ");
		found = 0;
		num_adapters = 0;
		last_index = 0;

		for (i = 0; (ret = AdapterProbe(i)) != ADAPTER_TABLE_END; i++) {
			if (ret != ADAPTER_NOT_FOUND) {
				num_adapters++;
				last_index = i;
				if (ret == BOOT_ROM_INSTALLED) {
					found = 1;
					break;
				}
			}
		}
		if (!found) {
			if (num_adapters == 1) {
				/*
				 * Since there is only 1 adapter, must be this
				 * one no matter this one has a boot ROM or
				 * not.  Here, we are dealing with the case
				 * that the card has a ROM but is booted by
				 * the boot floppy and for some reason, the
				 * passing of config parameters from the MDB
				 * driver to this driver fails.
				 */
				found = 1;
			} else {
				printf("\nThere are %d adapters in the",
					num_adapters);
				printf(" system, but none of them has a boot");
				printf(" ROM.\n Please make sure you have");
				printf(" configured the network adapters");
				printf(" correctly.\nYou must reboot the");
				printf(" system now.");
				while (1)
					;
			}
		}
	} else {
		last_index = index;
		found = 1;
	}

	if (found)
		AdapterIdentify(last_index, &current_portbase, &current_IRQ,
			&current_rambase, &current_ramsize);
}

static int spinner;
void
new_driver_interface(ushort ax, ushort bx, ushort cx, ushort dx,
			ushort si, ushort di, ushort es)
{
	union {
		struct {
			ushort off, seg;
		} s;
		char far *ptr;
	} fp;
	char *p;
	int wait;

	if ((ax >> 8) >= MAX_SERVICE)
		return;

	switch (ax >> 8) {
	case INIT_CALL:
		/*
		 * Register Set:
		 * BL = IRQ number
		 * BH = RAM size in K
		 * CX = I/O Address
		 * DX = RAM base segment
		 */

		DriverInit(cx, bx & 0xff, dx, bx >> 8);
		break;

	case OPEN_CALL:
		/*
		 * No register setup for this call
		 */

		AdapterOpen();
		break;

	case CLOSE_CALL:
		/*
		 * no register setup for this call
		 */

		AdapterClose();
		break;

	case SEND_CALL:
		/*
		 * Register Setup:
		 * BX:SI = packet buffer address
		 * CX    = length of outgoing packet
		 */
		fp.s.off = si;
		fp.s.seg = bx;
		AdapterSend(fp.ptr, cx);
		break;

	case RECEIVE_CALL:
		/*
		 * Register Setup:
		 * DX:DI = callers receive buffer
		 * CX    = size of receive buffer
		 * Return Setup:
		 * CX    = number of bytes received
		 */

		intr_disable();
		if (!packetCount) {
			cx = 0;
		} else {

			packetCount--;
			fp.s.seg = dx;
			fp.s.off = di;

			if (cx < receiveLengths[bufferReadIndex])
				receiveLengths[bufferReadIndex] = cx;
			else
				cx = receiveLengths[bufferReadIndex];
			bcopy(fp.ptr,
				(char _far *)receiveBufPtrs[bufferReadIndex],
				receiveLengths[bufferReadIndex]);
			receiveLengths[bufferReadIndex] = 0;
			bufferReadIndex = ++bufferReadIndex % NUMRXBUFS;
		}

		/*
		 * The receiver may not be enabled if
		 * this is the first call to receive.
		 */
		listen_packet();
		intr_enable();
		break;

	case GET_ADDR_CALL:
		bx = MAC_Addr[0] << 8 | MAC_Addr[1];
		cx = MAC_Addr[2] << 8 | MAC_Addr[3];
		dx = MAC_Addr[4] << 8 | MAC_Addr[5];
		break;

	case IDENTIFY_CALL:
		/*
		 * Register Setup:
		 * CX = contains the port
		 *
		 * Returns:
		 * BL = irq
		 * BH = ramsize in K
		 * CX = ioaddr
		 * DX = rambase as segment
		 */
		DriverIdentify(cx);
		bx = current_ramsize << 8 | current_IRQ;
		cx = current_portbase;
		dx = current_rambase;
		break;

	case GET_DRVNAME_CALL:
		/*
		 * Register Setup:
		 * BS:CX = location to store driver_name
		 */
		fp.s.off = cx;
		fp.s.seg = bx;
		for (p = driver_name; *p; )
			*fp.ptr++ = *p++;
		break;
	}
}

/*
 * The gluecode issues the linkage interrupt and control comes here.
 * Simply perform a long jump to the inetboot with the correct es:di
 * set to point to the pri_to_secboot structure
 */
void _far
glueboot()
{
	set_ds();
	Network_Traffic = INETBOOT_TRAFFIC;

	ASM {
		mov		ax, ds
		mov		es, ax
		mov		ax, offset Boot_Link
		mov		di, ax
	}
	ASM {
		mov		ax, 0
		push	ax
		mov		ax, 8000h
		push	ax
		retf
	}
}
