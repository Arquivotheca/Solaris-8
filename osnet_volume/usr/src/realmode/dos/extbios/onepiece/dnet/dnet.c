/*
 * Copyright (c) 1995 - 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident  "@(#)dnet.c 1.19     98/02/19 SMI\n"

/*
 * dnet --DNET 21040/21041/21140 - Real Mode Driver
 */

/*
#define	DEBUG
#define	DNET_NOISY
*/
#define	FARDATA 1
#include <sys/types.h>
#include <common.h>
#include <befext.h>
#include <bef.h>
#include <stdio.h>
#include "../rplagent.h"

/*
 * Exported global variables (used by external framework)
*/
InstallSpace(Name, 1);
InstallSpace(Slot, 1);
InstallSpace(Port, 1);
InstallSpace(Irq, 1);
InstallSpace(Mem, 1);
InstallSpace(Dma, 1);
char ident[] = "Dec21x4x";
char driver_name[8] = "dnet";

ushort Inetboot_IO_Addr = 0;
ushort Inetboot_Mem_Base = 0;
ushort Inetboot_IRQ_no = 0;
unchar MAC_Addr[6] = {0};

#ifdef DEBUG
int DebugFlag = 0;
#else
int DebugFlag = 0;
#endif

/*
 * Required system entry points
 */
int	AdapterProbe(short);
void	AdapterIdentify(short, ushort *, ushort *, ushort *, ushort *);
int	AdapterInit(ushort, ushort, ushort, ushort);
void	AdapterOpen(void);
void	AdapterClose(void);
void	AdapterSend(char __far *, ushort);
void	AdapterReceive(unchar __far *, ushort, void (__far *callback)(ushort));
void __far interrupt AdapterISR(void);
void	AdapterInterrupt();

/*
 * Imported external objects
*/
extern	int Network_Traffic;
extern	int Media_Type;

/* External Functions called */
extern unsigned long inl(ushort);
extern void outl(ushort, ulong);
extern microseconds(ushort);
extern int is_pci();
extern int pci_find_device(ushort, ushort, ushort, ushort *);
extern int pci_read_config_dword(ushort, ushort, ulong *);
extern void drv_usecwait(ulong);
extern void delay(ulong);

/*
 * Define a bunch of stuff to get the protected mode routines that
 * we #include and use here to compile.
*/

#define	CE_CONT			0
#define	CE_NOTE			1
#define	CE_WARN			2

#ifdef DEBUG
#define	ASSERT(cond) { if (!(cond)) printf(" dnet ASSERT FAIL:%s:%d %s ", \
	__FILE__, __LINE__, #cond); }
#else
#define	ASSERT(cond) /* */
#endif

#define	ddi_io_getl(handle, address)		inl(address)
#define	ddi_io_putb(handle, address, value)	outb(address, value)
#define	ddi_io_putl(handle, address, value)	outl(address, value)

#define	drv_usectohz(x)			((u_long)(x+999)/1000)

typedef void *ddi_acc_handle_t;
#define	DDI_PROP_SUCCESS	0
#define	DDI_PROP_FAILURE	1
#define	DDI_DEV_T_ANY		0
#define	DDI_PROP_DONTPASS	0
#define	PROP_LEN_AND_VAL_BUF	0

#define	ddi_getprop(p1, p2, p3, p4, p5)		(p5)
#define	ddi_getlongprop(p1, p2, p3, p4, p5, p6)	DDI_PROP_FAILURE
#define	ddi_prop_op(p1, p2, p3, p4, p5, p6, p7)	DDI_PROP_FAILURE
#define	ddi_get_instance(x)			(Inetboot_IO_Addr)
#define	kmem_free(ptr, size) 			/* */

#define	ETHERADDRL	6
#define	ETHERMAX	1514
#define	ETHERMIN	64

typedef
struct gld_mac_info {
	unsigned char	gldm_macaddr[ETHERADDRL];
	unsigned char	gldm_broadcast[ETHERADDRL];
	void *		gldm_private;	/* pointer to dnet structure */
} gld_mac_info_t;

#define	REALMODE 1
#include "../mii/mii.h"
#include "dnet.h"

