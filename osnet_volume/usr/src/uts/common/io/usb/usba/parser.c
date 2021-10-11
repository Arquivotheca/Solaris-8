/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)parser.c	1.3	99/09/24 SMI"

/*
 * Descriptor parsing functions
 */

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/usb/usba.h>

extern usb_config_pwr_descr_t default_config_power;
extern usb_interface_pwr_descr_t default_interface_power;

static size_t
usb_unpack_LE_data(char *format,
	uchar_t *data,
	size_t datalen,
	void *structure,
	size_t structlen)
{
	int	fmt;
	uchar_t	*dataend = data + datalen;
	char	*structstart = (char *)structure;
	void	*structend = (void *)((intptr_t)structstart + structlen);

	while ((fmt = *format++) != '\0') {

		if (fmt == 'c') {
			uint8_t	*cp = (uint8_t *)structure;

			cp = (uint8_t *)
				(((uintptr_t)cp + _CHAR_ALIGNMENT - 1) &
							~(_CHAR_ALIGNMENT - 1));
			if (data+1 > dataend || cp+1 > (uint8_t *)structend)
				break;

			*cp++ = *data++;
			structure = (void *)cp;
		} else if (fmt == 's') {
			uint16_t	*sp = (uint16_t *)structure;

			sp = (uint16_t *)
				(((uintptr_t)sp + _SHORT_ALIGNMENT - 1) &
						~(_SHORT_ALIGNMENT - 1));
			if (data+2 > dataend || sp+1 > (uint16_t *)structend)
				break;

			*sp++ = (data[1] << 8) + data[0];
			data += 2;
			structure = (void *)sp;
		} else if (fmt == 'l') {
			uint32_t 	*lp = (uint32_t *)structure;

			lp = (uint32_t *)
				(((uintptr_t)lp + _INT_ALIGNMENT - 1) &
							~(_INT_ALIGNMENT - 1));
			if (data+4 > dataend || lp+1 > (uint32_t *)structend)
				break;

			*lp++ = (((((
				(uint32_t)data[3] << 8) | data[2]) << 8) |
						data[1]) << 8) | data[0];
			data += 4;
			structure = (void *)lp;
		} else if (fmt == 'L') {
			uint64_t	*llp = (uint64_t *)structure;

			llp = (uint64_t *)
				(((uintptr_t)llp + _LONG_LONG_ALIGNMENT - 1) &
						~(_LONG_LONG_ALIGNMENT - 1));
			if (data+8 > dataend || llp+1 >= (uint64_t *)structend)
				break;

			*llp++ = (((((((((((((data[7] << 8) |
					data[6]) << 8) | data[5]) << 8) |
					data[4]) << 8) | data[3]) << 8) |
					data[2]) << 8) | data[1]) << 8) |
					data[0];
			data += 8;
			structure = (void *)llp;
		} else {
			break;
		}
	}

	return ((intptr_t)structure - (intptr_t)structstart);
}


size_t
usb_parse_CV_descr(char *format,
	uchar_t *data,
	size_t datalen,
	void *structure,
	size_t structlen)
{
	return (usb_unpack_LE_data(format, data, datalen, structure,
		structlen));
}

/*
 *	Helper function: returns pointer to n-th descriptor of
 *	type descr_type, unless the end of the buffer or a descriptor
 *	of type	stop_descr_type1 or stop_descr_type2 is encountered first.
 */
static uchar_t *
usb_nth_descr(
	uchar_t			*buf,
	size_t			buflen,
	int			descr_type,
	uint_t			n,
	int			stop_descr_type1,
	int			stop_descr_type2)
{
	uchar_t	*bufstart = buf;
	uchar_t *bufend = buf + buflen;

	while (buf + 2 <= bufend) {
		if (buf != bufstart &&
			(buf[1] == stop_descr_type1 ||
			    buf[1] == stop_descr_type2))

			return (NULL);

		if (descr_type == USB_DESCR_TYPE_ANY ||
			buf[1] == descr_type) {
			if (n == 0) {

				return (buf);
			}
			--n;
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infite loop
		 */
		if (buf[0] == 0) {
			break;

		} else {
			buf += buf[0];
		}

	}
	return (NULL);
}


size_t
usb_parse_device_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(DEVICE) */
	size_t			buflen,
	usb_device_descr_t	*ret_descr,
	size_t			ret_buf_len)
{
	if (buflen < 2 || buf[1] != USB_DESCR_TYPE_DEVICE)

		return (USB_PARSE_ERROR);

	return (usb_unpack_LE_data("ccsccccssscccc",
		buf, buflen, ret_descr, ret_buf_len));
}


