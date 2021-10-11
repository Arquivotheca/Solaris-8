/*
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uhciutil.c	1.20	99/11/18 SMI"

/*
 * Universal Host Controller Driver (UHCI)
 *
 * The UHCI driver is a software driver which interfaces to the Universal
 * Serial Bus Driver (USBA) and the Host Controller (HC). The interface to
 * the Host Controller is defined by the UHCI.
 * THis file contains misc functions.
 */

#include		<sys/usb/hcd/uhci/uhciutil.h>

/*
 * uhci_build_interrupt_lattice:
 *
 * Construct the interrupt lattice tree using static Queue Head pointers.
 * This interrupt lattice tree will have total of 63 queue heads and the
 * Host Controller (HC) processes queue heads every frame.
 */

static int
uhci_build_interrupt_lattice(uhci_state_t *uhcip)
{
	uint16_t		i, j, k;
	frame_lst_table_t	*frame_lst_tablep =
				uhcip->uhci_frame_lst_tablep;
	queue_head_t		*list_array = uhcip->uhci_qh_pool_addr;
	int			half_list = NUM_INTR_QH_LISTS / 2;
	queue_head_t		*tmp_qh;
	uintptr_t		addr;
	gtd			*sof_td;
	ushort_t		*temp, num_of_nodes;

	USB_DPRINTF_L4(PRINT_MASK_ATTA,
		uhcip->uhci_log_hdl, "uhci_build_interrupt_lattice:");

	/*
	 * Reserve the first 63 queue head structures in the pool as
	 * static queue heads & these are required for constructing interrupt
	 * lattice tree.
	 */

	for (i = 0; i < NUM_INTR_QH_LISTS; i++) {
		list_array[i].link_ptr		= HC_END_OF_LIST;
		list_array[i].element_ptr	= HC_END_OF_LIST;
		list_array[i].qh_flag		= QUEUE_HEAD_FLAG_STATIC;
		list_array[i].node		= i;
	}


	/* Build the interrupt lattice tree */
	for (i = 0; i < half_list - 1; i++) {

		/*
		 * The next  pointer in the host controller  queue head
		 * descriptor must contain an iommu address. Calculate
		 * the offset into the cpu address and add this to the
		 * starting iommu address.
		 */
		addr = (uint32_t)(QH_PADDR(&list_array[i]));

		list_array[2*i + 1].link_ptr = addr | HC_QUEUE_HEAD;
		list_array[2*i + 2].link_ptr = addr | HC_QUEUE_HEAD;
	}


	/*
	 *  Build the tree bottom.
	 */

	temp = (unsigned short *)kmem_zalloc(
			NUM_FRAME_LST_ENTRIES * 2, KM_SLEEP);

	num_of_nodes = 1;
	for (i = 0; i < log_2(NUM_FRAME_LST_ENTRIES); i++) {
		for (j = 0, k = 0; k < num_of_nodes; k++, j++) {
			tree_bottom_nodes[j++] = temp[k];
			tree_bottom_nodes[j]   = temp[k] + pow_2(i);
		}

		num_of_nodes *= 2;
		for (k = 0; k < num_of_nodes; k++)
			temp[k] = tree_bottom_nodes[k];

	}

	kmem_free((void *)temp, (NUM_FRAME_LST_ENTRIES*2));

	/*
	 * Initialize the interrupt list in the Frame list Table
	 * so that it points to the bottom of the tree.
	 */


	for (i = 0, j = 0; i < pow_2(TREE_HEIGHT); i++) {
		addr = (uint32_t)(QH_PADDR(&list_array[half_list + i - 1]));
		for (k = 0; k <  pow_2(VIRTUAL_TREE_HEIGHT); k++)
			frame_lst_tablep[tree_bottom_nodes[j++]] =
				addr | HC_QUEUE_HEAD;
	}

	/*
	 *  Create a controller and bulk Queue heads
	 */

	uhcip->uhci_ctrl_xfers_q_head =
			uhci_alloc_queue_head(uhcip);
	tmp_qh = uhcip->uhci_ctrl_xfers_q_tail =
			uhcip->uhci_ctrl_xfers_q_head;

	list_array[0].link_ptr =
			(uint32_t)(QH_PADDR(tmp_qh) | HC_QUEUE_HEAD);

	uhcip->uhci_bulk_xfers_q_head = uhci_alloc_queue_head(uhcip);
	uhcip->uhci_bulk_xfers_q_tail = uhcip->uhci_bulk_xfers_q_head;
	tmp_qh->link_ptr = (uint32_t)(QH_PADDR(
			uhcip->uhci_bulk_xfers_q_head)|HC_QUEUE_HEAD);

	uhcip->uhci_bulk_xfers_q_head->link_ptr = HC_END_OF_LIST;

	/*
	 * Add a dummy TD to the static queue head 0. THis is used
	 * to generate an at the end of frame.
	 */

	sof_td = uhci_allocate_td_from_pool(uhcip);

	list_array[0].element_ptr = TD_PADDR(sof_td) | HC_TD_HEAD;

	sof_td->link_ptr = HC_END_OF_LIST;

	return (UHCI_SUCCESS);
}


/*
 * uhci_allocate_pools:
 *
 * Allocate the system memory for the Queue Heads Descriptor and for  the
 * Transfer Descriptor (TD) pools. Both QH and TD structures must be aligned
 * to a 16 byte boundary.
 */

int
uhci_allocate_pools(dev_info_t *dip, uhci_state_t *uhcip)
{
	ddi_device_acc_attr_t	dev_attr;
	size_t			real_length;
	int			i;
	int			result;
	uint_t			ccount;
	uint_t			offset;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_allocate_pools:");

	/* The host controller will be little endian */
	dev_attr.devacc_attr_version		= DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags	= DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder		= DDI_STRICTORDER_ACC;

	/* Allocate the TD pool DMA handle */
	if (ddi_dma_alloc_handle(dip, &uhcip->uhci_dma_attr, DDI_DMA_SLEEP, 0,
		&uhcip->uhci_td_pool_dma_handle) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	/* Allocate the memory for the TD pool */
	if (ddi_dma_mem_alloc(uhcip->uhci_td_pool_dma_handle,
			td_pool_size * sizeof (gtd) + UHCI_TD_ALIGN_SZ,
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			0,
			(caddr_t *)&uhcip->uhci_td_pool_addr,
			&real_length,
			&uhcip->uhci_td_pool_mem_handle)) {

		return (DDI_FAILURE);
	}

	/* Map the TD pool into the I/O address space */
	result = ddi_dma_addr_bind_handle(
			uhcip->uhci_td_pool_dma_handle,
			NULL,
			(caddr_t)uhcip->uhci_td_pool_addr,
			real_length,
			DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			NULL,
			&uhcip->uhci_td_pool_cookie,
			&ccount);

	/* Process the result */
	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_allocate_pools: More than 1 cookie");
			return (DDI_FAILURE);
		}
	} else {
		USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_allocate_pools: Result = %d", result);

		uhci_decode_ddi_dma_addr_bind_handle_result(uhcip, result);

		return (DDI_FAILURE);
	}

	uhcip->uhci_dma_addr_bind_flag |= UHCI_TD_POOL_BOUND;

	/*
	 * 16 bytes alignment is required for every TD start addr
	 * First align the physical address and then shift the virtual pointer.
	 */

	offset = (uhcip->uhci_td_pool_cookie.dmac_address & 0xf);
	offset = UHCI_TD_ALIGN_SZ - offset;

	uhcip->uhci_td_pool_addr = (gtd *)((char *)
			uhcip->uhci_td_pool_addr+offset);
	uhcip->uhci_td_pool_cookie.dmac_address += offset;

	bzero((void *)uhcip->uhci_td_pool_addr, td_pool_size * sizeof (gtd));

	/* Initialize the TD pool */
	for (i = 0; i < td_pool_size; i++)
		uhcip->uhci_td_pool_addr[i].flag = TD_FLAG_FREE;

	/* Allocate the TD pool DMA handle */
	if (ddi_dma_alloc_handle(dip,
			&uhcip->uhci_dma_attr,
			DDI_DMA_SLEEP,
			0,
			&uhcip->uhci_qh_pool_dma_handle) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	/* Allocate the memory for the QH pool */
	if (ddi_dma_mem_alloc(uhcip->uhci_qh_pool_dma_handle,
			qh_pool_size * sizeof (queue_head_t) + UHCI_QH_ALIGN_SZ,
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			0,
			(caddr_t *)&uhcip->uhci_qh_pool_addr,
			&real_length,
			&uhcip->uhci_qh_pool_mem_handle) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	result = ddi_dma_addr_bind_handle(uhcip->uhci_qh_pool_dma_handle,
			NULL,
			(caddr_t)uhcip->uhci_qh_pool_addr,
			real_length,
			DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			NULL,
			&uhcip->uhci_qh_pool_cookie,
			&ccount);

	/* Process the result */
	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
				"uhci_allocate_pools: More than 1 cookie");
			return (DDI_FAILURE);
		}
	} else {
		uhci_decode_ddi_dma_addr_bind_handle_result(uhcip, result);
		return (DDI_FAILURE);
	}

	uhcip->uhci_dma_addr_bind_flag |= UHCI_QH_POOL_BOUND;

	/*
	 * 16 bytes alignment is required for every QH start addr
	 * First align the physical address and then shift the virtual pointer.
	 */

	offset = (uhcip->uhci_qh_pool_cookie.dmac_address & 0xf);
	offset = UHCI_QH_ALIGN_SZ - offset;

	uhcip->uhci_qh_pool_addr = (queue_head_t *)
			((char *)uhcip->uhci_qh_pool_addr + offset);
	uhcip->uhci_qh_pool_cookie.dmac_address += offset;

	bzero((void *)uhcip->uhci_qh_pool_addr,
			qh_pool_size*sizeof (queue_head_t));

	/* Initialize the QH pool */
	for (i = 0; i < qh_pool_size; i ++)
		uhcip->uhci_qh_pool_addr[i].qh_flag = QUEUE_HEAD_FLAG_FREE;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_allocate_pools: Completed");

	return (DDI_SUCCESS);
}

/*
 * uhci_decode_ddi_dma_addr_bind_handle_result:
 *
 * Process the return values of ddi_dma_addr_bind_handle()
 */

void
uhci_decode_ddi_dma_addr_bind_handle_result(uhci_state_t *uhcip,
	int result)
{
	USB_DPRINTF_L2(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"uhci_decode_ddi_dma_addr_bind_handle_result:");

	switch (result) {
		case DDI_DMA_PARTIAL_MAP:
			USB_DPRINTF_L2(PRINT_MASK_ALL, uhcip->uhci_log_hdl,
				"Partial transfers not allowed");
			break;
		case DDI_DMA_INUSE:
			USB_DPRINTF_L2(PRINT_MASK_ALL,  uhcip->uhci_log_hdl,
				"Handle is in use");
			break;
		case DDI_DMA_NORESOURCES:
			USB_DPRINTF_L2(PRINT_MASK_ALL,  uhcip->uhci_log_hdl,
				"No resources");
			break;
		case DDI_DMA_NOMAPPING:
			USB_DPRINTF_L2(PRINT_MASK_ALL,  uhcip->uhci_log_hdl,
				"No mapping");
			break;
		case DDI_DMA_TOOBIG:
			USB_DPRINTF_L2(PRINT_MASK_ALL,  uhcip->uhci_log_hdl,
				"Object is too big");
			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_ALL,  uhcip->uhci_log_hdl,
				"Unknown dma error");
	}
}

/*
 * uhci_register_intrs_and_init_mutex:
 *
 * Register interrupts and initialize each mutex and condition variables
 */
int
uhci_register_intrs_and_init_mutex(dev_info_t *dip, uhci_state_t *uhcip)
{

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_register_intrs_and_init_mutex:");

	/* Test for high level mutex */
	if (ddi_intr_hilevel(dip, 0) != 0) {
		USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_register_intrs_and_init_mutex:"
			"Hi level int not supported");
		return (DDI_FAILURE);
	}

	/*
	 * First call ddi_get_iblock_cookie() to retrieve the interrupt
	 * block cookie so that the mutexes may be initialized before
	 * adding the interrupt. If the mutexes are initialized after
	 * adding the interrupt, there could be a race condition.
	 */
	if (ddi_get_iblock_cookie(dip, 0,
			&uhcip->uhci_iblk_cookie) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/* Initialize the mutex */
	mutex_init(&uhcip->uhci_int_mutex, NULL, MUTEX_DRIVER,
				uhcip->uhci_iblk_cookie);

	if (ddi_add_intr(dip, 0, &uhcip->uhci_iblk_cookie, NULL, uhci_intr,
		(caddr_t)uhcip) != DDI_SUCCESS) {
		USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_reg_ints: ddi_add_intr failed");
		mutex_destroy(&uhcip->uhci_int_mutex);

		return (DDI_FAILURE);
	}

	/* Create prototype condition variable */
	cv_init(&uhcip->uhci_cv_SOF, "uhci_CV_SOF", CV_DRIVER, NULL);

	/* Semaphore to serialize opens and closes */
	sema_init(&uhcip->uhci_ocsem, 1, NULL, SEMA_DRIVER, NULL);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_register_intrs_and_init_mutex: Completed");

	return (DDI_SUCCESS);
}

/*
 * uhci_init_ctlr:
 *
 * Initialize the Host Controller (HC).
 */