#undef	DNET_KVTOP
#define	DNET_KVTOP(addr) \
	((paddr_t)(((ulong)((ulong)myds()<<4)) + ((u_short)(addr))))

/*
 * []------------------------------------------------------------[]
 *  | Debug Information Section					|
 * []------------------------------------------------------------[]
*/
/* #define DEBUG /* */
#ifdef DEBUG
#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#define	Dprintf(f, x) if (DebugDNet & DNET_##f) printf x
#define	DNET_FLOW	0x0001	/* ... general flow of program */
#define	DNET_ERR	0x0002	/* ... error conditions */
#define	DNET_BIOS	0x0004	/* ... data on bios calls */
#define	DNET_FLOW_IRQ	0x0008	/* ... info. during interrupts */
#define	DNET_ALL	-1	/* ... enable all flags */
#define	DNET_TIMING	0x0010
int	DebugDNet = DNET_TIMING;
#else
#define	Dprintf(f, x)
#endif
#ifdef DEBUG
void PAK(void) {
	putstr("\r\nPRESS A KEY\r\n");
	kbchar();
	putstr("\r\n");
}
#endif
/*
 * []------------------------------------------------------------[]
 *  | End of debug section
 * []------------------------------------------------------------[]
*/

/*
 * Internal function declarations
*/
static void find_adapters();
static void dnet_alloc_buffers();
static void dnet_init_txrx_bufs(gld_mac_info_t *);
static int alloc_descriptor(gld_mac_info_t *);
static void dnet_getp(void);
static void cmn_err(int, char *, ...);
static void bzero(char *, int);

/*
 * Include the relevant portions of the protected mode driver.
*/
#include "../mii/mii.c"
#include "dnetpm.c"

/*
 * We do our buffer management, send and receive stuff differently than
 * the protected mode driver.  Here are some definitions we need.
*/

#define	RX_BUF_SIZE		((ETHERMAX + 4 + 3) & ~3)
#define	TX_BUF_SIZE		((ETHERMAX + 4 + 3) & ~3) /* >= ETHERMAX */

struct shared_mem {
	struct	tx_desc_type	tx_desc[MAX_TX_DESC];
	struct	rx_desc_type	rx_desc[MAX_RX_DESC];
	char			tx_buffer[MAX_TX_DESC][TX_BUF_SIZE];
	char			rx_buffer[MAX_RX_DESC][RX_BUF_SIZE];
	char			setup_buf[SETUPBUF_SIZE];
	char			pad[4];
};

/*
 * Here's some definitions for the Realmode framework and probe
*/
#define	DNET_MAX_IO_PORTS	16

#define	PCI_COOKIE_TO_BUS(x)	((unchar)(((x)&0xff00)>>8))
#define	PCI_COOKIE_TO_DEV(x)	((unchar)(((x)&0x00f8)>>3))
#define	PCI_COOKIE_TO_FUNC(x)	((unchar)((x)&0x0007))
#define	IOBASE_REG		0x10
#define	INTERRUPT_REG		0x3C
#define	SUBVENID		0x2c	/* Subsystem Vendor ID */
#define	SUBSYSID		0x2e	/* Subsystem ID */

struct dnet_board_info {
	ushort		id;
	ushort		board_type;
	ushort		iobase;
	ushort		irq;
	ushort		subvenid;
	ushort		subsysid;
};

/*
 * Internal global variables, used by this module only
*/
unchar __far *Rx_buf = 0;
ushort Rx_len = 0;
void (__far *Rx_Callback)(ushort) = 0;
int Rx_Enable = 0;

struct dnet_board_info board_info[DNET_MAX_IO_PORTS] = {0};
int board_no = 0;
ulong result = 0;
ushort readword = 0;
u_char vendor_info[SROM_SIZE] = {0};

struct gld_mac_info mac_info = {0};
struct gld_mac_info *Macinfo = &mac_info;
struct dnetinstance dnets = {0};
struct dnetinstance *dnetp = &dnets;

struct shared_mem temp_struct = {0};
struct shared_mem *base_memory = &temp_struct;

