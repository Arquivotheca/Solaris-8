/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 */

#ident "@(#)network.c	1.19	99/06/02 SMI"

/*
 * Code to implement the network real mode driver interface in terms
 * of the generic driver interface.
 *
 * This layer implements the driver_init routine and expects the
 * device-dependent code to supply a network_driver_init routine
 * (called from this file) and the "stack" and "stack_size"
 * variables expected by the generic layer.
 */

#include "rmscnet.h"



/*
 * Debugging facilities in this file are partly intended for debugging
 * the network code itself and partly for helping to debug hardware-specific
 * realmode code that implements a network driver in terms of this code.
 *
 * Various classes of messages can be turned on by setting flags in
 * net_debug_flag.  Flags can be set by changing the definition of
 * NET_DEBUG_FLAG below, by setting NET_DEBUG_FLAG from the compiler
 * command line, or by writing net_debug_flag using a debugger.
 */
#ifdef DEBUG

#pragma	message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)

/* Network-specific flags.  See rmsc.h for common flags */
#define	DBG_OPEN	0x0001	/* enable messages from net_init */
#define	DBG_INTR	0x0002	/* enable messages from device interrupts */

#ifndef NET_DEBUG_FLAG
#define	NET_DEBUG_FLAG	DBG_ERRS
#endif
int net_debug_flag = NET_DEBUG_FLAG;
#define	MODULE_DEBUG_FLAG net_debug_flag

#endif /* DEBUG */



/* Receive buffer size and count */
#define	RXBUFSIZE	1540
#define	NUMRXBUFS	4

Static rmsc_network_driver_init network_init_data;
Static char	net_rx_flag;
Static ushort	net_packet_count;	/* # of received packets to be read */
Static ushort	bufferWriteIndex;	/* with these */
Static ushort	bufferReadIndex;	/* 2 read and write pointers */
Static unchar	receiveBuffer0[RXBUFSIZE];	/* and these buffers, */
Static unchar	receiveBuffer1[RXBUFSIZE];	/* implement ring buffering */
Static unchar	receiveBuffer2[RXBUFSIZE];
Static unchar	receiveBuffer3[RXBUFSIZE];
Static unchar	*receiveBufPtrs[NUMRXBUFS] = {	/* with this array of ptrs */
	receiveBuffer0,
	receiveBuffer1,
	receiveBuffer2,
	receiveBuffer3,
};
Static ushort	receiveLengths[NUMRXBUFS];

/* These items record data related to the selected device */
Static rmsc_handle network_device_handle;
Static struct bdev_info *network_device_info;
Static unchar network_device_addr[6];



/* Function prototypes for routines called from the generic layer */
Static int net_configure(void);
Static int net_init(rmsc_handle, struct bdev_info *);



/* Function prototypes for routines known to interrupt stubs */
void net_device_interrupt(void);
void net_service(ushort, ushort, ushort, ushort, ushort,
	ushort, ushort, ushort);



/* Function prototypes for routines used within this file */
Static int net_get_info(char *, struct bdev_info *);
Static void net_listen_packet(void);
#ifdef DEBUG
Static void net_missing_assign(char *);
#endif
Static void net_receive_callback(ushort);
Static void net_service_init(ushort, ushort, ushort, ushort, ushort);



/*
 * Initialize the network layer.  Depends on successful initialization
 * of the hardware-specific code.
 */
int
driver_init(struct driver_init *p)
{
	int ret = BEF_OK;

	Dprintf(DBG_INIT, ("Entering driver_init\n"));

	/* Set up init structure before calling device-specific init */
	memset(&network_init_data, 0, sizeof (network_init_data));
#ifdef DEBUG
	network_init_data.driver_name = "NAME NOT SUPPLIED";
#endif /* DEBUG */

	Dcall(DBG_INIT, "network_driver_init");
	if (network_driver_init(&network_init_data) != BEF_OK) {
		Dfail(DBG_INIT | DBG_ERRS, "network_driver_init");
		return (BEF_FAIL);
	}
	Dsucceed(DBG_INIT, "network_driver_init");

	/*
	 * Assign driver_init struct members.
	 * Network drivers do not require read routines.
	 */
	p->driver_name = network_init_data.driver_name;
	p->legacy_probe = network_init_data.legacy_probe;
	p->configure = net_configure;
	p->init = net_init;

#ifdef DEBUG
	/* Check for required items in network_init_data */
	if (network_init_data.driver_name == 0) {
		net_missing_assign("driver_name");
		ret = BEF_FAIL;
	}
	if (network_init_data.configure == 0) {
		net_missing_assign("configure");
		ret = BEF_FAIL;
	}
	if (network_init_data.initialize == 0) {
		net_missing_assign("initialize");
		ret = BEF_FAIL;
	}
	if (network_init_data.device_interrupt == 0) {
		net_missing_assign("device_interrupt");
		ret = BEF_FAIL;
	}
	if (network_init_data.open == 0) {
		net_missing_assign("open");
		ret = BEF_FAIL;
	}
	if (network_init_data.send_packet == 0) {
		net_missing_assign("send_packet");
		ret = BEF_FAIL;
	}
	if (network_init_data.receive_packet == 0) {
		net_missing_assign("receive_packet");
		ret = BEF_FAIL;
	}
#endif /* DEBUG */

	Dprintf(DBG_INIT,
		("Returning %s from driver_init\n", DBG_RET_STR(ret)));
	return (ret);
}



