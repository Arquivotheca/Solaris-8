/*
 * Copyright (c) 1997 - 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)devinfo_prop_decode.c 1.5     98/02/09 SMI"

/*
 * This file contains kernel property decode routines adopted from
 * sunddi.c and ddi_impl.c. The following changes have been applied.
 *
 * (1) Replace kmem_alloc by malloc. Remove negative indexing
 * (2) Decoding applies only to prom properties.
 * (3) For strings, the return value is a composite string, not a string array.
 * (4) impl_ddi_prop_int_from_prom() uses _LITTLE_ENDIAN from isa_defs.h
 *
 * XXX This file should be kept in sync with kernel property encoding.
 */

#include <stdlib.h>
#include <strings.h>
#include <synch.h>
#include <sys/types.h>
#include <sys/dditypes.h>
#include <sys/ddipropdefs.h>
#include <sys/isa_defs.h>

#include "libdevinfo.h"

/*
 * Return an integer in native machine format from an OBP 1275 integer
 * representation, which is big-endian, with no particular alignment
 * guarantees. intp points to the OBP data, and n the number of bytes.
 *
 * Byte-swapping may be needed on some implementations.
 */
int
impl_di_prop_int_from_prom(u_char *intp, int n)
{
	int i = 0;

#if defined(_LITTLE_ENDIAN)
	intp += n;
	while (n-- > 0) {
		i = (i << 8) | *(--intp);
	}
#else
	while (n-- > 0) {
		i = (i << 8) | *intp++;
	}
#endif	/* defined(_LITTLE_ENDIAN) */

	return (i);
}

/*
 * Reset the current location pointer in the property handle to the
 * beginning of the data.
 */
void
di_prop_reset_pos(prop_handle_t *ph)
{
	ph->ph_cur_pos = ph->ph_data;
	ph->ph_save_pos = ph->ph_data;
}

/*
 * Restore the current location pointer in the property handle to the
 * saved position.
 */
void
di_prop_save_pos(prop_handle_t *ph)
{
	ph->ph_save_pos = ph->ph_cur_pos;
}

/*
 * Save the location that the current location poiner is pointing to..
 */
void
di_prop_restore_pos(prop_handle_t *ph)
{
	ph->ph_cur_pos = ph->ph_save_pos;
}

/*
 * Property encode/decode functions
 */

/*
 * Decode an array of integers property
 */
static int
di_prop_fm_decode_ints(prop_handle_t *ph, void *data, u_int *nelements)
{
	int	i;
	int	cnt = 0;
	int	*tmp;
	int	*intp;
	int	n;

	/*
	 * Figure out how many array elements there are by going through the
	 * data without decoding it first and counting.
	 */
	for (;;) {
		i = DDI_PROP_INT(ph, DDI_PROP_CMD_SKIP, NULL);
		if (i < 0)
			break;
		cnt++;
	}

	/*
	 * If there are no elements return an error
	 */
	if (cnt == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * If we cannot skip through the data, we cannot decode it
	 */
	if (i == DDI_PROP_RESULT_ERROR)
		return (DDI_PROP_CANNOT_DECODE);

	/*
	 * Reset the data pointer to the beginning of the encoded data
	 */
	di_prop_reset_pos(ph);

	/*
	 * Allocated memory to store the decoded value in.
	 */
	intp = malloc(cnt * sizeof (int));

	/*
	 * Decode each elemente and place it in the space we just allocated
	 */
	tmp = intp;
	for (n = 0; n < cnt; n++, tmp++) {
		i = DDI_PROP_INT(ph, DDI_PROP_CMD_DECODE, tmp);
		if (i < DDI_PROP_RESULT_OK) {
			/*
			 * Free the space we just allocated
			 * and return an error.
			 */
			free(intp);
			switch (i) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_DECODE);
			}
		}
	}

	*nelements = cnt;
	*(int **)data = intp;

	return (DDI_PROP_SUCCESS);
}

/*
 * Decode an array of strings.
 */
