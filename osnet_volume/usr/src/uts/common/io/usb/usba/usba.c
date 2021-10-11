/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)usba.c	1.17	99/11/18 SMI"

/*
 * USBA: Solaris USB Architecture support
 */
#include <sys/usb/usba.h>
#include <sys/ddi_impldefs.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/usb/hubd/hub.h>
#include <sys/log.h>

/*
 * USBA private variables and tunables
 */
static kmutex_t	usba_mutex;

/*
 * ddivs forced binding:
 *
 *    usbc usbc_xhubs usbc_xaddress  node name
 *
 *	0	x	x	class name or "device"
 *
 *	1	0	0	ddivs_usbc
 *	1	0	>1	ddivs_usbc except device
 *				at usbc_xaddress
 *	1	1	0	ddivs_usbc except hubs
 *	1	1	>1	ddivs_usbc except hubs and
 *				device at usbc_xaddress
 */
uint_t usba_ddivs_usbc;
uint_t usba_ddivs_usbc_xhubs;
uint_t usba_ddivs_usbc_xaddress;

#ifdef	DEBUG
/*
 * dump descriptors
 */
static kmutex_t	usba_dump_mutex;
uint_t usba_dump_descriptors_flag = 0;
#endif	/* DEBUG */

/*
 * compatible name handling
 */
#define	USBA_MAX_COMPAT_NAMES	15
static char	usba_name[USBA_MAX_COMPAT_NAMES][64];
static char	*usba_compatible[USBA_MAX_COMPAT_NAMES];

_NOTE(MUTEX_PROTECTS_DATA(usba_mutex, usba_name usba_compatible))

/* double linked list for usb_devices */
static usba_list_entry_t	usb_device_list;

_NOTE(MUTEX_PROTECTS_DATA(usba_mutex, usb_device_list))


/*
 * modload support
 */
extern struct mod_ops mod_miscops;

struct modlmisc modlmisc	= {
	&mod_miscops,	/* Type	of module */
	"USBA: USB Architecture 1.17"
};

struct modlinkage modlinkage = {
	MODREV_1, (void	*)&modlmisc, NULL
};


static usb_log_handle_t	usba_log_handle;
static uint_t		usba_errlevel = USB_LOG_L4;
static uint_t		usba_errmask = (uint_t)-1;
static uint_t		usba_show_label = USB_ALLOW_LABEL;

_NOTE(SCHEME_PROTECTS_DATA("save sharing", usba_show_label))

extern usb_log_handle_t	hubdi_log_handle;

_init(void)
{
	int i, rval;

	usba_usba_initialization();
	usba_usbai_initialization();
	usba_hcdi_initialization();
	usba_hubdi_initialization();

	if ((rval = mod_install(&modlinkage)) != 0) {
		usba_usba_destroy();
		usba_usbai_destroy();
		usba_hcdi_destroy();
		usba_hubdi_destroy();
	}

	for (i = 0; i < USBA_MAX_COMPAT_NAMES; i++) {
		usba_compatible[i] = usba_name[i];
	}

	return (rval);
}


_fini()
{
	int rval;

	if ((rval = mod_remove(&modlinkage)) == 0) {
		usba_hubdi_destroy();
		usba_hcdi_destroy();
		usba_usba_destroy();
		usba_usbai_destroy();
	}

	return (rval);
}


_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * common bus ctl for hcd, usb_mid, and hubd
 */

int
usba_bus_ctl(dev_info_t	*dip,
	dev_info_t		*rdip,
	ddi_ctl_enum_t		op,
	void			*arg,
	void			*result)
{
	dev_info_t		*child_dip = (dev_info_t *)arg;
	usb_device_t		*usb_device;
	usb_hcdi_t		*usb_hcdi;
	usb_hcdi_ops_t		*usb_hcdi_ops;

	USB_DPRINTF_L4(DPRINT_MASK_USBA,
	    hubdi_log_handle, "usba_bus_ctl: %s%d %s%d op=%d",
		    ddi_node_name(rdip),	ddi_get_instance(rdip),
		    ddi_node_name(dip), ddi_get_instance(dip), op);

	switch (op) {

	case DDI_CTLOPS_REPORTDEV:
	{
		char *mfg;
		usb_device = usba_get_usb_device(rdip);

		mutex_enter(&usb_device->usb_mutex);
		if (usb_device->usb_string_descr == NULL) {
			mutex_exit(&usb_device->usb_mutex);
			mfg = kmem_zalloc(USB_MAXSTRINGLEN, KM_SLEEP);

			if (usba_get_mfg_product_sn_strings(rdip, mfg,
			    USB_MAXSTRINGLEN) == USB_SUCCESS) {
				mutex_enter(&usb_device->usb_mutex);
				usb_device->usb_string_descr = mfg;
				mutex_exit(&usb_device->usb_mutex);
			} else {
				kmem_free(mfg, USB_MAXSTRINGLEN);
				mfg = NULL;
			}
		} else {
			mfg = usb_device->usb_string_descr;
			mutex_exit(&usb_device->usb_mutex);
		}

		log_enter();
		cmn_err(CE_CONT, "?USB-device: %s@%s, %s%d at bus address %d\n",
		    ddi_node_name(rdip), ddi_get_name_addr(rdip),
		    ddi_driver_name(rdip),
		    ddi_get_instance(rdip), usb_device->usb_addr);

		if (mfg) {
			cmn_err(CE_CONT, "?\t%s\n", mfg);
		}
		log_exit();


		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_INITCHILD:
	{
		int			usb_addr;
		uint_t			n;
		char			name[32];
		int			*data;
		int			rval;
		int			len = sizeof (usb_addr);

		usb_hcdi	= usba_hcdi_get_hcdi(dip);
		usb_hcdi_ops	= usb_hcdi->hcdi_ops;
		ASSERT(usb_hcdi_ops != NULL);

		/*
		 * as long as the dip exists, it should have
		 * usb_device structure associated with it
		 */
		usb_device = usba_get_usb_device(child_dip);
		if (usb_device == NULL) {
			USB_DPRINTF_L2(DPRINT_MASK_USBA, hubdi_log_handle,
				"usba_bus_ctl: DDI_NOT_WELL_FORMED (%s (0x%p))",
				ddi_node_name(child_dip),
				(void *)child_dip);

			return (DDI_NOT_WELL_FORMED);
		}

		/* the dip should have an address and reg property */
		if (ddi_prop_op(DDI_DEV_T_NONE, child_dip, PROP_LEN_AND_VAL_BUF,
		    DDI_PROP_DONTPASS |	DDI_PROP_CANSLEEP,
		    "assigned-address",
		    (caddr_t)&usb_addr,	&len) != DDI_SUCCESS) {

			USB_DPRINTF_L1(DPRINT_MASK_USBA, hubdi_log_handle,
			    "usba_bus_ctl:\n\t"
			    "%s%d %s%d op=%d rdip = 0x%p dip = 0x%p",
			    ddi_node_name(rdip), ddi_get_instance(rdip),
			    ddi_node_name(dip), ddi_get_instance(dip), op);

			USB_DPRINTF_L1(DPRINT_MASK_USBA, hubdi_log_handle,
			    "usba_bus_ctl: DDI_NOT_WELL_FORMED (%s (0x%p))",
			    ddi_node_name(child_dip), (void *)child_dip);

			return (DDI_NOT_WELL_FORMED);
		}

		if ((rval = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, child_dip,
		    DDI_PROP_DONTPASS, "reg",
		    &data, &n)) != DDI_SUCCESS) {

			USB_DPRINTF_L1(DPRINT_MASK_USBA, hubdi_log_handle,
			    "usba_bus_ctl: %d, DDI_NOT_WELL_FORMED", rval);

			return (DDI_NOT_WELL_FORMED);
		}


		/*
		 * if the configuration is 1, the unit address is
		 * just the interface number
		 */
		if ((n == 1) || ((n > 1) && (data[1] == 1))) {
			(void) sprintf(name, "%x", data[0]);
		} else {
			(void) sprintf(name, "%x,%x", data[0], data[1]);
		}

		USB_DPRINTF_L3(DPRINT_MASK_USBA,
		    hubdi_log_handle, "usba_bus_ctl: name = %s", name);

		ddi_prop_free(data);
		ddi_set_name_addr(child_dip, name);

		/*
		 * increment the reference count for each child using this
		 * usb_device structure
		 */
		mutex_enter(&usb_device->usb_mutex);
		usb_device->usb_ref_count++;

		USB_DPRINTF_L3(DPRINT_MASK_USBA, hubdi_log_handle,
		    "usba_bus_ctl: init usb_device = 0x%p ref_count = %d",
		    (void *)usb_device, usb_device->usb_ref_count);

		if ((usb_device->usb_ref_count == 1) &&
		    (usb_device->usb_hcd_private == NULL)) {
			mutex_exit(&usb_device->usb_mutex);
			if (usb_hcdi_ops->usb_hcdi_client_init != NULL) {
				if ((*usb_hcdi_ops->usb_hcdi_client_init)
				    (usb_device) != USB_SUCCESS) {

					ddi_set_name_addr(child_dip, NULL);

					return (DDI_FAILURE);
				}
				ASSERT(usb_device->usb_hcd_private);
			}
		} else {
			mutex_exit(&usb_device->usb_mutex);
		}

		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_UNINITCHILD:
	{
		usb_device = usba_get_usb_device(child_dip);

		if (usb_device != NULL) {
			/*
			 * decrement the reference count for each child
			 * using this  usb_device structure
			 */
			mutex_enter(&usb_device->usb_mutex);
			usb_device->usb_ref_count--;

			USB_DPRINTF_L3(DPRINT_MASK_USBA, hubdi_log_handle,
			    "usba_hcdi_bus_ctl: uninit usb_device = 0x%p "
			    "ref_count = %d",
			    usb_device, usb_device->usb_ref_count);

			mutex_exit(&usb_device->usb_mutex);
		}
		ddi_set_name_addr(child_dip, NULL);

		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_IOMIN:
		/* Do nothing */
		return (DDI_SUCCESS);

	/*
	 * These ops correspond	to functions that "shouldn't" be called
	 * by a	USB client driver.  So	we whinge when we're called.
	 */
	case DDI_CTLOPS_DMAPMAPC:
	case DDI_CTLOPS_REPORTINT:
	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
	case DDI_CTLOPS_NINTRS:
	case DDI_CTLOPS_SIDDEV:
	case DDI_CTLOPS_SLAVEONLY:
	case DDI_CTLOPS_AFFINITY:
	case DDI_CTLOPS_POKE_INIT:
	case DDI_CTLOPS_POKE_FLUSH:
	case DDI_CTLOPS_POKE_FINI:
	case DDI_CTLOPS_INTR_HILEVEL:
	case DDI_CTLOPS_XLATE_INTRS:
		cmn_err(CE_CONT, "%s%d:	invalid	op (%d)	from %s%d",
			ddi_node_name(dip), ddi_get_instance(dip),
			op, ddi_node_name(rdip), ddi_get_instance(rdip));
		return (DDI_FAILURE);

	/*
	 * Everything else (e.g. PTOB/BTOP/BTOPR requests) we pass up
	 */
	default:
		return (ddi_ctlops(dip,	rdip, op, arg, result));
	}
}


/*
 * initialize and destroy USBA module
 */
void
usba_usba_initialization()
{
	usba_log_handle = usb_alloc_log_handle(NULL, "usba", &usba_errlevel,
				&usba_errmask, NULL, &usba_show_label, 0);

	USB_DPRINTF_L4(DPRINT_MASK_USBA,
	    usba_log_handle, "usba_usba_initialization");

	mutex_init(&usba_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&usb_device_list.list_mutex, NULL, MUTEX_DRIVER, NULL);

#ifdef	DEBUG
	mutex_init(&usba_dump_mutex, NULL, MUTEX_DRIVER, NULL);
#endif	/* DEBUG */
}


void
usba_usba_destroy()
{
	USB_DPRINTF_L4(DPRINT_MASK_USBA,
	    usba_log_handle, "usba_usba_destroy");

	mutex_destroy(&usba_mutex);
	mutex_destroy(&usb_device_list.list_mutex);

#ifdef	DEBUG
	mutex_destroy(&usba_dump_mutex);
#endif	/* DEBUG */

	usb_free_log_handle(usba_log_handle);
}


/*
 * usba_set_usb_address:
 *	set usb address in usb_device structure
 */
int
usba_set_usb_address(usb_device_t *usb_device)
{
	usb_addr_t address;
	uchar_t s = 8;
	usb_hcdi_t *hcdi;
	char *usb_address_in_use;

	mutex_enter(&usb_device->usb_mutex);

	hcdi = usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);

	mutex_enter(&hcdi->hcdi_mutex);
	usb_address_in_use = hcdi->hcdi_usb_address_in_use;

	for (address = ROOT_HUB_ADDR + 1;
	    address <= USB_MAX_ADDRESS; address++) {
		if (usb_address_in_use[address/s] & (1 << (address % s))) {
			continue;
		}
		usb_address_in_use[address/s] |= (1 << (address % s));
		hcdi->hcdi_device_count++;
		HCDI_HOTPLUG_STATS_DATA(hcdi)->hcdi_device_count.value.ui64++;
		mutex_exit(&hcdi->hcdi_mutex);

		USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_set_usb_address: %d", address);

		usb_device->usb_addr = address;

		mutex_exit(&usb_device->usb_mutex);

		return (USB_SUCCESS);
	}

	usb_device->usb_addr = 0;

	USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
	    "no usb address available");

	mutex_exit(&hcdi->hcdi_mutex);
	mutex_exit(&usb_device->usb_mutex);

	return (USB_FAILURE);
}