/*
 * Configure the devices supported by this driver by using node_op
 * to scan the list of device nodes.  For each node found, call the
 * hardware-specific configuration routine to configure the adapter.
 */
Static int
net_configure(void)
{
	int ret = BEF_FAIL;	/* Haven't configured any devices yet */
	rmsc_handle handle;
	struct bdev_info info;
	char *device_name;

	Dprintf(DBG_CONFIG, ("Entering net_configure\n"));
	while (node_op(NODE_START) == NODE_OK) {
		Dcall(DBG_CONFIG, "configure");
		if ((*network_init_data.configure)
					(&handle, &device_name) != BEF_OK) {
			node_op(NODE_DONE);
			Dfail(DBG_CONFIG | DBG_ERRS, "configure");
			continue;
		}
		Dsucceed(DBG_CONFIG, "configure");
		if (net_get_info(device_name, &info) != BEF_OK) {
			node_op(NODE_DONE);
			continue;
		}
		if (network_init_data.modify_dev_info) {
			Dcall(DBG_CONFIG, "modify_dev_info");
			if ((*network_init_data.modify_dev_info)
					(handle, &info) != BEF_OK) {
				node_op(NODE_DONE);
				Dfail(DBG_CONFIG | DBG_ERRS,
					"modify_dev_info");
				continue;
			}
			Dsucceed(DBG_CONFIG, "modify_dev_info");
		}
		node_op(NODE_DONE);
		if (rmsc_register(handle, &info) == BEF_OK)
			ret = BEF_OK;
	}
	Dprintf(DBG_CONFIG, ("Returning 0x%x from net_configure\n", ret));
	return (ret);
}



/*
 * Fill in a bdev_info structure for the active node.
 */
Static int
net_get_info(char *device_name, struct bdev_info *info)
{
	static ushort index = 0;
	int j;
	DWORD res_space[MaxTupleSize];
	DWORD len;

	Dprintf(DBG_CONFIG, ("Entering net_get_info\n"));

	/* Clear the structure to NULLs before making entries */
	memset((char far *)info, 0, sizeof (struct bdev_info));

	/* Extract base port address from hardware node */
	len = PortTupleSize;
	if (get_res("port", res_space, &len) != BEF_OK || len < 1) {
		info->base_port = 0;
	} else {
		info->base_port = (ushort)res_space[0];
	}

	/* Extract irq number from hardware node */
	len = IrqTupleSize;
	if (get_res("irq", res_space, &len) != BEF_OK || len < 1) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("get_res failed for \"irq\"\n"));
		return (BEF_FAIL);
	}
	info->MDBdev.net.irq_level = (ushort)res_space[0];

	/* Extract memory info from hardware node */
	len = MemTupleSize;
	if (get_res("mem", res_space, &len) == BEF_OK && len > 0) {
		if (len < 2) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("get_res \"mem\" gave only 1 value\n"));
			return (BEF_FAIL);
		}
		res_space[0] /= 16L;
		res_space[1] /= 1024L;
		info->MDBdev.net.mem_base = (ushort)res_space[0];
		info->MDBdev.net.mem_size = (ushort)res_space[1];
	} else {
		/* No "mem" resource.  Assume the device doesn't use memory */
		info->MDBdev.net.mem_base = 0;
		info->MDBdev.net.mem_size = 0;
	}

	/*
	 * The index field is obsolete.  Just make sure each device has
	 * a different index value.
	 */
	info->MDBdev.net.index = index++;

	info->dev_type = MDB_NET_CARD;
	info->version = 1;

	/* Copy device name with truncation or blank padding */
	memset(info->vid, ' ', 8);
	j = strlen(device_name);
	if (j > 8)
		j = 8;
	strncpy(info->vid, device_name, j);
	info->blank1 = ' ';
	sprintf(info->pid, "I/O=%x IRQ=%d", info->base_port,
		info->MDBdev.net.irq_level);

	/* Extract bus type info from hardware node.  Finished if not PCI */
	len = NameTupleSize;
	if (get_res("name", res_space, &len) != BEF_OK || len < 2) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("get_res failed for \"name\"\n"));
		return (BEF_FAIL);
	}
	if (res_space[1] != RES_BUS_PCI) {
		Dprintf(DBG_CONFIG,
			("net_get_info finished non-PCI device\n"));
		return (BEF_OK);
	}

	info->pci_ven_id = RES_PCI_NAME_TO_VENDOR(res_space[0]);
	info->pci_vdev_id = RES_PCI_NAME_TO_DEVICE(res_space[0]);

	/* Extract PCI cookie info from hardware node */
	len = AddrTupleSize;
	if (get_res("addr", res_space, &len) != BEF_OK || len < 1) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("get_res failed for \"addr\"\n"));
		return (BEF_FAIL);
	}

	info->pci_bus = PCI_COOKIE_TO_BUS((ushort)res_space[0]);
	info->pci_dev = PCI_COOKIE_TO_DEV((ushort)res_space[0]);
	info->pci_func = PCI_COOKIE_TO_FUNC((ushort)res_space[0]);

	info->pci_valid = 1;

	Dprintf(DBG_CONFIG, ("net_get_info finished PCI device\n"));
	return (BEF_OK);
}

