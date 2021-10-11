/*
 *  Copyright (c) 1997, by Sun Microsystems, Inc.
 *  All rights reserved.
 */

#ident "@(#)pcn.c	1.20	97/10/20 SMI"


/*
 * Solaris Primary Boot Subsystem - Realmode Driver
 * ===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: Advanced Micro Devices PC-Net (pcn.c)
 *
 */

/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This driver is intended to be both a working realmode driver and
 *	a sample network realmode driver for use as a guide to writing other
 *	network realmode drivers.  Where comments in this file are intended
 *	for guidance in writing other realmode drivers they start and end
 *	with SAMPLE. SunSoft personnel should keep this in mind when updating
 *	the file for either purpose.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */

/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	The hardware-specific code for a network realmode driver must define
 *	a stack (by supplying variables "stack" and "stack_size") and an
 *	initialization routine (network_driver_init).  All other communication
 *	is based on data filled in by network_driver_init.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */

#include <rmscnet.h>
#include "pcn.h"



/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	For most real mode drivers 1000 words of stack is plenty.  This
 *	definition can be changed locally or overridden from the makefile
 *	for drivers that require more.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
#ifndef STACKSIZE
#define	STACKSIZE 1000
#endif

ushort stack[STACKSIZE];
ushort stack_size = sizeof (stack);



/*
 * Various classes of messages can be turned on by setting flags in
 * net_debug_flag.  Flags can be set by changing the definition of
 * PCN_DEBUG_FLAG below, by setting PCN_DEBUG_FLAG from the compiler
 * command line, or by writing net_debug_flag using a debugger.
 */
#ifdef DEBUG

#pragma	message (__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment (user, __FILE__ ": DEBUG ON " __TIMESTAMP__)

/* pcn-specific flags.  See rmsc.h for common flags */
#define	DBG_FLOW	0x0001	/* enable messages for flow of program */
#define	DBG_FLOW_IRQ	0x0002	/* enable messages during interrupts */

#ifndef PCN_DEBUG_FLAG
#define	PCN_DEBUG_FLAG	DBG_ERRS
#endif

Static int pcn_debug_flag = PCN_DEBUG_FLAG;
#define	MODULE_DEBUG_FLAG pcn_debug_flag

#endif



/*
 *	Function prototypes for local routines made visible to the network
 *	layer by network_driver_init.
 */
Static	int	pcn_adapter_init(rmsc_handle, unchar *);
Static	void	pcn_close(void);
Static	int	pcn_configure(rmsc_handle *, char **);
Static	void	pcn_device_interrupt(void);
Static	void	pcn_legacy_probe(void);
Static	void	pcn_open(void);
Static	void	pcn_receive_packet(unchar far *, ushort, recv_callback_t);
Static	void	pcn_send_packet(unchar far *, ushort);



/*
 *	Function prototypes for local routines called only from other local
 *	routines.
 */
Static	ushort	pcn_csr_read(ushort);
Static	void	pcn_csr_write(ushort, ushort);
Static	int	pcn_find_iob(ushort);
Static	int	pcn_get_pic_irr(ushort);
Static	void	pcn_init_data(void);
Static	int	pcn_isa_get_irq(short, short *);
Static	int	pcn_isa_probe(short);
Static	void	pcn_lance_poke(ushort);
Static	void	pcn_lance_reset(void);
Static	void	pcn_lance_stop(void);
Static	void	pcn_make_desc(struct pcn_msg_desc *, ulong, ushort, ushort);
Static	ulong	pcn_off_to_lin(void *ptr);
Static	int	pcn_process_receive(int);
Static	void	pcn_receive_isr(void);
Static	void	pcn_setup_adapter(void);
Static	void	pcn_rxdesc_reset(struct pcn_msg_desc *);
#if	defined(DEBUG)
Static	void	pcn_show_frame(unchar *, ushort);
#endif  /* defined(DEBUG) */



/*
 *	Local variables used by the hardware-specific code.
 */

/*
 *	Default entries in this array are for ISA bus devices.  If a PCI bus
 *	device is discovered with the same iobase as one of the ISA entries,
 *	then the ISA entry is re-written to describe the PCI entry. The
 *	remaining slots can be used by devices that are autoconfigured.
 */
Static struct pcnIOBase pcn_iobase[PCN_IOBASE_ARRAY_SIZE] = {
	{ 0x300, PCN_BUS_ISA, 0 },
	{ 0x320, PCN_BUS_ISA, 0 },
	{ 0x340, PCN_BUS_ISA, 0 },
	{ 0x360, PCN_BUS_ISA, 0 }
};

/*
 *	These tables list the possible IRQ and DMA channels used by PC-Net
 *	ISA devices.
 */
Static ushort   pcn_irq_map[] = { 3, 4, 5, 9, 10, 11, 14, 15 };
Static ushort   pcn_dma_map[] = { 5, 3, 6, 7};

Static char pcns_arena[sizeof (pcn_instance) + 8];
Static pcn_instance *pcnp;
Static volatile int pcn_init_done;

/*
 *	Receiver variables
 */
Static unchar far	*Rx_Buf;
Static ushort		Rx_Len;
Static recv_callback_t	Rx_Callback;
Static int		Rx_Enable;