int
uhci_init_ctlr(dev_info_t *dip, uhci_state_t *uhcip)
{
	uint_t			cmd_reg;
	uint_t			frame_base_addr;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl, "init_ctlr:");

	mutex_enter(&uhcip->uhci_int_mutex);

	/*
	 * Clear the legacy mode register.
	 */

	if (pci_config_setup(dip, &uhcip->uhci_config_handle) != DDI_SUCCESS) {
		USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_init_ctlr: Config error");
		return (UHCI_FAILURE);
	}


	pci_config_put16(uhcip->uhci_config_handle,
		LEGACYMODE_REG_OFFSET,
		LEGACYMODE_REG_INIT_VALUE);

	pci_config_teardown(&uhcip->uhci_config_handle);

	cmd_reg = Get_OpReg16(USBCMD);
	cmd_reg &= (~USBCMD_REG_HC_RUN);

	/* Stop the controller */
	Set_OpReg16(USBCMD, cmd_reg);

	/* Reset the host controller */
	Set_OpReg16(USBCMD, USBCMD_REG_GBL_RESET);

	/* Wait 10ms for reset to complete */
	drv_usecwait(UHCI_RESET_DELAY);

	Set_OpReg16(USBCMD, 0);

	/* Set the frame number to zero */
	Set_OpReg16(FRNUM, 0);

	/* Initialize the Frame list base address area */
	if (uhci_init_frame_lst_table(dip, uhcip) != DDI_SUCCESS) {
		mutex_exit(&uhcip->uhci_int_mutex);
		return (DDI_FAILURE);
	}


	/* Save the contents of the Frame Interval Registers */
	uhcip->uhci_frame_interval = Get_OpReg8(SOFMOD);

	frame_base_addr = (uhcip->uhci_flt_cookie.dmac_address & 0xFFFFF000);

	/* Set the Frame list base address */
	Set_OpReg32(FRBASEADD, frame_base_addr);

	/*
	 * Set HcInterruptEnable to enable all interrupts except Root
	 * Hub Status change and SOF interrupts.
	 */
	Set_OpReg16(USBINTR, ENABLE_ALL_INTRS);

	/*
	 * Begin sending SOFs
	 * Set the Host Controller Functional State to Operational
	 */
	cmd_reg = Get_OpReg16(USBCMD);
	cmd_reg |= (USBCMD_REG_HC_RUN | USBCMD_REG_MAXPKT_64 |
				USBCMD_REG_CONFIG_FLAG);

	Set_OpReg16(USBCMD, cmd_reg);

	mutex_exit(&uhcip->uhci_int_mutex);

	USB_DPRINTF_L4(PRINT_MASK_ATTA,
		uhcip->uhci_log_hdl, "uhci_init_ctlr: Completed");

	return (DDI_SUCCESS);
}


/*
 * uhci_map_regs:
 *
 * The Host Controller (HC) contains a set of on-chip operational registers
 * and which should be mapped into a non-cacheable portion of the  system
 * addressable space.
 */

int
uhci_map_regs(dev_info_t *dip, uhci_state_t *uhcip)
{
	ddi_device_acc_attr_t	attr;
	uint16_t		command_reg;
	caddr_t			regs_list;
	ulong_t			*longp;
	int			regs_prop_len, index;
	int			rnumber;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl, "uhci_map_regs:");

	/* The host controller will be little endian */
	attr.devacc_attr_version	= DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags	= DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder	= DDI_STRICTORDER_ACC;

	if (ddi_getlongprop(DDI_DEV_T_ANY, uhcip->uhci_dip,
		DDI_PROP_DONTPASS, "reg", (caddr_t)&regs_list,
		&regs_prop_len) != DDI_PROP_SUCCESS) {
		return (DDI_FAILURE);
	}

	longp = (ulong_t *)regs_list;

	for (index = 0; index < (regs_prop_len / 4); index++)
		if (longp[index*5] & UHCI_PROP_MASK)
			break;

	rnumber = index;

	/*
	 * Deallocate the memory allocated by the ddi_getlongprop
	 */

	kmem_free(regs_list, regs_prop_len);

	/* Map in operational registers */
	if (ddi_regs_map_setup(dip, rnumber,
			(caddr_t *)&uhcip->uhci_regsp,
			0,
			sizeof (hc_regs_t),
			&attr,
			&uhcip->uhci_regs_handle) != DDI_SUCCESS) {

		cmn_err(CE_WARN, "ddi_regs_map_setup: failed");
		return (DDI_FAILURE);
	}

	if (pci_config_setup(dip, &uhcip->uhci_config_handle) != DDI_SUCCESS) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
				"uhci_map_regs: Config error");
			return (DDI_FAILURE);
	}

	/* Make sure Memory Access Enable and Master Enable are set */
	command_reg = pci_config_get16(uhcip->uhci_config_handle,
					PCI_CONF_COMM);

	if (!(command_reg & (PCI_COMM_MAE | PCI_COMM_ME)))
		USB_DPRINTF_L3(PRINT_MASK_ATTA,
			uhcip->uhci_log_hdl, "uhci_map_regs: No MAE/ME");

	command_reg |= PCI_COMM_MAE | PCI_COMM_ME;

	pci_config_put16(uhcip->uhci_config_handle, PCI_CONF_COMM, command_reg);

	pci_config_teardown(&uhcip->uhci_config_handle);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_map_regs: Completed");

	return (DDI_SUCCESS);
}

/*
 * uhci_set_dma_attributes:
 *
 * Set the limits in the DMA attributes structure. Most of the values used
 * in the  DMA limit structres are the default values as specified by  the
 * Writing PCI device drivers document.
 */

void
uhci_set_dma_attributes(uhci_state_t *uhcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
	    "uhci_set_dma_attributes:");

	/* Initialize the DMA attributes */
	uhcip->uhci_dma_attr.dma_attr_version = DMA_ATTR_V0;
	uhcip->uhci_dma_attr.dma_attr_addr_lo = 0x00000000ull;
	uhcip->uhci_dma_attr.dma_attr_addr_hi = 0xfffffffeull;

	/* 32 bit addressing */
	uhcip->uhci_dma_attr.dma_attr_count_max = 0xffffffull;

	/*
	 * Setting the dam_att_align to 512, some times fails the
	 * binding handle. I dont know why ? But setting to 1 will
	 * be right for our case
	 */

	/* Byte alignment */
	uhcip->uhci_dma_attr.dma_attr_align = 0x1;

	/*
	 * Since PCI  specification is byte alignment, the
	 * burstsize field should be set to 1 for PCI devices.
	 */
	uhcip->uhci_dma_attr.dma_attr_burstsizes = 0x1;

	uhcip->uhci_dma_attr.dma_attr_minxfer	= 0x1;
	uhcip->uhci_dma_attr.dma_attr_maxxfer	= 0xffffffull;
	uhcip->uhci_dma_attr.dma_attr_seg	= 0xffffffffull;
	uhcip->uhci_dma_attr.dma_attr_sgllen	= 1;
	uhcip->uhci_dma_attr.dma_attr_granular	= 1;
	uhcip->uhci_dma_attr.dma_attr_flags	= 0;
}

uint_t
pow_2(uint_t x)
{

	if (x == 0)
		return (1);
	else
		return (1 << x);

}

uint_t
log_2(uint_t x)
{
	int ret_val = 0;

	while (x != 1) {
		ret_val++;
		x = x >> 1;
	}

	return (ret_val);
}

/*
 * uhci_obtain_state:
 */
uhci_state_t *
uhci_obtain_state(dev_info_t *dip)
{
	int instance = ddi_get_instance(dip);

	uhci_state_t *state = ddi_get_soft_state(uhci_statep, instance);

	ASSERT(state != NULL);

	return (state);
}


/*
 * uhci_alloc_hcdi_ops:
 *
 * The HCDI interfaces or entry points are the software interfaces used by
 * the Universal Serial Bus Driver  (USBA) to  access the services of the
 * Host Controller Driver (HCD).  During HCD initialization, inform  USBA
 * about all available HCDI interfaces or entry points.
 */

usb_hcdi_ops_t *
uhci_alloc_hcdi_ops(uhci_state_t *uhcip)
{
	usb_hcdi_ops_t    *usb_hcdi_ops;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_alloc_hcdi_ops:");

	usb_hcdi_ops = usba_alloc_hcdi_ops();

	usb_hcdi_ops->usb_hcdi_client_init = uhci_hcdi_client_init;
	usb_hcdi_ops->usb_hcdi_client_free = uhci_hcdi_client_free;

	usb_hcdi_ops->usb_hcdi_pipe_open   = uhci_hcdi_pipe_open;
	usb_hcdi_ops->usb_hcdi_pipe_close  = uhci_hcdi_pipe_close;

	usb_hcdi_ops->usb_hcdi_pipe_reset = uhci_hcdi_pipe_reset;
	usb_hcdi_ops->usb_hcdi_pipe_abort = uhci_hcdi_pipe_abort;

	usb_hcdi_ops->usb_hcdi_pipe_get_policy    =
					uhci_hcdi_pipe_get_policy;
	usb_hcdi_ops->usb_hcdi_pipe_set_policy    =
					uhci_hcdi_pipe_set_policy;

	usb_hcdi_ops->usb_hcdi_pipe_device_ctrl_receive =
					uhci_hcdi_pipe_device_ctrl_receive;
	usb_hcdi_ops->usb_hcdi_pipe_device_ctrl_send =
					uhci_hcdi_pipe_device_ctrl_send;

	usb_hcdi_ops->usb_hcdi_bulk_transfer_size =
					uhci_hcdi_bulk_transfer_size;
	usb_hcdi_ops->usb_hcdi_pipe_receive_bulk_data =
					uhci_hcdi_pipe_receive_bulk_data;
	usb_hcdi_ops->usb_hcdi_pipe_send_bulk_data =
					uhci_hcdi_pipe_send_bulk_data;

	usb_hcdi_ops->usb_hcdi_pipe_start_polling =
					uhci_hcdi_pipe_start_polling;
	usb_hcdi_ops->usb_hcdi_pipe_stop_polling =
					uhci_hcdi_pipe_stop_polling;

	usb_hcdi_ops->usb_hcdi_pipe_send_isoc_data =
					uhci_hcdi_pipe_send_isoc_data;

	usb_hcdi_ops->usb_hcdi_console_input_init =
					uhci_hcdi_polled_input_init;

	usb_hcdi_ops->usb_hcdi_console_input_enter =
					uhci_hcdi_polled_input_enter;

	usb_hcdi_ops->usb_hcdi_console_read = uhci_hcdi_polled_read;

	usb_hcdi_ops->usb_hcdi_console_input_exit =
					uhci_hcdi_polled_input_exit;

	usb_hcdi_ops->usb_hcdi_console_input_fini =
					uhci_hcdi_polled_input_fini;
	return (usb_hcdi_ops);
}


/*
 * uhci_init_frame_lst_table :
 *
 * Allocate the system memory and initialize Host Controller
 * Frame list table area The starting of the Frame list Table
 * area must be 4096 byte aligned.
 */

int
uhci_init_frame_lst_table(dev_info_t *dip, uhci_state_t *uhcip)
{
	ddi_device_acc_attr_t	dev_attr;
	size_t			real_length;
	int			result;
	uintptr_t		addr;
	uint_t			offset;
	uint_t			ccount;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_init_frame_lst_table :");

	/* The host controller will be little endian */
	dev_attr.devacc_attr_version		= DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags	= DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder		= DDI_STRICTORDER_ACC;

	/* Create space for the HCCA block */
	if (ddi_dma_alloc_handle(dip, &uhcip->uhci_dma_attr,
			DDI_DMA_SLEEP,
			0,
			&uhcip->uhci_flt_dma_handle)
			!= DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	if (ddi_dma_mem_alloc(uhcip->uhci_flt_dma_handle,
			2 * SIZE_OF_FRAME_LST_TABLE,
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			0,
			(caddr_t *)&uhcip->uhci_frame_lst_tablep,
			&real_length,
			&uhcip->uhci_flt_mem_handle)) {

		return (DDI_FAILURE);
	}

	/* Map the whole Frame list base area into the I/O address space */
	result = ddi_dma_addr_bind_handle(uhcip->uhci_flt_dma_handle,
			NULL,
			(caddr_t)uhcip->uhci_frame_lst_tablep,
			real_length,
			DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP, NULL,
			&uhcip->uhci_flt_cookie,
			&ccount);

	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_init_frame_list_table: More than 1 cookie");

			return (DDI_FAILURE);
		}
	} else {
		uhci_decode_ddi_dma_addr_bind_handle_result(uhcip, result);

		return (DDI_FAILURE);
	}

	uhcip->uhci_dma_addr_bind_flag |= UHCI_FLA_POOL_BOUND;
	/*
	**   Align to 4K boundry
	*/

	bzero((void *)uhcip->uhci_frame_lst_tablep, real_length);

	addr   = uhcip->uhci_flt_cookie.dmac_address & 0xfffff000;
	offset = uhcip->uhci_flt_cookie.dmac_address - addr;

	offset = UHCI_4K_ALIGN - offset;

	uhcip->uhci_frame_lst_tablep = (uint32_t *)
			(((char *)uhcip->uhci_frame_lst_tablep) + offset);
	uhcip->uhci_flt_cookie.dmac_address =
			uhcip->uhci_flt_cookie.dmac_address + offset;

	/* Initialize the interrupt lists */
	if (uhci_build_interrupt_lattice(uhcip) == UHCI_FAILURE)
		return (DDI_FAILURE);

	return (DDI_SUCCESS);
}