/*
 * usba_unset_usb_address:
 *	unset usb_address in usb_device structure
 */
void
usba_unset_usb_address(usb_device_t *usb_device)
{
	usb_addr_t address;
	usb_hcdi_t *hcdi;
	uchar_t s = 8;
	char *usb_address_in_use;

	mutex_enter(&usb_device->usb_mutex);
	address = usb_device->usb_addr;
	hcdi = usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);

	if (address > ROOT_HUB_ADDR) {
		USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_unset_usb_address: address = %d", address);

		mutex_enter(&hcdi->hcdi_mutex);
		usb_address_in_use = hcdi->hcdi_usb_address_in_use;

		ASSERT(usb_address_in_use[address/s] & (1 << (address % s)));

		usb_address_in_use[address/s] &= ~(1 << (address % s));

		hcdi->hcdi_device_count--;
		HCDI_HOTPLUG_STATS_DATA(hcdi)->hcdi_device_count.value.ui64--;

		mutex_exit(&hcdi->hcdi_mutex);

		usb_device->usb_addr = 0;
	}
	mutex_exit(&usb_device->usb_mutex);
}


/*
 * allocate a usb device structure and link it in the list
 */
usb_device_t *
usba_alloc_usb_device()
{
	usb_device_t *usb_device;
	int i;

	/*
	 * create a new usb_device structure
	 */
	usb_device = kmem_zalloc(sizeof (usb_device_t), KM_SLEEP);

	/*
	 * initialize usb_device
	 */
	mutex_init(&usb_device->usb_mutex, NULL, MUTEX_DRIVER, NULL);
	cv_init(&usb_device->usb_cv_resrvd, NULL, CV_DRIVER, NULL);

	mutex_init(&usb_device->usb_device_list.list_mutex, NULL,
	    MUTEX_DRIVER, NULL);

	mutex_enter(&usb_device->usb_mutex);

	/*
	 * add to list of usb_devices
	 */
	mutex_enter(&usba_mutex);
	usba_add_list(&usb_device_list, &usb_device->usb_device_list);
	mutex_exit(&usba_mutex);

	for (i = 0; i < USB_N_ENDPOINTS; i++) {
		mutex_init(&usb_device->usb_pipehandle_list[i].list_mutex,
				NULL, MUTEX_DRIVER, NULL);
	}

	USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
	    "allocated usb_device 0x%p", (void *)usb_device);

	mutex_exit(&usb_device->usb_mutex);

	return (usb_device);
}


/*
 * free usb device structure
 */
void
usba_free_usb_device(usb_device_t *usb_device)
{
	int i;

	if (usb_device == NULL) {

		return;
	}

	mutex_enter(&usb_device->usb_mutex);
	ASSERT(usb_device->usb_ref_count == 0);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_free_usb_device 0x%p, address = 0x%x, ref cnt = %d",
	    (void *)usb_device, usb_device->usb_addr,
	    usb_device->usb_ref_count);

	mutex_exit(&usb_device->usb_mutex);

	mutex_enter(&usba_mutex);
	usba_remove_list(&usb_device_list, &usb_device->usb_device_list);
	mutex_exit(&usba_mutex);

	mutex_destroy(&usb_device->usb_device_list.list_mutex);

	USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
	    "deallocating usb_device = 0x%p, address = 0x%x",
	    (void *)usb_device, usb_device->usb_addr);

	/*
	 * ohci allocates descriptors for root hub so we can't
	 * deallocate these here
	 */
	if (usb_device->usb_addr != ROOT_HUB_ADDR) {
		if (usb_device->usb_dev_descr) {
			kmem_free(usb_device->usb_dev_descr,
				    sizeof (usb_device_descr_t));
		}

		if (usb_device->usb_config) {
			kmem_free(usb_device->usb_config,
				    usb_device->usb_config_length);
		}

		if (usb_device->usb_string_descr) {
			kmem_free(usb_device->usb_string_descr,
				USB_MAXSTRINGLEN);
		}

		usba_unset_usb_address(usb_device);
	}

#ifdef DEBUG
	/*
	 * verify that it is really safe to deallocate
	 */
	for (i = 0; i < USB_N_ENDPOINTS; i++) {
		mutex_enter(&usb_device->usb_mutex);
		ASSERT(usb_device->usb_pipehandle_list[i].next == NULL);
		ASSERT(usb_device->usb_pipehandle_list[i].prev == NULL);
		ASSERT(usb_device->usb_pipe_reserved[i] == NULL);
		ASSERT(usb_device->usb_endp_open[i] == 0);
		mutex_exit(&usb_device->usb_mutex);
	}
#endif	/* DEBUG */

	/*
	 * call hcd's client free so it can deallocate its private sources
	 */
	if (usb_device->usb_hcd_private) {
		usb_hcdi_t *usb_hcdi =
			usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);
		usb_hcdi_ops_t *usb_hcdi_ops = usb_hcdi->hcdi_ops;

		ASSERT(usb_hcdi_ops != NULL);

		if (usb_hcdi_ops->usb_hcdi_client_free != NULL) {
			(*(usb_hcdi_ops->usb_hcdi_client_free))(usb_device);
		}
	}

	for (i = 0; i < USB_N_ENDPOINTS; i++) {
		mutex_destroy(&usb_device->usb_pipehandle_list[i].list_mutex);
	}


	/*
	 * finally ready to destroy the structure
	 */
#ifdef DEBUG
	mutex_enter(&usb_device->usb_mutex);
	ASSERT(usb_device->usb_endp_excl_open == 0);
	mutex_exit(&usb_device->usb_mutex);
#endif	/* DEBUG */
	cv_destroy(&usb_device->usb_cv_resrvd);

	mutex_destroy(&usb_device->usb_mutex);

	kmem_free((caddr_t)usb_device, sizeof (usb_device_t));
}


/*
 * single thread bus enumeration
 */
void
usba_enter_enumeration(usb_device_t *usb_device)
{
	usb_hcdi_t *hcdi;

	mutex_enter(&usb_device->usb_mutex);
	hcdi = usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);
	mutex_exit(&usb_device->usb_mutex);

	sema_p(&hcdi->hcdi_init_ep_sema);
}


void
usba_exit_enumeration(usb_device_t *usb_device)
{
	usb_hcdi_t *hcdi;

	mutex_enter(&usb_device->usb_mutex);
	hcdi = usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);
	mutex_exit(&usb_device->usb_mutex);

	sema_v(&hcdi->hcdi_init_ep_sema);
}


/*
 * usba_create_child_devi():
 *	create a child devinfo node, usb_device, attach properties.
 *	the usb_device structure is shared between all interfaces
 */