/*
 * Prepare the specified device for communication.
 * Called from the generic layer in response to an INT 13
 * device initialization request.
 */
Static int
net_init(rmsc_handle handle, struct bdev_info *info)
{
	ushort irq;
	int i;
	static done = 0;
	static ushort IRQ_Vectors[] = {
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77
	};
	extern void far	intr_vec(void);
	extern void far	bios_intercept(void);

	/* Enforce the restriction that we initialize only one device */
	if (done)
		return (BEF_FAIL);
	done = 1;

	/* Record details of selected device */
	network_device_handle = handle;
	network_device_info = info;
	irq = network_device_info->MDBdev.net.irq_level;

	/* Set up service call vector */
	save_ds();	/* Necessary setup for bios_intercept */
	set_vector(SVR_INT_NUM, (char far *)
		MK_FP(get_code_selector(), FP_OFF(bios_intercept)));

	/*
	 * We expect all network devices to use interrupts but we don't
	 * require it.  Omit the interrupt set-up steps if the interrupt
	 * value is out of range or 0.
	 */
	if (irq > 0 && irq <= 15) {
		/* Disable interrupts for device */
		irq_mask(irq, 1);

		/* Catch device interrupts */
		set_vector(IRQ_Vectors[irq],
			MK_FP(get_code_selector(), FP_OFF(intr_vec)));
	}

	/* Initialize device */
	Dcall(DBG_OPEN, "initialize");
	if ((*network_init_data.initialize)(network_device_handle,
			network_device_addr) != BEF_OK) {
		irq_mask(irq, 1);
		Dfail(DBG_OPEN | DBG_ERRS, "initialize");
		return (BEF_FAIL);
	}
	Dsucceed(DBG_OPEN, "initialize");

	printf("Node Address");
	for (i = 0; i < 6; i++)
		printf(":%2.2x", network_device_addr[i]);
	printf("\n");

	/* Enable interrupts for device */
	if (irq > 0 && irq <= 15)
		irq_mask(irq, 0);

	/* Open the device */
	Dcall(DBG_OPEN, "open");
	(*network_init_data.open)();
	Dback(DBG_OPEN, "open");

	/* Report success */
	return (BEF_OK);
}

/*
 * Handle a service interrupt.
 *
 * The initial dummy argument is because net_service is called
 * from a multi-purpose interrupt handler wrapper.
 */
