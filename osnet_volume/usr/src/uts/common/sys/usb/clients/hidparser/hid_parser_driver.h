/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_HID_PARSER_DRIVER_H
#define	_SYS_USB_HID_PARSER_DRIVER_H

#pragma ident	"@(#)hid_parser_driver.h	1.1	99/02/25 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 *
 * This header file lists hidparser interfaces that are accessible only by
 * the hid driver.
 */


/*
 * hidparser_parse_report_descriptor():
 *	Parse a report descriptor according to the rules in the HID 1.0 spec.
 *	Return a pointer to a hidparser_handle_t which will be used for
 *	later queries to the parser.
 *
 * Arguments:
 *	report_descriptor:
 *		report_descriptor obtained from the HID device
 *	size:
 *		size of the report descriptor
 *	hid_descriptor:
 *		pointer to the hid descriptor
 *	parse_handle:
 *		pointer to a hidparser_handle_t
 *
 * Return values:
 *	HID_PARSER_SUCCESS - no errors
 * 	HID_PARSER_ERROR   - parsing the report descriptor failed
 *
 */
int hidparser_parse_report_descriptor(uchar_t *report_descriptor,
				size_t size,
				usb_hid_descr_t *hid_descriptor,
				hidparser_handle_t *parse_handle);



/*
 * hidparser_free_report_descriptor_handle():
 *	Free the report descriptor handle
 *
 * Arguments:
 *	parse_handle:
 *		handle to be freed
 *
 * Return values:
 *	HID_PARSER_SUCCESS - no errors
 * 	HID_PARSER_FAILURE - unspecified error when freeing descriptor
 */
int hidparser_free_report_descriptor_handle(hidparser_handle_t parse_handle);


/*
 * hidparser_get_top_level_collection_usage():
 *	Obtain the usage of the top level collection.  A streams module
 * 	will be pushed on top of the hid driver based on the usage.
 *
 * Arguments:
 *	parse_handle:  parser handle
 *	usage_page:    filled in with the usage page upon return
 *	usage:	       filled in with the usage upon return
 *
 * Return values:
 *	HID_PARSER_SUCCESS - no errors
 *	HID_PARSER_FAIOURE - unspecified error
 *
 */
int hidparser_get_top_level_collection_usage(hidparser_handle_t parse_handle,
					uint_t *usage_page,
					uint_t *usage);


#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_HID_PARSER_DRIVER_H */
