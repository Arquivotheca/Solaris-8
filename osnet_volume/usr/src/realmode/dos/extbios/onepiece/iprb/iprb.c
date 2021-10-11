/*
 * Copyright (c) 1995, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)iprb.c	1.6	99/07/19 SMI"
/*
 * iprb --	Intel PRO100/B Fast Ethernet  Real Mode Driver
 */
// VA edited on 06/30/1999 for 82559 enahncements and to fix
// a bug exposed by Lancewood system.
// Version 0.001.0
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char unchar;
typedef char *caddr_t;

#include <sys/types.h>
#include <befext.h>
#include <common.h>
#include <bef.h>
#include <stdio.h>
#include "rplagent.h"
#include "iprb.h"
#define	REALMODE
#define	drv_usecwait(a) microseconds(a)
#define	drv_usectohz(x)			((u_short)(((u_long)(x)+999)/1000))
#define	ASSERT(expr)  expr ||  \
	printf("Assertion Failure: %s.%d '%s'", __FILE__, __LINE__, #expr)
static void cmn_err(int, char *, ...);
#define	CE_CONT 0
#define	CE_NOTE 1
#define	CE_WARN 2
#include "../mii/mii.h"
#include "../mii/mii.c"

//#define DEBUG

#ifdef DEBUG
//#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
//#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#define	Dprintf(f, x) if (DebugDNet & IPRB_##f) printf x
#define	IPRB_FLOW	0x0001		// ... general flow of program 
#define	IPRB_ERR	0x0002		// ... error conditions 
#define	IPRB_BIOS	0x0004		// ... data on bios calls 
#define	IPRB_FLOW_IRQ	0x0008	// ... info. during interrupts 
#define	IPRB_ALL	-1			// ... enable all flags 
int	DebugDNet = IPRB_ALL;
#else
#define	Dprintf(f, x)
#endif
//#if defined(DEBUG) || defined(DEBUG1) || defined(DEBUG2) || \
//	defined(DEBUG3) || defined(DEBUG4) || defined(DEBUG5) || \
//	defined(DEBUG6)
//void PAK(void) {
//	putstr("\r\nPRESS A KEY\r\n");
//	kbchar();
//	putstr("\r\n");
//}
//#endif
/*
 * Exported global variables (used by external framework)
*/
InstallSpace(Name, 1);
InstallSpace(Slot, 1);
InstallSpace(Port, 1);
InstallSpace(Irq, 1);
InstallSpace(Mem, 1);
InstallSpace(Dma, 1);
extern int Network_Traffic;	/* to identify phase of booting */
extern int Media_Type;		/* ethernet or token ring */

static unchar iprb_ident[9] = "Pro100B";
char ident[9] = "Pro100B";
char driver_name[] = "iprb";
ushort Inetboot_IO_Addr = 0;	/* correspond to reg=0,0,0 in iprb.conf file */
ushort Inetboot_Mem_Base = 0;
unchar initial_cmd = 1;

unchar MAC_Addr[6] = {0};

unchar _far *Rx_Buffer = (unchar _far *)0;
ushort Rx_Buf_Len = 0;
int Listen_Posted = FALSE;
recv_callback_t Call_Back = (recv_callback_t)-1;

volatile struct iprb_rfd rfds[IPRB_NFRAMES] = {0};
volatile struct iprb_xmit_cmd xmits[IPRB_XMITS] = {0};
volatile struct iprb_cfg_cmd config = {0};
volatile struct iprb_ias_cmd iaddr = {0};
short iprb_irq = 0;
short xmit_current = 0;
short xmit_first = -1;
short xmit_last = 0;
short receive_current = 0;
short receive_last = 0;
short receive_first = 0;
long tmp_ioaddr = 0;
short iprb_ioaddr = 0;  
short iprb_eeprom_size = 0;
unsigned char iprb_type = IPRB_MII;

int DebugFlag = -1;

unsigned long inl();
int AdapterProbe(short);
void AdapterIdentify(short, ushort *, ushort *, ushort *, ushort *);
int AdapterInit(ushort, ushort, ushort, ushort);
void AdapterOpen(void);
void AdapterClose(void);
void AdapterSend(char _far *, ushort);
void AdapterReceive(unchar _far *, ushort, recv_callback_t func);
void _far interrupt AdapterISR(void);
void AdapterInterrupt();
void iprb_process_recv();
void iprb_readia(unsigned short *, unsigned short);
static ushort iprb_mdi_read(void *, int, int);
static void iprb_mdi_write(void *, int, int, int); 
static unsigned short iprb_get_eeprom_size();
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
 * MAX_ADAPTERS is used to indicate the maximum number of iprb PCI
 * adapters in the system.
*/

#define	MAX_ADAPTERS	16

/*
 * InstallConfig -- setup internal structures for card at given resources
 *			0 = okay, !0 = failure
*/
InstallConfig(void)
{
	DWORD	val[3], len;
	int	Idx, Reg;
	ushort	id = 0;
	unchar	cmd, irq;
	ulong	ioaddr;

	Dprintf(FLOW, ("InstallConfig(0x%x, %d)", (ushort)Port_Space[0],
	    (ushort)Irq_Space[0]));

	len = AddrTupleSize;
	if (get_res("addr", val, &len) != RES_OK)
		return (1);

	/*
	 * XXX what's the max number of PCI instances?
	 */
	for (Idx = 0; Idx < MAX_ADAPTERS; Idx++) {
		if (!pci_find_device(IPRB_VENID, IPRB_DEVID,
				    Idx, &id)) {
			/*
			 * This case will on occur if we've looked at all
			 * of the boards for this controller in the system
			 * and they don't match the resources passed in
			 * by the framework. This is a major error!
			 */
			Dprintf(ERR, ("%d instances searched, no boards\n",
			    Idx));
			return (1);
		}

		if ((ushort)val[0] == id)
			break;
	}

	if (Idx == MAX_ADAPTERS) {
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
		 * Bus Mastering. It appears that various bios's turn off the
		 * BME bit during P.O.S.T. and certain controllers need to have
		 * this enabled.
		*/
		Dprintf(FLOW, ("Enabling the BME bit\n"));
		pci_write_config_word((ushort)val[0], 0x04, Reg | 4);
	}

	pci_read_config_byte(id, PCI_CONF_COMM, &cmd);
	pci_read_config_byte(id, PCI_CONF_ILINE, &irq);

	/* Check the board IRQ agains't boot system one */
	if ((irq != (ushort)Irq_Space[0])) {
		Dprintf(ERR, ("Bad irq vals: Board 0x%X, Irq 0x%X\n",
		    irq, Irq_Space[0]));
		return (1);
	}

	pci_read_config_dword(id, PCI_CONF_BASE1, &ioaddr);

	/* Check board ioaddr agains't boot system */
	if ((ioaddr & 0xfffc) != (ushort)Port_Space[0]) {
		Dprintf(ERR, ("Bad port vals: Board 0x%lx, Port 0x%lx\n",
		    ioaddr & 0xfffc, Port_Space[0]));
		return (1);
	}

	net_dev_pci(PCI_COOKIE_TO_BUS(id), IPRB_VENID,
		    IPRB_DEVID, PCI_COOKIE_TO_DEV(id),
		    PCI_COOKIE_TO_FUNC(id));

	(ushort)iprb_ioaddr = (ushort)Port_Space[0];
	(ushort)iprb_irq = (ushort)Irq_Space[0];

	Dprintf(FLOW, ("(OK)"));
	return (0);
}

/*
 * Probe whether there is a recognized adapter at position "index".
 * Returns:
 *	ADAPTER_FOUND		if found
 *	ADAPTER_NOT_FOUND	if not found
 *	ADAPTER_TABLE_END	if this is the last logical "index" number
*/

int
AdapterProbe(short index)
{
	ushort	port;
	int	i;
	int	r;
	unchar cmdreg;
	ushort id = 0;

	if (index < 0 || index >= MAX_ADAPTERS)
		return (ADAPTER_TABLE_END);

	if (is_pci() == 0)
		return (ADAPTER_NOT_FOUND);

#ifdef DEBUG
	if (DebugFlag) {
		putstr("AdapterProbe:");
	}
#endif

	r = pci_find_device(IPRB_VENID, IPRB_DEVID, index, &id);

	if (!r)
		return (ADAPTER_TABLE_END);

	pci_read_config_byte(id, PCI_CONF_COMM, &cmdreg);
	pci_read_config_byte(id, PCI_CONF_ILINE, &(unchar)iprb_irq);
	if ((iprb_irq > 15) || (iprb_irq == 0)) {
#ifdef DEBUG6
		if (DebugFlag) {
			putstr("irq not valid");
			putstr("\r\n");
		}
#endif
		return (ADAPTER_NOT_FOUND);
	}

	net_dev_pci(PCI_COOKIE_TO_BUS(id), IPRB_VENID, IPRB_DEVID,
		    PCI_COOKIE_TO_DEV(id), PCI_COOKIE_TO_FUNC(id));

	pci_read_config_dword(id, PCI_CONF_BASE1, &tmp_ioaddr);
	iprb_ioaddr = (short)tmp_ioaddr & 0xfffc;

	return (ADAPTER_FOUND);
}


/*
 * This code relies upon the framework calling AdapterIdentify after
 * Adapter Probe
*/

void
AdapterIdentify(short index, ushort *ioaddr, ushort *irq, ushort *rambase,
	ushort *ramsize)
{


#ifdef DEBUG
	if (DebugFlag) {
		putstr("AdapterIdentify:");
	}
#endif
	*ioaddr = (short)iprb_ioaddr;
	*irq = (short)iprb_irq;
	*rambase = 0;
	*ramsize = 0;
}

int
AdapterInit(ushort ioaddr, ushort irq, ushort rambase, ushort ramsize)
{
	mii_handle_t mii_handle;
	int	i;
	volatile struct iprb_rfd *rfd, *prev_rfd;
	volatile struct iprb_xmit_cmd *xmit, *prev_xmit;
	volatile struct iprb_cfg_cmd *cfg;
	volatile struct iprb_ias_cmd *ias;
	long	times = 1000;

	char	_far *addr;
	unsigned long	raddr, seg, off;
	unsigned short	mdicreg, mdisreg;

	iprb_hard_reset();
	microseconds(100);
	/* LANCEWOOD FIX 
	 * VA edited on 06/30/1999 
	 * With out this fix Lancewood machines
	 * will stay in hang state while booting off
	 * the network
	 */ 
	outl(iprb_ioaddr + IPRB_SCB_PTR, 0);
    outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_LOAD_CUBASE);
    IPRB_SCBWAIT();
    drv_usecwait(100);
    outl(iprb_ioaddr + IPRB_SCB_PTR, 0);
    outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_LOAD_RUBASE);
    IPRB_SCBWAIT();
	drv_usecwait(100);
	/*
	 * 82559 support: get the size of the eeprom
	 */
	(void)iprb_get_eeprom_size();
	prev_rfd = &(rfds[IPRB_NFRAMES - 1]);
	prev_rfd->rfd_bits = IPRB_RFD_EL;

	for (i = 0; i < IPRB_NFRAMES; i++) {
		rfd = &(rfds[i]);
		addr = (char _far *)&(rfds[i]);
		seg = FP_SEG(addr);
		off = FP_OFF(addr);
		raddr = (seg << 4) + off;
		prev_rfd->rfd_next = raddr;
		rfd->rfd_size = IPRB_FRAMESIZE;
		prev_rfd = rfd;
	}

	prev_xmit = &(xmits[IPRB_XMITS - 1]);
	for (i = 0; i < IPRB_XMITS; i++) {
		xmit = &(xmits[i]);
		addr = (char _far *)&(xmits[i]);
		seg = FP_SEG(addr);
		off = FP_OFF(addr);
		raddr = (seg << 4) + off;
		prev_xmit->xmit_next = raddr;
		xmit->xmit_cmd = IPRB_XMIT_CMD;
		prev_xmit = xmit;
	}

	receive_first = 0;
	receive_last = IPRB_NFRAMES - 1;
	receive_current = 0;
	xmit_first = -1;
	xmit_last = 0;
	xmit_current = 0;

	ias = &iaddr;

	for (i = 0; i < (ETHERADDRL / sizeof (short)); i++) {
		(void) iprb_readia((ushort *)&(MAC_Addr[i * sizeof (short)]),
								(ushort)i);
	}

	ias->ias_cmd = IPRB_IAS_CMD | IPRB_CMD_EL;
	ias->ias_next = 0xffffffff;
    
	for (i = 0; i < ETHERADDRL; i++){
		ias->addr[i] = MAC_Addr[i];  
	} 

	addr = (char _far *)&iaddr;
	seg = FP_SEG(addr);
	off = FP_OFF(addr);
	raddr = (seg << 4) + off;

	outl(iprb_ioaddr + IPRB_SCB_PTR, raddr);
	outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_CU_START);
	IPRB_IASCMDCMPL(ias);

	mii_create(NULL, iprb_mdi_write, iprb_mdi_read, &mii_handle);

	if (mii_probe_phy(mii_handle, 1) == MII_SUCCESS) {
		mii_init_phy(mii_handle, 1);
		mii_disable_fullduplex(mii_handle, 1);
		iprb_type = IPRB_MII;
	}

	cfg = &config;
	cfg->cfg_cmd = IPRB_CFG_CMD | IPRB_CMD_EL;
	cfg->cfg_next = 0xffffffff;

	cfg->cfg_byte0 = IPRB_CFG_B0;
	cfg->cfg_byte1 = IPRB_CFG_B1;
	cfg->cfg_byte2 = IPRB_CFG_B2;
	cfg->cfg_byte3 = IPRB_CFG_B3;
	cfg->cfg_byte4 = IPRB_CFG_B4;
	cfg->cfg_byte5 = IPRB_CFG_B5;
	cfg->cfg_byte6 = IPRB_CFG_B6;
	cfg->cfg_byte7 = IPRB_CFG_B7 | IPRB_CFG_B7NOPROM;
	cfg->cfg_byte8 =
	    ((iprb_type == IPRB_MII) ? IPRB_CFG_B8_MII : IPRB_CFG_B8_503);
	cfg->cfg_byte9 = IPRB_CFG_B9;
	cfg->cfg_byte10 = IPRB_CFG_B10;
	cfg->cfg_byte11 = IPRB_CFG_B11;
	cfg->cfg_byte12 = IPRB_CFG_B12;
	cfg->cfg_byte13 = IPRB_CFG_B13;
	cfg->cfg_byte14 = IPRB_CFG_B14;
	cfg->cfg_byte15 = IPRB_CFG_B15;
	cfg->cfg_byte16 = IPRB_CFG_B16;
	cfg->cfg_byte17 = IPRB_CFG_B17;
	cfg->cfg_byte18 = IPRB_CFG_B18;
	cfg->cfg_byte19 = IPRB_CFG_B19;
	cfg->cfg_byte20 = IPRB_CFG_B20;
	cfg->cfg_byte21 = IPRB_CFG_B21;
	cfg->cfg_byte22 = IPRB_CFG_B22;
	cfg->cfg_byte23 = IPRB_CFG_B23;

	addr = (char _far *)&config;
	seg = FP_SEG(addr);
	off = FP_OFF(addr);
	raddr = (seg << 4) + off;
	outl(iprb_ioaddr + IPRB_SCB_PTR, raddr);
	outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_CU_START);
	IPRB_CFGCMDCMPL(cfg);
	intr_enable();  
}