/*
 * ISAAddr -- given instance 'idx' return port/size pair
 *		ADATER_TABLE_OK = okay, ADAPTER_TABLE_END = stop
*/
ISAAddr(short idx, short *Port, short *Portsize)
{
	return (ADAPTER_TABLE_END);
}

/*
 * ISAProbe -- given the port see if a non-Plug-and-Play card exists
 *		ADAPTER_FOUND = okay, ADAPTER_NOT_FOUND = fail
*/
ISAProbe(short port)
{
	return (ADAPTER_NOT_FOUND);
}

/*
 * ISAIdentify -- given the port return as much info as possible
 *		routine doesn't get called because ISAProbe always
 *		returns ADAPTER_NOT_FOUND;
*/
void
ISAIdentify(short Port, short *Irq, short *Mem, short *Memsize)
{
}

/*
 * InstallConfig -- setup internal structures for card at given
 * resources
 *			0 = okay, !0 = failure
*/
InstallConfig(void)
{
	DWORD	val[3], len;
	ulong	result,
		LReg;
	int	Idx,
		Reg;
	int board;

	Dprintf(FLOW, ("InstallConfig(0x%x, %d)\r\n",
	    (ushort)Port_Space[0], (ushort)Irq_Space[0]));

	len = AddrTupleSize;
	if (get_res("addr", val, &len) != RES_OK)
		return (1);

	find_adapters();

	for (board = 0; board < board_no; board++) {
		switch (board_info[board].board_type) {
			case DEVICE_ID_21140:
				Dprintf(FLOW, ("Board %d, Type: 21140\r\n",
				    board));
				break;
			case DEVICE_ID_21040:
				Dprintf(FLOW, ("Board %d, Type: 21040\r\n",
				    board));
				break;
			case DEVICE_ID_21143:
				Dprintf(FLOW, ("Board %d, Type: 21142/143\r\n",
				    board));
				break;
			case DEVICE_ID_21041:
				Dprintf(FLOW, ("Board %d, Type: 21041\r\n",
				    board));
				break;
			default:
				Dprintf(FLOW, ("Board %d, Type: Unknown\r\n",
				    board));
		}
		if ((ushort)val[0] == board_info[board].id)
			break;
	}

	if (board == board_no) {

		/*
		 * Safety valve. Because we've been called it means that
		 * a board of our type has been found in the system. We
		 * should be able to loop in the above case without limits.
		 * Prudence says otherwise.
		 */
		return (1);
	}

	if (!pci_read_config_word((ushort)val[0], 0x04, &Reg))
		return (1);

	if ((Reg & 5) == 1) {
	/*
	 * This section of code is needed if the PCI controller does
	 * Bus Mastering. It appears that various BIOS' turn off the
	 * BME bit during POST and certain controllers need to have
	 * this enabled.
	*/
		Dprintf(FLOW, ("Enabling the BME bit\n"));
		pci_write_config_word((ushort)val[0], 0x04, Reg | 4);
	}

	/* Make sure the device is not asleep */
	pci_read_config_dword((ushort)val[0], 0x40, &LReg);
	pci_write_config_dword((ushort)val[0], 0x40, LReg & 0x3fffffff);

	pci_read_config_dword(board_info[board].id, IOBASE_REG, &LReg);
	if ((LReg & 0xfffe) != (ushort)Port_Space[0]) {
		Dprintf(ERR, ("Bad port vals: LReg 0x%lx, Port 0x%lx\n",
		    LReg, Port_Space[0]));
		return (1);
	}

	pci_read_config_dword(board_info[board].id, INTERRUPT_REG, &LReg);
	if ((LReg & 0xff) != (ushort)Irq_Space[0]) {
		Dprintf(ERR, ("Bad irq vals: LReg 0x%lx, Irq 0x%lx\n",
		    (LReg & 0xff), Irq_Space[0]));
		return (1);
	}

	if (board_info[board].subvenid)
		net_dev_pci(
			PCI_COOKIE_TO_BUS(board_info[board].id),
			board_info[board].subvenid,
			board_info[board].subsysid,
			PCI_COOKIE_TO_DEV(board_info[board].id),
			PCI_COOKIE_TO_FUNC(board_info[board].id));
	else
		net_dev_pci(
			PCI_COOKIE_TO_BUS(board_info[board].id),
			DEC_VENDOR_ID,
			board_info[board].board_type,
			PCI_COOKIE_TO_DEV(board_info[board].id),
			PCI_COOKIE_TO_FUNC(board_info[board].id));

	board_info[board].iobase = (ushort)Port_Space[0];
	board_info[board].irq = (ushort)Irq_Space[0];

	Dprintf(FLOW, ("(OK)\r\n"));
	return (0);
}