/*
 * uhci_alloc_queue_head:
 *
 * Allocate a queue head
 */

queue_head_t *
uhci_alloc_queue_head(uhci_state_t *uhcip)
{
	int		index;
	queue_head_t	*queue_head;
	gtd		*dummy_td;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"uhci_alloc_queue_head");

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/*
	 * The first 63 queue heads in the Queue Head (QH)
	 * buffer pool are reserved for building interrupt lattice
	 * tree. Search for a blank Queue head in the QH buffer pool.
	 */
	for (index = NUM_STATIC_NODES; index < qh_pool_size; index++) {
		if (uhcip->uhci_qh_pool_addr[index].qh_flag ==
			QUEUE_HEAD_FLAG_FREE)
			break;
	}

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"uhci_alloc_queue_head: Allocated %d", index);

	if (index == qh_pool_size) {
		cmn_err(CE_WARN, "All the Queue heads are exhausted");
		USB_DPRINTF_L2(PRINT_MASK_ALLOC,  uhcip->uhci_log_hdl,
				"uhci_alloc_queue_head: QH exhausted");
		return (NULL);
	}

	queue_head = &uhcip->uhci_qh_pool_addr[index];

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"uhci_alloc_queue_head: Allocated address 0x%p",
		(void *)queue_head);

	bzero((void *)queue_head, sizeof (queue_head_t));

	queue_head->link_ptr	= HC_END_OF_LIST;
	queue_head->element_ptr	= HC_END_OF_LIST;
	queue_head->prev_qh	= NULL;
	queue_head->qh_flag	= QUEUE_HEAD_FLAG_BUSY;

	dummy_td = uhci_allocate_td_from_pool(uhcip);
	if (dummy_td == NULL) {
		cmn_err(CE_WARN, "uhci_allocate_td_from_pool failed\n");
		return (UHCI_FAILURE);
	}
	dummy_td->flag = TD_FLAG_DUMMY;

	bzero((char *)dummy_td, sizeof (gtd));

	queue_head->td_tailp    = dummy_td;
	queue_head->element_ptr = (uint_t)TD_PADDR(dummy_td);

	return (queue_head);
}


/*
 * uhci_allocate_bandwidth:
 *
 * Figure out whether or not this interval may be supported. Return
 * the index into the  lattice if it can be supported.  Return
 * allocation failure if it can not be supported.
 */

int
uhci_allocate_bandwidth(uhci_state_t *uhcip,
		usb_pipe_handle_impl_t *pipe_handle)
{
	int		bandwidth;	/* Requested bandwidth */
	uint_t		min, min_index;
	uint_t		i;
	uint_t		height;		/* Bandwidth's height in the tree */
	uint_t		node;
	uint_t		leftmost;
	uint_t		length;
	uint32_t	paddr;
	queue_head_t	*tmp_qh;
	usb_endpoint_descr_t *endpoint = pipe_handle->p_endpoint;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/*
	 * Calculate the length in bytes of a transaction on this
	 * periodic endpoint.
	 */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);

	length = uhci_compute_total_bandwidth(endpoint,
			pipe_handle->p_usb_device->usb_port_status);

	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/*
	 * If the length in bytes plus the allocated bandwidth exceeds
	 * the maximum, return bandwidth allocation failure.
	 */

	if ((length + uhcip->uhci_bandwidth_intr_min +
		uhcip->uhci_bandwidth_isoch_sum) >
		(MAX_PERIODIC_BANDWIDTH)) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_allocate_bandwidth: "
		"Reached maximum bandwidth value and cannot allocate "
		"bandwidth for a given Interrupt/Isoch endpoint");

		return (USB_FAILURE);
	}

	/*
	 * ISOC xfers are not supported at this point type
	 */
	if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK) == USB_EPT_ATTR_ISOCH)
		return (USB_FAILURE);

	/*
	 * This is an interrupt endpoint
	 * Adjust bandwidth to be a power of 2
	 */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);

	bandwidth = uhci_bandwidth_adjust(uhcip, endpoint,
			pipe_handle->p_usb_device->usb_port_status);

	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/*
	 * If this bandwidth can't be supported,
	 * return allocation failure.
	 */
	if (bandwidth == USB_FAILURE)
		return (USB_FAILURE);

	USB_DPRINTF_L4(PRINT_MASK_BW, uhcip->uhci_log_hdl,
		"The new bandwidth is %d", bandwidth);

	/* Find the leaf with the smallest allocated bandwidth */
	min_index = 0;
	min = uhcip->uhci_bandwidth[0];

	for (i = 1; i < NUM_FRAME_LST_ENTRIES; i++) {
		if (uhcip->uhci_bandwidth[i] < min) {
			min_index	= i;
			min		= uhcip->uhci_bandwidth[i];
		}
	}


	USB_DPRINTF_L4(PRINT_MASK_BW, uhcip->uhci_log_hdl,
		"The leaf with minimal bandwidth %d", min_index);

	USB_DPRINTF_L4(PRINT_MASK_BW, uhcip->uhci_log_hdl,
		"The smallest bandwidth %d", min);


	/*
	 * Find the index into the lattice given the
	 * leaf with the smallest allocated bandwidth.
	 */

	height = uhci_lattice_height(bandwidth);

	USB_DPRINTF_L4(PRINT_MASK_BW,
		uhcip->uhci_log_hdl, "The height is %d", height);

	node = tree_bottom_nodes[min_index];

	paddr = (uhcip->uhci_frame_lst_tablep[node] & FRAME_LST_PTR_MASK);

	tmp_qh = (queue_head_t *)(QH_VADDR(paddr));

	node = tmp_qh->node;

	for (i = 0; i < height; i++)
		node = uhci_lattice_parent(node);

	USB_DPRINTF_L4(PRINT_MASK_BW, uhcip->uhci_log_hdl,
		"The real node is %d", node);

	/*
	 * Find the leftmost leaf in the subtree
	 * specified by the node.
	 */
	leftmost = uhci_leftmost_leaf(node, height);

	USB_DPRINTF_L4(PRINT_MASK_BW,
		uhcip->uhci_log_hdl, "Leftmost %d", leftmost);

	for (i = leftmost; i < leftmost +
			(NUM_FRAME_LST_ENTRIES/bandwidth); i ++) {

		if ((length + uhcip->uhci_bandwidth_isoch_sum +
			uhcip->uhci_bandwidth[i]) > MAX_PERIODIC_BANDWIDTH) {

			USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_allocate_bandwidth: "
			"Reached maximum bandwidth value and cannot allocate "
			"bandwidth for Interrupt endpoint");
			return (USB_FAILURE);
		}
	}

	/*
	 * All the leaves for this node must be updated with the bandwidth.
	 */
	for (i = leftmost; i < leftmost +
				(NUM_FRAME_LST_ENTRIES/bandwidth); i ++)
		uhcip->uhci_bandwidth[i] = uhcip->uhci_bandwidth[i] + length;

	/* Find the leaf with the smallest allocated bandwidth */
	min_index = 0;
	min = uhcip->uhci_bandwidth[0];

	for (i = 1; i < NUM_FRAME_LST_ENTRIES; i++) {
		if (uhcip->uhci_bandwidth[i] < min) {
			min_index = i;
			min = uhcip->uhci_bandwidth[i];
		}
	}

	/* Save the minimum for later use */
	uhcip->uhci_bandwidth_intr_min = min;

	/*
	 * Return the index of the
	 * node within the lattice.
	 */

	return (node);
}


/*
 * uhci_deallocate_bandwidth:
 *
 * Deallocate bandwidth for the given node in the lattice and the length of
 * transfer.
 */
void
uhci_deallocate_bandwidth(uhci_state_t *uhcip,
    usb_pipe_handle_impl_t *pipe_handle)
{
	uint_t		bandwidth;
	uint_t		height;
	uint_t		leftmost;
	uint_t		i;
	uint_t		min;
	usb_endpoint_descr_t *endpoint = pipe_handle->p_endpoint;
	uint_t		node, length;
	uhci_pipe_private_t *pp =
			(uhci_pipe_private_t *)pipe_handle->p_hcd_private;

	/* This routine is protected by the uhci_int_mutex */
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/* Obtain the length */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);
	length = uhci_compute_total_bandwidth(endpoint,
			pipe_handle->p_usb_device->usb_port_status);
	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/*
	 * If this is an isochronous endpoint, just delete endpoint's
	 * bandwidth from the total allocated isochronous bandwidth.
	 */
	if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK)
		== USB_EPT_ATTR_ISOCH) {
		uhcip->uhci_bandwidth_isoch_sum =
			uhcip->uhci_bandwidth_isoch_sum - length;

		return;
	}

	/* Obtain the node */
	node = pp->pp_node;

	/* Adjust bandwidth to be a power of 2 */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);

	bandwidth = uhci_bandwidth_adjust(uhcip, endpoint,
			pipe_handle->p_usb_device->usb_port_status);

	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/* Find the height in the tree */
	height = uhci_lattice_height(bandwidth);

	/*
	 * Find the leftmost leaf in the subtree specified by the node
	 */
	leftmost = uhci_leftmost_leaf(node, height);

	/* Delete the bandwith from the appropriate lists */
	for (i = leftmost; i < leftmost +
		(NUM_FRAME_LST_ENTRIES/bandwidth); i ++)
		uhcip->uhci_bandwidth[i] = uhcip->uhci_bandwidth[i] - length;

	min = uhcip->uhci_bandwidth[0];

	/* Recompute the minimum */
	for (i = 1; i < NUM_FRAME_LST_ENTRIES; i++) {
		if (uhcip->uhci_bandwidth[i] < min) {
			min = uhcip->uhci_bandwidth[i];
		}
	}

	/* Save the minimum for later use */
	uhcip->uhci_bandwidth_intr_min = min;
}

/*
 * uhci_compute_total_bandwidth:
 *
 * Given a periodic endpoint (interrupt or isochronous) determine the total
 * bandwidth for one transaction. The UHCI host controller traverses the
 * endpoint descriptor lists on a first-come-first-serve basis. When the HC
 * services an endpoint, only a single transaction attempt is made. The  HC
 * moves to the next Endpoint Descriptor after the first transaction attempt
 * rather than finishing the entire Transfer Descriptor. Therefore, when  a
 * Transfer Descriptor is inserted into the lattice, we will only count the
 * number of bytes for one transaction.
 *
 * The following are the formulas used for  calculating bandwidth in  terms
 * bytes and it is for the single USB full speed and low speed  transaction
 * respectively. The protocol overheads will be different for each of  type
 * of USB transfer and all these formulas & protocol overheads are  derived
 * from the 5.9.3 section of USB Specification & with the help of Bandwidth
 * Analysis white paper which is posted on the USB  developer forum.
 *
 * Full-Speed:
 *        Protocol overhead  + ((MaxPacketSize * 7)/6 )  + Host_Delay
 *
 * Low-Speed:
 *              Protocol overhead  + Hub LS overhead +
 *                (Low-Speed clock * ((MaxPacketSize * 7)/6 )) + Host_Delay
 */

uint_t
uhci_compute_total_bandwidth(usb_endpoint_descr_t *endpoint,
		usb_port_status_t port_status)
{
	ushort_t	MaxPacketSize = endpoint->wMaxPacketSize;
	uint_t		bandwidth = 0;

	/* Add Host Controller specific delay to required bandwidth */
	bandwidth = HOST_CONTROLLER_DELAY;

	/* Add bit-stuffing overhead */
	MaxPacketSize = (ushort_t)((MaxPacketSize * 7) / 6);

	/* Low Speed interrupt transaction */
	if (port_status == USB_LOW_SPEED_DEV) {
		/* Low Speed interrupt transaction */
		bandwidth += (LOW_SPEED_PROTO_OVERHEAD +
				HUB_LOW_SPEED_PROTO_OVERHEAD +
				(LOW_SPEED_CLOCK * MaxPacketSize));
	} else {
		/* Full Speed transaction */
		bandwidth += MaxPacketSize;

		if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK) ==
			USB_EPT_ATTR_INTR) {
			/* Full Speed interrupt transaction */
			bandwidth += FS_NON_ISOC_PROTO_OVERHEAD;
		} else {
			/* Isochronus and input transaction */
			if ((endpoint->bEndpointAddress &
				USB_EPT_DIR_MASK) == USB_EPT_DIR_IN) {
				bandwidth += FS_ISOC_INPUT_PROTO_OVERHEAD;
			} else {
				/* Isochronus and output transaction */
				bandwidth += FS_ISOC_OUTPUT_PROTO_OVERHEAD;
			}
		}
	}

	return (bandwidth);
}


/*
 * uhci_bandwidth_adjust:
 */