/*
 *	Driver initialization entry point.
 *
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	The primary purpose of this routine is to provide information to
 *	the network framework layer.  Other driver initialization is permitted
 *	here provided that it does not involve accessing the device.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
int
network_driver_init(struct network_driver_init *p)
{
	Dprintf(DBG_INIT, ("Entering network_driver_init\n"));

	/*
	 * The PCN_Net hardware expects the pcn_instance structure
	 * to be on an 8-byte boundary.  Find an aligned location
	 * within the pcns_arena array.
	 */
	pcnp = (pcn_instance *)((ushort)(pcns_arena + 7) & 0xFFF8);

	p->driver_name = "pcn";
	p->legacy_probe = pcn_legacy_probe;
	p->configure = pcn_configure;
	p->initialize = pcn_adapter_init;
	p->device_interrupt = pcn_device_interrupt;
	p->open = pcn_open;
	p->close = pcn_close;
	p->send_packet = pcn_send_packet;
	p->receive_packet = pcn_receive_packet;

	Dprintf(DBG_INIT,
		("Leaving network_driver_init, returning BEF_OK\n"));

	return (BEF_OK);
}



/*
 *	Hardware-specific legacy probe routine.
 *
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	To avoid probe conflicts the driver must reserve resources
 *	before using them during the probe. Once it finds an adapter
 *	other devices will be prevented from accessing the resources
 *	reserved for this device.  If it does not find an adapter after
 *	reserving resources for the probe it must release the resources.
 *	Calling node_op(NODE_FREE) releases all resources reserved for
 *	the current node.  Individual resources can be released by
 *	calling rel_res.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
Static void
pcn_legacy_probe(void)
{
	short	i, j;
	ushort	port, irq;
	DWORD	val[3], len;

	Dprintf(DBG_PROBE, ("Entering pcn_legacy_probe\n"));

	for (i = 0; i < sizeof (pcn_iobase) / sizeof (pcn_iobase[0]); i++) {

		/* We are only looking for ISA devices */
		if (pcn_iobase[i].bustype != PCN_BUS_ISA)
			continue;

		/* Get the possible base address */
		port = pcn_iobase[i].iobase;
		pcnp->iobase = port;    /* required by low-level routines */

		Dprintf(DBG_PROBE, ("Checking port %x\n", port));

		/*
		 * Notify framework that we're going to need space to store
		 * some information.
		 */
		if (node_op(NODE_START) != NODE_OK) {
			Dprintf(DBG_PROBE | DBG_ERRS,
				("node_op(NODE_START) failed\n"));
			break;
		}

		/* Attempt to reserve the I/O address range */
		val[0] = port; val[1] = PCN_ISA_PORT_WIDTH; val[2] = 0;
		len = PortTupleSize;
		if (set_res("port", val, &len, 0)) {
			node_op(NODE_FREE);
			Dprintf(DBG_PROBE,
				("set_res for \"port\" %x failed\n", port));
			continue;
		}

		/* Determine whether the device is present */
		if (pcn_isa_probe(port) != BEF_OK) {
			node_op(NODE_FREE);
			Dprintf(DBG_PROBE, ("pcn_isa_probe failed\n"));
			continue;
		}

		/*
		 *	Reserve the DMA channel used for this device.
		 *
		 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
		 *	================================================
		 *	In most drivers that use DMA it should be possible
		 *	to read the DMA channel number directly from a
		 *	register.  For the PC-Net device we have to try
		 *	each DMA channel until we find the right one.  We
		 *	have to reserve each DMA channel before trying it
		 *	and release it if it is not the right one.
		 *	================================================
		 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
		 */
		for (j = 0; j < sizeof (pcn_dma_map) / sizeof (pcn_dma_map[0]);
					j++) {

			/* Try to reserve the next DMA channel in the table */
			val[0] = pcnp->dma = pcn_dma_map[j]; val[1] = 0;
			len = DmaTupleSize;
			if (set_res("dma", val, &len, 0) != RES_OK) {
				/* DMA channel is used by another device */
				Dprintf(DBG_PROBE,
					("DMA Channel %x reserved\n",
					pcnp->dma));
				continue;
			}

			/* Test whether the device uses this DMA channel */
			if (pcn_check_dma(pcnp->dma) != BEF_OK) {
				/* Device does not use this channel */
				rel_res("dma", val, &len);
				Dprintf(DBG_PROBE,
					("DMA Channel %x not used\n",
					pcnp->dma));
				continue;
			}

			/* Correct channel identified and reserved */
			Dprintf(DBG_PROBE, ("DMA Channel %x used\n",
					pcnp->dma));
			break;
		}

		/* Check whether we found the DMA channel */
		if (j >= sizeof (pcn_dma_map) / sizeof (pcn_dma_map[0])) {
			node_op(NODE_INCOMPLETE);
			Dprintf(DBG_PROBE | DBG_ERRS,
				("Failed to identify DMA channel\n"));
			continue;
		}

		/*
		 *	Identify the IRQ used for this device.
		 */
		if (pcn_isa_get_irq(port, &irq) != BEF_OK) {
			node_op(NODE_FREE);
			Dprintf(DBG_PROBE | DBG_ERRS,
				("Failed to identify IRQ level\n"));
			continue;
		}

		/*
		 *	Reserve the IRQ used for this device.
		 *
		 *	Versions of Solaris before 2.6 allowed network
		 *	drivers to work even though they used IRQ 3
		 *	which conflicts with common serial port usage.
		 *	This driver uses a private extension to the realmode
		 *	driver interface definition (RES_USURP) to enable
		 *	older Solaris systems to be upgraded to 2.6
		 *	without requiring hardware reconfiguration.  The
		 *	use of RES_USURP in new drivers is discouraged
		 *	and support for this flag is not guaranteed in
		 *	future releases of Solaris.
		 */
		val[0] = irq; val[1] = 0; val[2] = 0;
		len = IrqTupleSize;
		if (set_res("irq", val, &len, irq == 3 ? RES_USURP : 0)) {
			node_op(NODE_INCOMPLETE);
			Dprintf(DBG_PROBE, ("set_res for \"irq\" failed\n"));
			continue;
		}

		/*
		 *	PC-Net devices do not map any memory.
		 *
		 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
		 *	================================================
		 *	Some network devices map on-board memory into the
		 *	memory address space of the PC.  If you are writing
		 *	a driver that maps memory you will need to reserve
		 *	the mapped region using set_res with resource "mem".
		 *	See the realmode DDI specification for more details.
		 *	================================================
		 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
		 */

		node_op(NODE_DONE);
		Dprintf(DBG_PROBE,
			("Finished creating node for device at %x\n", port));
	}

	Dprintf(DBG_PROBE, ("Leaving pcn_legacy_probe\n"));
}



