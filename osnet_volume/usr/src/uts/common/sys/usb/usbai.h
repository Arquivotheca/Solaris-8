/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_USBAI_H
#define	_SYS_USB_USBAI_H

#pragma ident	"@(#)usbai.h	1.7	99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * USBAI: Interfaces Between USBA and Client Driver
 */

/*
 * Most USBA functions return success or failure
 */
#define	USB_SUCCESS		0
#define	USB_FAILURE		-1	/* unspecified USBA or HCD error */
#define	USB_NO_RESOURCES	-2	/* no resources available	*/
#define	USB_NO_BANDWIDTH	-3	/* no bandwidth available	*/
#define	USB_PIPE_RESERVED	-4	/* pipe is reserved		*/
#define	USB_PIPE_UNSHAREABLE	-5	/* pipe is unshareable		*/
#define	USB_NOT_SUPPORTED	-6	/* function not supported by HCD */
#define	USB_PIPE_ERROR		-8	/* error occured on the pipe */
#define	USB_PIPE_BUSY		-9	/* a sync call in progress */

/*
 * Universal USB device state management :
 *
 *	ONLINE-----1--->SUSPENDED----2---->ONLINE
 *	  |
 *	  +-----3--->DISCONNECTED----4----->ONLINE
 *	  |
 *	  +-----7--->POWERED DOWN----8----->ONLINE
 *
 *	POWERED DOWN----1--->SUSPENDED------2----->POWERED DOWN
 *	  |		      |     ^
 *	  |		      5     |
 *	  |		      |     6
 *	  |		      v     |
 *	  +---------3----->DISCONNECTED-------4----->POWERED DOWN
 *
 *	1 = CPR SUSPEND
 *	2 = CPR RESUME (with original device)
 *	3 = Device Unplug
 *	4 = Original Device Plugged in
 *	5 = CPR RESUME (with device disconnected or with a wrong device)
 *	6 = CPR SUSPEND on a disconnected device
 *	7 = Device idles for time T & transitions to low power state
 *	8 = Remote wakeup by device OR Application kicking off IO to device
 *
 *	NOTE : device states 0x80 to 0xff are device specific
 */
#define	USB_DEV_ONLINE		1	/* device is online */
#define	USB_DEV_DISCONNECTED	2	/* Indicate disconnect */
#define	USB_DEV_CPR_SUSPEND	3	/* for DDI_SUSPEND operation */
#define	USB_DEV_POWERED_DOWN	4	/* indicates power off state */


/*
 * As any usb device will have a max of 4 possible power states
 * the #define for them are provided below with mapping to the
 * corresponding OS power levels
 */
#define	USB_DEV_POWER_D0	USB_DEV_ONLINE
#define	USB_DEV_POWER_D1	5
#define	USB_DEV_POWER_D2	6
#define	USB_DEV_POWER_D3	USB_DEV_POWERED_DOWN

#define	USB_DEV_OS_POWER_0	0
#define	USB_DEV_OS_POWER_1	1
#define	USB_DEV_OS_POWER_2	2
#define	USB_DEV_OS_POWER_3	3
#define	USB_DEV_OS_POWER_OFF	USB_DEV_OS_POWER_0
#define	USB_DEV_OS_FULL_POWER	USB_DEV_OS_POWER_3

/* Bit Masks for Power States */
#define	USB_DEV_PWRMASK_D0	1
#define	USB_DEV_PWRMASK_D1	2
#define	USB_DEV_PWRMASK_D2	4
#define	USB_DEV_PWRMASK_D3	8

/* conversion for OS to Dx levels */
#define	USB_DEV_OS_PWR2USB_PWR(l)	(USB_DEV_OS_FULL_POWER - (l))

/* from OS level to Dx mask */
#define	USB_DEV_PWRMASK(l)	(1 << (USB_DEV_OS_FULL_POWER - (l)))

/* Macro to check valid power level */
#define	USB_DEV_PWRSTATE_OK(state, level) \
		(((state) & USB_DEV_PWRMASK((level))) == 0)

/*
 * USB spec defines 4 different power states of any usb device
 * They are D0, D1, D2 & D3. So, we need a total of 5 pm-components
 * 4 for power and 1 for name
 */
#define	USB_PMCOMP_NO		5


/*
 * USB Descriptor Management
 *
 * Standard USB descriptors:
 *
 * USB devices present their configuration information in response to
 * a GET_DESCRIPTOR request in a form which is little-endian and,
 * for multibyte integers, unaligned.  It is also position-dependent,
 * which makes non-sequential access to particular interface or
 * endpoint data inconvenient.
 * A GET_DESCRIPTOR request may yield a chunk of data that contains
 * multiple descriptor types.  For example, a GET_DESCRIPTOR request for a
 * CONFIGURATION descriptor could return the configuration descriptor
 * followed by an interface descriptor and the relevant endpoint
 * descriptors.
 * The intent of the USBA descriptor parsing functions below is to
 * make it easier for the client driver to sift through this data.
 *
 * First the descriptor structures:
 *
 * usb_device_descr:
 *	usb device descriptor, refer to	USB 1.0/9.6.1,
 */
typedef struct usb_device_descr {
	uint8_t		bLength;	/* Size of this descriptor	*/
	uint8_t		bDescriptorType; /* DEVICE descriptor Type	*/
	uint16_t	bcdUSB;		/* USB spec rel. number	in bcd	*/
	uint8_t		bDeviceClass;	/* class code			*/
	uint8_t		bDeviceSubClass; /* sub	class code		*/
	uint8_t		bDeviceProtocol; /* protocol code		*/
	uint8_t		bMaxPacketSize0; /* max	pkt size of e/p	0	*/
	uint16_t	idVendor;	/* vendor ID			*/
	uint16_t	idProduct;	/* product ID			*/
	uint16_t	bcdDevice;	/* device release number in bcd	*/
	uint8_t		iManufacturer;	/* Mfg.	string			*/
	uint8_t		iProduct;	/* Prod. string			*/
	uint8_t		iSerialNumber;	/* serial num. string index	*/
	uint8_t		bNumConfigurations; /* # possible configs	*/
} usb_device_descr_t;