int
uhci_bandwidth_adjust(uhci_state_t *uhcip,
		usb_endpoint_descr_t *endpoint,
		usb_port_status_t port_status)
{
	uint_t		interval;
	int		i = 0;

	/*
	 * Get the polling interval from the endpoint descriptor
	 */
	interval = endpoint->bInterval;

	/*
	 * The bInterval value in the endpoint descriptor can range
	 * from 1 to 255ms. The interrupt lattice has 32 leaf nodes,
	 * and the host controller cycles through these nodes every
	 * 32ms. The longest polling  interval that the  controller
	 * supports is 32ms.
	 */

	/*
	 * Return an error if the polling interval is less than 1ms
	 * and greater than 255ms
	 */
	if ((interval < MIN_POLL_INTERVAL) ||
			(interval > MAX_POLL_INTERVAL)) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_bandwidth_adjust: "
		"Endpoint's poll interval must be between %d and %d ms",
		MIN_POLL_INTERVAL, MAX_POLL_INTERVAL);

		return (USB_FAILURE);
	}

	/*
	 * According USB Specifications, a  full-speed endpoint can
	 * specify a desired polling interval 1ms to 255ms and a low
	 * speed  endpoints are limited to  specifying only 10ms to
	 * 255ms. But some old keyboards & mice uses polling interval
	 * of 8ms. For compatibility  purpose, we are using polling
	 * interval between 8ms & 255ms for low speed endpoints. But
	 * uhci driver will reject the any low speed endpoints which
	 * request polling interval less than 8ms.
	 */
	if ((port_status == USB_LOW_SPEED_DEV) &&
		(interval < MIN_LOW_SPEED_POLL_INTERVAL)) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_bandwidth_adjust: "
		"Low speed endpoint's poll interval must be less than %d ms",
		MIN_LOW_SPEED_POLL_INTERVAL);

		return (USB_FAILURE);
	}

	/*
	 * If polling interval is greater than 32ms,
	 * adjust polling interval equal to 32ms.
	 */
	if (interval > 32)
		interval = 32;

	/*
	 * Find the nearest power of 2 that'sless
	 * than interval.
	 */
	while ((pow_2(i)) <= interval)
		i++;

	return (pow_2((i - 1)));
}


/*
 * uhci_lattice_height:
 *
 * Given the requested bandwidth, find the height in the tree at
 * which the nodes for this bandwidth fall.  The height is measured
 * as the number of nodes from the leaf to the level specified by
 * bandwidth The root of the tree is at height TREE_HEIGHT.
 */
uint_t
uhci_lattice_height(uint_t bandwidth)
{
	return (TREE_HEIGHT - (log_2(bandwidth)));
}

uint_t
uhci_lattice_parent(uint_t node)
{
	if ((node % 2) == 0)
		return ((node/2) - 1);
	else
		return (node/2);
}

/*
 * uhci_leftmost_leaf:
 *
 * Find the leftmost leaf in the subtree specified by the node.
 * Height refers to number of nodes from the bottom of the tree
 * to the node,  including the node.
 */

uint_t
uhci_leftmost_leaf(uint_t node, uint_t height)
{
	node = pow_2(height+VIRTUAL_TREE_HEIGHT)*(node+1) -
			NUM_FRAME_LST_ENTRIES;
	return (node);
}

/*
 * uhci_insert_qh:
 *
 * Add the Queue Head (QH) into the Host Controller's (HC)
 * appropriate queue head list.
 */

void
uhci_insert_qh(uhci_state_t *uhcip, usb_pipe_handle_impl_t *ph)
{
	uhci_pipe_private_t *pp = (uhci_pipe_private_t *)ph->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_insert_qh:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	switch (ph->p_endpoint->bmAttributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_CONTROL:
			uhci_insert_ctrl_qh(uhcip, pp);
			break;
		case USB_EPT_ATTR_BULK:
			uhci_insert_bulk_qh(uhcip, pp);
			break;
		case USB_EPT_ATTR_INTR:
			uhci_insert_intr_qh(uhcip, pp);
			break;
		case USB_EPT_ATTR_ISOCH:
			cmn_err(CE_WARN, "Illegal request\n");
			break;
	}
}


/*
 * uhci_insert_ctrl_qh:
 *
 * Insert a control QH into the Host Controller's (HC)
 * control QH list.
 */

static void
uhci_insert_ctrl_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp)
{
	queue_head_t *qh = pp->pp_qh;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
		uhcip->uhci_log_hdl, "uhci_insert_ctrl_qh:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	if (uhcip->uhci_ctrl_xfers_q_head ==
			uhcip->uhci_ctrl_xfers_q_tail)
		uhcip->uhci_ctrl_xfers_q_head->prev_qh  = UHCI_INVALID_PTR;

	qh->link_ptr = uhcip->uhci_ctrl_xfers_q_tail->link_ptr;
	qh->prev_qh = uhcip->uhci_ctrl_xfers_q_tail;
	uhcip->uhci_ctrl_xfers_q_tail->link_ptr =
		(uint32_t)(QH_PADDR(qh)) | HC_QUEUE_HEAD;
	uhcip->uhci_ctrl_xfers_q_tail	= qh;

}


/*
 * uhci_insert_bulk_qh:
 *
 * Insert a bulk QH into the Host Controller's (HC) bulk
 * QH list.
 *
 */
static void
uhci_insert_bulk_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp)
{
	queue_head_t *qh = pp->pp_qh;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
		uhcip->uhci_log_hdl, "uhci_insert_bulk_qh:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	if (uhcip->uhci_bulk_xfers_q_head ==
		uhcip->uhci_bulk_xfers_q_tail)
		uhcip->uhci_bulk_xfers_q_head->prev_qh = UHCI_INVALID_PTR;

	qh->prev_qh = uhcip->uhci_bulk_xfers_q_tail;
	uhcip->uhci_bulk_xfers_q_tail->link_ptr =
		(uint32_t)(QH_PADDR(qh)) | HC_QUEUE_HEAD;
	uhcip->uhci_bulk_xfers_q_tail = qh;

}


/*
 * uhci_insert_intr_qh:
 *
 * Insert a periodic Queue head i.e Interrupt queue head into the
 * Host Controller's (HC) interrupt lattice tree.
 */
static void
uhci_insert_intr_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp)
{
	queue_head_t	*qh = pp->pp_qh;
	queue_head_t	*next_lattice_qh, *lattice_qh;

	/*
	 * The appropriate node was found
	 * during the opening of the pipe.
	 */
	uint_t node = pp->pp_node;


	USB_DPRINTF_L4(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_insert_intr_qh:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/* Find the lattice queue head */
	lattice_qh = &uhcip->uhci_qh_pool_addr[node];

	next_lattice_qh =
		(queue_head_t *)(QH_VADDR((lattice_qh->link_ptr
		& QH_LINK_PTR_MASK)));

	next_lattice_qh->prev_qh	= qh;
	qh->link_ptr			= lattice_qh->link_ptr;
	qh->prev_qh			= lattice_qh;
	lattice_qh->link_ptr		=
		(uint32_t)(QH_PADDR(qh)) | HC_QUEUE_HEAD;

	pp->pp_data_toggle = 0;
}

/*
 * uhci_insert_intr_td:
 * Create a Transfer Descriptor (TD) and a data buffer for a interrupt
 * endpoint.
 */

int
uhci_insert_intr_td(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*pipe_handle, uint_t flags,
	uhci_handler_function_t	tw_handle_td,
	usb_opaque_t		tw_handle_callback_value)
{
	uhci_trans_wrapper_t	*tw;
	size_t			length;
	uhci_pipe_private_t	*pp =
			(uhci_pipe_private_t *)pipe_handle->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
			uhcip->uhci_log_hdl, "uhci_insert_intr_td:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/* Obtain the maximum transfer size */
	length = pp->pp_policy.pp_periodic_max_transfer_size;

	/* Allocate a transaction wrapper */
	tw = uhci_create_transfer_wrapper(uhcip, pp, length, flags);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_insert_intr_td: TW allocation failed");

		return (USB_NO_RESOURCES);
	}

	/*
	 * Initialize the callback and any callback
	 * data for when the td completes.
	 */
	tw->tw_handle_td = tw_handle_td;

	tw->tw_handle_callback_value = tw_handle_callback_value;

	/* Insert the td onto the queue head */
	if ((uhci_insert_hc_td(uhcip,
			tw->tw_cookie.dmac_address,
			length,
			pp,
			tw, PID_IN)) != USB_SUCCESS) {
		return (USB_NO_RESOURCES);
	}

	return (USB_SUCCESS);
}

/*
 * uhci_create_transfer_wrapper:
 *
 * Create a Transaction Wrapper (TW) and this involves the allocating of DMA
 * resources.
 */

static uhci_trans_wrapper_t *
uhci_create_transfer_wrapper(
	uhci_state_t		*uhcip,
	uhci_pipe_private_t	*pp,
	size_t			length,
	uint_t			usb_flags)
{
	ddi_device_acc_attr_t	dev_attr;
	int			result;
	size_t			real_length;
	uint_t			ccount;
	uhci_trans_wrapper_t	*tw;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_create_transfer_wrapper: length = 0x%x flags = 0x%x",
		length, usb_flags);

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/* Allocate space for the transfer wrapper */
	tw = kmem_zalloc(sizeof (uhci_trans_wrapper_t), KM_NOSLEEP);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS,  uhcip->uhci_log_hdl,
			"uhci_create_transfer_wrapper: kmem_alloc failed");

		return (NULL);
	}

	tw->tw_length = length;

	/* Allocate the DMA handle */
	result = ddi_dma_alloc_handle(uhcip->uhci_dip,
			&uhcip->uhci_dma_attr,
			DDI_DMA_DONTWAIT,
			0,
			&tw->tw_dmahandle);

	if (result != DDI_SUCCESS) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_create_transfer_wrapper: Alloc handle failed");

		kmem_free(tw, sizeof (uhci_trans_wrapper_t));

		return (NULL);
	}

	dev_attr.devacc_attr_version		= DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags	= DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder		= DDI_STRICTORDER_ACC;

	/* Allocate the memory */
	result = ddi_dma_mem_alloc(tw->tw_dmahandle,
			tw->tw_length,
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_DONTWAIT,
			NULL,
			(caddr_t *)&tw->tw_buf,
			&real_length,
			&tw->tw_accesshandle);

	if (result != DDI_SUCCESS) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_create_transfer_wrapper: dma_mem_alloc fail");
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (uhci_trans_wrapper_t));

		return (NULL);
	}

	ASSERT(real_length >= length);

	/* Bind the handle */
	result = ddi_dma_addr_bind_handle(tw->tw_dmahandle,
			NULL,
			(caddr_t)tw->tw_buf,
			real_length,
			DDI_DMA_RDWR|DDI_DMA_CONSISTENT,
			DDI_DMA_DONTWAIT,
			NULL,
			&tw->tw_cookie,
			&ccount);

	/* Process the result */
	if (result != DDI_DMA_MAPPED) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_create_transfer_wrapper: Bind handle failed");
		ddi_dma_mem_free(&tw->tw_accesshandle);
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (uhci_trans_wrapper_t));

		return (NULL);
	}

	/* The cookie count should be 1 */
	if (ccount != 1) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"create_transfer_wrapper: More than 1 cookie");

		result = ddi_dma_unbind_handle(tw->tw_dmahandle);
		ASSERT(result == DDI_SUCCESS);

		ddi_dma_mem_free(&tw->tw_accesshandle);
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (uhci_trans_wrapper_t));

		return (NULL);
	}

	/*
	 * Only allow one wrapper to be added at a time. Insert the
	 * new transaction wrapper into the list for this pipe.
	 */
	if (pp->pp_tw_head == NULL) {
		pp->pp_tw_head = tw;
		pp->pp_tw_tail = tw;
	} else {
		pp->pp_tw_tail->tw_next = tw;
		pp->pp_tw_tail = tw;
	}

	/* Store a back pointer to the pipe private structure */
	tw->tw_pipe_private = pp;

	/* Store the transfer type - synchronous or asynchronous */
	tw->tw_flags = usb_flags;

	return (tw);
}

/*
 * uhci_insert_hc_td:
 *
 * Insert a Transfer Descriptor (TD) on an QH.
 */

int
uhci_insert_hc_td(uhci_state_t *uhcip,
	uint32_t		buffer_address,
	size_t			hcgtd_length,
	uhci_pipe_private_t	*pp,
	uhci_trans_wrapper_t	*tw,
	uchar_t			PID)
{
	gtd		*td, *current_dummy;
	queue_head_t	*qh = pp->pp_qh;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	td = uhci_allocate_td_from_pool(uhcip);

	if (td == NULL)
		return (USB_NO_RESOURCES);

	current_dummy = qh->td_tailp;

	/*
	 * Fill in the current dummy td and
	 * add the new dummy to the end.
	 */
	uhci_fill_in_td(uhcip,
		td,
		current_dummy,
		buffer_address,
		hcgtd_length,
		pp,
		tw, PID);

	/* Insert this td onto the tw */

	if (tw->tw_hctd_head == NULL) {
		ASSERT(tw->tw_hctd_tail == NULL);
		tw->tw_hctd_head = current_dummy;
		tw->tw_hctd_tail = current_dummy;
	} else {
		/* Add the td to the end of the list */
		tw->tw_hctd_tail->tw_td_next = current_dummy;
		tw->tw_hctd_tail = current_dummy;
	}

	/*
	 * Insert the TD on to the QH. When this occurs,
	 * the Host Controller will see the newly filled in TD
	 */

	current_dummy->oust_td_next	= uhcip->uhci_oust_tds_head;
	current_dummy->oust_td_prev	= NULL;
	if (uhcip->uhci_oust_tds_head != NULL)
		uhcip->uhci_oust_tds_head->oust_td_prev = current_dummy;
	uhcip->uhci_oust_tds_head = current_dummy;

	current_dummy->tw = tw;
	return (USB_SUCCESS);
}

/*
 * uhci_fill_in_td:
 *
 * Fill in the fields of a Transfer Descriptor (TD).
 */