size_t
usb_parse_configuration_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	usb_config_descr_t	*ret_descr,
	size_t			ret_buf_len)
{
	if (buflen < 2 || buf[1] != USB_DESCR_TYPE_CONFIGURATION)

		return (USB_PARSE_ERROR);

	return (usb_unpack_LE_data("ccsccccc",
		buf, buflen, ret_descr, ret_buf_len));
}

size_t
usb_parse_config_pwr_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	usb_config_pwr_descr_t	*ret_descr,
	size_t			ret_buf_len)
{
	uchar_t *bufend = buf + buflen;

	while (buf + 2 <= bufend) {

		if (buf[1] == USB_DESCR_TYPE_CONFIGURATION_POWER) {
			return (usb_unpack_LE_data("ccsccccccccsss",
				buf, buflen, ret_descr, ret_buf_len));
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infite loop
		 */
		if (buf[0] == 0) {
			break;

		} else {
			buf += buf[0];
		}
	}

	/* return the default configuration power descriptor */
	bcopy(&default_config_power, ret_descr, USB_CONF_PWR_DESCR_SIZE);

	return (ret_descr->bLength);

}


size_t
usb_parse_interface_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	usb_interface_descr_t	*ret_descr,
	size_t			ret_buf_len)
{
	uchar_t *bufend = buf + buflen;

	while (buf + 4 <= bufend) {
		if (buf[1] == USB_DESCR_TYPE_INTERFACE &&
		    buf[2] == interface_number &&
		    buf[3] == alt_interface_setting) {

			return (usb_unpack_LE_data("ccccccccc",
			    buf, bufend - buf, ret_descr, ret_buf_len));
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infite loop
		 */
		if (buf[0] == 0) {
			break;

		} else {
			buf += buf[0];
		}
	}

	return (USB_PARSE_ERROR);
}

size_t
usb_parse_interface_pwr_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	usb_interface_pwr_descr_t	*ret_descr,
	size_t			ret_buf_len)
{
	uchar_t *bufend = buf + buflen;

	while (buf + 4 <= bufend) {
		if (buf[1] == USB_DESCR_TYPE_INTERFACE &&
		    buf[2] == interface_number &&
		    buf[3] == alt_interface_setting) {

			buf += buf[0];

			if (buf + 2 <= bufend) {
				if (buf[1] == USB_DESCR_TYPE_INTERFACE_POWER) {

					return (
					usb_unpack_LE_data("cccccccccsss",
					buf, bufend - buf, ret_descr,
					ret_buf_len));
				} else {
					break;
				}
			} else {
				break;
			}
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infite loop
		 */
		if (buf[0] == 0) {
			break;

		} else {
			buf += buf[0];
		}
	}

	/* return the default interface power descriptor */
	bcopy(&default_interface_power, ret_descr, USB_IF_PWR_DESCR_SIZE);

	return (ret_descr->bLength);
}


/*
 * the endpoint index is relative to the interface. index 0 is
 * the first endpoint
 */
size_t
usb_parse_endpoint_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	uint_t			endpoint_index,
	usb_endpoint_descr_t	*ret_descr,
	size_t			ret_buf_len)
{
	uchar_t *bufend = buf + buflen;

	while ((buf + 4) <= bufend) {
		if (buf[1] == USB_DESCR_TYPE_INTERFACE &&
			buf[2] == interface_number &&
			buf[3] == alt_interface_setting) {
			if ((buf = usb_nth_descr(buf, bufend-buf,
			    USB_DESCR_TYPE_ENDPOINT, endpoint_index,
			    USB_DESCR_TYPE_INTERFACE, -1)) == NULL) {

				break;
			}

			return (usb_unpack_LE_data("ccccsc", buf, bufend - buf,
						ret_descr, ret_buf_len));
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infite loop
		 */
		if (buf[0] == 0) {
			break;

		} else {
			buf += buf[0];
		}

	}

	return (USB_PARSE_ERROR);
}


/*
 * Returns (at ret_descr) a null-terminated string.  Null termination is
 * guaranteed, even if the string is longer than the buffer.  Thus, a
 * maximum of (ret_buf_len - 1) characters are returned.
 * Stops silently on first character not in UNICODE format.
 */
/*ARGSUSED*/
size_t
usb_ascii_string_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(STRING) */
	size_t			buflen,
	char			*ret_descr,
	size_t			ret_buf_len)
{
	char	*retstart = ret_descr;
	uchar_t *bufend = buf + buflen;

	if (ret_buf_len == 0 || buflen < 2 ||
		buf[0] < 2 || buf[1] != USB_DESCR_TYPE_STRING) {

		return (USB_PARSE_ERROR);
	}

	for (buf = buf + 2; buf+1 < bufend && ret_buf_len > 1 &&
	    buf[0] != 0 && buf[1] == 0; buf += 2) {
		*ret_descr++ = buf[0];
	}

	*ret_descr++ = 0;

	return (ret_descr - retstart);
}


