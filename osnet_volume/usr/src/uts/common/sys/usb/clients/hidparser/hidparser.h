/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_HIDPARSER_H
#define	_SYS_USB_HIDPARSER_H

#pragma ident	"@(#)hidparser.h	1.1	99/02/25 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file contains interfaces accessible by both the hid driver and
 * a hid module.
 */

/*
 * HID parser handle
 *	The handle is opaque to the hid driver as well as the hid streams
 *	modules.
 */
typedef struct hidparser_handle_impl *hidparser_handle_t;


/*
 * hidparser_get_country_code():
 *	Obtain the country code value that was returned in the hid descriptor
 *	Fill in the country_code argument
 *
 * Arguments:
 *	parser_handle:
 *		hid parser handle
 * 	country code
 *		filled in with the country code value, upon success
 *
 * Return values:
 *	HIDPARSER_SUCCESS - returned on success
 *	HIDPARSER_FAILURE - returned on an unspecified error
 */
int hidparser_get_country_code(hidparser_handle_t parser_handle,
				uint16_t *country_code);


/*
 * hidparser_get_packet_size():
 *	Obtain the size(no. of bits) for a particular packet type. Note
 *	that a hid transfer may span more than one USB transaction.
 *
 * Arguments:
 *	parser_handle:
 * 		hid parser handle
 *	report_id:
 *		report id
 *      main_item_type:
 *              type of report, either Input, Output, or Feature
 *	size:
 *		the size if filled in upon success
 * Return values:
 *	HIDPARSER_SUCCESS - returned success
 *	HIDPARSER_FAILURE - returned failure
 */
int hidparser_get_packet_size(hidparser_handle_t parser_handle,
				uint_t report_id,
				uint_t main_item_type,
				uint_t *size);

/*
 * hidparser_get_usage_attribute()
 *	Find the specified local item associated with the given usage. For
 * 	example, this function may be used to find the logical minimum for
 *	an X usage.  Note that only short items are supported.
 *
 *
 * Arguments:
 *	parser_handle:
 *		hid parser handle
 *	report id:
 *		report id of the particular report that the usage may be
 *		found in.
 *	main_item_type:
 *		type of report, either Input, Output, or Feature
 *	usage_page:
 *		usage page that the Usage may be found on.
 *	usage:
 *		the Usage for which the local item will be found
 *	usage_attribute:
 *		type of local item to be found. Possible local and global
 *		items are given below.
 *
 *	usage_attribute_value:
 *		filled in with the value of the attribute upon return
 *
 * Return values:
 * 	HIDPARSER_SUCCESS - returned success
 *	HIDPARSER_NOT_FOUND - usage specified by the parameters was not found
 *	HIDPARSER_FAILURE - unspecified failure
 *
 */
int hidparser_get_usage_attribute(hidparser_handle_t parser_handle,
					uint_t report_id,
					uint_t main_item_type,
					uint_t usage_page,
					uint_t usage,
					uint_t usage_attribute,
					int *usage_attribute_value);

/*
 * hidparser_get_main_item_data_descr()
 *
 * Description:
 *      Query the parser to find the data description of the main item.
 *	Section 6.2.2.5 of the HID 1.0 specification gives details
 *	about the data descriptions. For example, this function may be
 *	used to find out if an X value sent by the a USB mouse is an
 *	absolute or relative value.
 *
 * Parameters:
 * 	parser_handle           parser handle
 *	report_id               report id of the particular report that the
 *				usage may be found in
 *	main_item_type          type of report - either Input, Output, Feature,
 *				or Collection
 *	usage_page              usage page that the usage may be found on
 *	usage			type of local item to be found
 *	main_item_descr_value   filled in with the data description
 *
 * Return values:
 *	HIDPARSER_SUCCESS       attribute found successfully
 *	HIDPARSER_NOT_FOUND     usage specified by the parameters was not found
 *      HIDPARSER_FAILURE       unspecified failure
 */

int
hidparser_get_main_item_data_descr(
			hidparser_handle_t	parser_handle,
			uint_t		report_id,
			uint_t		main_item_type,
			uint_t		usage_page,
			uint_t		usage,
			int		*main_item_descr_value);


/*
 * Local Items
 * 	See section 6.2.2.8 of the HID 1.0 specification for
 * 	more details.
 */
#define	HIDPARSER_ITEM_USAGE		0x08
#define	HIDPARSER_ITEM_USAGE_MIN	0x18
#define	HIDPARSER_ITEM_USAGE_MAX	0x28
#define	HIDPARSER_ITEM_DESIGNATOR_INDEX	0x38
#define	HIDPARSER_ITEM_DESIGNATOR_MIN	0x48
#define	HIDPARSER_ITEM_DESIGNATOR_MAX	0x58
#define	HIDPARSER_ITEM_STRING_INDEX	0x78
#define	HIDPARSER_ITEM_STRING_MIN	0x88
#define	HIDPARSER_ITEM_STRING_MAX	0x98