/* ARGSUSED */
static void
uhci_fill_in_td(uhci_state_t	*uhcip,
	gtd			*td,
	gtd			*current_dummy,
	uint32_t		buffer_address,
	size_t			length,
	uhci_pipe_private_t	*pp,
	uhci_trans_wrapper_t	*tw,
	uchar_t			PID)
{
	usb_pipe_handle_impl_t	*ph = pp->pp_pipe_handle;

	/* Clear the TD */
	bzero((char *)td, sizeof (gtd));

	/*
	 * If this is an isochronous TD, just return
	 */
	if ((ph->p_endpoint->bmAttributes & USB_EPT_ATTR_MASK) ==
			USB_EPT_ATTR_ISOCH)
		return;

	current_dummy->link_ptr = TD_PADDR(td);

	if (tw->tw_flags != USB_FLAGS_SHORT_XFER_OK)
		current_dummy->td_dword2.spd = 1;

	mutex_enter(&ph->p_usb_device->usb_mutex);

	if (ph->p_usb_device->usb_port_status == USB_LOW_SPEED_DEV)
		current_dummy->td_dword2.ls = LOW_SPEED_DEVICE;

	current_dummy->td_dword2.c_err = UHCI_MAX_ERR_COUNT;
	current_dummy->td_dword3.max_len	=
				(length == 0) ? 0x7ff: (length -1);

	current_dummy->td_dword3.data_toggle = pp->pp_data_toggle;

	if (pp->pp_data_toggle == 0)
		pp->pp_data_toggle = 1;
	else
		pp->pp_data_toggle = 0;

	current_dummy->td_dword3.device_addr = ph->p_usb_device->usb_addr;

	current_dummy->td_dword3.endpt	=
		ph->p_endpoint->bEndpointAddress & END_POINT_ADDRESS_MASK;

	current_dummy->td_dword3.PID	= PID;
	current_dummy->buffer_address	= buffer_address;

	if (pp->pp_state != PIPE_STOPPED) {
		current_dummy->td_dword2.ioc	= INTERRUPT_ON_COMPLETION;
		current_dummy->td_dword2.status	= 0x80;
	}

	td->qh_td_prev = current_dummy;
	current_dummy->qh_td_prev	= NULL;
	pp->pp_qh->td_tailp		= td;

	mutex_exit(&ph->p_usb_device->usb_mutex);
}


/*
 *  uhci_modify_td_active_bits ()
 *
 *  Sets active bit in all the tds of QH to INACTIVE so that
 *  the HC stops processing the TD's related to the QH.
 */

void
uhci_modify_td_active_bits(uhci_state_t *uhcip, queue_head_t *qh)
{
	gtd *tmp, *qh_td;

	tmp = qh->td_tailp;

	qh_td = (gtd *)TD_VADDR((qh->element_ptr & QH_ELEMENT_PTR_MASK));

	while ((tmp != NULL) && (tmp != qh_td)) {
		tmp->td_dword2.status &= TD_INACTIVE;
		tmp = tmp->qh_td_prev;
	}
}


/*
 * uhci_common_ctrl_routine:
 */
int
uhci_common_ctrl_routine(usb_pipe_handle_impl_t *ph,
	uchar_t		bmRequestType,
	uchar_t		bRequest,
	uint16_t	wValue,
	uint16_t	wIndex,
	uint16_t	wLength,
	mblk_t		*data,
	uint_t		usb_flags)
{
	uhci_state_t			*uhcip;
	uhci_pipe_private_t		*pp;
	usb_dev_t			*usb_dev;
	int				error = USB_SUCCESS;

	uhcip = uhci_obtain_state(
			ph->p_usb_device->usb_root_hub_dip);
	pp	  = (uhci_pipe_private_t *)ph->p_hcd_private;
	usb_dev = (usb_dev_t *)ph->p_usb_device->usb_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
	"uhci_common_ctrl_routine: Flags = %x", usb_flags);

	/*
	 *  Return failure if this is a SET_ADDRESS command and this
	 *  isn't the default pipe.
	 */
	mutex_enter(&uhcip->uhci_int_mutex);

	/*
	 * Intercept root hub requests if this isn't the default pipe
	 * with addr 0 of a device and if this is the root hub.
	 */

	if (usb_dev)
		mutex_enter(&usb_dev->usb_dev_mutex);

	if (ph->p_usb_device->usb_addr == ROOT_HUB_ADDR) {
		if (usb_dev)
			mutex_exit(&usb_dev->usb_dev_mutex);
		error = uhci_handle_root_hub_request(ph,
				bmRequestType,
				bRequest,
				wValue,
				wIndex,
				wLength,
				data,
				usb_flags);
		mutex_exit(&uhcip->uhci_int_mutex);
		return (error);
	}

	if (usb_dev)
		mutex_exit(&usb_dev->usb_dev_mutex);

	mutex_enter(&pp->pp_mutex);

	pp->pp_data_toggle = 0;

	/* Insert the td's on the Queue Head */
	error = uhci_insert_ctrl_td(uhcip, ph,
			bmRequestType,
			bRequest,
			wValue,
			wIndex,
			wLength,
			data, usb_flags);

	if (error != USB_SUCCESS) {
		mutex_exit(&pp->pp_mutex);
		mutex_exit(&uhcip->uhci_int_mutex);

		USB_DPRINTF_L2(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
			"uhci_common_ctrl_routine: No resources");
		return (error);
	}

	/* free mblk */
	if (data)
		freeb(data);

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	return (error);
}


/*
 * uhci_insert_ctrl_td:
 *
 * Create a Transfer Descriptor (TD) and a data buffer for a
 * control Queue Head.
 */

static int
uhci_insert_ctrl_td(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t  *ph,
	uchar_t			bmRequestType,
	uchar_t			bRequest,
	uint16_t		wValue,
	uint16_t		wIndex,
	uint16_t		wLength,
	mblk_t			 *data,
	uint_t			 usb_flags)
{
	uhci_trans_wrapper_t *tw;
	uhci_pipe_private_t  *pp =
			(uhci_pipe_private_t *)ph->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
		uhcip->uhci_log_hdl, "uhci_insert_ctrl_td:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/* Allocate a transaction wrapper */
	tw = uhci_create_transfer_wrapper(
		uhcip, pp, wLength + SETUP_SIZE, usb_flags);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_insert_ctrl_td: TW allocation failed");

		return (USB_NO_RESOURCES);
	}

	tw->tw_bytes_xfered = 0;
	tw->tw_bytes_pending = wLength;

	/*
	 * Initialize the callback and any callback
	 * data for when the td completes.
	 */
	tw->tw_handle_td		= uhci_handle_ctrl_td;

	tw->tw_handle_callback_value = NULL;

	if ((uhci_create_setup_pkt(uhcip, pp, tw,
			bmRequestType,
			bRequest,
			wValue,
			wIndex,
			wLength,
			data, usb_flags)) !=
			USB_SUCCESS) {
		return (USB_NO_RESOURCES);
	}

	tw->tw_ctrl_state = SETUP;

	return (USB_SUCCESS);
}


/*
 * uhci_create_setup_pkt:
 *
 * create a setup packet to initiate a control transfer.
 *
 * OHCI driver has seen the case where devices fail if there is
 * more than one control transfer to the device within a frame.
 * So, the UHCI ensures that only one TD will be put on the control
 * pipe to one device.
 */

static int
uhci_create_setup_pkt(uhci_state_t *uhcip,
	uhci_pipe_private_t *pp,
	uhci_trans_wrapper_t *tw,
	uchar_t bmRequestType,
	uchar_t bRequest,
	uint16_t wValue,
	uint16_t wIndex,
	uint16_t wLength,
	mblk_t *data,
	uint_t usb_flags)
{
	int		sdata;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"uhci_create_setup_pkt: 0x%x 0x%x 0x%x 0x%x 0x%x 0%xp 0%x",
		bmRequestType, bRequest, wValue, wIndex, wLength,
		(void *)data, usb_flags);

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));
	ASSERT(tw != NULL);

	/* Create the first four bytes of the setup packet */
	sdata = (bmRequestType | (bRequest << 8) | (wValue << 16));

	ddi_put32(tw->tw_accesshandle, (uint_t *)tw->tw_buf, sdata);

	/* Create the second four bytes */
	sdata = (uint32_t)(wIndex | (wLength << 16));

	ddi_put32(tw->tw_accesshandle,
		(uint_t *)(tw->tw_buf + sizeof (uint_t)), sdata);

	/*
	 * The TD's are placed on the QH one at a time.
	 * Once this TD is placed on the done list, the
	 * data or status phase TD will be enqueued.
	 */

	if ((uhci_insert_hc_td(uhcip, tw->tw_cookie.dmac_address, SETUP_SIZE,
		pp, tw, PID_SETUP)) != USB_SUCCESS)
		return (USB_NO_RESOURCES);

	USB_DPRINTF_L3(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"Create_setup: pp 0x%p", (void *)pp);

	/*
	 * If this controltransfer has a data phase, record the
	 * direction. If the data phase is an OUT transaction ,
	 * copy the data into the buffer of the transfer wrapper.
	 */
	if (wLength != 0) {
		/* There is a data stage.  Find the direction */
		if (bmRequestType & USB_DEV_REQ_DEVICE_TO_HOST) {
			tw->tw_direction = PID_IN;
		} else {
			tw->tw_direction = PID_OUT;

			/* Copy the data into the buffer */
			ddi_rep_put8(tw->tw_accesshandle,
				data->b_rptr,
				(uint8_t *)(tw->tw_buf + SETUP_SIZE),
				wLength,
				DDI_DEV_AUTOINCR);

		}
	}
	return (USB_SUCCESS);
}


/*
 * uhci_create_stats:
 *
 * Allocate and initialize the uhci kstat structures
 */
void
uhci_create_stats(uhci_state_t *uhcip)
{
	char			kstatname[KSTAT_STRLEN];
	const char		*dname = ddi_driver_name(uhcip->uhci_dip);
	uhci_intrs_stats_t	*isp;
	int			i;
	char			*usbtypes[USB_N_COUNT_KSTATS] =
				    {"ctrl", "isoch", "bulk", "intr"};
	uint_t			instance = uhcip->uhci_instance;

	if (UHCI_INTRS_STATS(uhcip) == NULL) {
		(void) sprintf(kstatname, "%s%d,intrs", dname, instance);
		UHCI_INTRS_STATS(uhcip) = kstat_create("usba", instance,
		    kstatname, "usb_interrupts", KSTAT_TYPE_NAMED,
		    sizeof (uhci_intrs_stats_t) / sizeof (kstat_named_t),
		    KSTAT_FLAG_PERSISTENT);

		if (UHCI_INTRS_STATS(uhcip) != NULL) {
			isp = UHCI_INTRS_STATS_DATA(uhcip);
			kstat_named_init(&isp->uhci_intrs_hc_halted,
			    "HC Halted", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->uhci_intrs_hc_process_err,
			    "HC Process Errors", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->uhci_intrs_host_sys_err,
			    "Host Sys Errors", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->uhci_intrs_resume_detected,
			    "Resume Detected", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->uhci_intrs_usb_err_intr,
			    "USB Error", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->uhci_intrs_usb_intr,
			    "USB Interrupts", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->uhci_intrs_total,
			    "Total Interrupts", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->uhci_intrs_not_claimed,
			    "Not Claimed", KSTAT_DATA_UINT64);

			UHCI_INTRS_STATS(uhcip)->ks_private = uhcip;
			UHCI_INTRS_STATS(uhcip)->ks_update = nulldev;
			kstat_install(UHCI_INTRS_STATS(uhcip));
		}
	}

	if (UHCI_TOTAL_STATS(uhcip) == NULL) {
		(void) sprintf(kstatname, "%s%d,total", dname, instance);
		UHCI_TOTAL_STATS(uhcip) = kstat_create("usba", instance,
		    kstatname, "usb_byte_count", KSTAT_TYPE_IO, 1,
		    KSTAT_FLAG_PERSISTENT);

		if (UHCI_TOTAL_STATS(uhcip) != NULL) {
			kstat_install(UHCI_TOTAL_STATS(uhcip));
		}
	}

	for (i = 0; i < USB_N_COUNT_KSTATS; i++) {
		if (uhcip->uhci_count_stats[i] == NULL) {
			(void) sprintf(kstatname, "%s%d,%s", dname, instance,
			    usbtypes[i]);
			uhcip->uhci_count_stats[i] = kstat_create("usba",
			    instance, kstatname, "usb_byte_count",
			    KSTAT_TYPE_IO, 1, KSTAT_FLAG_PERSISTENT);

			if (uhcip->uhci_count_stats[i] != NULL) {
				kstat_install(uhcip->uhci_count_stats[i]);
			}
		}
	}
}

/*
 * uhci_destroy_stats:
 *
 * Clean up uhci kstat structures
 */
void
uhci_destroy_stats(uhci_state_t *uhcip)
{
	int i;

	if (UHCI_INTRS_STATS(uhcip)) {
		kstat_delete(UHCI_INTRS_STATS(uhcip));
		UHCI_INTRS_STATS(uhcip) = NULL;
	}

	if (UHCI_TOTAL_STATS(uhcip)) {
		kstat_delete(UHCI_TOTAL_STATS(uhcip));
		UHCI_TOTAL_STATS(uhcip) = NULL;
	}

	for (i = 0; i < USB_N_COUNT_KSTATS; i++) {
		if (uhcip->uhci_count_stats[i]) {
			kstat_delete(uhcip->uhci_count_stats[i]);
			uhcip->uhci_count_stats[i] = NULL;
		}
	}
}