/*
 * usb_config_descr:
 *	usb configuration descriptor, refer to USB 1.0/9.6.2
 */
typedef struct usb_config_descr {
	uint8_t		bLength;	/* Size of this descriptor	*/
	uint8_t		bDescriptorType; /* CONFIGURATION		*/
	uint16_t	wTotalLength;	/* total length of data returned */
	uint8_t		bNumInterfaces;	/* # interfaces	in config	*/
	uint8_t		bConfigurationValue; /* arg for SetConfiguration */
	uint8_t		iConfiguration;	/* configuration string		*/
	uint8_t		bmAttributes;	/* config characteristics	*/
	uint8_t		MaxPower;	/* max pwr consumption		*/
} usb_config_descr_t;

/*
 * bmAttribute values for Configuration Descriptor
 */
#define	USB_CONF_ATTR_SELFPWR		0x40
#define	USB_CONF_ATTR_REMOTE_WAKEUP	0x20


/*
 * usb_interface_descr:
 *	usb interface descriptor, refer	to USB 1.0/9.6.3
 */
typedef  struct usb_interface_descr {
	uint8_t		bLength;		/* Size of descriptor	*/
	uint8_t		bDescriptorType;	/* INTERFACE		*/
	uint8_t		bInterfaceNumber;	/* interface number	*/
	uint8_t		bAlternateSetting;	/* alt. setting selection */
	uint8_t		bNumEndpoints;		/* # endpoints		*/
	uint8_t		bInterfaceClass;	/* class code		*/
	uint8_t		bInterfaceSubClass;	/* sub class code	*/
	uint8_t		bInterfaceProtocol;	/* protocol code	*/
	uint8_t		iInterface;		/* description string	*/
} usb_interface_descr_t;

/*
 * usb_endpoint_descr:
 *	usb endpoint descriptor, refer to USB 1.0/9.6.4
 */
typedef struct usb_endpoint_descr {
	uint8_t		bLength;		/* Size of descriptor	*/
	uint8_t		bDescriptorType;	/* ENDPOINT		*/
	uint8_t		bEndpointAddress;	/* the addr. of this e/p */
	uint8_t		bmAttributes;		/* e/p attributes	*/
	uint16_t	wMaxPacketSize;		/* maximum packet size	*/
	uint8_t		bInterval;		/* interval for	polling	e/p */
} usb_endpoint_descr_t;

_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_endpoint_descr))

/*
 * The compiler pads the above structures, so the following macros should
 * be used for the size of the structures.
 */
#define	USB_DEVICE_DESCR_SIZE	18	/* size of the device descriptor */
#define	USB_CONF_DESCR_SIZE	 9	/* size of the configuration desc. */
#define	USB_CONF_PWR_DESCR_SIZE 18	/* size of configuration pwr desc. */
#define	USB_IF_DESCR_SIZE	 9	/* size of the interface descriptor */
#define	USB_IF_PWR_DESCR_SIZE	15 	/* size of interface pwr descriptor */
#define	USB_EPT_DESCR_SIZE	 7	/* size of the endpoint descriptor */

/*
 * bEndpointAddress masks
 */
#define	USB_EPT_ADDR_MASK	0x0F	/* mask the addr part */
#define	USB_EPT_DIR_MASK	0x80	/* mask the direction part */
#define	USB_EPT_DIR_OUT		0x00	/* OUT endpoint */
#define	USB_EPT_DIR_IN		0x80	/* IN endpoint */

/*
 * bmAttribute values for Endpoint
 */
#define	USB_EPT_ATTR_CONTROL	0x00
#define	USB_EPT_ATTR_ISOCH	0x01
#define	USB_EPT_ATTR_BULK	0x02
#define	USB_EPT_ATTR_INTR	0x03
#define	USB_EPT_ATTR_MASK	0x03

/*
 * usb_string_descr:
 *	usb string descriptor, refer to	 USB 1.0/9.6.5
 */
typedef struct usb_string_descr {
	uint8_t		bLength;		/* size	of this	descriptor */
	uint8_t		bDescriptorType;	/* descriptor type	*/
	uint8_t		bString[1];		/* variable length unicode */
						/* encoded string	*/
} usb_string_descr_t;

#define	USB_MAXSTRINGLEN	255		/* max string descr length */

/*
 * Configuration Power Descriptor
 *	This reports the power consuption of the device core
 *	for all types of USB devices.
 */
typedef struct usb_config_pwr_descr {
	uint8_t		bLength;	/* size of this descriptor 0x12 */
	uint8_t		bDescriptorType;	/* config pwr descr 0x07 */
	uint16_t	SelfPowerConsumedD0_l;	/* power consumed lower word */
	uint8_t		SelfPowerConsumedD0_h;	/* power consumed upper byte */
	uint8_t		bPowerSummaryId;	/* ID for own power devices */
	uint8_t		bBusPowerSavingD1;	/* power saving in D1 */
	uint8_t		bSelfPowerSavingD1;	/* power saving in D1 */
	uint8_t		bBusPowerSavingD2;	/* power saving in D2 */
	uint8_t		bSelfPowerSavingD2;	/* power saving in D2 */
	uint8_t		bBusPowerSavingD3;	/* power saving in D3 */
	uint8_t		bSelfPowerSavingD3;	/* power saving in D3 */
	uint16_t	TransitionTimeFromD1;	/* D1 -> D0 transition time */
	uint16_t	TransitionTimeFromD2;	/* D2 -> D0 transition time */
	uint16_t	TransitionTimeFromD3;	/* D3 -> D0 transition time */
} usb_config_pwr_descr_t;