/*
 * AdapterProbe -- Determine if the device is present
 * Probe the hardware at relative index 'index'
 */
int
AdapterProbe(short index)
{
#ifdef DEBUG
	if (DebugFlag)
		putstr(" dnet Probe");
#endif

	/*
	 * Return ADAPTER_TABLE_END if there are no more physical slots
	 * or port base locations to probe for device.
	 */
	if (index < 0 || index >= DNET_MAX_IO_PORTS)
		return (ADAPTER_TABLE_END);

	/*
	 * Check if the device is PCI based
	 */
	if (is_pci() == 0)
		return (ADAPTER_TABLE_END);

	/*
	 * Probe for the boards, first time through.
	 */
	if (index == 0)
		find_adapters();

	if (index < board_no)
		return (ADAPTER_FOUND);

	return (ADAPTER_TABLE_END);
}

static void
find_adapters()
{
	int i;

/*
 * The documentation doesn't describe pci_find_device() very well.
 * In particular it would be nice to know if it could ever return
 * success for a higher index than it previously returned failure
 * for the same vendorid/deviceid.  If it cannot, then we can break
 * out of these loops earlier.
 */

	board_no = 0;

	for (i = 0; i < DNET_MAX_IO_PORTS; i++) {
		if (pci_find_device(DEC_VENDOR_ID, DEVICE_ID_21140, i,
		    &board_info[board_no].id) &&
		    pci_read_config_dword(board_info[board_no].id,
		    INTERRUPT_REG, &result)) {
			board_info[board_no].board_type = DEVICE_ID_21140;
			pci_read_config_word(board_info[board_no].id,
				SUBVENID, &readword);
			board_info[board_no].subvenid = readword;
			pci_read_config_word(board_info[board_no].id,
				SUBSYSID, &readword);
			board_info[board_no].subsysid = readword;
			if (++board_no >= DNET_MAX_IO_PORTS)
				return;
		}
	}

	for (i = 0; i < DNET_MAX_IO_PORTS; i++) {
		if (pci_find_device(DEC_VENDOR_ID, DEVICE_ID_21041, i,
		    &board_info[board_no].id) &&
		    pci_read_config_dword(board_info[board_no].id,
		    INTERRUPT_REG, &result)) {
			board_info[board_no].board_type = DEVICE_ID_21041;
			pci_read_config_word(board_info[board_no].id,
				SUBVENID, &readword);
			board_info[board_no].subvenid = readword;
			pci_read_config_word(board_info[board_no].id,
				SUBSYSID, &readword);
			board_info[board_no].subsysid = readword;
			if (++board_no >= DNET_MAX_IO_PORTS)
				return;
		}
	}

	for (i = 0; i < DNET_MAX_IO_PORTS; i++) {
		if (pci_find_device(DEC_VENDOR_ID, DEVICE_ID_21040, i,
		    &board_info[board_no].id) &&
		    pci_read_config_dword(board_info[board_no].id,
		    INTERRUPT_REG, &result)) {
			board_info[board_no].board_type = DEVICE_ID_21040;
			pci_read_config_word(board_info[board_no].id,
				SUBVENID, &readword);
			board_info[board_no].subvenid = readword;
			pci_read_config_word(board_info[board_no].id,
				SUBSYSID, &readword);
			board_info[board_no].subsysid = readword;
			if (++board_no >= DNET_MAX_IO_PORTS)
				return;
		}
	}

	for (i = 0; i < DNET_MAX_IO_PORTS; i++) {
		if (pci_find_device(DEC_VENDOR_ID, DEVICE_ID_21143, i,
		    &board_info[board_no].id) &&
		    pci_read_config_dword(board_info[board_no].id,
		    INTERRUPT_REG, &result)) {
			board_info[board_no].board_type = DEVICE_ID_21143;
			pci_read_config_word(board_info[board_no].id,
				SUBVENID, &readword);
			board_info[board_no].subvenid = readword;
			pci_read_config_word(board_info[board_no].id,
				SUBSYSID, &readword);
			board_info[board_no].subsysid = readword;
			if (++board_no >= DNET_MAX_IO_PORTS)
				return;
		}
	}

}