/*
 * uhci_cleanup:
 *
 * Cleanup on attach failure or detach
 */

void
uhci_cleanup(uhci_state_t *uhcip, int flags)
{
	uhci_trans_wrapper_t	*tw;
	int			i, flag;
	gtd			*td;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl, "uhci_cleanup:");

	if (flags & UHCI_INTR_HDLR_REGISTER) {

		mutex_enter(&uhcip->uhci_int_mutex);

		/* Disable all HC ED list processing */
		Set_OpReg16(USBINTR, 0);
		Set_OpReg16(USBCMD, 0);

		/* Wait for sometime */
		drv_usecwait(UHCI_TIMEWAIT);

		/* Remove interrupt handler */
		ddi_remove_intr(uhcip->uhci_dip, 0, uhcip->uhci_iblk_cookie);

		mutex_exit(&uhcip->uhci_int_mutex);
	}

	if (uhcip->uhci_timeout_id) {
		untimeout(uhcip->uhci_timeout_id);
		uhcip->uhci_timeout_id = 0;
	}

	if (flags & UHCI_ROOT_HUB_REGISTER)
		uhci_unload_root_hub_driver(uhcip);

	/* Unregister this HCD instance with USBA */
	if (flags & UHCI_REGS_MAPPING)
		(void) usba_hcdi_deregister(uhcip->uhci_dip);

	/* Unmap the UHCI registers */
	if (uhcip->uhci_regs_handle) {
		/* Reset the host controller */
		Set_OpReg16(USBCMD, USBCMD_REG_GBL_RESET);
		ddi_regs_map_free(&uhcip->uhci_regs_handle);
	}

	/* Free the all buffers */
	if (uhcip->uhci_td_pool_addr &&
			uhcip->uhci_td_pool_mem_handle) {
		int rval;

		for (i = 0; i < td_pool_size; i ++) {
			td = &uhcip->uhci_td_pool_addr[i];

			/*
			 * The following code needs changes
			 * after addition of bulk support
			 */
			flag = uhcip->uhci_td_pool_addr[i].flag;
			if ((flag != TD_FLAG_FREE) &&
				(flag != TD_FLAG_DUMMY) && (td->tw != NULL)) {
				tw = td->tw;
				uhci_free_tw(uhcip, tw);
			}

		}

		if (uhcip->uhci_dma_addr_bind_flag &
			UHCI_TD_POOL_BOUND) {
			rval = ddi_dma_unbind_handle(
					uhcip->uhci_td_pool_dma_handle);
			ASSERT(rval == DDI_SUCCESS);
		}

		ddi_dma_mem_free(&uhcip->uhci_td_pool_mem_handle);
	}

	/* Free the TD pool */
	if (uhcip->uhci_td_pool_mem_handle)
		ddi_dma_free_handle(&uhcip->uhci_td_pool_mem_handle);

	if (uhcip->uhci_qh_pool_addr &&
				uhcip->uhci_qh_pool_mem_handle) {
		int rval;
		if (uhcip->uhci_dma_addr_bind_flag &
			UHCI_QH_POOL_BOUND) {
			rval = ddi_dma_unbind_handle(
					uhcip->uhci_qh_pool_dma_handle);
			ASSERT(rval == DDI_SUCCESS);
		}
		ddi_dma_mem_free(&uhcip->uhci_qh_pool_mem_handle);
	}

	/* Free the QH pool */
	if (uhcip->uhci_qh_pool_dma_handle)
		ddi_dma_free_handle(&uhcip->uhci_qh_pool_dma_handle);

	/* Free the Frame list Table area */
	if (uhcip->uhci_frame_lst_tablep &&
			uhcip->uhci_flt_mem_handle) {
		int rval;

		if (uhcip->uhci_dma_addr_bind_flag &
			UHCI_FLA_POOL_BOUND) {
			rval = ddi_dma_unbind_handle(
					uhcip->uhci_flt_dma_handle);
			ASSERT(rval == DDI_SUCCESS);
		}
		ddi_dma_mem_free(&uhcip->uhci_flt_mem_handle);
	}

	if (uhcip->uhci_flt_dma_handle)
		ddi_dma_free_handle(&uhcip->uhci_flt_dma_handle);

	if (flags & UHCI_LOCKS_INIT) {
		mutex_destroy(&uhcip->uhci_int_mutex);
		cv_destroy(&uhcip->uhci_cv_SOF);
		sema_destroy(&uhcip->uhci_ocsem);
	}

	/* cleanup kstat structures */
	uhci_destroy_stats(uhcip);

	if (flags & UHCI_SOFT_STATE_ZALLOC) {
		usb_free_log_handle(uhcip->uhci_log_hdl);
		ddi_prop_remove_all(uhcip->uhci_dip);
		ddi_soft_state_free(uhci_statep,
				ddi_get_instance(uhcip->uhci_dip));
	}
#ifdef  DEBUG
	mutex_enter(&uhci_dump_mutex);
	if (uhcip->uhci_dump_ops) {
		usba_dump_deregister(uhcip->uhci_dump_ops);
		usba_free_dump_ops(uhcip->uhci_dump_ops);
	}
	mutex_exit(&uhci_dump_mutex);
#endif

}

/*
 * uhci_free_tw:
 *
 * Free the Transfer Wrapper (TW).
 */
void
uhci_free_tw(uhci_state_t *uhcip, uhci_trans_wrapper_t *tw)
{
	int rval;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"uhci_free_tw:");

	ASSERT(tw != NULL);

	rval = ddi_dma_unbind_handle(tw->tw_dmahandle);
	ASSERT(rval == DDI_SUCCESS);

	ddi_dma_mem_free(&tw->tw_accesshandle);
	ddi_dma_free_handle(&tw->tw_dmahandle);
	kmem_free(tw, sizeof (uhci_trans_wrapper_t));
}

/*
 * uhci_deallocate_tw:
 *
 * Deallocate of a Transaction Wrapper (TW) and this involves
 * the freeing of DMA resources.
 */

void
uhci_deallocate_tw(uhci_state_t *uhcip,
    uhci_pipe_private_t *pp, uhci_trans_wrapper_t *tw)
{

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
		"uhci_deallocate_tw:");

	/*
	 * If the transfer wrapper has no Host Controller (HC)
	 * Transfer Descriptors (TD) associated with it,  then
	 * remove the transfer wrapper. The transfers are done
	 * in FIFO order, so this should be the first transfer
	 * wrapper on the list.
	 */
	if (tw->tw_hctd_head != NULL) {
		ASSERT(tw->tw_hctd_tail != NULL);
		return;
	}

	ASSERT(tw->tw_hctd_tail == NULL);
	ASSERT(pp->pp_tw_head != NULL);
	ASSERT(pp->pp_tw_head == tw);

	/*
	 * If pp->pp_tw_head is NULL, set the tail also to NULL.
	 */
	pp->pp_tw_head = tw->tw_next;

	if (pp->pp_tw_head == NULL)
		pp->pp_tw_tail = NULL;

	uhci_free_tw(uhcip, tw);
}

void
uhci_delete_td(uhci_state_t *uhcip, gtd *td)
{
	uhci_trans_wrapper_t	*tw = td->tw;
	gtd *tmp_td;

	if ((td->oust_td_next == NULL) && (td->oust_td_prev == NULL)) {
		uhcip->uhci_oust_tds_head = NULL;
		uhcip->uhci_oust_tds_tail = NULL;
	} else if (td->oust_td_next == NULL) {
		td->oust_td_prev->oust_td_next = NULL;
		uhcip->uhci_oust_tds_tail = td->oust_td_prev;
	} else if (td->oust_td_prev == NULL) {
		td->oust_td_next->oust_td_prev = NULL;
		uhcip->uhci_oust_tds_head = td->oust_td_next;
	} else {
		td->oust_td_prev->oust_td_next = td->oust_td_next;
		td->oust_td_next->oust_td_prev = td->oust_td_prev;
	}

	tmp_td = tw->tw_hctd_head;
	if (tmp_td != td) {
		while (tmp_td->tw_td_next != td)
			tmp_td = tmp_td->tw_td_next;

		ASSERT(tmp_td);
		tmp_td->tw_td_next = td->tw_td_next;
		if (td->tw_td_next == NULL)
			tw->tw_hctd_tail = tmp_td;
	} else {
		tw->tw_hctd_head = tw->tw_hctd_head->tw_td_next;
		if (tw->tw_hctd_head == NULL)
			tw->tw_hctd_tail = NULL;
	}

	td->flag  = TD_FLAG_FREE;

}

void
uhci_remove_tds_tws(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t *ph)
{
	uhci_pipe_private_t	*pp;
	uhci_trans_wrapper_t	*tw;

	pp = (uhci_pipe_private_t *)ph->p_hcd_private;
	tw = pp->pp_tw_head;

	while (tw != NULL) {
		while (tw->tw_hctd_head != NULL)
			uhci_delete_td(uhcip, tw->tw_hctd_head);

		uhci_deallocate_tw(uhcip, pp, tw);
		tw = pp->pp_tw_head;
	}
}


/*
 * uhci_remove_qh:
 *
 * Remove the Queue Head from the Host Controller's
 * appropriate QH list.
 */

void
uhci_remove_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp)
{
	uchar_t attributes;
	gtd		*dummy_td;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	USB_DPRINTF_L4(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_remove_qh:");

	attributes = pp->pp_pipe_handle->p_endpoint->bmAttributes &
				USB_EPT_ATTR_MASK;

	dummy_td = pp->pp_qh->td_tailp;
	dummy_td->flag = TD_FLAG_FREE;

	switch (attributes) {
		case USB_EPT_ATTR_CONTROL:
			uhci_remove_ctrl_qh(uhcip, pp);
			break;
		case USB_EPT_ATTR_BULK:
			uhci_remove_bulk_qh(uhcip, pp);
			break;
		case USB_EPT_ATTR_INTR:
			uhci_remove_intr_qh(uhcip, pp);
			break;
	}
}

void
uhci_remove_intr_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp)
{
	queue_head_t   *qh = pp->pp_qh;
	queue_head_t   *next_lattice_qh;

	next_lattice_qh = (queue_head_t *)QH_VADDR((qh->link_ptr &
			QH_LINK_PTR_MASK));

	qh->prev_qh->link_ptr    = qh->link_ptr;
	next_lattice_qh->prev_qh = qh->prev_qh;

	qh->qh_flag = QUEUE_HEAD_FLAG_FREE;

}

void
uhci_remove_bulk_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp)
{
	queue_head_t   *qh = pp->pp_qh;
	queue_head_t   *next_lattice_qh;
	uint32_t	paddr;


	paddr = (qh->link_ptr & QH_LINK_PTR_MASK);
	if (paddr == 0)
		next_lattice_qh = 0;
	else
		next_lattice_qh = (queue_head_t *)QH_VADDR(paddr);

	qh->prev_qh->link_ptr = qh->link_ptr;

	if (next_lattice_qh == NULL)
		uhcip->uhci_bulk_xfers_q_tail = qh->prev_qh;
	else
		next_lattice_qh->prev_qh = qh->prev_qh;

	qh->qh_flag = QUEUE_HEAD_FLAG_FREE;

}

void
uhci_remove_ctrl_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp)
{
	queue_head_t   *qh = pp->pp_qh;
	queue_head_t   *next_lattice_qh;

	next_lattice_qh = (queue_head_t *)QH_VADDR(
			(qh->link_ptr&QH_LINK_PTR_MASK));

	qh->prev_qh->link_ptr = qh->link_ptr;
	if (next_lattice_qh->prev_qh != NULL)
		next_lattice_qh->prev_qh = qh->prev_qh;
	else
		uhcip->uhci_ctrl_xfers_q_tail = qh->prev_qh;

	qh->qh_flag = QUEUE_HEAD_FLAG_FREE;
}

/*
 * uhci_allocate_td_from_pool:
 *
 * Allocate a Transfer Descriptor (TD) from the TD buffer pool.
 */
gtd *
uhci_allocate_td_from_pool(uhci_state_t *uhcip)
{
	int		index;
	gtd		*td;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/*
	 * Search for a blank Transfer Descriptor (TD)
	 * in the TD buffer pool.
	 */
	for (index = 0; index < td_pool_size; index ++) {
		if (uhcip->uhci_td_pool_addr[index].flag == TD_FLAG_FREE)
			break;
	}

	if (index == td_pool_size) {
		USB_DPRINTF_L2(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
			"uhci_allocate_td_from_pool: TD exhausted");

		return (NULL);
	}

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, uhcip->uhci_log_hdl,
			"uhci_allocate_td_from_pool: Allocated %d", index);

	/* Create a new dummy for the end of the TD list */
	td = &uhcip->uhci_td_pool_addr[index];

	/* Mark the newly allocated TD as a dummy */
	td->flag =  TD_FLAG_DUMMY;
	td->qh_td_prev  =  NULL;

	return (td);
}

/*
 * uhci_common_bulk_routine:
 * NOTE: In case of memory allocation failure, we just return
 * USB_NORESOURCES as there is no mechanism designed yet to
 * server the request. This could become a problem under stress.
 */