/*
 * Interface Power Descriptor
 *	This reports the power states implemented by the interface
 *	and its wake-up capabilities.
 */
typedef struct usb_interface_pwr_descr {
	uint8_t		bLength;	/* size of this descriptor 0x0F */
	uint8_t		bDescriptorType;	/* i/f pwr descr 0x08 */
	uint8_t		bmCapabilitiesFlags;	/* wakeup & pwr transition */
	uint8_t		bBusPowerSavingD1;	/* power saving in D1 */
	uint8_t		bSelfPowerSavingD1;	/* power saving in D1 */
	uint8_t		bBusPowerSavingD2;	/* power saving in D2 */
	uint8_t		bSelfPowerSavingD2;	/* power saving in D2 */
	uint8_t		bBusPowerSavingD3;	/* power saving in D3 */
	uint8_t		bSelfPowerSavingD3;	/* power saving in D3 */
	uint16_t	TransitionTimeFromD1;	/* D1 -> D0 transition time */
	uint16_t	TransitionTimeFromD2;	/* D2 -> D0 transition time */
	uint16_t	TransitionTimeFromD3;	/* D3 -> D0 transition time */
} usb_interface_pwr_descr_t;

/*
 * functions to return a pre-processed device descriptor to the client driver.
 * These all extract data from a buffer full of data returned by a
 * GET_DESCRIPTOR request.
 * The pre-processed descriptor is returned into a buffer supplied by
 * the caller
 * The size of the buffer should allow for padding
 *
 * In the following:
 *	buf		buffer containing data returned by GET_DESCRIPTOR
 *	buflen		length of the data at buf
 *	ret_descr	buffer the data is to be returned in
 *	ret_buf_len	size of the buffer at ret_descr
 *
 *	interface_number the index in the array of concurrent interfaces
 *			supported by this configuration
 *	alt_interface_setting alternate settingfor the interface identified
 *			by interface_number
 *	endpoint_index	the index in the array of endpoints supported by
 *			this configuration
 *
 * These functions return the length of the returned descriptor structure,
 * or USB_PARSE_ERROR on error.
 *
 * No error is returned if ret_buf_len is too small but
 * the data is truncated
 * This allows successful parsing of descriptors that have been
 * extended in a later rev of the spec.
 */
#define	USB_PARSE_ERROR		0

size_t usb_parse_device_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(DEVICE) */
	size_t			buflen,
	usb_device_descr_t	*ret_descr,
	size_t			ret_buf_len);

size_t usb_parse_configuration_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	usb_config_descr_t	*ret_descr,
	size_t			ret_buf_len);

size_t usb_parse_config_pwr_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	usb_config_pwr_descr_t	*ret_descr,
	size_t			ret_buf_len);

size_t usb_parse_interface_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	usb_interface_descr_t	*ret_descr,
	size_t			ret_buf_len);

size_t usb_parse_interface_pwr_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	usb_interface_pwr_descr_t	*ret_descr,
	size_t			ret_buf_len);

/*
 * the endpoint index is relative to the interface. index 0 is
 * the first endpoint
 */
size_t usb_parse_endpoint_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	uint_t			endpoint_index,
	usb_endpoint_descr_t	*ret_descr,
	size_t			ret_buf_len);

/*
 * Returns (at ret_descr) a null-terminated string.  Null termination is
 * guaranteed, even if the string is longer than the buffer.  Thus, a
 * maximum of (ret_buf_len - 1) characters are returned.
 */
size_t usb_ascii_string_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(STRING) */
	size_t			buflen,
	char			*ret_descr,
	size_t			ret_buf_len);

/*
 * functions to handle arbitrary descriptors. USBA doesn't know the format
 * and therefore cannot do any automatic pre-processing.
 *
 * In the following:
 *	buf		buffer containing data returned by GET_DESCRIPTOR
 *	buflen		length of the data at buf allowing for padding
 *	fmt		a null terminated string describing the format of
 *			the data structure for general-purpose byte swapping,
 *			use NULL for raw access.
 *			The letters "c", "s", "l", and "L"
 *			represent 1, 2, 4, and 8 byte quantities,
 *			respectively.  A descriptor that consists of a
 *			short and two bytes would be described by "scc\0".
 *	descr_type	type of the desired descriptor, USB_DESCR_TYPE_ANY
 *			to get any type.
 *	descr_index	index of the desired descriptor
 *	ret_descr	buffer the data is to be returned in
 *	ret_buf_len	size of the buffer at ret_descr
 *
 * Specifying descr_index=0 returns the first descriptor of the specified
 * type, specifying descr_index=1 returns the second, and so on.
 *
 * No error is returned if ret_buf_len is too small. This allows successful
 * parsing of descriptors that have been extended in a later rev of the spec.
 */
size_t usb_parse_CV_configuration_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	char			*fmt,
	uint_t			descr_type,
	uint_t			descr_index,
	void			*ret_descr,
	size_t			ret_buf_len);

size_t usb_parse_CV_interface_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	char			*fmt,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	uint_t			descr_type,
	uint_t			descr_index,
	void			*ret_descr,
	size_t			ret_buf_len);

size_t usb_parse_CV_endpoint_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	char			*fmt,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	uint_t			endpoint_index,
	uint_t			descr_type,
	uint_t			descr_index,
	void			*ret_descr,
	size_t			ret_buf_len);

/*
 * for unpacking any kind of LE data
 */
size_t usb_parse_CV_descr(
	char			*format,
	uchar_t			*data,
	size_t			datalen,
	void			*structure,
	size_t			structlen);