/*
 *  AdapterIdentify -- Get complete hardware configuration, consisting of
 *  base I/O address, IRQ used, shared memory base segment and size
 *  for the given 'index'.
 */
void
AdapterIdentify(short index, ushort *ioaddr, ushort *irq,
    ushort *rambase, ushort *ramsize)
{
#ifdef DEBUG
	if (DebugFlag) {
		putstr(" dnet AdapterIdentify, index = ");
		puthex(index);
	}
#endif
	if (index < 0 || index >= board_no)
		return;

	/*
	 * The doc says net_dev_pci takes four arguments, but everyone
	 * seems to use five; our driver will go over the same cliff...
	 */
	if (board_info[index].subvenid)
		net_dev_pci(
			PCI_COOKIE_TO_BUS(board_info[index].id),
			board_info[index].subvenid,
			board_info[index].subsysid,
			PCI_COOKIE_TO_DEV(board_info[index].id),
			PCI_COOKIE_TO_FUNC(board_info[index].id));
	else
		net_dev_pci(
			PCI_COOKIE_TO_BUS(board_info[index].id),
			DEC_VENDOR_ID,
			board_info[index].board_type,
			PCI_COOKIE_TO_DEV(board_info[index].id),
			PCI_COOKIE_TO_FUNC(board_info[index].id));

	/*
	 * Get the iobase from the configuration space
	 */
	pci_read_config_dword(board_info[index].id, IOBASE_REG, &result);
	board_info[index].iobase = (int)(result & 0xFFFE);

	/*
	 * Get the irq number from the configuration space
	 */
	pci_read_config_dword(board_info[index].id, INTERRUPT_REG, &result);
	board_info[index].irq = (int)(result & 0x000000FF);

	/*
	 * Set the device configuration parameters, return 0 for any
	 * parameter that does not apply to the board.
	 */
	*ioaddr		= board_info[index].iobase;
	*irq		= board_info[index].irq;
	*rambase 	= 0;
	*ramsize	= 0;

#ifdef DEBUG
	if (DebugFlag) {
		putstr(" AdapterIdentify: ioaddr="); puthex(*ioaddr);
		putstr(" irq="); putnum(*irq);
	}
#endif

}

/*
 * AdapterInit -- Initialise the board for operation
 */
int
AdapterInit(ushort ioaddr, ushort irq, ushort rambase, ushort ramsize)
{
	int i;

#ifdef DEBUG
	if (DebugFlag) {
		putstr(" dnet AdapterInit: ioaddr="); puthex(ioaddr);
		putstr(" irq="); putnum(irq);
	}
#endif

	for (i = 0; i < board_no; i++) {
		if (board_info[i].iobase == ioaddr &&
		    board_info[i].irq == irq)
			break;
	}
	if (i >= board_no)
		return (0);

	/*
	 * Save the input configuration parameters in global variables
	 */
	Inetboot_IO_Addr  = ioaddr;
	Inetboot_IRQ_no   = irq;
	Inetboot_Mem_Base = rambase;

	/*
	 * Set the connector/media type
	 */
	Media_Type = MEDIA_ETHERNET;

	/*
	 * Init the dnetp variables that we (and the protected mode routines
	 * we use) need.
	 */
	dnetp->io_reg = ioaddr;
	dnetp->board_type = board_info[i].board_type;
	dnetp->full_duplex = 0;
	dnetp->bnc_indicator = -1;
	dnetp->speed = 0;
	dnetp->promisc = 0;
	dnetp->need_saddr = 0;

	/*
	 * We fake up a little macinfo to get the protected mode routines
	 * to work for us.
	 */
	Macinfo->gldm_private = (void *)dnetp;

	dnet_reset_board(Macinfo);

	setup_legacy_blocks();
	dnet_read_srom(0, dnetp->board_type, 0, ioaddr,
			vendor_info, sizeof (vendor_info));
	dnet_parse_srom(dnetp, &dnetp->sr, vendor_info);
	BCOPY((caddr_t)dnetp->sr.netaddr,
		(caddr_t)Macinfo->gldm_macaddr, ETHERADDRL);

	BCOPY((caddr_t)dnetp->sr.netaddr, (caddr_t)MAC_Addr, ETHERADDRL);

	for (i = 0; i < ETHERADDRL; i++)
		Macinfo->gldm_broadcast[i] = (unsigned char) 0xFF;

	dnet_alloc_buffers();

	dnetp->phyaddr = -1;
	if (dnetp->board_type == DEVICE_ID_21140 ||
	    dnetp->board_type == DEVICE_ID_21143)
		do_phy(Macinfo);	/* Initialize the PHY, if any */

	intr_enable();
	find_active_media(Macinfo);
	bzero((char *)dnetp->setup_buf_vaddr, SETUPBUF_SIZE);
	dnet_reset_board(Macinfo);
	dnet_init_board(Macinfo);

	return (1);
}