int
usba_create_child_devi(dev_info_t	*dip,
		char		*node_name,
		usb_hcdi_ops_t	*usb_hcdi_ops,
		dev_info_t	*usb_root_hub_dip,
		usb_hubdi_ops_t	*usb_hubdi_ops,
		usb_port_status_t port_status,
		usb_device_t	*usb_device,
		dev_info_t	**child_dip)
{
	int rval;
	int usb_device_allocated = 0;
	usb_addr_t	address;

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_create_child_devi: %s usb_device = 0x%p "
	    "port status = 0x%x", node_name,
	    (void *)usb_device, port_status);

	ndi_devi_alloc_sleep(dip, node_name, (dnode_t)DEVI_SID_NODEID,
				child_dip);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "child dip=0x%p", *child_dip);

	if (usb_device == NULL) {

		usb_device = usba_alloc_usb_device();

		/* grab the mutex to keep warlock happy */
		mutex_enter(&usb_device->usb_mutex);
		usb_device->usb_hcdi_ops	= usb_hcdi_ops;
		usb_device->usb_parent_hubdi_ops = usb_hubdi_ops;
		usb_device->usb_root_hub_dip = usb_root_hub_dip;
		usb_device->usb_port_status	= port_status;
		mutex_exit(&usb_device->usb_mutex);

		usb_device_allocated++;
	} else {
		mutex_enter(&usb_device->usb_mutex);
		if (usb_hcdi_ops) {
			ASSERT(usb_device->usb_hcdi_ops	== usb_hcdi_ops);
		}
		if (usb_hubdi_ops) {
			ASSERT(usb_device->usb_parent_hubdi_ops ==
							usb_hubdi_ops);
		}
		if (usb_root_hub_dip) {
			ASSERT(usb_device->usb_root_hub_dip ==
						usb_root_hub_dip);
		}

		usb_device->usb_port_status	= port_status;

		mutex_exit(&usb_device->usb_mutex);
	}

	if (usb_device->usb_addr == 0) {
		if (usba_set_usb_address(usb_device) == USB_FAILURE) {
			address = 0;

			USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
				"cannot set usb address for dip=0x%p",
				*child_dip);

			goto fail;
		}
	}
	address = usb_device->usb_addr;

	/* attach properties */
	rval = ndi_prop_update_int(DDI_DEV_T_NONE, *child_dip,
		"assigned-address", address);
	if (rval != DDI_PROP_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
			"cannot set usb address property for dip=0x%p",
			*child_dip);
		rval = NDI_FAILURE;

		goto fail;
	}

	/*
	 * store the usb_device point in the dip
	 */
	usba_set_usb_device(*child_dip, usb_device);

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_create_child_devi: devi=0x%p (%s) ud=0x%p",
		*child_dip, ddi_driver_name(*child_dip), usb_device);

	return (USB_SUCCESS);

fail:
	if (*child_dip) {
		int rval = usba_destroy_child_devi(*child_dip,
				NDI_DEVI_REMOVE|NDI_DEVI_FORCE);
		ASSERT(rval == USB_SUCCESS);
		*child_dip = NULL;
	}

	if (usb_device_allocated) {
		usba_free_usb_device(usb_device);
	} else if (address && usb_device) {
		usba_unset_usb_address(usb_device);
	}

	USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_create_child_devi failed: rval=%d", rval);

	switch (rval) {
	case NDI_NOMEM:
		return (USB_NO_RESOURCES);
	case NDI_FAILURE:
	default:
		return (USB_FAILURE);
	}
}


int
usba_destroy_child_devi(dev_info_t *dip, uint_t flag)
{
	usb_device_t	*usb_device;

	USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_destroy_child_devi: %s%d (0x%p)",
	    ddi_driver_name(dip), ddi_get_instance(dip), dip);

	usb_device = usba_get_usb_device(dip);


	/*
	 * if the child hasn't been bound yet, we can just
	 * free the dip
	 */
	if (DEVI_NEEDS_BINDING(dip)) {
		/*
		 * do not call ndi_devi_free() since it might
		 * deadlock
		 */
		(void) ddi_remove_child(dip, 0);

	} else {
		int rval;

		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_destroy_child_devi:\n\t"
		    "offlining dip 0x%p usb_device = 0x%p", dip,
		    (void *)usb_device);

		rval =	ndi_devi_offline(dip, flag);
		if (rval != DDI_SUCCESS) {

			USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
			    "offlining %s%d failed (%d)",
			    ddi_driver_name(dip), ddi_get_instance(dip),
			    rval);

			return (USB_FAILURE);
		}
	}

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_destroy_child_devi: done");

	return (USB_SUCCESS);
}


/*
 * list management
 */
void
usba_add_list(usba_list_entry_t *head, usba_list_entry_t *element)
{

	mutex_enter(&head->list_mutex);
	mutex_enter(&element->list_mutex);

	element->next = NULL;

	if (head->next == NULL) {
		head->prev = head->next = element;
		element->prev = NULL;
	} else {
		/* add to tail */
		head->prev->next = element;
		element->prev = head->prev;
		head->prev = element;
	}

	mutex_exit(&head->list_mutex);
	mutex_exit(&element->list_mutex);

}


void
usba_remove_list(usba_list_entry_t *head, usba_list_entry_t *element)
{
	usba_list_entry_t *e;
	int found = 0;

	mutex_enter(&element->list_mutex);
	e = head->next;

	while (e) {
		if (e == element) {
			found++;
			break;
		}
		e = e->next;
	}

	if (!found) {
		mutex_exit(&element->list_mutex);
		return;
	}


	mutex_enter(&head->list_mutex);

	if (element->next) {
		element->next->prev = element->prev;
	}
	if (element->prev) {
		element->prev->next = element->next;
	}
	if (head->next == element) {
		head->next = element->next;
	}
	if (head->prev == element) {
		head->prev = element->prev;
	}

	element->prev = element->next = NULL;

	mutex_exit(&head->list_mutex);
	mutex_exit(&element->list_mutex);

}


int
usba_check_in_list(usba_list_entry_t *head, usba_list_entry_t *element)
{
	int rval = -1;
	usba_list_entry_t *next;

	mutex_enter(&head->list_mutex);
	mutex_enter(&element->list_mutex);
	for (next = head->next; next != NULL; next = next->next) {
		if (next == element) {
			rval = 0;
			break;
		}
	}
	mutex_exit(&element->list_mutex);
	mutex_exit(&head->list_mutex);

	return (rval);
}


/*
 * check whether this dip is the root hub. instead of doing a
 * strcmp on the node name we could also check the address
 */
int
usba_is_root_hub(dev_info_t *dip)
{
	if (dip) {
		return (ddi_prop_exists(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, "root-hub"));
	}
	return (0);
}


/*
 * get and store usb_device pointer in the devi
 */
usb_device_t *
usba_get_usb_device(dev_info_t *dip)
{
	/*
	 * we cannot use parent_data in the usb node because its
	 * bus parent (eg. PCI nexus driver) uses this data
	 *
	 * we cannot use driver data in the other usb nodes since
	 * usb drivers may need to use this
	 */
	if (usba_is_root_hub(dip)) {
		usb_hcdi_t *hcdi = usba_hcdi_get_hcdi(dip);
		return (hcdi->hcdi_usb_device);
	} else {
		return ((usb_device_t *)(DEVI(dip)->devi_parent_data));
	}
}


/*
 * Retrieve the usb_device pointer from the dev without checking for
 * the root hub first.  This function is only used in polled mode.
 */
usb_device_t *
usba_polled_get_usb_device(dev_info_t *dip)
{
	/*
	 * Don't call usba_is_root_hub() to find out if this is
	 * the root hub  usba_is_root_hub() calls into the DDI
	 * where there are locking issues. The dip sent in during
	 * polled mode will never be the root hub, so just get
	 * the usb_device pointer from the dip.
	 */
	return ((usb_device_t *)(DEVI(dip)->devi_parent_data));
}


void
usba_set_usb_device(dev_info_t *dip, usb_device_t *usb_device)
{
	if (usba_is_root_hub(dip)) {
		usb_hcdi_t *hcdi = usba_hcdi_get_hcdi(dip);
		/* no locking is needed here */
		_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(hcdi->hcdi_usb_device))
		hcdi->hcdi_usb_device = usb_device;
		_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(hcdi->hcdi_usb_device))
	} else {
		DEVI(dip)->devi_parent_data = (caddr_t)usb_device;
	}
}


/*
 * usba_set_node_name() according to class, subclass, and protocol.
 * a subclass == -1 or protocol == -1 is considered a "don't care".
 */
#define	DONTCARE	-1

static struct node_name_entry {
	uint8_t class;
	int16_t subclass;
	int16_t protocol;
	char	*name;
} node_name_table[] = {
{ HUB_CLASS_CODE,	DONTCARE,	DONTCARE,	"hub" },
{ HID_CLASS_CODE, HID_SUBCLASS, HID_KEYBOARD_PROTOCOL,	"keyboard" },
{ HID_CLASS_CODE, HID_SUBCLASS, HID_MOUSE_PROTOCOL,	"mouse" },
{ HID_CLASS_CODE,	DONTCARE,	DONTCARE,	"hid" },
{ MASS_STORAGE_CLASS_CODE, DONTCARE,	DONTCARE,	"storage" }
};

static int node_name_table_size = sizeof (node_name_table)/
					sizeof (struct node_name_entry);

static void
usba_set_node_name(dev_info_t *dip, uint8_t class, uint8_t subclass,
    uint8_t protocol)
{
	int i;

	for (i = 0; i < node_name_table_size; i++) {
		uint8_t c = node_name_table[i].class;
		int16_t s = node_name_table[i].subclass;
		int16_t p = node_name_table[i].protocol;

		if ((c == class) && ((s == DONTCARE) || (s == subclass)) &&
		    ((p == DONTCARE) || (p == protocol))) {
			char *name = node_name_table[i].name;
			(void) ndi_devi_set_nodename(dip, name, 0);
			break;
		}
	}
}


/*
 * check if child_dip is indeed a child of parent_dip
 * (it is assumed that the parent keeps the device tree stable)
 * (to be sure, we also grep the lock)
 */