/*
 * In the setup packet, the descriptor type is passed in the high byte of the
 * wValue field.
 * descriptor types:
 */
#define	USB_DESCR_TYPE_ANY			-1	/* Wild card */
#define	USB_DESCR_TYPE_SETUP_DEVICE		0x0100
#define	USB_DESCR_TYPE_SETUP_CONFIGURATION	0x0200
#define	USB_DESCR_TYPE_SETUP_STRING		0x0300
#define	USB_DESCR_TYPE_SETUP_INTERFACE		0x0400
#define	USB_DESCR_TYPE_SETUP_ENDPOINT		0x0500
#define	USB_DESCR_TYPE_SETUP_CONFPOWER		0x0700
#define	USB_DESCR_TYPE_SETUP_IFPOWER		0x0800

#define	USB_DESCR_TYPE_DEVICE			0x01
#define	USB_DESCR_TYPE_CONFIGURATION		0x02
#define	USB_DESCR_TYPE_STRING			0x03
#define	USB_DESCR_TYPE_INTERFACE		0x04
#define	USB_DESCR_TYPE_ENDPOINT			0x05
#define	USB_DESCR_TYPE_CONFIGURATION_POWER	0x07
#define	USB_DESCR_TYPE_INTERFACE_POWER		0x08


/*
 * usb_get_addr returns the current usb address, mostly for
 * debugging purposes. The address may change at any time.
 */
int usb_get_addr(dev_info_t *dip);

/*
 * usb_get_interface_number returns -1 if the driver is responsible for
 * the entire device. Otherwise it returns the interface number
 */
#define	USB_COMBINED_NODE	-1

int usb_get_interface_number(dev_info_t *dip);

/*
 * utility functions to get device and config descriptor
 */
usb_device_descr_t *usb_get_dev_descr(dev_info_t *dip);
uchar_t *usb_get_raw_config_data(dev_info_t *dip, size_t *length);

/*
 * utility function to get a string descriptor
 */
int usb_get_string_descriptor(dev_info_t *dip, uint16_t langid, uint8_t index,
    char *buf, size_t buflen);

/*
 * utility function to check if the new device is same as the old one
 */
int usb_check_same_device(dev_info_t *dip);
char *usb_get_usbdev_strdescr(dev_info_t *dip);

/*
 * Pipe	Management:
 *
 * usb_pipe_state:
 *
 * the pipe states are mutually exclusive
 *
 * PIPE_STATE_ACTIVE:
 *	The pipe's policy has been set, and the pipe is able to tramsit data.
 *	When a control or bulk pipe is opened, the pipe's state is
 * 	automatically set to PIPE_STATE_ACTIVE.  For an interrupt or
 *	isochronous pipe, the pipe state becomes PIPE_STATE_ACTIVE once
 *	usb_pipe_start_polling() is called on the pipe.
 *
 * PIPE_STATE_IDLE:
 *	The pipe's policy is set, but the pipe currently isn't transmitting
 *	data.  PIPE_STATE_IDLE, only applies to interrupt and isochronous pipes.
 *	When an interrupt or isochronous pipe is openend, polling doesn't
 *  	start until usb_pipe_start_polling() is explicity called.  If polling
 *	is happening on the pipe, and usb_pipe_stop_polling() is called, then
 *	the pipe state goes to PIPE_STATE_IDLE.
 *
 * PIPE_STATE_ERROR:
 *	The device has generated a error on the pipe.  The client driver
 * 	must call usb_pipe_reset() to clear any leftover state that's associated
 * 	with the pipe, clear the data toggle, and reset the state of the pipe.
 *
 *	Calling usb_pipe_reset() on a control or bulk pipe resets the state to
 *	PIPE_STATE_ACTIVE.  Calling usb_pipe_reset() on an interrupt or
 *	isochronous pipe, resets the state to PIPE_STATE_IDLE.
 *
 * State Diagram for Bulk/Control
 *
 * usb_pipe_open-->[PIPE_STATE_ACTIVE]--device error-->[PIPE_STATE_ERROR]
 *				 ^				    |
 *				 |--<----<-usb_pipe_reset---<------<-
 *
 * State Diagram for Interrupt/Isochronous
 *
 *			--<-----<------usb_pipe_stop_polling----<-------^
 *			|						|
 *			V						|
 * usb_pipe_open-->[PIPE_STATE_IDLE]-usb_pipe_start_polling->[PIPE_STATE_ACTIVE]
 *			^						  |
 *			|						  v
 *		        - usb_pipe_reset<-[PIPE_STATE_ERROR]<-device error
 *
 */
typedef enum {
	USB_PIPE_STATE_ACTIVE = 1,
	USB_PIPE_STATE_IDLE = 2,
	USB_PIPE_STATE_ERROR = 3,
	USB_PIPE_STATE_SYNC_CLOSING = 4,
	USB_PIPE_STATE_ASYNC_CLOSING = 5
} usb_pipe_state_t;

/*
 * opaque pipe handle:
 *	this pipe handle is returned on successfully opening a pipe
 *	and passed to all other usb_pipe_*() functions
 *
 *	the pipe_handle is opaque to the client driver.
 */
typedef	struct usb_pipe_handle	*usb_pipe_handle_t;

/*
 * Most USBA functions have a usb_flags argument
 */
#define	USB_FLAGS_SLEEP		0x0001 /* block until resources are avail. */
#define	USB_FLAGS_ENQUEUE	0x0002 /* enqueue even if no res. are avail */
#define	USB_FLAGS_OPEN_EXCL	0x0004 /* exclusive pipe open */
#define	USB_FLAGS_SHORT_XFER_OK	0x0008 /* short transfers are permitted */

/*
 * usb_pipe_policy
 *
 *	pipe policy specifies how a pipe to an endpoint	should be used
 *	by the client driver and the HCD
 */