int
uhci_common_bulk_routine(usb_pipe_handle_impl_t  *ph,
	size_t		length,
	mblk_t		*data,
	uint_t		usb_flags)
{
	uhci_state_t			*uhcip;
	uhci_pipe_private_t		*pp;
	int 				error = USB_SUCCESS;

	uhcip 	= (uhci_state_t *)
			uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_common_bulk_routine: Flags = %x", usb_flags);

	pp		= (uhci_pipe_private_t *)ph->p_hcd_private;

	while (pp->pp_qh->bulk_xfer_info != NULL)
		delay(drv_usectohz(10));

	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/* Add the TD into the Host Controller's bulk list */
	error = uhci_insert_bulk_td(uhcip, ph,
			uhci_handle_bulk_td,
			length,
			data,
			usb_flags);

	if (error != USB_SUCCESS)
		USB_DPRINTF_L2(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
			"uhci_common_bulk_routine: No resources");

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	return (error);
}

/*
 * uhci_insert_bulk_td:
 *
 */
/* ARGSUSED */
int
uhci_insert_bulk_td(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*ph,
	uhci_handler_function_t	tw_handle_td,
	size_t			length,
	mblk_t			*data,
	uint_t			flags)
{
	uhci_pipe_private_t	*pp =
			(uhci_pipe_private_t *)ph->p_hcd_private;
	uhci_trans_wrapper_t	*tw;
	uint_t			MaxPacketSize;
	uint_t			num_bulk_tds, i;
	uhci_bulk_xfer_t	*bulk_xfer_info;
	gtd			*bulk_td_ptr;
	gtd			*current_dummy;
	uint32_t		buf_addr;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
		uhcip->uhci_log_hdl, "uhci_insert_bulk_td:");

	/*
	 * Create transfer wrapper
	 */

	tw = uhci_create_transfer_wrapper(uhcip, pp, length, flags);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_insert_bulk_td: TW allocation failed");
		return (USB_NO_RESOURCES);
	}

	tw->tw_bytes_xfered		= 0;
	tw->tw_bytes_pending 		= length;
	tw->tw_handle_td 		= uhci_handle_bulk_td;
	tw->tw_handle_callback_value	= (usb_opaque_t)data;

	/*
	 * If the DATA OUT, copy the data into transfer buffer.
	 */

	if (data) {
		tw->tw_direction = PID_OUT;
		ddi_rep_put8(tw->tw_accesshandle,
				data->b_rptr,
				(uint8_t *)tw->tw_buf,
				length,
				DDI_DEV_AUTOINCR);
		freeb(data);
	}
	else
		tw->tw_direction = PID_IN;

	/*
	 * Get the max packet size.
	 */

	MaxPacketSize =
		pp->pp_pipe_handle->p_endpoint->wMaxPacketSize;


	length = MaxPacketSize;

	/*
	 * Calculate number of TD's to insert in the current
	 * frame interval.
	 * Max number TD's allowed (driver implementation) is 32
	 * in one frame interval. Once all the TD's are completed
	 * then the reamaing TD's will be inserted on to the lattice
	 * in the uhci_hanble_bulk_td().
	 */

	if ((tw->tw_bytes_pending / MaxPacketSize) >=
		MAX_NUM_BULK_TDS_PER_XFER)
		num_bulk_tds = MAX_NUM_BULK_TDS_PER_XFER;
	else {
		num_bulk_tds = (tw->tw_bytes_pending / MaxPacketSize);
		if (tw->tw_bytes_pending % MaxPacketSize) {
			num_bulk_tds++;
			length = (tw->tw_bytes_pending % MaxPacketSize);
		}
	}

	/*
	 * Allocate memory for the bulk xfer information struct
	 */

	bulk_xfer_info = (uhci_bulk_xfer_t *)kmem_zalloc(
				sizeof (uhci_bulk_xfer_t), KM_NOSLEEP);

	if (bulk_xfer_info == NULL)
		return (USB_FAILURE);

	/*
	 * Allocate memory for the buld TD's
	 */

	if (uhci_alloc_mem_bulk_tds(uhcip,
		num_bulk_tds, bulk_xfer_info) == UHCI_FAILURE)
		return (UHCI_FAILURE);

	bulk_td_ptr	= (gtd *)bulk_xfer_info->bulk_pool_addr;

	bulk_td_ptr[0].qh_td_prev = NULL;

	current_dummy = pp->pp_qh->td_tailp;

	buf_addr = tw->tw_cookie.dmac_address;

	pp->pp_qh->bulk_xfer_info	= bulk_xfer_info;

	/*
	 * Fill up all the bulk TD's
	 */

	for (i = 0; i < (num_bulk_tds-1); i++) {
		uhci_fill_in_bulk_td(uhcip,
			&bulk_td_ptr[i],
			&bulk_td_ptr[i+1],
			BULKTD_PADDR(bulk_xfer_info,
			&bulk_td_ptr[i+1]),
			ph,
			buf_addr,
			MaxPacketSize, tw);
		buf_addr += MaxPacketSize;
	}

	uhci_fill_in_bulk_td(uhcip,
			&bulk_td_ptr[i],
			current_dummy,
			TD_PADDR(current_dummy),
			ph,
			buf_addr,
			length,
			tw);

	bulk_xfer_info->num_tds	= num_bulk_tds;

	/*
	 * Insert on the bulk queue head for the excution by HC
	 */

	pp->pp_qh->element_ptr	=
		bulk_xfer_info->uhci_bulk_cookie.dmac_address;

	return (USB_SUCCESS);
}

/*
 * uhci_fill_in_bulk_td
 *     Fills the bulk TD
 */

void
uhci_fill_in_bulk_td(uhci_state_t *uhcip, gtd *current_td,
	gtd			*next_td,
	uint32_t		next_td_paddr,
	usb_pipe_handle_impl_t	*ph,
	uint_t			buffer_address,
	uint_t			length,
	uhci_trans_wrapper_t	*tw)
{
	uhci_pipe_private_t *pp =
			(uhci_pipe_private_t *)ph->p_hcd_private;

	bzero((char *)current_td, sizeof (gtd));

	current_td->link_ptr = next_td_paddr | 0x4;

	if (tw->tw_flags != USB_FLAGS_SHORT_XFER_OK)
		current_td->td_dword2.spd = 1;

	mutex_enter(&ph->p_usb_device->usb_mutex);

	current_td->td_dword2.c_err	= UHCI_MAX_ERR_COUNT;
	current_td->td_dword2.status	= TD_ACTIVE;
	current_td->td_dword2.ioc	= INTERRUPT_ON_COMPLETION;
	current_td->td_dword3.max_len	= (length-1);
	current_td->td_dword3.data_toggle = pp->pp_data_toggle;

	if (pp->pp_data_toggle == 0)
		pp->pp_data_toggle = 1;
	else
		pp->pp_data_toggle = 0;

	current_td->td_dword3.device_addr	=
					ph->p_usb_device->usb_addr;
	current_td->td_dword3.endpt			=
		ph->p_endpoint->bEndpointAddress & END_POINT_ADDRESS_MASK;
	current_td->td_dword3.PID	= tw->tw_direction;
	current_td->buffer_address	= buffer_address;

	next_td->qh_td_prev = current_td;
	pp->pp_qh->td_tailp = next_td;

	current_td->oust_td_next = uhcip->uhci_oust_tds_head;
	current_td->oust_td_prev = NULL;
	if (uhcip->uhci_oust_tds_head != NULL)
		uhcip->uhci_oust_tds_head->oust_td_prev = current_td;
	uhcip->uhci_oust_tds_head = current_td;

	current_td->tw = tw;

	if (tw->tw_hctd_head == NULL) {
		ASSERT(tw->tw_hctd_tail == NULL);
		tw->tw_hctd_head = current_td;
		tw->tw_hctd_tail = current_td;
	} else {
		/* Add the td to the end of the list */
		tw->tw_hctd_tail->tw_td_next = current_td;
		tw->tw_hctd_tail = current_td;
	}

	mutex_exit(&ph->p_usb_device->usb_mutex);
}

/* ARGSUSED */
uint_t
uhci_alloc_mem_bulk_tds(
	uhci_state_t *uhcip, uint_t num_tds,
	uhci_bulk_xfer_t *info)
{
	ddi_device_acc_attr_t	dev_attr;
	size_t			real_length;
	int			result;
	uint_t			ccount;
	uint_t			offset;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_allocate_pools:");

	/* The host controller will be little endian */
	dev_attr.devacc_attr_version		= DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags	= DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder		= DDI_STRICTORDER_ACC;

	/* Allocate the bulk TD pool DMA handle */
	if (ddi_dma_alloc_handle(
		uhcip->uhci_dip, &uhcip->uhci_dma_attr,
		DDI_DMA_SLEEP, 0,
		&info->uhci_bulk_dma_handle) != DDI_SUCCESS) {
		return (UHCI_FAILURE);
	}

	/* Allocate the memory for the bulk TD pool */
	if (ddi_dma_mem_alloc(info->uhci_bulk_dma_handle,
			num_tds * sizeof (gtd) + UHCI_TD_ALIGN_SZ,
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			0,
			(caddr_t *)&info->bulk_pool_addr,
			&real_length,
			&info->uhci_bulk_mem_handle)) {

		return (UHCI_FAILURE);
	}

	/* Map the bulk TD pool into the I/O address space */
	result = ddi_dma_addr_bind_handle(
			info->uhci_bulk_dma_handle,
			NULL,
			(caddr_t)info->bulk_pool_addr,
			real_length,
			DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			NULL,
			&info->uhci_bulk_cookie,
			&ccount);

	/* Process the result */
	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_allocate_pools: More than 1 cookie");
			return (UHCI_FAILURE);
		}
	} else {
		USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"uhci_allocate_pools: Result = %d", result);

		uhci_decode_ddi_dma_addr_bind_handle_result(
			uhcip, result);

		return (UHCI_FAILURE);
	}

	offset = (info->uhci_bulk_cookie.dmac_address & 0xf);
	offset = UHCI_TD_ALIGN_SZ - offset;

	info->bulk_pool_addr = (uint32_t)(info->bulk_pool_addr+offset);
	info->uhci_bulk_cookie.dmac_address += offset;

	bzero((void *)info->bulk_pool_addr, num_tds * sizeof (gtd));

	return (UHCI_SUCCESS);
}

void
uhci_handle_bulk_td(uhci_state_t *uhcip, gtd *td)
{
	uhci_pipe_private_t	*pp;
	uhci_trans_wrapper_t	*tw;
	uint_t			flags = 0;
	ushort_t		MaxPacketSize;
	uint_t			num_bulk_tds;
	uhci_bulk_xfer_t	*bulk_xfer_info;
	usb_pipe_handle_impl_t	*ph;
	gtd			*bulk_td_ptr;
	uint_t			i;
	uint32_t		buf_addr;
	gtd			*current_dummy;
	uint_t			rval, length;
	mblk_t			*message;

	tw		= td->tw;
	pp		= tw->tw_pipe_private;

	mutex_enter(&pp->pp_mutex);

	/*
	 * Check whether there are any errors occurred in the xfer.
	 * If so, update the data_toggle for the queue head and
	 * return error to the upper layer.
	 */

	if (td->td_dword2.status & TD_STATUS_MASK) {
		uhci_hanlde_bulk_td_errors(uhcip, td);
		pp->pp_qh->element_ptr = TD_PADDR(pp->pp_qh->td_tailp);
		mutex_exit(&pp->pp_mutex);
		return;
	}

	/*
	 * Update the tw_bytes_pending, and tw_bytes_xfered
	 */

	if (td->td_dword2.Actual_len != ZERO_LENGTH) {
		tw->tw_bytes_pending -= (td->td_dword2.Actual_len + 1);
		tw->tw_bytes_xfered  += (td->td_dword2.Actual_len + 1);
	}

	bulk_xfer_info	= pp->pp_qh->bulk_xfer_info;

	ph = tw->tw_pipe_private->pp_pipe_handle;

	/*
	 * If the TD's in the current frame are completed, then check
	 * whether we have any more bytes to xfer. IF so, insert TD's.
	 * If no more bytes needs to xfered, then do callback to the
	 * upper layer.
	 * If the TD's in the current frame are not completed, then
	 * just delete the TD from the linked lists.
	 */

	if (--bulk_xfer_info->num_tds == 0) {
		uhci_delete_td(uhcip, td);
		if ((tw->tw_bytes_pending) &&
			((td->td_dword3.max_len -
			td->td_dword2.Actual_len) == 0)) {

			MaxPacketSize =
				pp->pp_pipe_handle->p_endpoint->wMaxPacketSize;

			length = MaxPacketSize;
			if ((tw->tw_bytes_pending / MaxPacketSize) >=
				MAX_NUM_BULK_TDS_PER_XFER) {
				num_bulk_tds = MAX_NUM_BULK_TDS_PER_XFER;
			} else {
				num_bulk_tds =
					(tw->tw_bytes_pending / MaxPacketSize);
				if (tw->tw_bytes_pending % MaxPacketSize) {
					num_bulk_tds++;
					length = (tw->tw_bytes_pending %
								MaxPacketSize);
				}
			}

			current_dummy = pp->pp_qh->td_tailp;

			bulk_td_ptr	= (gtd *)bulk_xfer_info->bulk_pool_addr;

			buf_addr = tw->tw_cookie.dmac_address +
					tw->tw_bytes_xfered;
			for (i = 0; i < (num_bulk_tds-1); i++) {

				uhci_fill_in_bulk_td(uhcip,
					&bulk_td_ptr[i],
					&bulk_td_ptr[i+1],
					BULKTD_PADDR(bulk_xfer_info,
							&bulk_td_ptr[i+1]),
					ph, buf_addr,
					MaxPacketSize, tw);
				buf_addr += MaxPacketSize;
			}

			uhci_fill_in_bulk_td(uhcip,
					&bulk_td_ptr[i],
					current_dummy,
					TD_PADDR(current_dummy),
					ph,
					buf_addr,
					length, tw);

			pp->pp_qh->bulk_xfer_info = bulk_xfer_info;
			bulk_xfer_info->num_tds	= num_bulk_tds;
			pp->pp_qh->element_ptr	=
				bulk_xfer_info->uhci_bulk_cookie.dmac_address;
		} else {
			if (tw->tw_direction == PID_IN) {
				if (tw->tw_bytes_pending)
					tw->tw_tmp = UHCI_UNDERRUN_OCCURRED;

				/* Sync the streaming buffer */
				rval = ddi_dma_sync(tw->tw_dmahandle, 0,
					tw->tw_length, DDI_DMA_SYNC_FORCPU);

				ASSERT(rval == DDI_SUCCESS);

				if ((uhci_sendup_td_message(uhcip, pp, tw))
					== USB_NO_RESOURCES) {
					USB_DPRINTF_L2(PRINT_MASK_LISTS,
					uhcip->uhci_log_hdl,
					"uhci_handle_ctrl_td: Drop message");
				}
			} else {
				message = NULL;

				UHCI_DO_BYTE_STATS(uhcip, tw->tw_length,
				    tw->tw_pipe_private->pp_pipe_handle->
					p_usb_device,
				    tw->tw_pipe_private->pp_pipe_handle->
					p_endpoint->bmAttributes,
				    tw->tw_pipe_private->pp_pipe_handle->
					p_endpoint->bEndpointAddress);

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&uhcip->uhci_int_mutex);

				usba_hcdi_callback(
					tw->tw_pipe_private->pp_pipe_handle,
					flags,
					message,
					0,
					USB_CC_NOERROR,
					USB_SUCCESS);
				mutex_enter(&uhcip->uhci_int_mutex);
				mutex_enter(&pp->pp_mutex);

			}

			/*
			 * Deallocate DMA memory
			 */
			uhci_deallocate_tw(uhcip, pp, tw);
			rval = ddi_dma_unbind_handle(
				bulk_xfer_info->uhci_bulk_dma_handle);
			ddi_dma_mem_free(
				&bulk_xfer_info->uhci_bulk_mem_handle);
			ddi_dma_free_handle(
				&bulk_xfer_info->uhci_bulk_dma_handle);
			kmem_free(bulk_xfer_info,
				sizeof (uhci_bulk_xfer_t));
			pp->pp_qh->bulk_xfer_info = NULL;
		}
	}
	else
		uhci_delete_td(uhcip, td);

	mutex_exit(&pp->pp_mutex);
}