size_t
usb_parse_CV_configuration_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	char			*fmt,
	uint_t			descr_type,
	uint_t			descr_index,
	void			*ret_descr,
	size_t			ret_buf_len)
{
	uchar_t *bufend = buf + buflen;

	if (buflen < 2 || (buf = usb_nth_descr(buf, buflen, descr_type,
				descr_index, -1, -1)) == NULL) {

		return (USB_PARSE_ERROR);
	}

	return (usb_unpack_LE_data(fmt, buf, bufend - buf, ret_descr,
			ret_buf_len));
}


size_t
usb_parse_CV_interface_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	char			*fmt,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	uint_t			descr_type,
	uint_t			descr_index,
	void			*ret_descr,
	size_t			ret_buf_len)
{
	uchar_t *bufend = buf + buflen;

	while (buf + 4 <= bufend) {
		if (buf[1] == USB_DESCR_TYPE_INTERFACE &&
			buf[2] == interface_number &&
			buf[3] == alt_interface_setting) {
			if ((buf = usb_nth_descr(buf, bufend-buf, descr_type,
			    descr_index, USB_DESCR_TYPE_INTERFACE, -1)) ==
			    NULL) {
				break;
			}

			return (usb_unpack_LE_data(fmt,
				buf, bufend - buf, ret_descr, ret_buf_len));
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infinite loop
		 */
		if (buf[0] == 0) {
			break;

		} else {
			buf += buf[0];
		}

	}

	return (USB_PARSE_ERROR);
}


size_t
usb_parse_CV_endpoint_descr(
	uchar_t			*buf,	/* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	char			*fmt,
	uint_t			interface_number,
	uint_t			alt_interface_setting,
	uint_t			endpoint_index,
	uint_t			descr_type,
	uint_t			descr_index,
	void			*ret_descr,
	size_t			ret_buf_len)
{
	uchar_t *bufend = buf + buflen;

	while (buf + 4 <= bufend) {
		if (buf[1] == USB_DESCR_TYPE_INTERFACE &&
			buf[2] == interface_number &&
			buf[3] == alt_interface_setting) {
			if ((buf = usb_nth_descr(buf, bufend-buf,
			    USB_DESCR_TYPE_ENDPOINT, endpoint_index,
			    USB_DESCR_TYPE_INTERFACE, -1)) == NULL) {

				break;
			}
			if ((buf = usb_nth_descr(buf, bufend-buf,
			    descr_type, descr_index,
			    USB_DESCR_TYPE_ENDPOINT,
			    USB_DESCR_TYPE_INTERFACE)) == NULL) {

				break;
			}

			return (usb_unpack_LE_data(fmt, buf, bufend - buf,
						ret_descr, ret_buf_len));
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infite loop
		 */
		if (buf[0] == 0) {
			break;

		} else {
			buf += buf[0];
		}

	}

	return (USB_PARSE_ERROR);
}