void
AdapterReceive(unchar _far *buffer, ushort buflen, recv_callback_t func)
{
	caddr_t addr;

	Rx_Buffer = buffer;
	Rx_Buf_Len = buflen;
	Call_Back = func;
	Listen_Posted = TRUE;
}
void
AdapterSend(char _far *packet, ushort packetlen)
{
	char	_far *addr;
	unsigned long	raddr, seg, off;
	volatile struct iprb_xmit_cmd *xmit;
	int	i;
	ushort status;
    intr_disable();

	status = inw(iprb_ioaddr + IPRB_SCB_STATUS);

	if (xmit_first != -1) {
		for (i = xmit_first; i <= xmit_last; i++) {
			xmit = &(xmits[i]);

			if (!(xmit->xmit_bits & IPRB_CMD_COMPLETE)) {
				break;
			}

			xmit->xmit_cmd = IPRB_XMIT_CMD;
			xmit->xmit_count = 0;
			xmit->xmit_bits = 0;

		}

		if (i > xmit_last)
			xmit_first = -1;
			else
			xmit_first = i;
	}

	if (xmit_current == xmit_first) {
		putstr("Command buffer full\n");
		return;
	}

	xmit = &(xmits[xmit_current]);
	for (i = 0; i < packetlen; i++)
		xmit->xmit_data[i] = packet[i];

	xmit->xmit_count = packetlen | IPRB_XMIT_EOF;
	xmit->xmit_tbd = (paddr_t)0xffffffff;
	xmit->xmit_tbd_number = 0x64;
	xmit->xmit_cmd |= IPRB_XMIT_SUSPEND;
	if (xmit_current  != xmit_last) {
		xmits[xmit_last].xmit_cmd &= ~(IPRB_XMIT_SUSPEND);
	}
	xmit_last = xmit_current;
	if (xmit_first == -1)
		xmit_first = xmit_current;

	xmit_current++;

	if (xmit_current == IPRB_XMITS)
		xmit_current = 0;
    IPRB_SCBWAIT();

    if (initial_cmd) {
		initial_cmd = 0;
		addr = (char _far *)&(xmits[xmit_last]);
		seg = FP_SEG(addr);
		off = FP_OFF(addr);
		raddr = (seg << 4) + off;
		xmit = &(xmits[xmit_last]);
		outl(iprb_ioaddr + IPRB_SCB_PTR, raddr);
		outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_CU_START);
	} else {
		outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_CU_RESUME);
	}
 	intr_enable();
}