int
usba_child_exists(dev_info_t *parent_dip, dev_info_t *child_dip)
{
	struct dev_info	*dip;
	boolean_t	found;

	rw_enter(&(devinfo_tree_lock), RW_READER);
	dip = DEVI(parent_dip)->devi_child;
	found = (dip == DEVI(child_dip));
	while ((found == B_FALSE) && dip) {
		dip = dip->devi_sibling;
		found = (dip == DEVI(child_dip));
	}
	rw_exit(&(devinfo_tree_lock));

	return (found ? USB_SUCCESS : USB_FAILURE);
}


/*
 * walk the children of the parent of this devi and compare the
 * name and  reg property of each child. If there is a match
 * return this node
 */
static dev_info_t *
usba_find_existing_node(dev_info_t *odip)
{
	dev_info_t *ndip, *child, *pdip;
	int	*odata, *ndata;
	uint_t	n_odata, n_ndata;

	pdip = ddi_get_parent(odip);
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY,
	    odip, DDI_PROP_DONTPASS, "reg",
	    &odata, &n_odata) != DDI_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_HCDI, usba_log_handle,
		    "usba_find_existing_node: "
		    "%s: DDI_NOT_WELL_FORMED", ddi_driver_name(odip));

		return (NULL);
	}

	rw_enter(&(devinfo_tree_lock), RW_READER);
	ndip = (dev_info_t *)(DEVI(pdip)->devi_child);
	while ((child = ndip) != NULL) {

		ndip = (dev_info_t *)(DEVI(child)->devi_sibling);

		if (child == odip) {
			continue;
		}

		if (strcmp(DEVI(child)->devi_node_name,
		    DEVI(odip)->devi_node_name)) {
			continue;
		}

		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY,
		    child, DDI_PROP_DONTPASS, "reg",
		    &ndata, &n_ndata) != DDI_SUCCESS) {

			USB_DPRINTF_L1(DPRINT_MASK_HCDI, usba_log_handle,
			    "usba_find_existing_node: "
			    "%s DDI_NOT_WELL_FORMED", ddi_driver_name(child));

		} else if (n_ndata && n_odata && (bcmp(odata, ndata,
		    max(n_odata, n_ndata) * sizeof (int)) == 0)) {

			USB_DPRINTF_L3(DPRINT_MASK_HCDI, usba_log_handle,
			    "usba_find_existing_node: found %s%d (%p)",
			    ddi_driver_name(child),
			    ddi_get_instance(child), (void *)child);

			USB_DPRINTF_L3(DPRINT_MASK_HCDI, usba_log_handle,
			    "usba_find_existing_node: "
			    "reg: %x %x %x - %x %x %x",
			    n_odata, odata[0], odata[1],
			    n_ndata, ndata[0], ndata[1]);

			ddi_prop_free(ndata);

			break;

		} else {
			ddi_prop_free(ndata);
		}
	}

	rw_exit(&(devinfo_tree_lock));

	ddi_prop_free(odata);

	return (child);
}


/*
 * driver binding support at device level
 */
dev_info_t *
usba_bind_driver_to_device(dev_info_t *child_dip, uint_t flag)
{
	int		rval, i;
	int		n = 0;
	usb_device_t	*usb_device = usba_get_usb_device(child_dip);
	usb_device_descr_t	*usb_dev_descr;
	uint_t		n_configs;	/* number of configs */
	uint_t		n_interfaces;	/* number of interfaces */
	uint_t		port;
	size_t		usb_config_length;
	uchar_t 	*usb_config;
	int		reg[1];
	usb_addr_t	address = usb_get_addr(child_dip);
	usb_interface_descr_t	interface_descriptor;
	size_t		size;
	dev_info_t	*ndip;
	dev_info_t	*old_dip = NULL;
	int		combined_node = 0;
	int		load_ddivs = 0;
	int		is_hub;

	usb_config = usb_get_raw_config_data(child_dip,
					&usb_config_length);

	mutex_enter(&usb_device->usb_mutex);
	mutex_enter(&usba_mutex);

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_bind_driver_to_device: child=0x%p, flag=%x",
	    (void *)child_dip, flag);

	port = usb_device->usb_port;
	usb_dev_descr = usb_device->usb_dev_descr;
	n_configs = usb_device->usb_n_configs;
	n_interfaces = usb_device->usb_n_interfaces;


	if (address != ROOT_HUB_ADDR) {
		size = usb_parse_interface_descr(
				usb_config,
				usb_config_length,
				0,		/* interface index */
				0,		/* alt interface index */
				&interface_descriptor,
				USB_IF_DESCR_SIZE);

		if (size != USB_IF_DESCR_SIZE) {

			USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
			    "parsing interface: "
			    "size (%d) != USB_IF_DESCR_SIZE (%d)",
			    size, USB_IF_DESCR_SIZE);
			rval = USB_FAILURE;

			goto exit;
		}
	} else {
		/* fake an interface descriptor for the root hub */
		bzero(&interface_descriptor, sizeof (interface_descriptor));

		interface_descriptor.bInterfaceClass = HUB_CLASS_CODE;
	}

	reg[0] = port;

	rval = ndi_prop_update_int_array(
		DDI_DEV_T_NONE, child_dip, "reg", reg, 1);

	if (rval != DDI_PROP_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_bind_driver_to_device: property update failed");

		goto exit;
	}

	combined_node = ((n_configs == 1) && (n_interfaces == 1) &&
	    ((usb_dev_descr->bDeviceClass == HUB_CLASS_CODE) ||
	    (usb_dev_descr->bDeviceClass == 0)));

	is_hub = (interface_descriptor.bInterfaceClass == HUB_CLASS_CODE) ||
		(usb_dev_descr->bDeviceClass == HUB_CLASS_CODE);

	/*
	 * change the devi name to hub or device
	 */
	if ((address != ROOT_HUB_ADDR) && usba_ddivs_usbc &&
	    (address != usba_ddivs_usbc_xaddress) &&
	    (!(usba_ddivs_usbc_xhubs && is_hub))) {
		(void) ndi_devi_set_nodename(child_dip, "ddivs_usbc", 0);
		load_ddivs++;

	} else if (combined_node) {
		usba_set_node_name(child_dip,
		    interface_descriptor.bInterfaceClass,
		    interface_descriptor.bInterfaceSubClass,
		    interface_descriptor.bInterfaceProtocol);
	} else {
		usba_set_node_name(child_dip,
		    usb_dev_descr->bDeviceClass,
		    usb_dev_descr->bDeviceSubClass,
		    usb_dev_descr->bDeviceProtocol);
	}

	/*
	 * check whether there is another dip with this name and address
	 */
	ndip = usba_find_existing_node(child_dip);

	if (ndip) {
		usb_device_t *ud = usba_get_usb_device(ndip);

		/*
		 * we found an existing dip with the same reg property.
		 * update the address property and online this node
		 */
		USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_bind_driver_to_device: using dip %s%d (%p)"
		    "for address=%d (%d)",
		    ddi_driver_name(ndip), ddi_get_instance(ndip),
		    (void *)ndip, address,
		    (ud ? ud->usb_addr : -1));

		/*
		 * use usb_device of the existing dip
		 */
		if (ndi_dev_is_prom_node(ndip) == 0) {

			if (ud) {
				ASSERT(ud == usb_device);
				ASSERT(usb_device->usb_addr == address);
			}
			usba_set_usb_device(child_dip, NULL);

		} else {
			USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
			    "usba_bind_driver_to_device: prom dip %s (%p)",
			    ddi_node_name(ndip), ndip);
		}

		/*
		 * continue with the existing node, destroy
		 * the old dip and deallocate address later
		 */
		usba_set_usb_device(ndip, usb_device);
		old_dip = child_dip;
		child_dip = ndip;

		goto online;
	}

	if (load_ddivs) {
		n = 0;	/* no compatible names needed */

	} else if (combined_node) {

		/* 1. usbVID,PID.REV */
		(void) sprintf(usba_name[n++],
			"usb%x,%x.%x",
			usb_dev_descr->idVendor,
			usb_dev_descr->idProduct,
			usb_dev_descr->bcdDevice);

		/* 2. usbVID,PID */
		(void) sprintf(usba_name[n++],
			"usb%x,%x",
			usb_dev_descr->idVendor,
			usb_dev_descr->idProduct);

		if (usb_dev_descr->bDeviceClass != 0) {
			/* 3. usbVID,classDC.DSC.DPROTO */
			(void) sprintf(usba_name[n++],
				"usb%x,class%x.%x.%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass,
				usb_dev_descr->bDeviceProtocol);

			/* 4. usbVID,classDC.DSC */
			(void) sprintf(usba_name[n++],
				"usb%x,class%x.%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass);

			/* 5. usbVID,classDC */
			(void) sprintf(usba_name[n++],
				"usb%x,class%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->bDeviceClass);

			/* 6. usb,classDC.DSC.DPROTO */
			(void) sprintf(usba_name[n++],
				"usb,class%x.%x.%x",
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass,
				usb_dev_descr->bDeviceProtocol);

			/* 7. usb,classDC.DSC */
			(void) sprintf(usba_name[n++],
				"usb,class%x.%x",
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass);

			/* 8. usb,classDC */
			(void) sprintf(usba_name[n++],
				"usb,class%x",
				usb_dev_descr->bDeviceClass);
		}

		if (interface_descriptor.bInterfaceClass != 0) {
			/* 9. usbifVID,classIC.ISC.IPROTO */
			(void) sprintf(usba_name[n++],
				"usbif%x,class%x.%x.%x",
				usb_dev_descr->idVendor,
				interface_descriptor.bInterfaceClass,
				interface_descriptor.bInterfaceSubClass,
				interface_descriptor.bInterfaceProtocol);

			/* 10. usbifVID,classIC.ISC */
			(void) sprintf(usba_name[n++],
				"usbif%x,class%x.%x",
				usb_dev_descr->idVendor,
				interface_descriptor.bInterfaceClass,
				interface_descriptor.bInterfaceSubClass);

			/* 11. usbifVID,classIC */
			(void) sprintf(usba_name[n++],
				"usbif%x,class%x",
				usb_dev_descr->idVendor,
				interface_descriptor.bInterfaceClass);

			/* 12. usbif,classIC.ISC.IPROTO */
			(void) sprintf(usba_name[n++],
				"usbif,class%x.%x.%x",
				interface_descriptor.bInterfaceClass,
				interface_descriptor.bInterfaceSubClass,
				interface_descriptor.bInterfaceProtocol);

			/* 13. usbif,classIC.ISC */
			(void) sprintf(usba_name[n++],
				"usbif,class%x.%x",
				interface_descriptor.bInterfaceClass,
				interface_descriptor.bInterfaceSubClass);

			/* 14. usbif,classIC */
			(void) sprintf(usba_name[n++],
				"usbif,class%x",
				interface_descriptor.bInterfaceClass);
		}
	} else {
		if (n_configs > 1) {
			/* 1. usbVID,PID.REV.configCN */
			(void) sprintf(usba_name[n++],
				"usb%x,%x.%x.config%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->idProduct,
				usb_dev_descr->bcdDevice,
				usb_device->usb_configuration_value);
		}

		/* 2. usbVID,PID.REV */
		(void) sprintf(usba_name[n++],
			"usb%x,%x.%x",
			usb_dev_descr->idVendor,
			usb_dev_descr->idProduct,
			usb_dev_descr->bcdDevice);

		/* 3. usbVID,PID.configCN */
		if (n_configs > 1) {
			(void) sprintf(usba_name[n++],
				"usb%x,%x.%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->idProduct,
				usb_device->usb_configuration_value);
		}

		/* 4. usbVID,PID */
		(void) sprintf(usba_name[n++],
			"usb%x,%x",
			usb_dev_descr->idVendor,
			usb_dev_descr->idProduct);

		if (usb_dev_descr->bDeviceClass != 0) {
			/* 5. usbVID,classDC.DSC.DPROTO */
			(void) sprintf(usba_name[n++],
				"usb%x,class%x.%x.%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass,
				usb_dev_descr->bDeviceProtocol);

			/* 6. usbVID,classDC.DSC */
			(void) sprintf(usba_name[n++],
				"usb%x.class%x.%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass);

			/* 7. usbVID,classDC */
			(void) sprintf(usba_name[n++],
				"usb%x.class%x",
				usb_dev_descr->idVendor,
				usb_dev_descr->bDeviceClass);

			/* 8. usb,classDC.DSC.DPROTO */
			(void) sprintf(usba_name[n++],
				"usb,class%x.%x.%x",
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass,
				usb_dev_descr->bDeviceProtocol);

			/* 9. usb,classDC.DSC */
			(void) sprintf(usba_name[n++],
				"usb,class%x.%x",
				usb_dev_descr->bDeviceClass,
				usb_dev_descr->bDeviceSubClass);

			/* 10. usb,classDC */
			(void) sprintf(usba_name[n++],
				"usb,class%x",
				usb_dev_descr->bDeviceClass);
		}

		/* 11. usb,device */
		(void) sprintf(usba_name[n++], "usb,device");
	}

	if (n) {
		rval = ndi_prop_update_string_array(
		    DDI_DEV_T_NONE, child_dip,
		    "compatible", (char **)usba_compatible, n);

		if (rval != DDI_PROP_SUCCESS) {

			USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
			    "usba_bind_driver_to_device: "
			    "property update failed");

			goto exit;
		}
	}

	for (i = 0; i < n; i += 2) {
		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
		    "compatible name:\t%s\t%s", usba_compatible[i],
		    (((i+1) < n)? usba_compatible[i+1] : ""));
	}

#ifdef	DEBUG
	if (usba_dump_descriptors_flag) {
		usba_dump_descriptors(child_dip, USB_ALLOW_LABEL);
	}
#endif	/* DEBUG */

online:
	/* update the address property */
	rval = ndi_prop_update_int(DDI_DEV_T_NONE, child_dip,
			"assigned-address", usb_device->usb_addr);
	if (rval != DDI_PROP_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_bind_driver_to_device: address update failed");
	}

	if (!combined_node) {
		/* update the configuration property */
		rval = ndi_prop_update_int(DDI_DEV_T_NONE, child_dip,
			"configuration#", usb_device->usb_configuration_value);
		if (rval != DDI_PROP_SUCCESS) {
			USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
			    "usba_bind_driver_to_device: "
			    "config prop update failed");
		}
	}

	if (usb_device->usb_port_status == USB_LOW_SPEED_DEV) {
		/* create boolean property */
		rval = ndi_prop_create_boolean(DDI_DEV_T_NONE, child_dip,
			"low-speed");
		if (rval != DDI_PROP_SUCCESS) {
			USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
			    "usba_bind_driver_to_device: "
			    "low speed prop update failed");
		}
	}

	USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
	    "%s%d at port %d: %s, dip = 0x%p",
	    ddi_node_name(ddi_get_parent(child_dip)),
	    ddi_get_instance(ddi_get_parent(child_dip)),
	    port, ddi_node_name(child_dip), child_dip);

	if (flag & USBA_BIND_ATTACH) {
		/* attach device's driver */
		rval = ndi_devi_online(child_dip, NDI_CONFIG);
	}

	if (rval != NDI_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
		    "%s online failed",  ddi_node_name(child_dip));

		/* wait for disconnect event to clean up */
	}