static void
dnet_alloc_buffers()
{
	bzero((char *)&temp_struct, sizeof (temp_struct));

	/* Align on longword boundary */
	base_memory =
	    (struct shared_mem *)(((ushort)&temp_struct + 3) & 0xFFFC);

	dnetp->tx_desc = base_memory->tx_desc;
	dnetp->rx_desc = base_memory->rx_desc;
	dnetp->setup_buf_vaddr = base_memory->setup_buf;
	dnetp->max_tx_desc = MAX_TX_DESC;
}

static void
dnet_init_txrx_bufs(gld_mac_info_t *macinfo)
{
	int i;

	for (i = 0; i < MAX_TX_DESC; i++) {
		*(ulong *)&dnetp->tx_desc[i].desc0 = 0;
		*(ulong *)&dnetp->tx_desc[i].desc1 = 0;
		dnetp->tx_desc[i].buffer1 = (paddr_t)(0);
		dnetp->tx_desc[i].buffer2 = (paddr_t)(0);
	}
	dnetp->tx_desc[MAX_TX_DESC-1].desc1.end_of_ring = 1;

	for (i = 0; i < MAX_RX_DESC; i++) {
		*(ulong *)&dnetp->rx_desc[i].desc0 = 0;
		*(ulong *)&dnetp->rx_desc[i].desc1 = 0;
		dnetp->rx_desc[i].desc1.buffer_size1 = RX_BUF_SIZE;
		dnetp->rx_desc[i].buffer1 =
		    DNET_KVTOP(base_memory->rx_buffer[i]);
		dnetp->rx_desc[i].desc0.own = 1;
	}
	dnetp->rx_desc[MAX_RX_DESC-1].desc1.end_of_ring = 1;
}

static int
alloc_descriptor(gld_mac_info_t *macinfo)
{
	register struct tx_desc_type *ring = dnetp->tx_desc;
	register int index = dnetp->tx_current_desc;

	if (dnetp->free_desc <= 0) {
#ifdef DEBUG
		cmn_err(CE_NOTE, "dnet: Ring buffer is full");
#endif
		/* XXX Should we reset now ??? */
		return (FAILURE);
	}

	/* sanity, make sure the next descriptor is free for use (should be) */
	if (ring[index].desc0.own) {
#ifdef DEBUG
		cmn_err(CE_WARN, "dnet:  next descriptor is not free for use");
#endif
		return (FAILURE);
	}

	*(ulong *)&ring[index].desc0 = 0;  /* init descs */
	*(ulong *)&ring[index].desc1 &= DNET_END_OF_RING;

	/* hardware will own this descriptor when poll activated */
	dnetp->free_desc--;

	/* point to next free descriptor to be used */
	dnetp->tx_current_desc = (dnetp->tx_current_desc + 1) % MAX_TX_DESC;

	return (SUCCESS);
}

/*
 * AdapterOpen -- Enable the board
 */