void interrupt
AdapterISR()
{    
	#ifdef DEBUG
	printf("intr_disable in...\n");
	#endif
	intr_disable();
	#ifdef DEBUG
	printf("intr_disable out...\n");
	#endif
	#ifdef DEBUG
	printf("set_ds in...\n");
	#endif
	set_ds();
	#ifdef DEBUG
	printf("set_ds out...\n");
	#endif
	#ifdef DEBUG
	printf("AdapterInterrupt in...\n");
	#endif
	AdapterInterrupt();
	#ifdef DEBUG
	printf("AdapterInterrupt out...\n");
	#endif
	#ifdef DEBUG
	printf("intr_enable in...\n");
	#endif
	intr_enable();
	#ifdef DEBUG
	printf("intr_enable out...\n");
	#endif
	
}

void
AdapterInterrupt(void)
{
	short	status;
	short	intr_status;
	char	_far *addr;
	unsigned long	raddr, seg, off;
    #ifdef DEBUG
    printf("AdapterInterrupt in...\n");
    #endif
	status = inw(iprb_ioaddr + IPRB_SCB_STATUS);
	intr_status = status & IPRB_SCB_INTR_MASK;

	if (!intr_status)
		return;

	outw(iprb_ioaddr + IPRB_SCB_STATUS, intr_status);

	if (intr_status & IPRB_INTR_FR)
		iprb_process_recv();

	if (intr_status & IPRB_INTR_RNR) {
		addr = (char _far *)&(rfds[receive_current]);
		seg = FP_SEG(addr);
		off = FP_OFF(addr);
		raddr = (seg << 4) + off;
		outl(iprb_ioaddr + IPRB_SCB_PTR, raddr);
		outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_RU_START);
	}

	if (iprb_irq > 7)
		outb(0xa0, 0x20);
	outb(0x20, 0x20);
	#ifdef DEBUG
    printf("AdapterInterrupt out...\n");
    #endif
}