exit:
	mutex_exit(&usba_mutex);
	mutex_exit(&usb_device->usb_mutex);

	if (old_dip) {
		int rval = usba_destroy_child_devi(old_dip,
			NDI_DEVI_REMOVE|NDI_DEVI_FORCE);
		ASSERT(rval == USB_SUCCESS);
	}

	return (child_dip);
}


/*
 * driver binding at interface level, the first arg will be the
 * the parent dip
 */
/*ARGSUSED*/
dev_info_t *
usba_bind_driver_to_interface(dev_info_t *dip, uint_t interface,
    uint_t flag)
{
	dev_info_t		*child_dip = NULL;
	usb_device_t		*child_ud =
					usba_get_usb_device(dip);
	usb_device_descr_t	*usb_dev_descr;
	size_t			usb_config_length;
	uchar_t 		*usb_config;
	usb_interface_descr_t	interface_descriptor;
	int			i, n, rval;
	int			reg[2];
	size_t			size;
	dev_info_t		*ndip;
	dev_info_t		*rm_dip = NULL;
	usb_port_status_t	port_status;

	usb_config = usb_get_raw_config_data(dip, &usb_config_length);

	mutex_enter(&child_ud->usb_mutex);

	usb_dev_descr = child_ud->usb_dev_descr;

	/*
	 * for each interface, determine all compatible names
	 */
	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_bind_driver_to_interface: "
	    "port %d, interface = %d port status = %x",
	    child_ud->usb_port, interface,
	    child_ud->usb_port_status);

	/* Parse the interface descriptor */
	size = usb_parse_interface_descr(
			usb_config,
			usb_config_length,
			interface,	/* interface index */
			0,		/* alt interface index */
			&interface_descriptor,
			USB_IF_DESCR_SIZE);

	if (size != USB_IF_DESCR_SIZE) {

		USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
		    "parsing interface: "
		    "size (%d) != USB_IF_DESCR_SIZE (%d)",
		    size, USB_IF_DESCR_SIZE);

		mutex_enter(&usba_mutex);
		rval = USB_FAILURE;

		goto exit;
	}

	port_status = child_ud->usb_port_status;
	mutex_exit(&child_ud->usb_mutex);

	/* clone this dip */
	rval =	usba_create_child_devi(dip,
			"interface",
			NULL,		/* usb_hcdi ops */
			NULL,		/* root hub dip */
			NULL,		/* usb_usbai_ops */
			port_status,	/* port status */
			child_ud,	/* share this usb_device */
			&child_dip);

	mutex_enter(&child_ud->usb_mutex);
	mutex_enter(&usba_mutex);

	if (rval != USB_SUCCESS) {

		goto exit;
	}

	/* create reg property */
	reg[0] = interface;
	reg[1] = child_ud->usb_configuration_value;

	rval = ndi_prop_update_int_array(
		DDI_DEV_T_NONE, child_dip, "reg", reg, 2);

	if (rval != DDI_PROP_SUCCESS) {

		goto exit;
	}

	usba_set_node_name(child_dip,
	    interface_descriptor.bInterfaceClass,
	    interface_descriptor.bInterfaceSubClass,
	    interface_descriptor.bInterfaceProtocol);

	/*
	 * check whether there is another dip with this name and address
	 */
	ndip = usba_find_existing_node(child_dip);
	if (ndip) {
		/*
		 * we found an existing dip with the same reg property.
		 * update the address property and online this node
		 */
		USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_bind_driver_to_interface: using dip %s%d (%p)"
		    " address = %d",
		    ddi_driver_name(ndip), ddi_get_instance(ndip),
		    (void *)ndip, child_ud->usb_addr);

		if (ndi_dev_is_prom_node(ndip) == 0) {

			USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
			    "usba_bind_driver_to_device: non-prom dip %s (%p)",
			    ddi_node_name(ndip), ndip);

			usba_set_usb_device(child_dip, NULL);

		} else {
			USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
			    "usba_bind_driver_to_device: prom dip %s (%p)",
			    ddi_node_name(ndip), ndip);
		}

		/* continue with the existing node */
		usba_set_usb_device(ndip, child_ud);
		rm_dip = child_dip;
		child_dip = ndip;

		goto online;
	}

	n = 0;

	/* 1) usbifVID,PID.REV.configCN.IN */
	(void) sprintf(usba_name[n++],
			"usbif%x,%x.%x.config%x.%x",
			usb_dev_descr->idVendor,
			usb_dev_descr->idProduct,
			usb_dev_descr->bcdDevice,
			child_ud->usb_configuration_value,
			interface);

	/* 2) usbifVID,PID.configCN.IN */
	(void) sprintf(usba_name[n++],
			"usbif%x,%x.config%x.%x",
			usb_dev_descr->idVendor,
			usb_dev_descr->idProduct,
			child_ud->usb_configuration_value,
			interface);


	if (interface_descriptor.bInterfaceClass) {
		/* 3) usbifVID,classIC.ISC.IPROTO */
		(void) sprintf(usba_name[n++],
			"usbif%x,class%x.%x.%x",
			usb_dev_descr->idVendor,
			interface_descriptor.bInterfaceClass,
			interface_descriptor.bInterfaceSubClass,
			interface_descriptor.bInterfaceProtocol);

		/* 4) usbifVID,classIC.ISC */
		(void) sprintf(usba_name[n++],
			"usbif%x,class%x.%x",
			usb_dev_descr->idVendor,
			interface_descriptor.bInterfaceClass,
			interface_descriptor.bInterfaceSubClass);

		/* 5) usbifVID,classIC */
		(void) sprintf(usba_name[n++],
			"usbif%x,class%x",
			usb_dev_descr->idVendor,
			interface_descriptor.bInterfaceClass);

		/* 6) usbif,classIC.ISC.IPROTO */
		(void) sprintf(usba_name[n++],
			"usbif,class%x.%x.%x",
			interface_descriptor.bInterfaceClass,
			interface_descriptor.bInterfaceSubClass,
			interface_descriptor.bInterfaceProtocol);

		/* 7) usbif,classIC.ISC */
		(void) sprintf(usba_name[n++],
			"usbif,class%x.%x",
			interface_descriptor.bInterfaceClass,
			interface_descriptor.bInterfaceSubClass);

		/* 8) usbif,classIC */
		(void) sprintf(usba_name[n++],
			"usbif,class%x",
			interface_descriptor.bInterfaceClass);

	}

	/* create compatible property */
	if (n) {
		rval = ndi_prop_update_string_array(
		    DDI_DEV_T_NONE, child_dip,
		    "compatible", (char **)usba_compatible,
		    n);

		if (rval != DDI_PROP_SUCCESS) {

			goto exit;
		}
	}

	for (i = 0; i < n; i += 2) {
		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
		    "compatible name:\t%s\t%s", usba_compatible[i],
		    (((i+1) < n)? usba_compatible[i+1] : ""));
	}