#define	USB_PIPE_POLICY_V_0	0

typedef struct usb_pipe_policy {
	uchar_t		pp_version;

	/*
	 * save the timeout value. This value will be used by HCD as the
	 * timeout value per transfer and it must be  specified in terms
	 * of seconds.
	 */
	int		pp_timeout_value;

	/*
	 * Maximum transfer size for a periodic pipe
	 *
	 * In many cases, the maximum transfer size for a periodic pipe
	 * equals the wMaxPacketSize field of the Endpoint descriptor.
	 * However, it is possible for the periodic transfer to span
	 * multiple USB transactions.  For example, a HID device may
	 * have 10 byte reports that are returned over the HID
	 * interrupt pipe.  (See HID 1.0 Draft #4, section 8.4)  If the
	 * device is a low speed device, the wMaxPacketSize will be 8, and
	 * the transfer of the 10 byte report will require two USB
	 * transactions.  In this case, pp_periodic_max_transfer_size
	 * must equal 10.
	 *
	 * A periodic pipe may support different transfer types with varying
	 * sizes.  For example, 3 byte mouse data as well as 8 byte keyboard
	 * data may be returned on the same HID interrupt pipe.  In the
	 * case of multiple transfer sizes, pp_max_transfer_size must
	 * equal the maximum transfer size.  In the HID example,
	 * pp_periodic_max_transfer_size would equal 8.  In addition, the
	 * USB_FLAGS_SHORT_XFER_OK flag must be used in the usb flags so that
	 * an error is not returned when mouse data arrives.
	 *
	 * If pp_max_transfer_size is not set, then the wMaxPacketSize of the
	 * endpoint descriptor is used as the default.
	 */
	uint_t		pp_periodic_max_transfer_size;

	/*
	 * Notification:
	 * one callback function for sending and receiving.
	 *
	 * conditions for a callback are:
	 *	- requested data transferred (bulk/control)
	 *	- every frame in which a data xfer occurred (isoc/intr)
	 *	- an exception/error (all types)
	 *
	 * callback arguments are:
	 *	- the pipe_handle
	 *	- callback_arg specified in this pipe policy
	 *	- mblk ptr (NULL for sending callbacks)
	 */
	usb_opaque_t		pp_callback_arg;

	int		(*pp_callback)(
				usb_pipe_handle_t	pipe,
				usb_opaque_t		callback_arg,
				mblk_t			*data);

	/*
	 * any exception or partial transfer will invoke this callback
	 * instead of the above. The ordering of callbacks may not
	 * preserved.
	 */
	int		(*pp_exception_callback)(
				usb_pipe_handle_t	pipe,
				usb_opaque_t		callback_arg,
				uint_t			completion_reason,
				mblk_t			*data,
				uint_t			flag);
} usb_pipe_policy_t;


/*
 * Default value for pp_timeout_value (timeout of 3 secs)
 * This is the value for which the HCD would try for a request
 * before timing out the request. Timing out of the request
 * would result in a completion reason of USB_CC_TIMEOUT.
 */
#define	USB_PIPE_TIMEOUT	3

/*
 * transport completion codes
 *	<refer to openHCI, 1.0, 4.3.3>
 * upper 16 bits are reserved for our implementation
 */
#define	USB_CC_NOERROR		0x0000	/* no errors detected		*/
#define	USB_CC_CRC		0x0001	/* crc error detected in last irp */
#define	USB_CC_BITSTUFFING	0x0002	/* bit stuffing violation	*/
#define	USB_CC_DATA_TOGGLE_MM	0x0003	/* d/t PID did not match	*/
#define	USB_CC_STALL		0x0004	/* e/p returned stall PID	*/
#define	USB_CC_DEV_NOT_RESP	0x0005	/* device not responding	*/
#define	USB_CC_PID_CHECKFAILURE 0x0006	/* check bits on PID failed	*/
#define	USB_CC_UNEXP_PID	0x0007	/* receive PID was not valid	*/
#define	USB_CC_DATA_OVERRUN	0x0008	/* data size exceeded		*/
#define	USB_CC_DATA_UNDERRUN	0x0009	/* less data rcv'ed than requested */
#define	USB_CC_BUFFER_OVERRUN	0x000a	/* memory write can't keep up	*/
#define	USB_CC_BUFFER_UNDERRUN	0x000b	/* buffer underrun */
#define	USB_CC_TIMEOUT		0x000c	/* command timed out */
#define	USB_CC_UNSPECIFIED_ERR	0xff00	/* unspecified usba or hcd error */

/*
 * usb_pipe_open():
 *
 * Before using any pipe including the default pipe, it should be opened
 * using usb_pipe_open(). On a successful open, a pipe handle is returned
 * for use in other usb_pipe_*() functions
 *
 * Arguments:
 *
 *	- devinfo ptr
 *
 *	- endpoint descriptor pointer (use NULL for default endpoint)
 *
 *	- a pipe policy which provides detailed information on how
 *	  the pipe will be used.
 *	  the pipe policy can be NULL in which case a default USBA
 *	  pipe policy will be used
 *
 *	- flags:
 *		USB_FLAGS_SLEEP	wait for resources, bandwidth or pipe
 *				to become available
 *				(interruptable sleep)
 *		USB_FLAGS_OPEN_EXCL
 *				excl. open the pipe and reserve immediately
 *
 *	- a pipe handle pointer. On a successful open, a pipehandle
 *	  is returned in this pointer.
 *	  implementation note: the pipe may be shared but the
 *	  pipehandle is always unique, ie. each pipe open returns a
 *	  unique handle
 *
 *	The bandwidth has been allocated and guaranteed on successful
 *	opening of an isoc/intr pipes.
 *
 * return values:
 *	USB_SUCCESS	 - open succeeded
 *	USB_FAILURE	 - unspecified open failure or pipe cannot
 *			   be opened exclusively
 *	USB_NO_RESOURCES - no resources were available to complete the
 *			   open
 *	USB_NO_BANDWIDTH - no bandwidth available (isoc/intr pipes)
 *	USB_PIPE_UNSHAREABLE - pipe is not shareable
 *	USB_PIPE_RESERVED - pipe was reserved by another client
 */