/*  Enable receive */

void
AdapterOpen(void)
{
	char	_far *addr;
	unsigned long	raddr, seg, off;

	IPRB_SCBWAIT();

	addr = (char _far *)&(rfds[0]);
	seg = FP_SEG(addr);
	off = FP_OFF(addr);
	raddr = (seg << 4) + off;
	outl(iprb_ioaddr + IPRB_SCB_PTR, raddr);
	intr_enable();
	outw(iprb_ioaddr + IPRB_SCB_CMD, IPRB_RU_START);
}



/* Disable receive */

void
AdapterClose(void)
{
	iprb_hard_reset();
}

iprb_hard_reset()
{
	outl(iprb_ioaddr + IPRB_SCB_PORT, IPRB_PORT_SW_RESET);
}



void
iprb_process_recv()
{
	int	save_end;
	ushort len;
	volatile struct iprb_rfd *rfd, *rfd_end;
	int	i;
    #ifdef DEBUG
    printf("iprb_process_recv in ...\n");
    #endif
	save_end = receive_last;

	do {
		rfd = &(rfds[receive_current]);

		if (!(rfd->rfd_bits & IPRB_RFD_COMPLETE))
			break;

		len = rfd->rfd_count & IPRB_RFD_COUNT_MASK;

		if (Listen_Posted)
			for (i = 0; i < len; i++)
				Rx_Buffer[i] = rfd->rfd_data[i];

		Listen_Posted = FALSE;
		Call_Back(len);

		rfd_end = &(rfds[receive_last]);

		rfd->rfd_bits = IPRB_RFD_EL;
		rfd->rfd_count = 0;
		receive_last = receive_current;
		rfd_end->rfd_bits &= ~(IPRB_RFD_EL);

		receive_current++;
		if (receive_current == IPRB_NFRAMES)
			receive_current = 0;
	} while (receive_last != save_end);
	#ifdef DEBUG
	printf("iprb_process_recv out...\n");
	#endif
	
}