online:
	/* update the address property */
	rval = ndi_prop_update_int(DDI_DEV_T_NONE, child_dip,
			"assigned-address", child_ud->usb_addr);
	if (rval != DDI_PROP_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_USBA, usba_log_handle,
		    "usba_bind_driver_to_interface: "
		    "address update failed");
	}

	/* create property with interface number */
	rval = ndi_prop_update_int(DDI_DEV_T_NONE, child_dip,
		"interface", interface);

	if (rval != DDI_PROP_SUCCESS) {
		goto exit;
	}


	/* attach device's driver */
	rval = ndi_devi_online(child_dip, 0);

	USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
	    "%s%d port %d: %s, dip = 0x%p",
	    ddi_node_name(ddi_get_parent(dip)),
	    ddi_get_instance(ddi_get_parent(dip)),
	    child_ud->usb_port, ddi_node_name(child_dip), child_dip);

	if (rval != NDI_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_USBA,
		    usba_log_handle, "%s devi attach failed",
		    ddi_node_name(child_dip));
	}

exit:
	mutex_exit(&usba_mutex);
	mutex_exit(&child_ud->usb_mutex);

	if (rm_dip) {
		int rval =  usba_destroy_child_devi(rm_dip,
			NDI_DEVI_REMOVE|NDI_DEVI_FORCE);
		ASSERT(rval == USB_SUCCESS);
	}

	return (child_dip);
}


/*
 * retrieve string descriptors for manufacturer, vendor and serial
 * number
 */

int
usba_get_mfg_product_sn_strings(dev_info_t *dip, char *buf, size_t len)
{
	usb_device_t	*ud = usba_get_usb_device(dip);
	usb_device_descr_t *usb_dev_descr = ud->usb_dev_descr;
	char *tmpbuf;
	size_t tmpbuf_len = USB_MAXSTRINGLEN;
	int rval = 0;
	int l;

	*buf = '\0';
	tmpbuf = kmem_zalloc(tmpbuf_len, KM_SLEEP);

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usba_log_handle,
	    "usba_get_mfg_product_sn_strings: %s%d: m=%d, p=%d, s=%d",
	    ddi_node_name(dip), ddi_get_instance(dip),
	    usb_dev_descr->iManufacturer,
	    usb_dev_descr->iProduct,
	    usb_dev_descr->iSerialNumber);


	if (usb_dev_descr->iManufacturer &&
	    ((rval = usb_get_string_descriptor(
		dip, USB_LANG_ID, usb_dev_descr->iManufacturer,
		tmpbuf, tmpbuf_len)) != USB_SUCCESS)) {

			goto done;
	}

	l = strlen(tmpbuf);
	if ((l == 0) || (l > len)) {
		goto done;
	}

	(void) strcat(buf, tmpbuf);

	if (usb_dev_descr->iProduct &&
	    ((rval = usb_get_string_descriptor(
		dip, USB_LANG_ID, usb_dev_descr->iProduct,
		tmpbuf, tmpbuf_len)) != USB_SUCCESS)) {

			goto done;
	}

	l = strlen(tmpbuf);
	if ((l == 0) || ((l + strlen(buf)) > len)) {
		goto done;
	}

	(void) strcat(buf, ", ");
	(void) strcat(buf, tmpbuf);

	if (usb_dev_descr->iSerialNumber &&
	    ((rval = usb_get_string_descriptor(
		dip, USB_LANG_ID, usb_dev_descr->iSerialNumber,
		tmpbuf, tmpbuf_len)) != USB_SUCCESS)) {

			goto done;
	}

	l = strlen(tmpbuf);
	if ((l == 0) || ((l + strlen(buf)) > len)) {
		goto done;
	}

	(void) strcat(buf, ", ");
	(void) strcat(buf, tmpbuf);

done:
	kmem_free(tmpbuf, tmpbuf_len);

	if (strlen(buf) == 0) {
		if (rval == USB_SUCCESS) {
			return (USB_FAILURE);
		}
		return (rval);
	}

	return (rval);
}


#ifdef	DEBUG
/*
 * USBA dump support:
 */


/* Linked list of all registered dump_ops */
usb_dump_ops_t	*usba_dump_ops_list = NULL;


/*
 * Allocate usb_dump_ops. Called from a USB module
 */
usb_dump_ops_t *
usba_alloc_dump_ops()
{
	usb_dump_ops_t	*dump_ops;

	dump_ops = kmem_zalloc(sizeof (usb_dump_ops_t), KM_SLEEP);
	return (dump_ops);
}


/*
 * Free usb_dump_ops
 */
void
usba_free_dump_ops(usb_dump_ops_t *dump_ops)
{
	kmem_free(dump_ops, sizeof (usb_dump_ops_t));
}

/*
 * Register the dump function with USBA framework.
 */
void
usba_dump_register(usb_dump_ops_t *dump_ops)
{
	usb_dump_ops_t **plist, *list;

	mutex_enter(&usba_dump_mutex);
	plist = &usba_dump_ops_list;
	list = usba_dump_ops_list;

	while (list && (list != dump_ops)) {
		plist = &list->usb_dump_ops_next;
		list = list->usb_dump_ops_next;
	}
	if (list == NULL) {
		*plist = dump_ops;
	}
	mutex_exit(&usba_dump_mutex);
}


/*
 * Deregister the dump function from USBA framework.
 */
void
usba_dump_deregister(usb_dump_ops_t *dump_ops)
{
	usb_dump_ops_t **plist, *list;

	mutex_enter(&usba_dump_mutex);
	plist = &usba_dump_ops_list;
	list = usba_dump_ops_list;

	while (list) {
		if (list == dump_ops) {
			*plist = dump_ops->usb_dump_ops_next;
			dump_ops->usb_dump_ops_next = NULL;
			break;
		}
		plist = &list->usb_dump_ops_next;
		list = list->usb_dump_ops_next;
	}
	mutex_exit(&usba_dump_mutex);
}


/*
 * This function is invoked to display all USB dump information.
 * It could be invoked from kadb using the following
 *		usba_dump_all::call 0xffff
 * Note: The dump_flags in the previous call is set to 0xffff.
 * This lets one to see all the dump information. The dump
 * display could be controlled by setting the flags appropriately.
 *
 * Alternately this function could be invoked from any module.
 * It goes through the list of registered dump functions and calls
 * them one after another.
 */
void
usba_dump_all(uint_t dump_flags)
{
	int	order;
	usb_dump_ops_t	*dump_ops;

	mutex_enter(&usba_dump_mutex);

	dump_ops = usba_dump_ops_list;

	if (!dump_ops) {
		mutex_exit(&usba_dump_mutex);

		return;
	}

	/*
	 * We call the usb_dump functions in a preferred order. This order
	 * is different from the order in which these functions might have
	 * registered. For the preferred order see file usba_impl.h
	 */
	for (order = USB_DUMPOPS_HCDI_ORDER; order <= USB_DUMPOPS_OTHER_ORDER;
		order++) {

		while (dump_ops) {
			if (dump_ops->usb_dump_func &&
				dump_ops->usb_dump_order == order) {
				dump_ops->usb_dump_func(dump_flags,
					dump_ops->usb_dump_cb_arg);
			}

			dump_ops = dump_ops->usb_dump_ops_next;
		}

		dump_ops = usba_dump_ops_list;
	}

	mutex_exit(&usba_dump_mutex);
}


/*
 * utility function to dump a USB device
 */