int	usb_pipe_open(
		dev_info_t		*dip,
		usb_endpoint_descr_t	*endpoint,
		usb_pipe_policy_t	*pipe_policy,
		uint_t			usb_flags,
		usb_pipe_handle_t	*pipe_handle);

/*
 * closing pipe:
 *
 * Close a pipe and release all resources and free the pipe_handle
 *
 * A pipe reserved by another client cannot be closed
 *
 * A pipe reservation by this driver instance will be released.
 *
 * Automatic polling, if active,  will be terminated
 *
 * Closing a pipe may block until the pipe is completely IDLE and
 * guarantees no more callbacks
 *
 * return values:
 *	USB_SUCCESS	 - close succeeded
 *	USB_FAILURE	 - unspecified close failure
 *	USB_PIPE_RESERVED - pipe was reserved by another client
 */
int	usb_pipe_close(
		usb_pipe_handle_t	*pipe_handle,
		uint_t			usb_flags,
		void		(*callback)(usb_opaque_t, int, uint_t),
		usb_opaque_t	callback_arg);

/*
 * the client driver can store a private data pointer in the
 * pipe_handle
 * it might be convenient for the client driver to be able to store
 * per-pipe data in the pipe handle
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 */
int	usb_pipe_set_private(
		usb_pipe_handle_t	pipe_handle,
		usb_opaque_t		data);

usb_opaque_t usb_pipe_get_private(
		usb_pipe_handle_t	pipe_handle);


/*
 * Get and set the pipe policy:
 * The same version should be returned as specified at open time
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 */
int	usb_pipe_get_policy(
		usb_pipe_handle_t	pipe_handle,
		usb_pipe_policy_t	*pipe_policy);

int	usb_pipe_set_policy(
		usb_pipe_handle_t	pipe_handle,
		usb_pipe_policy_t	*pipe_policy,
		uint_t			usb_flags);

/*
 * Pipe Reservation:
 *
 * Refer to USB 1.0/10.3.4:
 *	"The USBA must provide a mechanism to designate a group of
 *	uninterruptable set of vendor or class specific requests
 *	to any pipe including default pipe".
 *	The reservation is enforced by USBA
 *
 * Reserving an already reserved pipe by this instance succeeds quietly
 *
 * If the pipe is already reserved by another client, and this client
 * did not specify USB_FLAGS_SLEEP, a resource callback will be
 * made when the pipe is released
 *
 * flags:
 *	USB_FLAGS_SLEEP	- wait for a pipe release
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was already reserved by another instance
 */
int	usb_pipe_reserve(
		usb_pipe_handle_t	pipe_handle,
		uint_t			usb_flags);

int	usb_pipe_release(
		usb_pipe_handle_t	pipe_handle);

/*
 * pipe state control:
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 */
int	usb_pipe_get_state(
		usb_pipe_handle_t	pipe_handle,
		usb_pipe_state_t	*pipe_state,
		uint_t			usb_flags);

/*
 * Refer to USB 1.0/10.5.2.2
 *	Aborting a pipe: All requests scheduled for a pipe are retired
 *		asap and returned to the client with a status
 *		indicating they have been aborted. The client is notified
 *		using the exception callback. Neither the host state nor
 *		the reflected endpoint state of the pipe is affected
 *
 *		The callback function is called when the abort completed
 *		and no more exception callbacks will be made
 *
 *		If the callback is NULL, this call will not block
 *		and the abort occurs asynchronously
 *
 * flags:
 *	USB_FLAGS_SLEEP	- wait for the abort to complete
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 */
int	usb_pipe_abort(
		usb_pipe_handle_t	pipe_handle,
		uint_t			usb_flags,
		void			(*callback)(usb_opaque_t,
					    int, uint_t),
		usb_opaque_t		callback_arg);

/*
 *	Resetting a pipe: The pipe's requests are aborted. The host state
 *		is moved to active. If the reflected endpoint state needs
 *		to be changed, that must explicitly requested by the client
 *		driver
 *
 *		The callback function is called when the reset completed
 *		and no more exception callbacks will be made
 *
 *		If the callback is NULL, this call will not block
 *		and the abort occurs asynchronously
 *
 * flags:
 *	USB_FLAGS_SLEEP	- wait for the reset to complete
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 *
 * callback and callback_arg should be NULL if USB_FLAGS_SLEEP has
 * been specified
 */
int	usb_pipe_reset(
		usb_pipe_handle_t	pipe_handle,
		uint_t			usb_flags,
		void			(*callback)(usb_opaque_t,
					    int, uint_t),
		usb_opaque_t		callback_arg);

/*
 * Configuration and Device Control Management:
 *
 * Asynchronous device requests on control pipes including default pipe
 *
 * Uninterrupted access for multiple sequential requests should be
 * ensured using usb_pipe_reserve()
 *
 * Note that all setup packets are always 8 bytes (USB 1.0/5.5.3)
 *
 * flags:
 *	USB_FLAGS_ENQUEUE	- if no resources are available
 *				  or pipe is busy enqueue the request anyway
 *	USB_SHORT_XFER_OK	- returning less data than requested is OK
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 *	USB_NO_RESOURCES  - no resources
 */