void
iprb_readia(unsigned short *addr, unsigned short offset)
{

	unsigned char	eex;
	// 82559 SM Bus related changes
	// VA edited on 06/28/1999
	// Get actual address length and use it.
	// instead of hardcoded length 6.
	unsigned short	address_length=6;
	#ifdef DEBUG
	printf("iprb_readia in...\n");
	#endif
	address_length = IPRB_EEPROM_ADDRESS_SIZE(iprb_eeprom_size);
    eex = inb(iprb_ioaddr + IPRB_SCB_EECTL);
	eex &= ~(IPRB_EEDI | IPRB_EEDO | IPRB_EESK);
	eex |= IPRB_EECS;
	outb(iprb_ioaddr + IPRB_SCB_EECTL, eex);
	iprb_shiftout(IPRB_EEPROM_READ, 3);
	// VA edited on 06/28/1999 
	// Added address length instead of 6
	iprb_shiftout(offset, address_length);
	*addr = iprb_shiftin();
	iprb_eeclean();
	#ifdef DEBUG
	printf("iprb_readia out...\n");
	#endif
}


iprb_shiftout(unsigned short data, unsigned short count)
{
	unsigned char	eex, mask;
	#ifdef DEBUG
    printf("iprb_shiftout in...\n");
	#endif
	mask = 0x01 << (count - 1);
	eex = inb(iprb_ioaddr + IPRB_SCB_EECTL);
	eex &= ~(IPRB_EEDO | IPRB_EEDI);

	do {
		eex &= ~IPRB_EEDI;
		if (data & mask)
			eex |= IPRB_EEDI;

		outb(iprb_ioaddr + IPRB_SCB_EECTL, eex);
		microseconds(100);
		iprb_raiseclock((unsigned short *) & eex);
		iprb_lowerclock((unsigned short *) & eex);
		mask = mask >> 1;
	} while (mask);

	eex &= ~IPRB_EEDI;
	outb(iprb_ioaddr + IPRB_SCB_EECTL, eex);
	#ifdef DEBUG
	printf("iprb_shiftout out...\n");
	#endif
}