/*
 *	Hardware-specific device configuration routine.
 *
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This routine should determine whether the device described by the
 *	active node is installed and supply a device handle and identification
 *	string if it is installed.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
Static	int
pcn_configure(rmsc_handle *handle, char **ident)
{
	DWORD	res_space[MaxTupleSize];
	DWORD	len;
	ushort	index,		/* ... used to find matching iobase */
		iobase,		/* ... setup */
		cookie,		/* ... pci id value */
		irq_level,	/* ... interrupt level (fom config) */
		irq,		/* ... interrupt level (from device) */
		dma_channel,	/* ... dma channel (ISA only) */
		reg;		/* ... location for reading config info */
	unchar	pci_irq;

	Dprintf(DBG_CONFIG, ("Entering pcn_configure\n"));

	/* Get I/O base address from hardware node */
	len = PortTupleSize;
	if (get_res("port", res_space, &len) != BEF_OK || len < 1) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("get_res failed for \"port\"\n"));
		Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
		return (BEF_FAIL);
	}
	iobase = (ushort)res_space[0];

	Dprintf(DBG_CONFIG, ("Configuring device at port %x\n", iobase));

	/* Find table slot.  Give up if table already full */
	if ((index = pcn_find_iob(iobase)) == -1) {
		Dprintf(DBG_CONFIG | DBG_ERRS, ("Device table is full\n"));
		Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
		return (BEF_FAIL);
	}

	/* Extract irq number from hardware node */
	len = IrqTupleSize;
	if (get_res("irq", res_space, &len) != BEF_OK || len < 1) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("get_res failed for \"irq\"\n"));
		Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
		return (BEF_FAIL);
	}
	irq_level = (ushort)res_space[0];

	/* Extract bus type info from hardware node */
	len = NameTupleSize;
	if (get_res("name", res_space, &len) != BEF_OK || len < 2) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("get_res failed for \"name\"\n"));
		Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
		return (BEF_FAIL);
	}

	switch (res_space[1]) {
	case RES_BUS_ISA:
	case RES_BUS_PNPISA:
		/* Extract DMA channel number from hardware node */
		len = DmaTupleSize;
		if (get_res("dma", res_space, &len) != BEF_OK || len < 2) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("get_res failed for \"dma\"\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}
		dma_channel = (ushort)res_space[0];

		/*
		 * Verify that device is present and uses the reserved DMA
		 * channel and interrupt level.
		 */
		pcnp->iobase = iobase;    /* required by low-level routines */
		if (pcn_isa_probe(iobase) != BEF_OK) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("Device not found at port %x\n",
					iobase));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}
		if (pcn_check_dma(dma_channel) != BEF_OK) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("DMA channel config error\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}
		if (pcn_isa_get_irq(iobase, &irq) != BEF_OK ||
				irq != irq_level) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("IRQ config error\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}

		/*
		 * Record the config info in the device table.
		 */
		pcn_iobase[index].iobase = iobase;
		pcn_iobase[index].bustype = PCN_BUS_ISA;
		pcn_iobase[index].cookie = 0;
		pcn_iobase[index].irq = irq_level;
		pcn_iobase[index].dma = dma_channel;
		break;
	
	case RES_BUS_PCI:
		len = 1;
		if (get_res("addr", res_space, &len) != RES_OK || len < 1) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("get_res failed for \"addr\"\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}
		cookie = (ushort)res_space[0];

		/*
		 * Make sure the device is really present and uses the
		 * IRQ reserved for it.
		 */
		if (!pci_read_config_word(cookie, PCI_CONF_VENID, &reg) ||
				reg != PCI_AMD_VENDOR_ID) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("Device is not from AMD\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}
		if (!pci_read_config_word(cookie, PCI_CONF_DEVID, &reg) ||
				reg != PCI_PCNET_ID) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("Device is not PC-Net\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}
		if (!pci_read_config_byte(cookie, PCI_CONF_ILINE, &pci_irq) ||
				pci_irq != irq_level) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("IRQ config error\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}

		/*
		 * Turn on bus master enable if it isn't on already.
		 */
		if (!pci_read_config_word(cookie, PCI_CONF_COMM, &reg)) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("Failed to read PCI command reg\n"));
			Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
			return (BEF_FAIL);
		}
		if ((reg & 5) == 1) {
			/*
			 * If the BIOS didn't set the Bus Master Enable
			 * (bit 2) yet the I/O Enable is set (bit 0) enable
			 * the BME bit.
			 */

			Dprintf(DBG_CONFIG, ("Enabling the BME bit\n"));
			pci_write_config_word(cookie, PCI_CONF_COMM, reg | 4);
		}

		/*
		 * Record the config info in the device table.
		 */
		pcn_iobase[index].iobase = iobase;
		pcn_iobase[index].bustype = PCN_BUS_PCI;
		pcn_iobase[index].cookie = cookie;
		pcn_iobase[index].irq = irq_level;
		pcn_iobase[index].dma = 0xFFFF;
		break;
	
	default:
		Dprintf(DBG_CONFIG | DBG_ERRS, ("Bus config error\n"));
		Dprintf(DBG_CONFIG, ("Leaving pcn_configure\n"));
		return (BEF_FAIL);
	}

	/*
	 * Use the pcn_iobase table index as the device handle.
	 * Use "PCnet" as the ID string for all devices.
	 */
	*handle = index;
	*ident = "PCnet";
	Dprintf(DBG_CONFIG, ("Config successful: leaving pcn_configure\n"));
	return (BEF_OK);
}



/*
 *	Hardware-specific device initialization routine.
 *
 *	Initialize the adapter whose index into the pcn_iobase table
 *	is in handle.
 */
Static int
pcn_adapter_init(rmsc_handle handle, unchar *network_addr)
{
	ushort index = (ushort)handle;
	int i;

	Dprintf(DBG_INIT, ("Entering pcn_adapter_init\n"));

	/*
	 * Calling this routine means that the specified
	 * device has been chosen for boot.  Record the
	 * details for use by service routines.
	 */
	Dprintf(DBG_INIT, ("Port %x\n", pcn_iobase[index].iobase));
	pcnp->iobase = pcn_iobase[index].iobase;
	pcnp->irq = pcn_iobase[index].irq;

	/* Return the device hardware network address */
	for (i = 0; i < 6; i++)
		network_addr[i] = inb(pcnp->iobase + i);

	/*
	 * For ISA devices need to check/set up the DMA channel
	 */
	if (pcn_iobase[index].bustype == PCN_BUS_ISA)
		if (pcn_check_dma(pcn_iobase[index].dma) != BEF_OK) {
			Dprintf(DBG_INIT | DBG_ERRS, ("DMA config error\n"));
			Dprintf(DBG_INIT, ("Leaving pcn_adapter_init\n"));
			return (BEF_FAIL);
		}

	/* setup the board for the base address and IRQ configurations */
	pcn_init_data();
	pcn_setup_adapter();

	pcn_init_done = 0;
	pcn_csr_write(CSR0, CSR0_INIT | CSR0_INEA);	/* start the init */
	irq_mask(pcnp->irq, 0);

	/*
	 * wait for interrupt handler to signal that the adapter generated an
	 * initialization done interrupt.
	 */
	for (i = 0; i < 1000 && !pcn_init_done; i++)
		drv_msecwait(1);

	if (pcn_init_done) {
		Dprintf(DBG_INIT, ("Adapter initialized\n"));
		Dprintf(DBG_INIT, ("Leaving pcn_adapter_init\n"));
		return (BEF_OK);
	}
	Dprintf(DBG_INIT | DBG_ERRS, ("Initialization failed\n"));
	Dprintf(DBG_INIT, ("Leaving pcn_adapter_init\n"));
	return (BEF_FAIL);
}


/*
 *	Hardware-specific device interrupt routine.
 *
 *	Debug output from interrupts is terse to minimize effect on timing.
 */
Static void
pcn_device_interrupt(void)
{
	ushort  interrupt_status;

	interrupt_status = pcn_csr_read(CSR0);
	if (!(interrupt_status & CSR0_INTR)) {
		Dprintf(DBG_FLOW_IRQ, ("F"));
		return;
	}

	Dprintf(DBG_FLOW_IRQ, ("Q"));

	while (interrupt_status &
	    (CSR0_TINT | CSR0_RINT | CSR0_IDON | CSR0_MERR | CSR0_BABL |
	    CSR0_MISS)) {
		/* clear cause of interrupt */
		pcn_csr_write(CSR0, interrupt_status);

		/* babble error interrupt */
		if (interrupt_status & CSR0_BABL) {
			Dprintf(DBG_FLOW_IRQ, ("BE,"));
			/* Do a hot reset */
			pcn_lance_stop();
			pcn_init_data();
			pcn_setup_adapter();
			pcn_csr_write(CSR0, CSR0_STRT | CSR0_INEA);
		}

		/* memory error interrupt */
		if (interrupt_status & CSR0_MERR) {
			Dprintf(DBG_FLOW_IRQ, ("ME,"));
			/* Do a hot reset */
			pcn_lance_stop();
			pcn_init_data();
			pcn_setup_adapter();
			pcn_csr_write(CSR0, CSR0_STRT | CSR0_INEA);
		}

		/*    receive interrupt   */
		if (interrupt_status & CSR0_RINT) {
			pcn_receive_isr();
		}

		/*    overflow receive the packets   */
		if (interrupt_status & CSR0_MISS) {
			Dprintf(DBG_FLOW_IRQ, ("MI,"));
			pcn_receive_isr();
		}

		if (interrupt_status & CSR0_IDON) {
			pcn_init_done = 1;
			Dprintf(FLOW_IRQ, ("ID,"));
		}

		interrupt_status = pcn_csr_read(CSR0);
	}
	Dprintf(DBG_FLOW_IRQ, ("q"));
}



/*
 *	Hardware-specific device open routine.
 *
 *	Prepare the device for transmission and reception.
 */
Static void
pcn_open()
{
	int	i;

	Dprintf(DBG_FLOW, ("Entering pcn_open\n"));

	/*  start the board and enable the interrupt   */
	splx(RMSC_INTR_ENABLE);
	pcn_csr_write(CSR0, CSR0_STRT | CSR0_INEA);

	/*
	 * wait for the LANCE to start
	 */
	for (i = 0; i < 1000; i++) {
		drv_msecwait(1);
		if (pcn_csr_read(CSR0) & CSR0_STRT) {
			Dprintf(DBG_FLOW, ("Leaving pcn_open\n"));
			return;
		}
	}

	Dprintf(DBG_FLOW | DBG_ERRS, ("Leaving pcn_open after error\n"));
}



/*
 *	Hardware-specific device close routine.
 *
 *	Shut down the device after use.  Mostly we want to prevent
 *	the device from generating any interrupts.
 */
Static void
pcn_close(void)
{
	Dprintf(DBG_FLOW, ("Entering pcn_close\n"));

	pcn_lance_stop();

	Dprintf(DBG_FLOW, ("Leaving pcn_close\n"));
}



/*
 *	Hardware-specific packet transmission routine.
 *
 *	Attempt to send a packet.  If anything goes wrong, the
 *	higher level protocol is supposed to sort it out.
 */
Static void
pcn_send_packet(unchar far *pkt, ushort packet_length)
{
	int	i, index;

	Dprintf(DBG_FLOW, ("Entering pcn_send_packet\n"));

	/* Discard a frame larger than the Ether MTU */
	if (packet_length > ETH_MAX_TU) {
		Dprintf(DBG_FLOW | DBG_ERRS, ("Packet too big\n"));
		Dprintf(DBG_FLOW, ("Leaving pcn_send_packet\n"));
		return;
	}

	/* Pad a frame smaller than the Ether minimum */
	if (packet_length < ETH_MIN_TU) {
		Dprintf(DBG_FLOW, ("Padding short packet\n"));
		packet_length = ETH_MIN_TU;
	}

	/*
	 * Look for a free buffer in TX ring.  It really
	 * doesn't matter where we start, since any buffer
	 * owned by the host (that's us) is big enough.
	 */
	for (i = 0, index = pcnp->tx_index; i < PCN_TX_RING_SIZE;
	    i++, index = NextTXIndex(index)) {
		if (!(pcnp->tx_ring[index].MD[1] & MD1_OWN))
			break;  /* got one */
	}
	if (i >= PCN_TX_RING_SIZE) {
		pcnp->tx_no_buff++;
		Dprintf(DBG_FLOW | DBG_ERRS,
			("No available TX descriptor\n"));
		Dprintf(DBG_FLOW, ("Leaving pcn_send_packet\n"));
		return;
	}

	/*
	 * Copy the packet into the buffer
	 */
	memcpy((char far *)&pcnp->tx_buffer[index], pkt, packet_length);
	pcnp->tx_ring[index].MD[2] = (-packet_length) | 0xf000;
	pcnp->tx_ring[index].MD[3] = 0;		/* clear the status field */
	pcnp->tx_ring[index].MD[1] |= MD1_TXFLAGS; /* must be last */

	/*
	 * Kick the LANCE into immediate action
	 */
	pcn_csr_write(CSR0, pcn_csr_read(CSR0) | CSR0_TDMD);

	pcnp->tx_index = NextTXIndex(index);

	Dprintf(DBG_FLOW, ("Leaving pcn_send_packet\n"));
}



/*
 *	Hardware-specific device receive enable routine.
 *
 *	Prepare to handle the next packet received.
 */
Static void
pcn_receive_packet(unchar far *buffer, ushort buflen, recv_callback_t callback)
{
	Dprintf(DBG_FLOW, ("Entering pcn_receive_packet\n"));

	Rx_Buf = buffer;
	Rx_Len = buflen + 14;
	Rx_Callback = callback;
	Rx_Enable = 1;
	splx(RMSC_INTR_ENABLE);

	Dprintf(DBG_FLOW, ("Leaving pcn_receive_packet\n"));
}



/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	All the routines after this point in the file are private routines
 *	called from the routines declared above.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */

/*
 * pcn_isa_probe -- given the port see if a non-Plug-and-Play card exists
 */
Static int
pcn_isa_probe(short port)
{
	int	csum, j;

	Dprintf(DBG_PROBE, ("Probing port 0x%x for pcn ISA adapter\n", port));

	csum = 0;

	pcn_lance_reset();

	if (inb(port + 14) != 'W' || inb(port +15) != 'W') {
		Dprintf(DBG_PROBE, ("pcn ISA probe: bad value at 14/15\n"));
		return (BEF_FAIL);
	}

	for (j = 0; j < 12; j++)
		csum += inb(port+j);

	csum += inb(port+14);
	csum += inb(port+15);

	if ((csum == 0) || (csum == inw(port + 12))) {
		Dprintf(DBG_PROBE, ("Found pcn adapter\n"));
		return (BEF_OK);
	}
	Dprintf(DBG_PROBE, ("pcn ISA probe: invalid checksum\n"));
	return (BEF_FAIL);
}

/*
 * pcn_isa_get_irq -- given the port return the IRQ number
 */
Static int
pcn_isa_get_irq(short port, short *irq)
{
	int	len,
		i;
	unchar  oldpicmask1, oldpicmask2,
		picmask1, picmask2;

	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	=======================================================
	 *	In most drivers the interrupt level can be read from a
	 *	register.  For the PC-Net device we have to mask off all
	 *	the possible interrupt lines, cause an interrupt, check
	 *	the interrupt request lines in the PIC, reset the
	 *	interrupt and check the PIC again.
	 *	=======================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */

	/*
	 * Mask off all interrupts the LANCE can generate
	 */
	len = sizeof (pcn_irq_map) / sizeof (pcn_irq_map[0]);

	picmask1 = oldpicmask1 = inb(MPIC_IMR);
	picmask2 = oldpicmask2 = inb(SPIC_IMR);

	for (i = 0; i < len; i++) {
		if (pcn_irq_map[i] > 7)
			picmask2 |= (1<<(pcn_irq_map[i]-8));
		else
			picmask1 |= (1<<pcn_irq_map[i]);
	}

	outb(MPIC_IMR, picmask1);
	outb(SPIC_IMR, picmask2);

	/*
	 * Goose the LANCE to inspire an interrupt
	 */
	pcn_lance_poke(port);

	/*
	 * Now look to see if the IRR is set
	 */

	for (i = 0; i < len; i++) {
		if (pcn_get_pic_irr(pcn_irq_map[i])) {
			/*
			 * Disable Interrupt enable and make sure the
			 * interrupt goes away.
			 */
			pcn_csr_write(CSR0, 0);
			if (!pcn_get_pic_irr(pcn_irq_map[i])) {
				break;
			}
			/*
			 * If the interrupt didn't go away, it wasn't ours so
			 * re-enable Interrupt Enable and keep looking.
			 */
			pcn_csr_write(CSR0, CSR0_INEA);
		}
	}

	/*
	 * Shut the LANCE up, now that we're done
	 */
	pcn_lance_reset();

	outb(MPIC_IMR, oldpicmask1);
	outb(SPIC_IMR, oldpicmask2);

	if (i >= len) {
		/*
		 * Couldn't find IRQ. Is there anything that we can
		 * do here?
		 */
		*irq = 0;
		Dprintf(DBG_ERRS, ("Failed to identify irq\n"));
		return (BEF_FAIL);
	}
	*irq = (ushort)pcn_irq_map[i];
	return (BEF_OK);
}

Static void
pcn_setup_adapter(void)
{
	ulong   linaddr = pcnp->linear_base_address;

	pcn_lance_reset();

	pcn_csr_write(CSR1, (ushort) linaddr & 0xfffe);
	pcn_csr_write(CSR2, (ushort) (linaddr>>16) & 0xff);

	return;
}

/*
 * Starting with the buffer described by index, process received
 * frames out of the ring.  Return the index of the last descriptor
 * processed.  This assumes the STP bit is set in the descriptor
 * pointed at by index.
 */
Static int
pcn_process_receive(int index)
{
	struct pcn_msg_desc *mdp;
	ushort  pktlen;

	Dprintf(DBG_FLOW_IRQ, ("P"));

	/*
	 * Simplifying assumption: if the first descriptor isn't
	 * an entire frame, give it back to the LANCE and return.
	 */
	mdp = &pcnp->rx_ring[index];

	if (mdp->MD[1] & MD1_ERR) {
		pcn_rxdesc_reset(mdp);
		Dprintf(DBG_FLOW_IRQ, ("1p"));
		return (index);
	}

	/*
	 * If this is the first frame of a chained message,
	 * reset all the descriptors until the ENP is found set
	 */
	if (!(mdp->MD[1] & MD1_ENP)) {
		ushort i;

		for (i = 0; i < PCN_RX_RING_SIZE; i++) {
			int enp;

			enp = mdp->MD[1] & MD1_ENP;
			pcn_rxdesc_reset(mdp);
			if (enp)
				break;
			index = NextRXIndex(index);
			mdp = &pcnp->rx_ring[index];
		}
		Dprintf(DBG_FLOW_IRQ, ("2p"));
		return (index);
	}


	/*
	 * Check frame size and transfer it if receiver is enabled
	 */
	if (Rx_Enable) {
		pktlen = mdp->MD[3];
		pktlen -= LANCE_FCS_SIZE;	/* discard FCS at end */

		if (pktlen > Rx_Len) {
			Dprintf(DBG_FLOW_IRQ, ("3"));
			goto done;
		}
		memcpy(Rx_Buf, (char __far *)&pcnp->rx_buffer[index], pktlen);

		Rx_Enable = 0;
		Rx_Callback((int) pktlen);
		Dprintf(DBG_FLOW_IRQ, ("4"));
	}

	/*
	 * Reset the frame descriptor  and return it to the LANCE
	 */
done:
	pcn_rxdesc_reset(mdp);
	Dprintf(DBG_FLOW_IRQ, ("p"));
	return (index);
}

/*
 * Receive interrupt handler - only called at interrupt time.
 * Look at the receiver ring and transfer a received frame if
 * an AdapterReceive() is outstanding.
 *
 * Cleanup for error frames, record statistics and return buffers to
 * the LANCE.
 */
Static void
pcn_receive_isr(void)
{
	int	i, index;

	Dprintf(DBG_FLOW_IRQ, ("R"));
	for (i = 0, index = pcnp->rx_index; i < PCN_RX_RING_SIZE;
	    i++, index = NextRXIndex(index)) {
		if (!(pcnp->rx_ring[index].MD[1] & MD1_OWN)) {
			/*
			 * we own it?
			 */

			if (pcnp->rx_ring[index].MD[1] & MD1_STP) {
				/*
				 * first desc?
				 */

				index = pcn_process_receive(index);
				pcnp->rx_index = index;
			}
		}
	}
	Dprintf(DBG_FLOW_IRQ, ("r"));
}



/*
 * Return 1 if specified IRQ is being requested, 0 otherwise.
 */
Static int
pcn_get_pic_irr(ushort irq)
{
	ushort  picport;
	unchar  irr;

	picport = (irq > 7) ? SPIC_CMD : MPIC_CMD;
	outb(picport, PIC_IRR_READ);
	irr = inb(picport);
	return ((irr >> (irq & 7)) & 1);
}

/*
 *
 */
Static void
pcn_lance_stop(void)
{
	pcn_csr_write(CSR0, CSR0_STOP);	/* also resets CSR0_INEA */
	while (!(pcn_csr_read(CSR0) & CSR0_STOP))
		drv_msecwait(1);	/* wait for the STOP to take */
	pcn_lance_reset();
}

/*
 * Initialize all data structures used to communicate with the LANCE.
 * The LANCE is stopped if necessary before changing the data.
 */
Static void
pcn_init_data(void)
{
	ulong	linaddr;
	int	i;
	unchar	*p;

	pcn_lance_stop();
	pcnp->linear_base_address = pcn_off_to_lin(&pcnp->initblock);

	/*
	 * Default LANCE MODE setting is 0.
	 */
	pcnp->initblock.MODE = 0;

	/*
	 * Copy in the Ethernet address
	 */
	p = (unchar *) &pcnp->initblock.PADR[0];
	for (i = 0; i < ETH_ADDR_SIZE; i++)
		*(p++) = inb(pcnp->iobase + i);

	/*
	 * Fill the Multicast array with 0
	 */
	p = (unchar *) &pcnp->initblock.LADRF[0];
	for (i = 0; i < 8; i++)
		*(p++) = 0;

	/*
	 * Create the RX Ring Ptr
	 */
	linaddr = pcn_off_to_lin(pcnp->rx_ring) |
		((ulong) PCN_RX_RING_VAL) << 29;
	pcnp->initblock.RDRA[0] = (ushort) linaddr;
	pcnp->initblock.RDRA[1] = (ushort) (linaddr >> 16);

	/*
	 * Create the TX Ring Ptr
	 */
	linaddr = pcn_off_to_lin(pcnp->tx_ring) |
		((ulong) PCN_TX_RING_VAL) << 29;
	pcnp->initblock.TDRA[0] = (ushort) linaddr;
	pcnp->initblock.TDRA[1] = (ushort) (linaddr >> 16);

	/*
	 * Initialize the RX Ring
	 */
	for (i = 0; i < PCN_RX_RING_SIZE; i++) {
		linaddr = pcn_off_to_lin(&pcnp->rx_buffer[i]);
		pcn_make_desc(&pcnp->rx_ring[i], linaddr,
			-PCN_RX_BUF_SIZE, MD1_OWN);
	}

	/*
	 * Initialize the TX Ring
	 */
	for (i = 0; i < PCN_TX_RING_SIZE; i++) {
		linaddr = pcn_off_to_lin(&pcnp->tx_buffer[i]);
		pcn_make_desc(&pcnp->tx_ring[i], linaddr,
			-PCN_TX_BUF_SIZE, 0);
	}

	/*
	 * Initialize the ring pointers
	 */
	pcnp->tx_index = 0;
	pcnp->rx_index = 0;

}


/*
 *
 */
Static void
pcn_lance_poke(ushort port)
{
	int	i;

	pcnp->iobase = port;

	pcn_init_data();
	pcn_setup_adapter();

	/*
	 * Start initialization, enable an interrupt at completion
	 */
	pcn_csr_write(CSR0, CSR0_INIT | CSR0_INEA);

	/*
	 * wait for the LANCE to finish the initialization sequence
	 */
	for (i = 0; i < 1000; i++) {
		drv_msecwait(1);
		if (pcn_csr_read(CSR0) & CSR0_INTR)
			return;
	}

	/*
	 * This should never, ever, happen.
	 */
	printf("pcn: failed to initialize the LANCE chip\n");
}

/*
 * Check whether a given DMA channel is in use.
 */
Static int
pcn_check_dma(ushort channel)
{
	int j;

	pcnp->dma = channel;
	if (pcnp->dma > 4) {
		outb(PCN_DMA_2_MODE_REGS,
			(unchar) (PCN_CASCADE | (pcnp->dma-4)));
		outb(PCN_DMA_2_MASK_REGS, (unchar) (pcnp->dma-4));
	} else {
		outb(PCN_DMA_1_MODE_REGS,
			(unchar) (PCN_CASCADE | pcnp->dma));
		outb(PCN_DMA_1_MASK_REGS, (unchar) pcnp->dma);
	}

	/*
	 * Attempt to initialize the LANCE
	 */
	pcn_lance_reset();
	pcn_init_data();
	pcn_setup_adapter();
	pcn_csr_write(CSR0, CSR0_INIT);
	for (j = 0; j < 1000; j++) {
		if (pcn_csr_read(CSR0) & CSR0_INTR)
			break;
	}
	if (j >= 1000) {
		return (0);
	}

	/*
	 * Check to see if the initialization
	 * succeeded (i.e., correct DMA)
	 */
	return ((pcn_csr_read(CSR0) & CSR0_IDON) != 0 ? BEF_OK : BEF_FAIL);
}

/*
 * Given an iobase value, look for an entry in the pcn_iobase array
 * which matches.  If one is found, return the index.  If none are
 * found, return the index of the next available entry (bustype ==
 * PCN_BUS_NONE).  If no entries are available, return -1.
 */
Static int
pcn_find_iob(ushort iobase)
{
	int	i;

	for (i = 0; i < PCN_IOBASE_ARRAY_SIZE; i++) {
		if ((pcn_iobase[i].iobase == iobase) ||
		    (pcn_iobase[i].bustype == PCN_BUS_NONE))
			return (i);
	}
	return (-1);
}

/*
 *
 */
Static void
pcn_lance_reset(void)
{
	inb(PCN_IO_RESET+pcnp->iobase);
	outb(PCN_IO_RESET+pcnp->iobase, 0);
	drv_msecwait(10);
}


/*
 *
 */
Static ushort
pcn_csr_read(ushort csr)
{
	ushort  iobase = pcnp->iobase;

	outw(PCN_IO_RAP+iobase, csr);
	return (inw(PCN_IO_RDP+iobase));
}


/*
 *
 */
Static void
pcn_csr_write(ushort csr, ushort val)
{
	ushort  iobase = pcnp->iobase;

	outw(PCN_IO_RAP+iobase, csr);
	outw(PCN_IO_RDP+iobase, val);
}

Static ulong
pcn_off_to_lin(void *ptr)
{
	void __far *fp;

	fp = MK_FP(get_data_selector(), (ushort)ptr);
	return (FP_TO_LINEAR(fp));
}

/*
 *
 */
Static void
pcn_make_desc(struct pcn_msg_desc *mdp, ulong addr, ushort size,
	ushort flags)
{
	mdp->MD[0] = (ushort) addr;
	mdp->MD[1] = (flags & 0xff00) | ((ushort)(addr>>16) & 0xff);
	mdp->MD[2] = size | 0xf000;
	mdp->MD[3] = 0;
}


/*
 *
 */
Static void
pcn_rxdesc_reset(struct pcn_msg_desc *mdp)
{
	/* reset received size */
	mdp->MD[3] = 0;

	/* reset buffer size */
	mdp->MD[2] = (-PCN_RX_BUF_SIZE) | 0xf000;

	/* always do this last */
	mdp->MD[1] = (mdp->MD[1]&0xff) | MD1_OWN;
}

#if defined(DEBUG)
/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This routine is never called by the driver.  It is intended to be
 *	called directly from a debugger for the purpose of examining a packet.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
/*
 * Dump an ether frame
 */
Static void
pcn_show_frame(unchar *pkt, ushort len)
{
	ushort  i;

	printf("\nreceived frame:\n");
	for (i = 0; i < len; i++) {
		printf("%0.2x%c", *pkt++,
			((i > 0) && (i % 16) == 0) ? '\n' : '.');
	}
	printf("\n");
}
#endif  /* if defined(DEBUG) */