static int
di_prop_fm_decode_strings(prop_handle_t *ph, void *data, u_int *nelements)
{
	int		cnt = 0;
	char		*strs;
	char		*tmp;
	int		size;
	int		i;
	int		n;
	int		nbytes;

	/*
	 * Figure out how much memory we need for the sum total
	 */
	nbytes = 0;

	for (;;) {
		/*
		 * Get the decoded size of the current encoded string.
		 */
		size = DDI_PROP_STR(ph, DDI_PROP_CMD_GET_DSIZE, NULL);
		if (size < 0)
			break;

		cnt++;
		nbytes += size;
	}

	/*
	 * If there are no elements return an error
	 */
	if (cnt == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * If we cannot skip through the data, we cannot decode it
	 */
	if (size == DDI_PROP_RESULT_ERROR)
		return (DDI_PROP_CANNOT_DECODE);

	/*
	 * Allocate memory in which to store the decoded strings.
	 */
	strs = malloc(nbytes);

	/*
	 * Finally, we can decode each string
	 */
	di_prop_reset_pos(ph);
	tmp = strs;
	for (n = 0; n < cnt; n++) {
		i = DDI_PROP_STR(ph, DDI_PROP_CMD_DECODE, tmp);
		if (i < DDI_PROP_RESULT_OK) {
			/*
			 * Free the space we just allocated
			 * and return an error
			 */
			free(strs);
			switch (i) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_DECODE);
			}
		}
		tmp += strlen(tmp) + 1;
	}

	*(char **)data = strs;
	*nelements = cnt;

	return (DDI_PROP_SUCCESS);
}

/*
 * Decode an array of bytes.
 */