int	usb_pipe_device_ctrl_receive(
	usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	uint16_t	wValue,		/* varies according to request	*/
	uint16_t	wIndex,		/* index or offset		*/
	uint16_t	wLength,	/* number of bytes to xfer	*/
	uint_t		usb_flags);

int	usb_pipe_device_ctrl_send(
	usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	uint16_t	wValue,		/* varies according to request	*/
	uint16_t	wIndex,		/* index or offset		*/
	uint16_t	wLength,	/* number of bytes to xfer	*/
	mblk_t		*data,		/* the data for the data phase	*/
					/* also includes length		*/
	uint_t		usb_flags);

/*
 * synchronous device control function:
 * the caller will block until completion and no callbacks will be
 * made
 *
 * flags:
 *	USB_FLAGS_ENQUEUE	- if no resources are available
 *				  or pipe is busy, enqueue the request
 *				  anyway
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 *	USB_NO_RESOURCES  - no resources
 *
 */
int	usb_pipe_sync_device_ctrl_receive(
	usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	uint16_t	wValue,		/* varies according to request	*/
	uint16_t	wIndex,		/* index or offset		*/
	uint16_t	wLength,	/* number of bytes to xfer	*/
	mblk_t		**data,		/* the data for the data phase	*/
					/* allocated by the HCD		*/
					/* deallocated by consumer	*/
	uint_t		*completion_reason,
	uint_t		usb_flags);

int	usb_pipe_sync_device_ctrl_send(
	usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	uint16_t	wValue,		/* varies according to request	*/
	uint16_t	wIndex,		/* index or offset		*/
	uint16_t	wLength,	/* number of bytes to xfer	*/
	mblk_t		*data,		/* the data for the data phase	*/
					/* allocated by the client driver */
					/* deallocated by the HCD	*/
	uint_t		*completion_reason,
	uint_t		usb_flags);

/* return endpoint no. for a given pipe handle */
ushort_t usb_endpoint_num(usb_pipe_handle_t pipe_handle);

/*
 * device request type
 */
#define	USB_DEV_REQ_HOST_TO_DEV			0x00
#define	USB_DEV_REQ_DEVICE_TO_HOST		0x80
#define	USB_DEV_REQ_TYPE_STANDARD		0x00
#define	USB_DEV_REQ_TYPE_CLASS			0x20
#define	USB_DEV_REQ_TYPE_VENDOR			0x40

#define	USB_DEV_REQ_RECIPIENT_DEVICE		0x00
#define	USB_DEV_REQ_RECIPIENT_INTERFACE		0x01
#define	USB_DEV_REQ_RECIPIENT_ENDPOINT		0x02
#define	USB_DEV_REQ_RECIPIENT_OTHER		0x03

/*
 * device request
 */
#define	USB_REQ_GET_STATUS			0x00
#define	USB_REQ_CLEAR_FEATURE			0x01
#define	USB_REQ_SET_FEATURE			0x03
#define	USB_REQ_SET_ADDRESS			0x05
#define	USB_REQ_GET_DESCRIPTOR			0x06
#define	USB_REQ_SET_DESCRIPTOR			0x07
#define	USB_REQ_GET_CONFIGURATION		0x08
#define	USB_REQ_SET_CONFIGURATION		0x09
#define	USB_REQ_GET_INTERFACE			0x0a
#define	USB_REQ_SET_INTERFACE			0x0b
#define	USB_REQ_SYNC_FRAME			0x0c

/* language ID for string descriptors */
#define	USB_LANG_ID 				0x0409

/*
 * Standard Feature Selectors
 */
#define	USB_ENDPOINT_HALT		0x0000
#define	USB_DEVICE_REMOTE_WAKEUP	0x0001
#define	USB_INTERFACE_POWER_D0		0x0002
#define	USB_INTERFACE_POWER_D1		0x0003
#define	USB_INTERFACE_POWER_D2		0x0004
#define	USB_INTERFACE_POWER_D3		0x0005

/*
 * Bit status mask for data returned by GetStatus(device)
 */
#define	USB_REMOTE_WAKEUP	2		/* remote wakeup bit */
#define	USB_SELF_POWER		1		/* self power bit */

/*
 * Data Transfer Management
 *
 * Isoc data-in/intr pipes:
 *	Data transfer is automatic and kicked off by usb_pipe_start_polling()
 *	The client driver is notified of incoming data using the pipe policy
 *	pp_callback
 *	The mblk needs to be deallocated either by the client or upstream
 *
 * Isoc Data-out pipes:
 *	Data transfer is request by the client driver
 *	The client driver is notified of data xfer completion using the
 *	pipe policy pp_callback
 *	The HCD will deallocate the mblk
 *
 * Bulk pipes:
 *	Data transfer is requested by the client driver
 *	The client driver is notified of data xfer completion using the
 *	pipe policy pp_callback
 *
 * The client driver is notified of any errors or exceptions thru the
 * pipe policy pp_exception_callback
 *
 * All data transfer functions are asynchronous.
 *
 * Uninterrupted access for multiple sequential requests should be
 * ensured using usb_pipe_reserve()
 *
 * If the call cannot complete because of resource problems
 * (and USB_FLAGS_ENQUEUE has not been set in usb_flags) then
 * USB_NO_RESOURCES is returned. If the pipe policy supplies
 * a resource callback function, then the client driver will be
 * notified when resources might be available
 */

/*
 * To start off the periodic and automatic data-in transfers for
 * isoc and intr pipes, the pipe should be started explicitly
 * These functions are not applicable to other types of pipes
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 *	USB_NO_RESOURCES  - no resources
 */
int	usb_pipe_start_polling(
		usb_pipe_handle_t	pipe_handle,
		uint_t			usb_flags);