/*ARGSUSED*/
void
usba_dump_usb_device(usb_device_t *usb_device, uint_t flags)
{
	int	i;
	uint_t	show = usba_show_label;

	if (usb_device == NULL) {

		return;
	}

	usba_show_label = USB_DISALLOW_LABEL;

	mutex_enter(&usb_device->usb_mutex);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_device: 0x%p", usb_device);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_device_list: 0x%p\tusb_hcdi_ops: 0x%p",
	    usb_device->usb_device_list, usb_device->usb_hcdi_ops);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_parent_hubdi_ops: 0x%p",
	    usb_device->usb_parent_hubdi_ops);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_root_hub_dip: 0x%p\tusb_addr: 0x%p",
	    usb_device->usb_root_hub_dip, usb_device->usb_addr);

	USB_DPRINTF_L3(DPRINT_MASK_USBAI, usba_log_handle,
	    "\tusb_root_hubd: 0x%p", usb_device->usb_root_hubd);

	USB_DPRINTF_L3(DPRINT_MASK_USBAI, usba_log_handle,
	    "\tusb_dev_descr: 0x%p\tusb_config: 0x%p",
	    usb_device->usb_dev_descr, usb_device->usb_config);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_config_length: 0x%p \tusb_port: 0x%p",
	    usb_device->usb_config_length, usb_device->usb_port);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_string_descr: 0x%p \tusb_port_status: 0x%x",
	    (void *)usb_device->usb_string_descr, usb_device->usb_port_status);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_endp_excl_open: 0x%x\t\tusb_hcd_private: 0x%p",
	    usb_device->usb_endp_excl_open, usb_device->usb_hcd_private);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle, "\tusb_endp_open:");

	for (i = 0; i < USB_N_ENDPOINTS; i += 4) {
		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
		    "\t\t0x%x\t0x%x\t0x%x\t0x%x",
		    usb_device->usb_endp_open[i],
		    usb_device->usb_endp_open[i + 1],
		    usb_device->usb_endp_open[i + 2],
		    usb_device->usb_endp_open[i + 3]);
	}

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_pipe_reserved:");

	for (i = 0; i < USB_N_ENDPOINTS; i += 4) {
		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
		    "\t\t0x%x\t0x%x\t0x%x\t0x%x",
		    usb_device->usb_pipe_reserved[i],
		    usb_device->usb_pipe_reserved[i + 1],
		    usb_device->usb_pipe_reserved[i + 2],
		    usb_device->usb_pipe_reserved[i + 3]);
	}

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_resvrd_callback_id: 0x%p"
	    "\tusb_configuration_value: 0x%x",
	    usb_device->usb_resvrd_callback_id,
	    usb_device->usb_configuration_value);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_ref_count: 0x%x \t\tusb_n_configs: 0x%x",
	    usb_device->usb_ref_count, usb_device->usb_n_configs);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tusb_n_interfaces: 0x%x\t\tusb_hubdi: 0x%p",
	    usb_device->usb_n_interfaces, usb_device->usb_hubdi);


	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
		"\t***pipe handle information ***: ");

	if (flags & USB_DUMP_PIPE_HANDLE) {
		for (i = 0; i < USB_N_ENDPOINTS; i++) {
			usb_pipe_handle_t ph;
			ph = (usb_pipe_handle_t)
				usb_device->usb_pipehandle_list[i].next;
			if (ph != NULL) {
				USB_DPRINTF_L3(DPRINT_MASK_USBA,
				usba_log_handle, "\tPipe on ep# %d", i);
				mutex_exit(&usb_device->usb_mutex);
				usba_dump_usb_pipe_handle(ph, 0);
				mutex_enter(&usb_device->usb_mutex);
			}
		}
	}

	mutex_exit(&usb_device->usb_mutex);

	usba_show_label = show;
}


/*
 * Utility used to dump a USB pipe handle.
 */
/*ARGSUSED*/
void
usba_dump_usb_pipe_handle(usb_pipe_handle_t pipe_handle, uint_t flags)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	uint_t	show = usba_show_label;

	if (ph == NULL) {
		return;
	}

	usba_show_label = USB_DISALLOW_LABEL;

	mutex_enter(&ph->p_mutex);
	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tPIPE_HANDLE: 0x%p", ph);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_pipe_handle_size: 0x%x", ph->p_pipe_handle_size);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_pipe_handle_list: 0x%p", ph->p_pipe_handle_list);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_usb_device: 0x%p \tp_state: 0x%p",
	    ph->p_usb_device, ph->p_state);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_last_state: 0x%x \t\tp_pipe_flag: 0x%x",
	    ph->p_last_state, ph->p_pipe_flag);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_policy: 0x%p \tp_client_private: 0x%p",
	    ph->p_policy, ph->p_client_private);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_endpoint: 0x%p \tp_usba_private: 0x%p",
	    ph->p_endpoint, ph->p_usba_private);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_busy: 0x%x\t\t\tp_hcd_private: 0x%p",
	    ph->p_busy, ph->p_hcd_private);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\t\tp_sync_result:");
	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\t\tp_done: 0x%p\t\tp_rval: 0x%p",
	    ph->p_sync_result.p_done, ph->p_sync_result.p_rval);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\t\tp_data: 0x%p\t\tp_flag: 0x%p",
	    ph->p_sync_result.p_data, ph->p_sync_result.p_flag);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\t\tp_completion_reason: 0x%p",
	    ph->p_sync_result.p_completion_reason);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\t\tp_pending_async_cbs: 0x%p",
	    ph->p_n_pending_async_cbs);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_hcd_bandwidth: 0x%x \t\tp_async_requests_count:0x%x",
	    ph->p_hcd_bandwidth, ph->p_async_requests_count);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_callers_pipe_handle_p: 0x%p \tp_max_callback_time: 0x%p",
	    ph->p_callers_pipe_handle_p, ph->p_max_callback_time);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_n_pending_async_cbs: 0x%x \tp_max_time_waiting: 0x%p",
	    ph->p_n_pending_async_cbs, ph->p_max_time_waiting);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tp_max_callback:0x%p", ph->p_max_callback);

	mutex_exit(&ph->p_mutex);

	usba_show_label = show;
}


/*
 * Utility used to dump a USB pipe policy.
 */
/*ARGSUSED*/
void
usba_dump_usb_pipe_policy(usb_pipe_policy_t *policy, uint_t flags)
{
	uint_t  show;

	if (policy == NULL) {
		return;
	}

	show = usba_show_label;

	usba_show_label = USB_DISALLOW_LABEL;

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tPIPE_POLICY: 0x%p \t\tpp_version: 0x%x",
	    policy, policy->pp_version);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tpp_periodic_max_transfer_size: 0x%x \tpp_timeout_value: 0x%x",
	    policy->pp_periodic_max_transfer_size, policy->pp_timeout_value);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tpp_callback_arg: 0x%p \t\tpp_callback: 0x%p",
	    policy->pp_callback_arg, policy->pp_callback);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tpp_exception_callback: 0x%p", policy->pp_exception_callback);

	usba_show_label = show;
}


/*
 * utility function to dump all descriptors in the config cloud
 */