void
AdapterOpen()
{
#ifdef DEBUG
	if (DebugFlag)
		putstr(" dnet AdapterOpen");
#endif
	dnet_start_board(Macinfo);
}

/*
 * AdapterClose -- Reset the board to its power-up state
 */
void
AdapterClose()
{
#ifdef DEBUG
	if (DebugFlag)
		putstr(" dnet AdapterClose");
#endif
	dnet_reset_board(Macinfo);
}

/*
 * AdapterSend -- Send the packet onto the network
 */

void
AdapterSend(char __far *packet, ushort packet_length)
{
	int	current_desc = dnetp->tx_current_desc;
	struct tx_desc_type *desc = dnetp->tx_desc;

#ifdef DEBUG
	    if (DebugFlag)
		putstr(" dnet AdapterSend");
#endif
	/*
	 * reject packet length greater than maximum allowed
	 */
	if (packet_length > ETHERMAX)
		return;

	/*
	 * pad if the packet length is less than minimum allowed
	 */
	if (packet_length < ETHERMIN)
		packet_length = ETHERMIN;

	if ((alloc_descriptor(Macinfo)) == FAILURE) {
#ifdef DEBUG
		cmn_err(CE_WARN, "DNET send:alloc descriptor failure");
#endif
		return;
	}

	ASSERT(packet_length <= TX_BUF_SIZE);	/* fits in one packet */

	BCOPY((caddr_t)packet,
	    (caddr_t)(base_memory->tx_buffer[current_desc]), packet_length);

	desc[current_desc].desc1.first_desc = 1;
	desc[current_desc].desc1.last_desc = 1;
	desc[current_desc].desc1.int_on_comp = 1;
	desc[current_desc].desc1.buffer_size1 = packet_length;
	desc[current_desc].buffer1 =
	    DNET_KVTOP(base_memory->tx_buffer[current_desc]);

	desc[current_desc].desc0.own = 1;

	/*
	 * Demand polling by the chip
	 */
	outl(dnetp->io_reg + TX_POLL_REG, TX_POLL_DEMAND);
}

/*
 * AdapterReceive -- Set up the driver to receive a packet when one
 * later arrives.
 */
void
AdapterReceive(unchar __far *buffer, ushort buflen,
    void (__far *callback)(ushort))
{
#ifdef DEBUG
	    if (DebugFlag)
		putstr(" dnet AdapterReceive");
#endif
	/*
	 * Update global variables with the packet buffer pointer , packet
	 * buffer length and the function to call when packet reception
	 * is complete. Also update a global variable to indicate to the ISR
	 * that the driver can receive packets.
	 */
	Rx_buf = buffer;
	Rx_len = buflen;
	Rx_Callback = callback;
	Rx_Enable = 1;
}

/*
 * AdapterISR -- Entry point for the device's interrupts
*/
void __far interrupt
AdapterISR()
{
	intr_disable();
	set_ds();
	AdapterInterrupt();
	intr_enable();
}

/*
 * AdapterInterrupt -- Service the device's Interrupts
*/
void
AdapterInterrupt()
{
	ulong int_status;

#ifdef DEBUG
	if (DebugFlag)
		putstr(" dnet AdapterInterrupt");
#endif

	int_status = inl(dnetp->io_reg + STATUS_REG);
	/* **** If interrupt was not from this board **** */
	if (!(int_status & (NORMAL_INTR_SUMM | ABNORMAL_INTR_SUMM))) {
#ifdef DEBUG
		if (DebugFlag)
			putstr(" Intr isn't ours!");
#endif
		return;
	}

	/* **** Check for transmit complete interrupt **** */
	if (int_status & TX_INTR)
		outl(dnetp->io_reg + STATUS_REG, int_status | TX_INTR);

	/* Look for tx descriptors we can reclaim */
	while (((dnetp->free_desc == 0) ||
	    (dnetp->transmitted_desc != dnetp->tx_current_desc)) &&
	    !(dnetp->tx_desc[dnetp->transmitted_desc].desc0.own)) {
		dnetp->free_desc++;
		dnetp->transmitted_desc =
		    (dnetp->transmitted_desc + 1) % MAX_TX_DESC;
	}

	/* **** Check for receive completed **** */
	if (int_status & RX_INTR) {
		outl(dnetp->io_reg + STATUS_REG, int_status | RX_INTR);
		dnet_getp();		/* Read in the packet */
	}

	if (int_status & ABNORMAL_INTR_SUMM) {
		/* **** Check for system error **** */
		if (int_status & SYS_ERR) {
			putstr(" dnet System Error");
		}
		if (int_status & TX_JABBER_TIMEOUT) {
			putstr(" dnet Jabber Timeout");
		}
		if (int_status & TX_UNDERFLOW) {
			putstr(" dnet tx underflow");
		}
		dnet_reset_board(Macinfo);
		dnet_init_board(Macinfo);
		dnet_start_board(Macinfo);
	}

	/*
	 *  Generate EOI to the interrupt controller
	 */
	outb(0x20, 0x20);
	if (Inetboot_IRQ_no > 7)
		outb(0xa0, 0x20);

}