/*
 * Global Items
 *	See section 6.2.2.7 of the HID 1.0 specifations for
 *	more details.
 */
#define	HIDPARSER_ITEM_LOGICAL_MINIMUM	0x14
#define	HIDPARSER_ITEM_LOGICAL_MAXIMUM	0x24
#define	HIDPARSER_ITEM_PHYSICAL_MINIMUM	0x34
#define	HIDPARSER_ITEM_PHYSICAL_MAXIMUM	0x44
#define	HIDPARSER_ITEM_EXPONENT		0x54
#define	HIDPARSER_ITEM_UNIT		0x64
#define	HIDPARSER_ITEM_REPORT_SIZE	0x74
#define	HIDPARSER_ITEM_REPORT_ID	0x84
#define	HIDPARSER_ITEM_REPORT_COUNT	0x94

/*
 * Main Items
 *	See section 6.2.2.5 of the HID 1.0 specification for
 * 	more details.
 */
#define	HIDPARSER_ITEM_INPUT		0x80
#define	HIDPARSER_ITEM_OUTPUT		0x90
#define	HIDPARSER_ITEM_FEATURE		0xB0
#define	HIDPARSER_ITEM_COLLECTION	0xA0


/*
 * Usage Pages
 *	See the "Universal Serial Bus HID Usages Table"
 *	specification for more information
 */
#define	HID_UNDEFINED			0x00
#define	HID_GENERIC_DESKTOP		0x01
#define	HID_KEYBOARD_KEYPAD_KEYS	0x07
#define	HID_LEDS			0x08

/*
 * Generic Desktop Page (0x01)
 *	See the "Universal Serial Bus HID Usages Table"
 *      specification for more information
 */
#define	HID_GD_MOUSE		0x02
#define	HID_GD_KEYBOARD		0x06
#define	HID_GD_X		0x30
#define	HID_GD_Y		0x31
#define	HID_GD_BUTTON		0x38

/*
 * LED Page (0x08)
 *      See the "Universal Serial Bus HID Usages Table"
 *      specification for more information
 */
#define	HID_LED_UNDEFINED	0x00
#define	HID_LED_NUM_LOCK	0x01
#define	HID_LED_CAPS_LOCK	0x02
#define	HID_LED_SCROLL_LOCK	0x03
#define	HID_LED_COMPOSE		0x04
#define	HID_LED_KANA		0x05

/*
 * Main Item Data Descriptor Information for
 * 	Input, Output, and Feature Main Items
 * 	See section 6.2.2.5 of the HID 1.0 specification for
 * 	more details.
 */


#define	HID_MAIN_ITEM_DATA		0x0000
#define	HID_MAIN_ITEM_CONSTANT		0x0001
#define	HID_MAIN_ITEM_ARRAY		0x0000
#define	HID_MAIN_ITEM_VARIABLE		0x0002
#define	HID_MAIN_ITEM_ABSOLUTE		0x0000
#define	HID_MAIN_ITEM_RELATIVE		0x0004
#define	HID_MAIN_ITEM_NO_WRAP		0x0000
#define	HID_MAIN_ITEM_WRAP		0x0008
#define	HID_MAIN_ITEM_LINEAR		0x0000
#define	HID_MAIN_ITEM_NONLINEAR		0x0010
#define	HID_MAIN_ITEM_PREFERRED 	0x0000
#define	HID_MAIN_ITEM_NO_PREFERRED 	0x0020
#define	HID_MAIN_ITEM_NO_NULL		0x0000
#define	HID_MAIN_ITEM_NULL		0x0040
#define	HID_MAIN_ITEM_NON_VOLATILE	0x0000
#define	HID_MAIN_ITEM_VOLATILE		0x0080
#define	HID_MAIN_ITEM_BIT_FIELD		0x0000
#define	HID_MAIN_ITEM_BUFFERED_BYTE	0x0100

/*
 * Main Item Data Descriptor Information for
 * 	Collection Main Items
 * 	See section 6.2.2.4 of the HID 1.0 specification for
 * 	more details.
 */
#define	HID_MAIN_ITEM_PHYSICAL		0x0000
#define	HID_MAIN_ITEM_APPLICATION	0x0001
#define	HID_MAIN_ITEM_LOGICAL		0x0002


/*
 * Other
 */
#define	HIDPARSER_SUCCESS	0
#define	HIDPARSER_FAILURE	1
#define	HIDPARSER_NOT_FOUND	2

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_HIDPARSER_H */