/*ARGSUSED*/
void
usba_dump_descriptors(dev_info_t *dip, int show_label)
{
	usb_device_t		*usb_device = usba_get_usb_device(dip);
	usb_device_descr_t	*usb_dev_descr;
	size_t			usb_config_length;
	uchar_t 		*usb_config;
	usb_config_descr_t	config_descriptor;
	usb_config_pwr_descr_t	config_pwr_descriptor;
	usb_interface_descr_t	interface_descriptor;
	usb_interface_pwr_descr_t interface_pwr_descriptor;
	usb_endpoint_descr_t	endpoint_descriptor;
	char			*pathname;
	uint_t			ep;
	int			i;
	size_t			size;
	uint_t			show = usba_show_label;

	usba_show_label = show_label;

	usb_config = usb_device->usb_config;
	usb_config_length = usb_device->usb_config_length;
	usb_dev_descr = usb_device->usb_dev_descr;

	pathname = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	(void) ddi_pathname(dip, pathname);
	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "****** DEVICE: %s", pathname);

	kmem_free(pathname, MAXPATHLEN);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tdevice descriptor:\n\t\t\t"
	    "bLength: 0x%x\t\t\tbDescriptorType: 0x%x\n\t\t\t"
	    "bcdUSB: 0x%x\t\t\tbDeviceClass: 0x%x",
	    usb_dev_descr->bLength, usb_dev_descr->bDescriptorType,
	    usb_dev_descr->bcdUSB, usb_dev_descr->bDeviceClass);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\t\tbDeviceSubClass: 0x%x\t\tbDeviceProtocol: 0x%x\n\t\t\t"
	    "bMaxPacketSize0: 0x%x\t\tidVendor: 0x%x\n\t\t\t"
	    "idProduct: 0x%x  \t\tbcdDevice: 0x%x",
	    usb_dev_descr->bDeviceSubClass, usb_dev_descr->bDeviceProtocol,
	    usb_dev_descr->bMaxPacketSize0, usb_dev_descr->idVendor,
	    usb_dev_descr->idProduct, usb_dev_descr->bcdDevice);

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\t\tiManufacturer: 0x%x\t\tiProduct: 0x%x\n\t\t\t"
	    "iSerialNumber: 0x%x\t\tbNumConfigurations: 0x%x",
	    usb_dev_descr->iManufacturer, usb_dev_descr->iProduct,
	    usb_dev_descr->iSerialNumber, usb_dev_descr->bNumConfigurations);

	/*
	 * Parse the configuration descriptor to find the size of everything
	 */
	size = usb_parse_configuration_descr(usb_config, usb_config_length,
		    &config_descriptor, USB_CONF_DESCR_SIZE);
	if (size != USB_CONF_DESCR_SIZE)  {
		USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
		    "parsing configuration descriptor fails : "
		    "size (%d) != USB_CONF_DESCR_SIZE(%d)",
		    size, USB_CONF_DESCR_SIZE);

		goto done;
	}

	USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
	    "\tconfig descriptor:\n\t\t\t"
	    "bLength: 0x%x\t\t\tbDescriptorType: 0x%x\n\t\t\t"
	    "wTotalLength: 0x%x\t\tbNumInterfaces: 0x%x\n\t\t\t"
	    "bConfigurationValue: 0x%x\tiConfiguration: 0x%x\n\t\t\t"
	    "bmAttributes: 0x%x\t\tMaxPower: 0x%x",
	    config_descriptor.bLength, config_descriptor.bDescriptorType,
	    config_descriptor.wTotalLength, config_descriptor.bNumInterfaces,
	    config_descriptor.bConfigurationValue,
	    config_descriptor.iConfiguration,
	    config_descriptor.bmAttributes, config_descriptor.MaxPower);

	/*
	 * Parse the configuration power descriptor
	 */
	size = usb_parse_config_pwr_descr(usb_config, usb_config_length,
		&config_pwr_descriptor, USB_CONF_PWR_DESCR_SIZE);

	if (size != USB_CONF_PWR_DESCR_SIZE) {
		USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
		    "parsing configuration power descriptor fails:"
		    "size(%d) != USB_CONF_PWR_DESCR_SIZE(%d)",
		    size, USB_CONF_PWR_DESCR_SIZE);
	} else {
		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
			"\tConfiguration Power Descriptor:\n\t\t\t"
			"bLength: %d\t\t\tbDescriptorType: 0x%x\n\t\t\t"
			"SelfPowerConsumedD0_l: 0x%x\n\t\t\t"
			"SelfPowerConsumedD0_h: 0x%x\n\t\t\t"
			"bPowerSummaryId: 0x%x\n\t\t\t"
			"bBusPowerSavingD1: %d",
			config_pwr_descriptor.bLength,
			config_pwr_descriptor.bDescriptorType,
			config_pwr_descriptor.SelfPowerConsumedD0_l,
			config_pwr_descriptor.SelfPowerConsumedD0_h,
			config_pwr_descriptor.bPowerSummaryId,
			config_pwr_descriptor.bBusPowerSavingD1);

		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
			"\t\tbSelfPowerSavingD1: %d\n\t\t\t"
			"bBusPowerSavingD2: %d\n\t\t\t"
			"bSelfPowerSavingD2: %d\n\t\t\t"
			"bBusPowerSavingD3: %d\n\t\t\t"
			"bSelfPowerSavingD3: %d\n\t\t\t"
			"TransitionTimeFromD1: %d\n\t\t\t"
			"TransitionTimeFromD2: %d\n\t\t\t"
			"TransitionTimeFromD3: %d\n\t\t\t",
			config_pwr_descriptor.bSelfPowerSavingD1,
			config_pwr_descriptor.bBusPowerSavingD2,
			config_pwr_descriptor.bSelfPowerSavingD2,
			config_pwr_descriptor.bBusPowerSavingD3,
			config_pwr_descriptor.bSelfPowerSavingD3,
			config_pwr_descriptor.TransitionTimeFromD1,
			config_pwr_descriptor.TransitionTimeFromD2,
			config_pwr_descriptor.TransitionTimeFromD3);
	}

	for (i = 0; i < config_descriptor.bNumInterfaces; i++) {
		size = usb_parse_interface_descr(usb_config,
				usb_config_length,
				i,		/* interface index */
				0,		/* alt interface index */
				&interface_descriptor,
				USB_IF_DESCR_SIZE);

		if (size != USB_IF_DESCR_SIZE) {
			USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
			    "\tparsing interface: "
			    "size (%d) != USB_IF_DESCR_SIZE (%d)",
			    size, USB_IF_DESCR_SIZE);

			goto done;
		}

		USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
		    "\tinterface descriptor:\n\t\t\t"
		    "bLength: 0x%x\t\t\tbDescriptorType: 0x%x\n\t\t\t"
		    "bInterfaceNumber: 0x%x\t\tbAlternateSetting: 0x%x"
		    "\n\t\t\tbNumEndpoints: 0x%x\t\tbInterfaceClass: 0x%x"
		    "\n\t\t\tbInterfaceSubClass: 0x%x\t\t"
		    "bInterfaceProtocol: 0x%x\n\t\t\tiInterface: 0x%x",
		    interface_descriptor.bLength,
		    interface_descriptor.bDescriptorType,
		    interface_descriptor.bInterfaceNumber,
		    interface_descriptor.bAlternateSetting,
		    interface_descriptor.bNumEndpoints,
		    interface_descriptor.bInterfaceClass,
		    interface_descriptor.bInterfaceSubClass,
		    interface_descriptor.bInterfaceProtocol,
		    interface_descriptor.iInterface);

		size = usb_parse_interface_pwr_descr(usb_config,
			usb_config_length,
			i,		/* interface index */
			0,		/* alt interface index */
			&interface_pwr_descriptor,
			USB_IF_PWR_DESCR_SIZE);

		if (size != USB_IF_PWR_DESCR_SIZE) {
			USB_DPRINTF_L2(DPRINT_MASK_USBA, usba_log_handle,
			    "\tparsing interface power descriptor fails : "
			    "size (%d) != USB_IF_DESCR_SIZE (%d)",
			    size, USB_IF_PWR_DESCR_SIZE);
		} else {
			USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
			"\tInterface Power Descriptor:\n\t\t\t"
			"bLength: %d\n\t\t\t"
			"bDescriptorType: %d\n\t\t\t"
			"bmCapabilitiesFlags: 0x%x\n\t\t\t"
			"bBusPowerSavingD1: %d\n\t\t\t"
			"bSelfPowerSavingD1: %d\n\t\t\t"
			"bBusPowerSavingD2: %d",
			interface_pwr_descriptor.bLength,
			interface_pwr_descriptor.bDescriptorType,
			interface_pwr_descriptor.bmCapabilitiesFlags,
			interface_pwr_descriptor.bBusPowerSavingD1,
			interface_pwr_descriptor.bSelfPowerSavingD1,
			interface_pwr_descriptor.bBusPowerSavingD2);

			USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
			"\t\tbSelfPowerSavingD2: %d\n\t\t\t"
			"bBusPowerSavingD3: %d\n\t\t\t"
			"bSelfPowerSavingD3: %d\n\t\t\t"
			"TransitionTimeFromD1: %d\n\t\t\t"
			"TransitionTimeFromD2: %d\n\t\t\t"
			"TransitionTimeFromD3: %d\n\t\t\t",
			interface_pwr_descriptor.bSelfPowerSavingD2,
			interface_pwr_descriptor.bBusPowerSavingD3,
			interface_pwr_descriptor.bSelfPowerSavingD3,
			interface_pwr_descriptor.TransitionTimeFromD1,
			interface_pwr_descriptor.TransitionTimeFromD2,
			interface_pwr_descriptor.TransitionTimeFromD3);
		}

		for (ep = 0; ep < interface_descriptor.bNumEndpoints; ep++) {

			/* Parse the endpoint descriptor */
			size = usb_parse_endpoint_descr(usb_config,
				usb_config_length,
				i,		/* interface index */
				0,		/* alt interface index */
				ep,		/* ep index */
				&endpoint_descriptor,
				USB_EPT_DESCR_SIZE);

			if (size != USB_EPT_DESCR_SIZE) {
				USB_DPRINTF_L2(DPRINT_MASK_USBA,
				    usba_log_handle,
				    "size != USB_EPT_DESCR_SIZE");

				goto done;
			}

			USB_DPRINTF_L3(DPRINT_MASK_USBA, usba_log_handle,
			    "\tendpoint descriptor:\n\t\t\t"
			    "bLength: 0x%x\t\t\tbDescriptorType: 0x%x\n\t\t"
			    "\tbEndpointAddress 0x%x\t\t"
			    "bmAttributes: 0x%x\n\t\t\t"
			    "wMaxPacketSize: 0x%x\t\tbInterval: 0x%x",
			    endpoint_descriptor.bLength,
			    endpoint_descriptor.bDescriptorType,
			    endpoint_descriptor.bEndpointAddress,
			    endpoint_descriptor.bmAttributes,
			    endpoint_descriptor.wMaxPacketSize,
			    endpoint_descriptor.bInterval);
		}
	}

done:
	usba_show_label = show;
}
#endif

/*
 * USB enumeration statistic functions
 */

/*
 * Increments the hotplug statistics based on flags.
 */
void
usba_update_hotplug_stats(dev_info_t *dip, uint_t flags)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);
	usb_hcdi_t	*hcdi =
			    usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);

	mutex_enter(&hcdi->hcdi_mutex);
	if (flags & USB_TOTAL_HOTPLUG_SUCCESS) {
		hcdi->hcdi_total_hotplug_success++;
		HCDI_HOTPLUG_STATS_DATA(hcdi)->
		    hcdi_hotplug_total_success.value.ui64++;
	}
	if (flags & USB_HOTPLUG_SUCCESS) {
		hcdi->hcdi_hotplug_success++;
		HCDI_HOTPLUG_STATS_DATA(hcdi)->
		    hcdi_hotplug_success.value.ui64++;
	}
	if (flags & USB_TOTAL_HOTPLUG_FAILURE) {
		hcdi->hcdi_total_hotplug_failure++;
		HCDI_HOTPLUG_STATS_DATA(hcdi)->
		    hcdi_hotplug_total_failure.value.ui64++;
	}
	if (flags & USB_HOTPLUG_FAILURE) {
		hcdi->hcdi_hotplug_failure++;
		HCDI_HOTPLUG_STATS_DATA(hcdi)->
		    hcdi_hotplug_failure.value.ui64++;
	}
	mutex_exit(&hcdi->hcdi_mutex);
}

/*
 * Retrieve the current enumeration statistics
 */
void
usba_get_hotplug_stats(dev_info_t *dip, ulong_t *total_success,
    ulong_t *success, ulong_t *total_failure, ulong_t *failure,
    uchar_t *device_count)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);
	usb_hcdi_t	*hcdi =
			    usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);

	mutex_enter(&hcdi->hcdi_mutex);
	*total_success = hcdi->hcdi_total_hotplug_success;
	*success = hcdi->hcdi_hotplug_success;
	*total_failure = hcdi->hcdi_total_hotplug_failure;
	*failure = hcdi->hcdi_hotplug_failure;
	*device_count = hcdi->hcdi_device_count;
	mutex_exit(&hcdi->hcdi_mutex);
}

/*
 * Reset the resetable hotplug stats
 */
void
usba_reset_hotplug_stats(dev_info_t *dip)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);
	usb_hcdi_t	*hcdi =
			    usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);
	hcdi_hotplug_stats_t *hsp;

	mutex_enter(&hcdi->hcdi_mutex);
	hcdi->hcdi_hotplug_success = 0;
	hcdi->hcdi_hotplug_failure = 0;

	hsp = HCDI_HOTPLUG_STATS_DATA(hcdi);
	hsp->hcdi_hotplug_success.value.ui64 = 0;
	hsp->hcdi_hotplug_failure.value.ui64 = 0;
	mutex_exit(&hcdi->hcdi_mutex);
}