iprb_raiseclock(unsigned char *eex)
{
	*eex = *eex | IPRB_EESK;
	outb(iprb_ioaddr + IPRB_SCB_EECTL, *eex);
	microseconds(100);
}


iprb_lowerclock(unsigned char *eex)
{
	*eex = *eex & ~IPRB_EESK;
	outb(iprb_ioaddr + IPRB_SCB_EECTL, *eex);
	microseconds(100);
}


iprb_shiftin()
{
	unsigned short	d, i;
	unsigned char	x;

	x = inb(iprb_ioaddr + IPRB_SCB_EECTL);
	x &= ~(IPRB_EEDO | IPRB_EEDI);
	d = 0;

	for (i = 0; i < 16; i++) {
		d = d << 1;
		iprb_raiseclock(&x);
		x = inb(iprb_ioaddr + IPRB_SCB_EECTL);
		x &= ~(IPRB_EEDI);
		if (x & IPRB_EEDO)
			d |= 1;

		iprb_lowerclock(&x);
	}

	return (d);
}


iprb_eeclean()
{
	unsigned char	eex;

	eex = inb(iprb_ioaddr + IPRB_SCB_EECTL);
	eex &= ~(IPRB_EECS | IPRB_EEDI);
	outb(iprb_ioaddr + IPRB_SCB_EECTL, eex);

	iprb_raiseclock(&eex);
	iprb_lowerclock(&eex);
}



static ushort
iprb_mdi_read(void *null, int phy_addr, int reg_addr)
{
	unsigned long phy_command = 0;
	unsigned long out_data = 0;
	short foo;
	
    phy_command = (((ulong)(reg_addr)) << 16) |
	    (((ulong)(phy_addr)) << 21) |
	    (((ulong)(IPRB_MDI_READ)) << 26);

	outl(iprb_ioaddr + IPRB_SCB_MDI, phy_command);

	while (!((out_data = inl(iprb_ioaddr + IPRB_SCB_MDI)) &
		    IPRB_MDI_READY))
	    microseconds(100);

	return (out_data & 0x0000ffff);
}