static int
di_prop_fm_decode_bytes(prop_handle_t *ph, void *data, u_int *nelements)
{
	u_char		*tmp;
	int		nbytes;
	int		i;

	/*
	 * If there are no elements return an error
	 */
	if (ph->ph_size == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * Get the size of the encoded array of bytes.
	 */
	nbytes = DDI_PROP_BYTES(ph, DDI_PROP_CMD_GET_DSIZE,
		data, ph->ph_size);
	if (nbytes < DDI_PROP_RESULT_OK) {
		switch (nbytes) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	/*
	 * Allocated memory to store the decoded value in.
	 */
	tmp = malloc(nbytes);

	/*
	 * Decode each element and place it in the space we just allocated
	 */
	i = DDI_PROP_BYTES(ph, DDI_PROP_CMD_DECODE, tmp, nbytes);
	if (i < DDI_PROP_RESULT_OK) {
		/*
		 * Free the space we just allocated
		 * and return an error
		 */
		free(tmp);
		switch (i) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	*(u_char **)data = tmp;
	*nelements = nbytes;

	return (DDI_PROP_SUCCESS);
}

/*
 * OBP 1275 integer, string and byte operators.
 *
 * DDI_PROP_CMD_DECODE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot decode the data
 *	DDI_PROP_RESULT_EOF:		end of data
 *	DDI_PROP_OK:			data was decoded
 *
 * DDI_PROP_CMD_ENCODE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot encode the data
 *	DDI_PROP_RESULT_EOF:		end of data
 *	DDI_PROP_OK:			data was encoded
 *
 * DDI_PROP_CMD_SKIP:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot skip the data
 *	DDI_PROP_RESULT_EOF:		end of data
 *	DDI_PROP_OK:			data was skipped
 *
 * DDI_PROP_CMD_GET_ESIZE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot get encoded size
 *	DDI_PROP_RESULT_EOF:		end of data
 *	> 0:				the encoded size
 *
 * DDI_PROP_CMD_GET_DSIZE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot get decoded size
 *	DDI_PROP_RESULT_EOF:		end of data
 *	> 0:				the decoded size
 */

/*
 * OBP 1275 integer operator
 *
 * OBP properties are a byte stream of data, so integers may not be
 * properly aligned. Therefore we need to copy them one byte at a time.
 */
int
di_prop_1275_int(prop_handle_t *ph, u_int cmd, int *data)
{
	int	i;

	switch (cmd) {
	case DDI_PROP_CMD_DECODE:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0)
			return (DDI_PROP_RESULT_ERROR);
		if (ph->ph_flags & PH_FROM_PROM) {
			i = ph->ph_size < PROP_1275_INT_SIZE ?
			    ph->ph_size : PROP_1275_INT_SIZE;
			if ((int *)ph->ph_cur_pos > ((int *)ph->ph_data +
			    ph->ph_size - i))
				return (DDI_PROP_RESULT_ERROR);
		} else if (ph->ph_size < sizeof (int) ||
		    ((int *)ph->ph_cur_pos > ((int *)ph->ph_data +
		    ph->ph_size - sizeof (int)))) {
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Copy the integer, using the implementation-specific
		 * copy function if the property is coming from the PROM.
		 */
		if (ph->ph_flags & PH_FROM_PROM) {
			*data = impl_di_prop_int_from_prom(
			    (u_char *)ph->ph_cur_pos,
			    (ph->ph_size < PROP_1275_INT_SIZE) ?
			    ph->ph_size : PROP_1275_INT_SIZE);
		} else {
			bcopy(ph->ph_cur_pos, (caddr_t)data, sizeof (int));
		}

		/*
		 * Move the current location to the start of the next
		 * bit of undecoded data.
		 */
		ph->ph_cur_pos = (u_char *)ph->ph_cur_pos + PROP_1275_INT_SIZE;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_ENCODE:
		/*
		 * Check that there is room to encoded the data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
		    ph->ph_size < PROP_1275_INT_SIZE ||
		    ((int *)ph->ph_cur_pos > ((int *)ph->ph_data +
		    ph->ph_size - sizeof (int))))
			return (DDI_PROP_RESULT_ERROR);

		/*
		 * Encode the integer into the byte stream one byte at a
		 * time.
		 */
		bcopy((caddr_t)data, ph->ph_cur_pos, sizeof (int));

		/*
		 * Move the current location to the start of the next bit of
		 * space where we can store encoded data.
		 */
		ph->ph_cur_pos = (u_char *)ph->ph_cur_pos + PROP_1275_INT_SIZE;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_SKIP:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
		    ph->ph_size < PROP_1275_INT_SIZE)
			return (DDI_PROP_RESULT_ERROR);


		if ((caddr_t)ph->ph_cur_pos ==
		    (caddr_t)ph->ph_data + ph->ph_size) {
			return (DDI_PROP_RESULT_EOF);
		} else if ((caddr_t)ph->ph_cur_pos >
		    (caddr_t)ph->ph_data + ph->ph_size) {
			return (DDI_PROP_RESULT_EOF);
		}

		/*
		 * Move the current location to the start of the next bit of
		 * undecoded data.
		 */
		ph->ph_cur_pos = (u_char *)ph->ph_cur_pos + PROP_1275_INT_SIZE;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_GET_ESIZE:
		/*
		 * Return the size of an encoded integer on OBP
		 */
		return (PROP_1275_INT_SIZE);

	case DDI_PROP_CMD_GET_DSIZE:
		/*
		 * Return the size of a decoded integer on the system.
		 */
		return (sizeof (int));
	}

	/*NOTREACHED*/
}

/*
 * OBP 1275 string operator.
 *
 * OBP strings are NULL terminated.
 */
int
di_prop_1275_string(prop_handle_t *ph, u_int cmd, char *data)
{
	int	n;
	char	*p;
	char	*end;

	switch (cmd) {
	case DDI_PROP_CMD_DECODE:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0) {
			return (DDI_PROP_RESULT_ERROR);
		}

		n = strlen((char *)ph->ph_cur_pos) + 1;
		if ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
		    ph->ph_size - n)) {
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Copy the NULL terminated string
		 */
		bcopy((char *)ph->ph_cur_pos, data, n);

		/*
		 * Move the current location to the start of the next bit of
		 * undecoded data.
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + n;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_ENCODE:
		/*
		 * Check that there is room to encoded the data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0) {
			return (DDI_PROP_RESULT_ERROR);
		}

		n = strlen(data) + 1;
		if ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
		    ph->ph_size - n)) {
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Copy the NULL terminated string
		 */
		bcopy(data, (char *)ph->ph_cur_pos, n);

		/*
		 * Move the current location to the start of the next bit of
		 * space where we can store encoded data.
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + n;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_SKIP:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0) {
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Return the string length plus one for the NULL
		 * We know the size of the property, we need to
		 * ensure that the string is properly formatted,
		 * since we may be looking up random OBP data.
		 */
		p = (char *)ph->ph_cur_pos;
		end = (char *)ph->ph_data + ph->ph_size;

		if (p == end) {
			return (DDI_PROP_RESULT_EOF);
		}

		for (n = 0; p < end; n++) {
			if (*p++ == 0) {
				ph->ph_cur_pos = p;
				return (DDI_PROP_RESULT_OK);
			}
		}

		return (DDI_PROP_RESULT_ERROR);

	case DDI_PROP_CMD_GET_ESIZE:
		/*
		 * Return the size of the encoded string on OBP.
		 */
		return (strlen(data) + 1);

	case DDI_PROP_CMD_GET_DSIZE:
		/*
		 * Return the string length plus one for the NULL
		 * We know the size of the property, we need to
		 * ensure that the string is properly formatted,
		 * since we may be looking up random OBP data.
		 */
		p = (char *)ph->ph_cur_pos;
		end = (char *)ph->ph_data + ph->ph_size;
		for (n = 0; p < end; n++) {
			if (*p++ == '\0') {
				ph->ph_cur_pos = p;
				return (n+1);
			}
		}

		/*
		 * Add check here to separate EOF and ERROR.
		 */
		if (p == end)
			return (DDI_PROP_RESULT_EOF);

		return (DDI_PROP_RESULT_ERROR);

	}

	/*NOTREACHED*/
}