/*
 * to temporarily stop the automatic data-in transfers without
 * closing the pipe (and giving up the allocated bandwidth), the polling can
 * be stopped
 *
 * the callback is called when all polling requests have been terminated
 * and no more completion callbacks will be made
 *
 * the polling must be stopped with the pipe handle that requested the
 * polling
 *
 * A NULL callback indicates that the client wants to wait for completion

 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 */
int	usb_pipe_stop_polling(
		usb_pipe_handle_t	pipe_handle,
		uint_t			usb_flags,
		void			(*callback)(usb_opaque_t, int, uint_t),
		usb_opaque_t		callback_arg);

/*
 * Isochronous, asynchronous, send data xfers
 * (receive data xfers are automatic)
 *
 * flags:
 *	USB_FLAGS_ENQUEUE - enqueue even if no resources are available
 *	USB_SHORT_XFER_OK - short data-in transfers are permitted
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 *	USB_NO_RESOURCES  - no resources
 */
int	usb_pipe_send_isoc_data(
		usb_pipe_handle_t	pipe_handle,
		mblk_t			*data,
		uint_t			usb_flags);

/*
 * Bulk data transfers
 *
 * flags:
 *	USB_FLAGS_ENQUEUE - enqueue even if no resources are available
 *	USB_SHORT_XFER_OK - short data-in transfers are permitted
 *
 * return values:
 *	USB_SUCCESS	 - success
 *	USB_FAILURE	 - unspecified failure
 *	USB_PIPE_RESERVED - pipe was reserved
 *	USB_NO_RESOURCES  - no resources
 */
int	usb_pipe_receive_bulk_data(
		usb_pipe_handle_t	pipe_handle,
		size_t			length,
		uint_t			usb_flags);

int	usb_pipe_send_bulk_data(
		usb_pipe_handle_t	pipe_handle,
		mblk_t			*data,
		uint_t			usb_flags);

/*
 * usb wrapper around pm_raise_power & pm_lower_power to allow for
 * non blocking behavior
 */
int usb_request_raise_power(dev_info_t *dip, int comp, int level,
	void (*callback)(void *, int), void *arg, uint_t flags);
int usb_request_lower_power(dev_info_t *dip, int comp, int level,
	void (*callback)(void *, int), void *arg, uint_t flags);

/* PM support functions */
int usb_is_pm_enabled(dev_info_t *dip);
int usb_enable_remote_wakeup(dev_info_t *dip);
void usb_enable_parent_notification(dev_info_t *dip);
int usb_create_pm_components(dev_info_t *dip, uint_t *pwrstates);

/*
 * usb wrapper functions to set usb device power level
 * Note : Power levels indicated here are USB power levels
 *        and not OS power levels
 */
int usb_set_device_pwrlvl0(dev_info_t *dip);
int usb_set_device_pwrlvl1(dev_info_t *dip);
int usb_set_device_pwrlvl2(dev_info_t *dip);
int usb_set_device_pwrlvl3(dev_info_t *dip);

/*
 * Issue a request to the taskq
 */
int usb_taskq_request(void (*func)(void *), void *arg, uint_t flag);


/*
 * usb logging, debug and console message handling
 */
typedef struct usb_log_handle *usb_log_handle_t;

#define	USB_LOG_L0	0	/* warnings, console & syslog buffer */
#define	USB_LOG_L1	1	/* errors, syslog buffer */
#define	USB_LOG_L2	2	/* recoverable errors, debug only */
#define	USB_LOG_L3	3	/* interesting data, debug only */
#define	USB_LOG_L4	4	/* tracing, debug only */

#ifdef DEBUG
#define	USB_DPRINTF_L4	usb_dprintf4
#define	USB_DPRINTF_L3	usb_dprintf3
#define	USB_DPRINTF_L2	usb_dprintf2

void usb_dprintf4(uint_t mask, usb_log_handle_t handle, char *fmt, ...);
void usb_dprintf3(uint_t mask, usb_log_handle_t handle, char *fmt, ...);
void usb_dprintf2(uint_t mask, usb_log_handle_t handle, char *fmt, ...);
#else
#define	USB_DPRINTF_L4
#define	USB_DPRINTF_L3
#define	USB_DPRINTF_L2
#endif

#define	USB_DPRINTF_L1	usb_dprintf1
#define	USB_DPRINTF_L0	usb_dprintf0

void usb_dprintf1(uint_t mask, usb_log_handle_t handle, char *fmt, ...);
void usb_dprintf0(uint_t mask, usb_log_handle_t handle, char *fmt, ...);


/* allocate a log handle */
usb_log_handle_t usb_alloc_log_handle(dev_info_t *dip, char *name,
	uint_t *errlevel, uint_t *mask, uint_t *instance_filter,
	uint_t *show_label, uint_t flags);

/* free the log handle */
void usb_free_log_handle(usb_log_handle_t handle);

/* log message */
int usb_log(usb_log_handle_t handle, uint_t level, uint_t mask,
	char *fmt, ...);

/*
 * USB enumeration statistics support
 */

/* Flags telling which stats usba_update_hotplug_stats should update */
#define	USB_TOTAL_HOTPLUG_SUCCESS	0x01
#define	USB_HOTPLUG_SUCCESS		0x02
#define	USB_TOTAL_HOTPLUG_FAILURE	0x04
#define	USB_HOTPLUG_FAILURE		0x08

/*
 * Increment enumeration stats indicated by the flags
 */
void usba_update_hotplug_stats(dev_info_t *dip, uint_t flags);

/* Retrieve the current enumeration hotplug statistics */
void usba_get_hotplug_stats(dev_info_t *dip, ulong_t *total_success,
	ulong_t *success, ulong_t *total_failure, ulong_t *failure,
	uchar_t *device_count);

/* Reset the resetable hotplug stats */
void usba_reset_hotplug_stats(dev_info_t *dip);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_USB_USBAI_H */