static void
iprb_mdi_write(void *null, int phy_addr, int reg_addr, int data)
{
	unsigned long command;
	unsigned char timeout = 4;
	
   	command = (ulong)data | ((ulong)reg_addr<<16) |
	    ((ulong)phy_addr<<21) | (1UL<<26);

	outl(iprb_ioaddr + IPRB_SCB_MDI, command);

	while ((--timeout) &&
	    (!((inl(iprb_ioaddr + IPRB_SCB_MDI)) & IPRB_MDI_READY)))
		microseconds(100);

	if (timeout == 0)
		cmn_err(CE_WARN, "!iprb: timeout writing MDI frame");
}


static void cmn_err(int level, char *format, ...)
{
}
   
// Vinay's real mode changes begin...
// Jun 28, 1999
//
static unsigned short
iprb_get_eeprom_size()
{
	unsigned char eex;		/* This is the manipulation bit */
	unsigned short size = 1; 	/* Size must be initialized to 1*/
	/* The algorithm used to enable is dummy zero mechanism
	* From the 82558/82559 Technical Reference Manual
	* 1.   First activate EEPROM by writing a '1' to the EECS bit
	* 2.   Write the read opcode including the start bit (110b), 
	* 	  one bit at a time, starting with the Msb('1'):
	* 2.1. Write the opcode bit to the EEDI bit
	* 2.2. Write a '1' to EESK bit and then wait for the minimum SK
	* 	  high time
	* 2.3. Write a '0' to EESK bit and then wait for the minimum SK
	* 	  low time.
	* 2.4. Repeat steps 3 to 5 for next 2 opcode bits
	* 3.   Write the address field, one bit at a time, keeping track
	* 	  of the number of bits shifted in, starting with MSB.
	* 3.1  Write the address bit to the EEDI bit
	* 3.2  Write a '1' to EESK bit and then wait for the minimum SK
	* 	  SK high time
	* 3.3. Write a '0' to EESK bit and then wait for the minimum SK
	* 	  low time.
	* 3.4  Read the EEDO bit and check for "dummy zero bit"
	* 3.5  Repeat steps 3.1 to 3.4 unttk the EEDO bit is set to '0'.
	* 	  The number of loop operations performed will be equal to
	* 	  number of bits in the address field.
	* 4.	  Read a 16 bit word from the EEPROM one bit at a time, starting
	* 	  with the MSB, to complete the transaction. 
	* 4.1  Write a '1' to EESK bit and then wait for the minimum SK
	* 	  high time
	* 4.2  Read data bit from the EEDO bit
	* 4.3  Write a '0' to EESK bit and then wait for the minimum SK
	* 	  low time.
	* 4.4  Repeat steps 4.1 to 4.3 for next 15 times
	* 5.	  Deactivate the EEPROM by writing a 0 codeto EECS bit
	* 	  VINAY edited on 2/21/99 
	* 	  To support 82558/82559 related requirements...
	*/

	eex = (unsigned short) inb(iprb_ioaddr + IPRB_SCB_EECTL);

	eex &= ~(IPRB_EEDI | IPRB_EEDO | IPRB_EESK);
	eex |= IPRB_EECS;
	outb(iprb_ioaddr + IPRB_SCB_EECTL, eex);

	/* Write the read opcode */
	iprb_shiftout(IPRB_EEPROM_READ, 3);

	/*
	 * experiment to discover the size of the eeprom.  request register
	 * zero and wait for the eeprom to tell us it has accepted the entire 
	 * address.
	 */

	eex = (unsigned short)inb(iprb_ioaddr + IPRB_SCB_EECTL);
	do
	{
		/* each bit of address doubles eeprom size */
		size *= 2;            

		eex |= IPRB_EEDO;     /* set bit to detect "dummy zero" */
		eex &= ~IPRB_EEDI;    /* address consists of all zeros */
		outb(iprb_ioaddr + IPRB_SCB_EECTL, eex);
		drv_usecwait(100);
		iprb_raiseclock(&eex);
		iprb_lowerclock(&eex);

		/* check for "dummy zero" */
		eex = (unsigned short)inb(iprb_ioaddr + IPRB_SCB_EECTL);
		if (size > 256)
		{
			size = 0;
			break;
		}
	} while (eex & IPRB_EEDO);

	/* read in the value requested */
	iprb_shiftin();
	iprb_eeclean();
	iprb_eeprom_size = size;
	return size;
}