/*
 * OBP 1275 byte operator
 *
 * Caller must specify the number of bytes to get. OBP encodes bytes
 * as a byte so there is a 1-to-1 translation.
 */
int
di_prop_1275_bytes(prop_handle_t *ph, u_int cmd, u_char *data, u_int nelements)
{
	switch (cmd) {
	case DDI_PROP_CMD_DECODE:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
		    ph->ph_size < nelements ||
		    ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
		    ph->ph_size - nelements)))
			return (DDI_PROP_RESULT_ERROR);

		/*
		 * Copy out the bytes
		 */
		bcopy((char *)ph->ph_cur_pos, (char *)data, nelements);

		/*
		 * Move the current location
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + nelements;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_ENCODE:
		/*
		 * Check that there is room to encode the data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
		    ph->ph_size < nelements ||
		    ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
		    ph->ph_size - nelements)))
			return (DDI_PROP_RESULT_ERROR);

		/*
		 * Copy in the bytes
		 */
		bcopy((char *)data, (char *)ph->ph_cur_pos, nelements);

		/*
		 * Move the current location to the start of the next bit of
		 * space where we can store encoded data.
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + nelements;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_SKIP:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
		    ph->ph_size < nelements)
			return (DDI_PROP_RESULT_ERROR);

		if ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
		    ph->ph_size - nelements))
			return (DDI_PROP_RESULT_EOF);

		/*
		 * Move the current location
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + nelements;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_GET_ESIZE:
		/*
		 * The size in bytes of the encoded size is the
		 * same as the decoded size provided by the caller.
		 */
		return (nelements);

	case DDI_PROP_CMD_GET_DSIZE:
		/*
		 * Just return the number of bytes specified by the caller.
		 */
		return (nelements);

	}

	/*NOTREACHED*/
}

/*
 * Used for properties that come from the OBP, hardware configuration files,
 * or that are created by calls to ddi_prop_update(9F).
 */
static struct prop_handle_ops prop_1275_ops = {
	di_prop_1275_int,
	di_prop_1275_string,
	di_prop_1275_bytes
};

/*
 * Now the real thing:
 * Extract type-specific values of an property
 */
int
di_prop_decode_common(void *data, int size, int prop_type, int prom)
{
	int rval;
	int nelements;
	char *cp;
	prop_handle_t ph;
	int (*prop_decoder)(prop_handle_t *, void *, u_int *);

	/*
	 * If the encoded data came from software, no decoding needed
	 */
	if (!prom) {
		switch (prop_type) {
		case DI_PROP_TYPE_INT:
			if (size % sizeof (int))
				nelements = -1;
			else
				nelements = size / sizeof (int);
			break;

		case DI_PROP_TYPE_STRING:
			nelements = 0;
			cp = *(char **)data;
			while (size > 0) {
				rval = strlen(cp) + 1;
				size -= rval;
				cp += rval;
				nelements++;
			}

			if (size != 0)	/* make sure it's made of strings */
				nelements = -1;

			break;

		case DI_PROP_TYPE_BYTE:
			nelements = size;
		}

		return (nelements);
	}

	/*
	 * Get the encoded data
	 */
	bzero((caddr_t)&ph, sizeof (prop_handle_t));
	ph.ph_data = *(uchar_t **)data;
	ph.ph_size = size;

	/*
	 * The data came from prom, use the 1275 OBP decode/encode routines.
	 */
	ph.ph_cur_pos = ph.ph_data;
	ph.ph_save_pos = ph.ph_data;
	ph.ph_ops = &prop_1275_ops;
	ph.ph_flags = PH_FROM_PROM;

	switch (prop_type) {
	case DI_PROP_TYPE_INT:
		prop_decoder = di_prop_fm_decode_ints;
		break;
	case DI_PROP_TYPE_STRING:
		prop_decoder = di_prop_fm_decode_strings;
		break;
	case DI_PROP_TYPE_BYTE:
	default:
		prop_decoder = di_prop_fm_decode_bytes;
		break;
	}

	if ((*prop_decoder)(&ph, data, (uint_t *)&nelements)
	    != DDI_PROP_SUCCESS)
		return (-1);

	/*
	 * Free the encoded data
	 */
	if (size != 0)
		free(ph.ph_data);

	return (nelements);
}

/* end of devinfo_prop_decode.c */