void
uhci_hanlde_bulk_td_errors(uhci_state_t *uhcip, gtd *td)
{
	uint_t			usb_err;
	uint_t			NAK_received;
	uhci_trans_wrapper_t	*tw = td->tw;
	usb_pipe_handle_impl_t	*ph;
	uhci_pipe_private_t	*pp;

	/*
	 * Find the type of error occurred and return the error to the upper
	 * layer
	 */

	usb_err = uhci_parse_td_error(td, &NAK_received);

	tw = td->tw;
	ph = tw->tw_pipe_private->pp_pipe_handle;
	pp = (uhci_pipe_private_t *)ph->p_hcd_private;

	uhci_remove_bulk_tds_tws(uhcip, ph, tw->tw_pipe_private->pp_qh);
	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	usba_hcdi_callback(ph, tw->tw_flags, NULL, 0, usb_err, USB_FAILURE);

	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);
}

/* ARGSUSED */
void
uhci_remove_bulk_tds_tws(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*ph,
	queue_head_t		*qh)
{
	gtd		*head, *tmp_td;
	queue_head_t	*tmp_qh;
	uhci_bulk_xfer_t *info;
	uint_t		rval;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	info = qh->bulk_xfer_info;

	if (info == NULL)
		return;

	head = uhcip->uhci_oust_tds_head;

	while (head) {
		tmp_qh = head->tw->tw_pipe_private->pp_qh;
		if (tmp_qh == qh) {
			tmp_td = head;
			head = head->oust_td_next;
			uhci_delete_td(uhcip, tmp_td);
		} else
			head = head->oust_td_next;
	}

	rval = ddi_dma_unbind_handle(info->uhci_bulk_dma_handle);
	ASSERT(rval == DDI_SUCCESS);
	ddi_dma_mem_free(&info->uhci_bulk_mem_handle);
	ddi_dma_free_handle(&info->uhci_bulk_dma_handle);
	kmem_free(info, sizeof (uhci_bulk_xfer_t));
	qh->bulk_xfer_info = NULL;

}


/*
 * uhci_ctrl_bulk_timeout_hdlr()
 *	This routine will get called for every one second. It checks for
 *	timed out control commands/bulk commands. Time out any commands
 *	that exceeds the time out period specified by the pipe policy.
 */

void
uhci_ctrl_bulk_timeout_hdlr(void *arg)
{
	uhci_state_t	*uhcip = (uhci_state_t *)arg;
	gtd		*head;
	uint_t		flag = UHCI_FALSE;
	uint_t		command_timeout_value;
	uint_t		timval;
	uchar_t 	attributes;

	/*
	 * Check whether any of the control xfers are timed out.
	 * If so, complete those commands with time out as reason.
	 */
	mutex_enter(&uhcip->uhci_int_mutex);
	head = uhcip->uhci_oust_tds_head;

	while (head) {
		/*
		 * Get the time out value from the pipe policy
		 */
		uhci_pipe_private_t *pp = head->tw->tw_pipe_private;

		attributes = pp->pp_pipe_handle->p_endpoint->bmAttributes;
		timval = pp->pp_policy.pp_timeout_value;

		/*
		 * If timeout out is not specified in the pipe policy,
		 * a default value is assumed.
		 */
		switch (attributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_BULK:
			command_timeout_value = max(timval, UHCI_BULK_TIMEOUT);
			break;
		case USB_EPT_ATTR_CONTROL:
			command_timeout_value = timval ? timval :
							UHCI_CTRL_TIMEOUT;
			break;
		}

		if ((head->tw->tw_timeout_cnt++ == command_timeout_value) &&
			((head->tw->tw_handle_td == uhci_handle_ctrl_td) ||
			(head->tw->tw_handle_td == uhci_handle_bulk_td))) {

			/*
			 * Check finaly whether the command completed
			 */
			if (head->td_dword2.status & TD_ACTIVE) {
				head->td_dword2.status = UHCI_TD_CRC_TIMEOUT;
				pp->pp_qh->element_ptr = head->link_ptr;
			}

			flag = UHCI_TRUE;
		}
		head = head->oust_td_next;
	}


	/*
	 * Process the td which was compled before shifting from normal
	 * mode to polled mode
	 */
	if (uhcip->uhci_polled_flag == UHCI_POLLED_FLAG_TRUE) {
		uhci_process_submitted_td_queue(uhcip);
		uhcip->uhci_polled_flag = UHCI_POLLED_FLAG_FALSE;
	} else if (flag) {
		/* Process the completed/timed out commands */
		uhci_process_submitted_td_queue(uhcip);
	}

	/* Re-register the timeout handler */
	uhcip->uhci_ctrl_bulk_timeout_id =
			timeout(uhci_ctrl_bulk_timeout_hdlr,
			(void *)uhcip, drv_usectohz(UHCI_ONE_SECOND));

	mutex_exit(&uhcip->uhci_int_mutex);
}

#ifdef DEBUG

/*
 * uhci_dump_state:
 *	Dump all UHCI related information. This function is registered
 *	with USBA framework.
 */
void
uhci_dump(uint_t flag, usb_opaque_t arg)
{
	uhci_state_t	*uhcip;

	mutex_enter(&uhci_dump_mutex);
	uhci_show_label = USB_DISALLOW_LABEL;

	uhcip = (uhci_state_t *)arg;
	uhci_dump_state(uhcip, flag);
	uhci_show_label = USB_ALLOW_LABEL;
	mutex_exit(&uhci_dump_mutex);
}


void
uhci_dump_registers(uhci_state_t *uhcip)
{

	USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"\tUHCI Operational register values");

	USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"\tUSBCMD : 0x%x\tUSBSTS : 0x%x ",
		Get_OpReg16(USBCMD), Get_OpReg16(USBSTS));

	USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"\tUSBINTR : 0x%x\tFRNUM : 0x%x ",
		Get_OpReg16(USBINTR), Get_OpReg16(FRNUM));

	USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"\tFRBASEADD : 0x%x\tSOFMOD : 0x%x ",
		Get_OpReg16(FRBASEADD), Get_OpReg16(SOFMOD));

	USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"\tPORTSC1 : 0x%x\tPORTSC2 : 0x%x ",
		Get_OpReg16(PORTSC[0]), Get_OpReg16(PORTSC[1]));

}

void
uhci_dump_pending_cmds(uhci_state_t *uhcip)
{
	gtd *head;

	USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"\tCommands in the Oustanding Queue");

	head = uhcip->uhci_oust_tds_head;

	while (head) {
		if (head->tw->tw_handle_td == uhci_handle_ctrl_td) {
			USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"\tQH : 0x%x\tTD : 0x%x Ctrl xfer",
			head->tw->tw_pipe_private->pp_qh, head);
		} else if (head->tw->tw_handle_td == uhci_handle_bulk_td) {
			USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"\tQH : 0x%x\tTD : 0x%x Bulk xfer",
			head->tw->tw_pipe_private->pp_qh, head);
		} else {
			USB_DPRINTF_L3(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"\tQH : 0x%x\tTD : 0x%x Intr xfer",
			head->tw->tw_pipe_private->pp_qh, head);
		}
		head = head->oust_td_next;
	}
}

/*
 * uhci_dump_state:
 *	Dump UHCI state information.
 */
void
uhci_dump_state(uhci_state_t *uhcip, uint_t flag)
{
	int		i;
	char		pathname[MAXNAMELEN];

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
	    "\n***** UHCI Information *****");

	if (flag & USB_DUMP_STATE) {
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "****** uhci%d ****** dip: 0x%p uhcip: 0x%p",
		    uhcip->uhci_instance, uhcip->uhci_dip, uhcip);

		(void) ddi_pathname(uhcip->uhci_dip, pathname);
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "****** DEVICE: %s", pathname);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "uhci_hcdi_ops: 0x%p \tuhci_flags: 0x%x",
		    uhcip->uhci_hcdi_ops, uhcip->uhci_flags);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "uhci_frame_lst_tablep: 0x%p \tuhci_flt_cookie: 0x%p",
		    uhcip->uhci_frame_lst_tablep, uhcip->uhci_flt_cookie);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "uhci_td_pool_addr: 0x%p", uhcip->uhci_td_pool_addr);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "uhci_qh_pool_addr: 0x%p", uhcip->uhci_qh_pool_addr);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "\t\tuhci_ocsem: 0x%p", uhcip->uhci_ocsem);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "uhci_bandwidth:");
		for (i = 0; i < NUM_FRAME_LST_ENTRIES; i += 8) {
			USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
			    "\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x"
			    "\t0x%x\t0x%x", uhcip->uhci_bandwidth[i],
			    uhcip->uhci_bandwidth[i + 1],
			    uhcip->uhci_bandwidth[i + 2],
			    uhcip->uhci_bandwidth[i + 3],
			    uhcip->uhci_bandwidth[i + 4],
			    uhcip->uhci_bandwidth[i + 5],
			    uhcip->uhci_bandwidth[i + 6],
			    uhcip->uhci_bandwidth[i + 7]);
		}

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "uhci_bandwidth_isoch_sum: 0x%x\t"
		    "uhci_bandwidth_intr_min: 0x%x",
		    uhcip->uhci_bandwidth_isoch_sum,
		    uhcip->uhci_bandwidth_intr_min);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "uhci_root_hub:");

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "\tRoot hub port states:");

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		    "\t\t0x%x\t0x%x",
		    uhcip->uhci_root_hub.root_hub_port_state[0],
		    uhcip->uhci_root_hub.root_hub_port_state[1]);

		/* Print op registers */
		uhci_dump_registers(uhcip);

		/* Dump the pending commands */
		uhci_dump_pending_cmds(uhcip);

	}
}

void
uhci_print_td(uhci_state_t *uhcip, gtd *td)
{
	uint_t		*ptr = (uint_t *)td;

	USB_DPRINTF_L1(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		"\tDWORD 1 0x%x\t DWORD 2 0x%x", ptr[0], ptr[1]);
	USB_DPRINTF_L1(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		"\tDWORD 3 0x%x\t DWORD 4 0x%x", ptr[2], ptr[4]);
	USB_DPRINTF_L1(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		"\tBytes xfered    = %d", td->tw->tw_bytes_xfered);
	USB_DPRINTF_L1(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		"\tBytes Pending   = %d", td->tw->tw_bytes_pending);
	USB_DPRINTF_L1(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		"Queue Head Details:");
	uhci_print_qh(uhcip, td->tw->tw_pipe_private->pp_qh);
}

void
uhci_print_qh(uhci_state_t *uhcip, queue_head_t *qh)
{
	uint_t		*ptr = (uint_t *)qh;

	USB_DPRINTF_L1(PRINT_MASK_DUMPING, uhcip->uhci_log_hdl,
		"\tLink Ptr = %x Element Ptr = %x", ptr[0], ptr[1]);
}
#endif