void
net_service(ushort dummy, ushort ax, ushort bx, ushort cx, ushort dx,
		ushort si, ushort di, ushort es)
{
	ushort old_spl;

	/*
	 * Caller is likely to poll with RECEIVE CALL.  Suppress messages
	 * to avoid swamping other output.
	 */
	if (ax >> 8 != RECEIVE_CALL)
		Dprintf(DBG_SERVICE, ("net_service: function %x\n", ax >> 8));

	switch (ax >> 8) {

	case CLOSE_CALL:
		/*
		 * No register setup for this call.
		 * Handler routine is optional.
		 */
		if (network_init_data.close) {
			Dcall(DBG_SERVICE, "close");
			(*network_init_data.close)();
			Dback(DBG_SERVICE, "close");
		}
		break;

	case SEND_CALL:
		/*
		 * Register Setup:
		 * BX:SI = packet buffer address
		 * CX    = length of outgoing packet
		 */
		Dcall(DBG_SERVICE, "send_packet");
		(*network_init_data.send_packet)(MK_FP(bx, si), cx);
		Dback(DBG_SERVICE, "send_packet");
		break;

	case RECEIVE_CALL:
		/*
		 * Register Setup:
		 * DX:DI = callers receive buffer
		 * CX    = size of receive buffer
		 * Return Setup:
		 * CX    = number of bytes received
		 */
		old_spl = splhi();
		if (net_packet_count == 0) {
			cx = 0;
		} else {

			net_packet_count--;

			if (cx < receiveLengths[bufferReadIndex])
				receiveLengths[bufferReadIndex] = cx;
			else
				cx = receiveLengths[bufferReadIndex];
			memcpy(MK_FP(dx, di),
				(char far *)receiveBufPtrs[bufferReadIndex],
				receiveLengths[bufferReadIndex]);
			receiveLengths[bufferReadIndex] = 0;
			bufferReadIndex = ++bufferReadIndex % NUMRXBUFS;
		}

		/*
		 * The receiver may not be enabled if
		 * this is the first call to receive.
		 */
		net_listen_packet();
		splx(old_spl);
		break;

	case GET_ADDR_CALL:
		bx = network_device_addr[0] << 8 | network_device_addr[1];
		cx = network_device_addr[2] << 8 | network_device_addr[3];
		dx = network_device_addr[4] << 8 | network_device_addr[5];
		break;
	}

	if (ax >> 8 != RECEIVE_CALL)
		Dprintf(DBG_SERVICE, ("net_service done\n"));
}

/*
 * Handle a device interrupt for the active adapter.
 */
void
net_device_interrupt(void)
{
	ushort irq = network_device_info->MDBdev.net.irq_level;
	ushort old_mask;

	/* Mask off device interrupts, allow other interrupts, call handler */
	old_mask = irq_mask(irq, 1);
	splx(RMSC_INTR_ENABLE);

	/* These messages are brief to minimize timing effect */
	Dprintf(DBG_INTR | DBG_CALL, ("I"));
	(*network_init_data.device_interrupt)();
	Dprintf(DBG_INTR | DBG_CALL, ("i"));

	/* Prevent all interrupts, send the EOI(s), restore device mask */
	splhi();
	if (irq > 7)
		outb(SPIC_CMD, PIC_EOI);
	outb(MPIC_CMD, PIC_EOI);
	irq_mask(irq, old_mask);
}

/*
 * Enable packet reception.
 */
Static void
net_listen_packet(void)
{
	if (!net_rx_flag) {
		Dcall(DBG_SERVICE, "receive_packet");
		(*network_init_data.receive_packet)((unchar far *)
			receiveBufPtrs[bufferWriteIndex], RXBUFSIZE,
			net_receive_callback);
		Dback(DBG_SERVICE, "receive_packet");
		net_rx_flag = 1;
	}
}

/*
 * Record the arrival of a packet.  Called from the hardware-specific
 * code, typically from its interrupt routine.
 */
Static void
net_receive_callback(ushort rxlen)
{
	/*
	 * Rx buffer no longer posted
	 */
	net_rx_flag = 0;

	/*
	 * The current buffer being filled is pointed to by the index
	 * bufferWriteIndex.
	 */
	receiveLengths[bufferWriteIndex] = rxlen;
	net_packet_count++;

	/* update to receive into the next buffer, if available */
	if (++bufferWriteIndex == NUMRXBUFS)
		bufferWriteIndex = 0;

	if (net_packet_count == NUMRXBUFS) {
		/*
		 * The ring buffer is full. There is no good thing to do
		 * here, but the choice is to keep the receiver going
		 * by simulating a read and dumping the oldest packet.
		 * Odds are, the upper layers are not interested in
		 * these packets and will dump them anyway. If we
		 * stop the receiver, we force the driver to deal with
		 * incoming packets and no place to put them.
		 */
		net_packet_count--;
		bufferReadIndex = ++bufferReadIndex % NUMRXBUFS;
	}
	net_listen_packet();
}

#ifdef DEBUG
/* Report absence of required init struct member assignment */
Static void
net_missing_assign(char *which)
{
	printf("Network driver %s did not assign \"%s\" member.\n",
		network_init_data.driver_name ?
			network_init_data.driver_name : "UNNAMED", which);
}
#endif