static void
dnet_getp()
{
	struct rx_desc_type *desc = dnetp->rx_desc;
	int packet_length, index;

	/*
	 * While host owns the current descriptor
	 */
	while (!(desc[dnetp->rx_current_desc].desc0.own)) {
		index = dnetp->rx_current_desc;
		ASSERT(desc[dnetp->rx_current_desc].desc0.first_desc != 0);
		/*
		 * If we get an oversized packet it could span multiple
		 * descriptors.  If this happens an error bit should be set.
		 */
		while (desc[index].desc0.last_desc == 0) {
			index = (index + 1) % MAX_RX_DESC;
			if (desc[index].desc0.own)
				return;
		}
		while (dnetp->rx_current_desc != index) {
			desc[dnetp->rx_current_desc].desc0.own = 1;
			dnetp->rx_current_desc =
			    (dnetp->rx_current_desc + 1) % MAX_RX_DESC;
#ifdef DEBUG
			cmn_err(CE_WARN, "dnet: received large packet");
#endif
		}

		/*
		 * Get the packet length that includes the CRC
		 */
		packet_length = desc[index].desc0.frame_len;
#ifdef DEBUG
		if (packet_length > ETHERMAX + 4) {
			cmn_err(CE_WARN, "dnet: large packet size %d",
			    packet_length);
		}
#endif

		packet_length = desc[index].desc0.frame_len;

		/*
		 * Error bit set in last descriptor, or not ready to receive
		 */
		if (desc[index].desc0.err_summary || !Rx_Enable) {
			/*
			 * Reset ownership to chip; discard the received packet
			 */
			desc[index].desc0.own = 1;
			dnetp->rx_current_desc =
			    (dnetp->rx_current_desc + 1) % MAX_RX_DESC;
			outl(dnetp->io_reg + RX_POLL_REG, RX_POLL_DEMAND);
			continue;
		}

		/*
		 * Store the incoming packets in the global buffer
		 */
		BCOPY((caddr_t)(base_memory->rx_buffer[index]),
		    (caddr_t)Rx_buf, packet_length);

		desc[index].desc0.own = 1;
		dnetp->rx_current_desc =
		    (dnetp->rx_current_desc + 1) % MAX_RX_DESC;
		outl(dnetp->io_reg + RX_POLL_REG, RX_POLL_DEMAND);

		/*
		 * Reset global flag indicating 'accept packets'
		 */
		Rx_Enable = 0;

		/*
		 * Allow upper layer to process the packet
		 */
		Rx_Callback((int)packet_length);
	}
}


static void
cmn_err(int level, char *format, ...)
{
#ifndef DEBUG
	if (*format == '!')
		return;
#endif
	if (level == CE_NOTE)
		putstr("\nNOTICE: ");
	if (level == CE_WARN)
		putstr("\nWARNING: ");
	__asm {
		pop bp /* destroy cmn_err's stack frame */
		pop ax /* Return address to caller */
		add sp, 2 /* Delete parameter 'level' */
		push ax /* Replace return address on stack */
		jmp printf
	}
}

static void
bzero(char *buffer, int count)
{
	int i;

	for (i = 0; i < count; i++)
		buffer[i] = '\0';
}
