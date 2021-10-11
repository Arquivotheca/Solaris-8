/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dhcp_inittab.c	1.5	99/08/18 SMI"

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <sys/isa_defs.h>
#include <arpa/inet.h>

#include "dhcp_inittab.h"

/* LINTLIBRARY */

static uint64_t		dhcp_htonll(uint64_t);
static uint64_t		dhcp_ntohll(uint64_t);
static void		inittab_msg(const char *fmt, ...);
static uint8_t		ie_type_to_size(inittab_entry_t *);
static uchar_t		category_to_code(const char *);
static boolean_t	encode_number(uint8_t, uint8_t, boolean_t, uint8_t,
			    const char *, uint8_t *);
static boolean_t	decode_number(uint8_t, uint8_t, boolean_t, uint8_t,
			    const uint8_t *, char *);
static inittab_entry_t *inittab_lookup(uchar_t, char, const char *, int32_t,
			    size_t *);
/*
 * forward declaration of our internal inittab_table[].  too bulky
 * to put up front -- check the end of this file for its definition.
 */

static inittab_entry_t	inittab_table[];

/*
 * an inittab_file_entry_t is used to describe how the data appears in
 * the inittab file, for use in inittab_lookup().  this structure is
 * an implementation detail and thus not visible outside of this file.
 */

#define	IFE_CATEGORY_SIZE	12
#define	IFE_TYPE_SIZE		12
#define	IFE_CONSUMER_SIZE	10
#define	IFE_NUM_FIELDS		7

typedef struct inittab_file_entry
{
	char		ife_name[DHCP_SYMBOL_SIZE + 1];
	char		ife_category[IFE_CATEGORY_SIZE + 1];
	uchar_t		ife_category_code;
	uint16_t	ife_code;
	char		ife_type[IFE_TYPE_SIZE + 1];
	CDTYPE		ife_type_code;
	uint16_t	ife_gran;
	uint16_t	ife_max;
	char		ife_consumers[IFE_CONSUMER_SIZE + 1];

} inittab_file_entry_t;

/*
 * valid values for the `type' field.
 * note: must be kept in sync with CDTYPE definition in <netinet/dhcp.h>
 */

static const char *ife_types[] = {
	"ASCII", "OCTET", "IP", "NUMBER", "BOOL", "INCLUDE", "UNUMBER8",
	"UNUMBER16", "UNUMBER32", "UNUMBER64", "SNUMBER8", "SNUMBER16",
	"SNUMBER32", "SNUMBER64", NULL
};

/*
 * ick.  in order to be robust, we need to include the size of the various
 * strings in the sscanf() format string.  don't try this at home, kids.
 */

#define	STRINGIFY2(x)		#x
#define	STRINGIFY(x)		STRINGIFY2(x)
#define	IFE_FMT_STRING		"%"STRINGIFY(DHCP_SYMBOL_SIZE)"s "	    \
				"%"STRINGIFY(IFE_CATEGORY_SIZE)"[^,],%hd, " \
				"%"STRINGIFY(IFE_TYPE_SIZE)"[^,],%hd,%hd, " \
				"%"STRINGIFY(IFE_CONSUMER_SIZE)"s"

/*
 * inittab_load(): returns all inittab entries with the specified criteria
 *
 *   input: uchar_t: the categories the consumer is interested in
 *	    char: the consumer type of the caller
 *	    size_t *: set to the number of entries returned
 *  output: inittab_entry_t *: an array of dynamically allocated entries
 *	    on success, NULL upon failure
 */

inittab_entry_t	*
inittab_load(uchar_t categories, char consumer, size_t *n_entries)
{
	return (inittab_lookup(categories, consumer, NULL, -1, n_entries));
}

/*
 * inittab_getbyname(): returns an inittab entry with the specified criteria
 *
 *   input: int: the categories the consumer is interested in
 *	    char: the consumer type of the caller
 *	    char *: the name of the inittab entry the consumer wants
 *  output: inittab_entry_t *: a dynamically allocated inittab entry
 *	    on success, NULL upon failure
 */

inittab_entry_t	*
inittab_getbyname(uchar_t categories, char consumer, const char *name)
{
	return (inittab_lookup(categories, consumer, name, -1, NULL));
}

/*
 * inittab_getbycode(): returns an inittab entry with the specified criteria
 *
 *   input: uchar_t: the categories the consumer is interested in
 *	    char: the consumer type of the caller
 *	    uint16_t: the code of the inittab entry the consumer wants
 *  output: inittab_entry_t *: a dynamically allocated inittab entry
 *	    on success, NULL upon failure
 */

inittab_entry_t	*
inittab_getbycode(uchar_t categories, char consumer, uint16_t code)
{
	return (inittab_lookup(categories, consumer, NULL, code, NULL));
}

/*
 * inittab_lookup(): returns inittab entries with the specified criteria
 *
 *   input: uchar_t: the categories the consumer is interested in
 *	    char: the consumer type of the caller
 *	    uint16_t: the code of the inittab entry the consumer wants
 *	    const char *: the name of the entry the caller is interested
 *		in, or NULL if the caller doesn't care
 *	    int32_t: the code the caller is interested in, or -1 if the
 *		caller doesn't care
 *	    size_t *: set to the number of entries returned
 *  output: inittab_entry_t *: dynamically allocated inittab entries
 *	    on success, NULL upon failure
 */

static inittab_entry_t *
inittab_lookup(uchar_t categories, char consumer, const char *name,
    int32_t code, size_t *n_entriesp)
{
	FILE			*inittab_fp;
	inittab_entry_t		*new_entries, *entries = NULL;
	inittab_file_entry_t	entry;
	char			buffer[ITAB_MAX_LINE_LEN];
	unsigned long		line = 0;
	int			n_fields;
	size_t			i, n_entries = 0;
	char			*inittab_path;

	inittab_path = getenv("DHCP_INITTAB_PATH");
	if (inittab_path == NULL)
		inittab_path = ITAB_INITTAB_PATH;

	inittab_fp = fopen(inittab_path, "r");
	if (inittab_fp == NULL) {
		inittab_msg("inittab_lookup: fopen: %s: %s",
		    ITAB_INITTAB_PATH, strerror(errno));
		return (NULL);
	}

	while (fgets(buffer, sizeof (buffer), inittab_fp) != NULL) {

		line++;

		/*
		 * make sure the string didn't overflow our buffer
		 */

		if (strchr(buffer, '\n') == NULL) {
			inittab_msg("inittab_lookup: line %li: too long "
			    "(skipping)", line);
			continue;
		}

		/*
		 * skip `pure comment' lines
		 */

		for (i = 0; buffer[i] != '\0'; i++)
			if (isspace(buffer[i]) == 0)
				break;

		if (buffer[i] == ITAB_COMMENT_CHAR || buffer[i] == '\0')
			continue;

		/*
		 * parse out the entry
		 */

		n_fields = sscanf(buffer, IFE_FMT_STRING, entry.ife_name,
		    entry.ife_category, &entry.ife_code, entry.ife_type,
		    &entry.ife_gran, &entry.ife_max, entry.ife_consumers);

		if (n_fields == EOF)
			break;

		if (n_fields != IFE_NUM_FIELDS) {

			/*
			 * it's legal to have a line with no consumers, in
			 * which case n_fields will be IFE_NUM_FIELDS - 1.
			 * we just ignore these lines.
			 */

			if (n_fields != IFE_NUM_FIELDS - 1)
				inittab_msg("inittab_lookup: line %li: syntax "
				    "error (skipping)", line);
			continue;
		}

		if (entry.ife_gran > ITAB_GRAN_MAX) {
			inittab_msg("inittab_lookup: line %li: granularity `%d'"
			    " out of range (skipping)", line, entry.ife_gran);
			continue;
		}

		if (entry.ife_max > ITAB_MAX_MAX) {
			inittab_msg("inittab_lookup: line %li: maximum `%d' "
			    "out of range (skipping)", line, entry.ife_max);
			continue;
		}

		for (i = 0; ife_types[i] != NULL; i++)
			if (strcasecmp(ife_types[i], entry.ife_type) == 0)
				break;

		if (ife_types[i] == NULL) {
			inittab_msg("inittab_lookup: line %li: type `%s' "
			    "is invalid (skipping)", line, entry.ife_type);
			continue;
		}

		entry.ife_type_code = i;

		/*
		 * find out whether this entry of interest to our consumer,
		 * and if so, throw it onto the set of entries we'll return.
		 * check categories last since it's the most expensive check.
		 */

		if (strchr(entry.ife_consumers, consumer) == NULL)
			continue;

		if (code != -1 && entry.ife_code != code)
			continue;

		if (name != NULL && strcasecmp(entry.ife_name, name) != 0)
			continue;

		entry.ife_category_code = category_to_code(entry.ife_category);
		if ((entry.ife_category_code & categories) == 0)
			continue;

		/*
		 * looks like a match.  allocate an entry and fill it in
		 */

		new_entries = realloc(entries, (n_entries + 1) *
		    sizeof (inittab_entry_t));

		/*
		 * if we run out of memory, might as well return what we can
		 */

		if (new_entries == NULL) {
			inittab_msg("inittab_lookup: ran out of memory "
			    "allocating inittab_entry_t's");
			break;
		}

		entries = new_entries;
		entries[n_entries].ie_category	= entry.ife_category_code;
		entries[n_entries].ie_code	= entry.ife_code;
		entries[n_entries].ie_type	= entry.ife_type_code;
		entries[n_entries].ie_gran	= entry.ife_gran;
		entries[n_entries].ie_max	= entry.ife_max;
		entries[n_entries].ie_max_size  = entry.ife_max *
		    ie_type_to_size(&entries[n_entries]);

		(void) strcpy(entries[n_entries].ie_name, entry.ife_name);

		n_entries++;
	}

	if (ferror(inittab_fp) != 0) {
		inittab_msg("inittab_lookup: error on inittab stream");
		clearerr(inittab_fp);
	}

	(void) fclose(inittab_fp);

	if (n_entriesp != NULL)
		*n_entriesp = n_entries;

	return (entries);
}

/*
 * inittab_verify(): verifies that a given inittab entry matches an internal
 *		     definition
 *
 *   input: inittab_entry_t *: the inittab entry to verify
 *	    inittab_entry_t *: if non-NULL, a place to store the internal
 *			       inittab entry upon return
 *  output: int: ITAB_FAILURE, ITAB_SUCCESS, or ITAB_UNKNOWN
 */

int
inittab_verify(inittab_entry_t *inittab_ent, inittab_entry_t *internal_ent)
{
	unsigned int	i;

	for (i = 0; inittab_table[i].ie_name[0] != '\0'; i++) {

		if (inittab_ent->ie_category != inittab_table[i].ie_category)
			continue;

		if (inittab_ent->ie_code == inittab_table[i].ie_code) {

			if (internal_ent != NULL)
				*internal_ent = inittab_table[i];

			if (inittab_table[i].ie_gran != inittab_ent->ie_gran ||
			    inittab_table[i].ie_type != inittab_ent->ie_type ||
			    inittab_table[i].ie_max  != inittab_ent->ie_max)
				return (ITAB_FAILURE);

			return (ITAB_SUCCESS);
		}
	}

	return (ITAB_UNKNOWN);
}

/*
 * inittab_encode(): converts a string representation of a given datatype into
 *		     binary; used for encoding ascii values into a form that
 *		     can be put in DHCP packets to be sent on the wire.
 *
 *   input: inittab_entry_t *: the entry describing the value option
 *	    const char *: the value to convert
 *	    uint16_t *: set to the length of the binary data returned
 *	    boolean_t: if false, return a full DHCP option
 *  output: uchar_t *: a dynamically allocated byte array with converted data
 */

uchar_t *
inittab_encode(inittab_entry_t *ie, const char *value, uint16_t *lengthp,
    boolean_t just_payload)
{
	uint16_t	length = 0;
	uchar_t		n_entries = 0;
	const char	*valuep;
	uchar_t		*result;
	struct in_addr	in_addr;
	unsigned int	i;
	uint8_t		type_size = ie_type_to_size(ie);
	boolean_t	is_signed;

	if (type_size == 0)
		return (NULL);

	/*
	 * figure out the number of entries by counting the spaces
	 * in the value string
	 */

	for (valuep = value; valuep++ != NULL; n_entries++)
		valuep = strchr(valuep, ' ');

	if (ie->ie_type == ASCII)
		n_entries = strlen(value);		/* no NUL */

	/*
	 * if we're gonna return a complete option, then include the
	 * option length and code in the size of the packet we allocate
	 */

	if (just_payload == B_FALSE)
		length += 2;

	length += n_entries * type_size;
	result  = malloc(length);
	if (result == NULL)
		return (NULL);

	switch (ie->ie_type) {

	case ASCII:

		(void) memcpy(result, value, length);
		break;

	case OCTET:

		for (valuep = value, i = 0; i < n_entries; i++)
			result[i] = strtoul(valuep, (char **)&valuep, 0);
		break;

	case IP:

		if (n_entries % ie->ie_gran != 0) {
			inittab_msg("inittab_encode: number of entries "
			    "not compatible with option granularity");
			return (NULL);
		}

		for (valuep = value, i = 0; i < n_entries; i++, valuep++) {

			in_addr.s_addr = inet_addr(valuep);
			(void) memcpy(&result[i * sizeof (ipaddr_t)],
			    &in_addr.s_addr, sizeof (ipaddr_t));

			valuep = strchr(valuep, ' ');
			if (valuep == NULL)
				break;
		}
		break;

	case NUMBER:					/* FALLTHRU */
	case UNUMBER8:					/* FALLTHRU */
	case SNUMBER8:					/* FALLTHRU */
	case UNUMBER16:					/* FALLTHRU */
	case SNUMBER16:					/* FALLTHRU */
	case UNUMBER32:					/* FALLTHRU */
	case SNUMBER32:					/* FALLTHRU */
	case UNUMBER64:					/* FALLTHRU */
	case SNUMBER64:

		if (ie->ie_type == SNUMBER64 || ie->ie_type == SNUMBER32 ||
		    ie->ie_type == SNUMBER16 || ie->ie_type == SNUMBER8)
			is_signed = B_TRUE;
		else
			is_signed = B_FALSE;

		if (encode_number(n_entries, type_size, is_signed, 0, value,
		    result) == B_FALSE) {
			free(result);
			return (NULL);
		}
		break;

	default:
		inittab_msg("inittab_encode: unsupported type `%d'",
		    ie->ie_type);
		free(result);
		return (NULL);
	}

	/*
	 * if just_payload is false, then we need to slide the option
	 * code and length fields in. (length includes them in its
	 * count, so we have to subtract 2)
	 */

	if (just_payload == B_FALSE) {
		(void) memmove(result + 2, result, length - 2);
		result[0] = ie->ie_code;
		result[1] = length - 2;
	}

	if (lengthp != NULL)
		*lengthp = length;

	return (result);
}

/*
 * inittab_decode(): converts a binary representation of a given datatype into
 *		     a string; used for decoding DHCP options in a packet off
 *		     the wire into ascii
 *
 *   input: inittab_entry_t *: the entry describing the payload option
 *	    uchar_t *: the payload to convert
 *	    uint16_t: the payload length (only used if just_payload is true)
 *	    boolean_t: if false, payload is assumed to be a DHCP option
 *  output: char *: a dynamically allocated string containing the converted data
 */

char *
inittab_decode(inittab_entry_t *ie, uchar_t *payload, uint16_t length,
    boolean_t just_payload)
{
	char		*resultp, *result = NULL;
	uchar_t		n_entries;
	struct in_addr	in_addr;
	uint8_t		type_size = ie_type_to_size(ie);
	boolean_t	is_signed;

	if (type_size == 0)
		return (NULL);

	if (just_payload == B_FALSE) {
		length = payload[1];
		payload += 2;
	}

	/*
	 * figure out the number of elements to convert.  note that
	 * for ie_type NUMBER, the granularity is really 1 since the
	 * value of ie_gran is the number of bytes in the number.
	 */

	if (ie->ie_type == NUMBER)
		n_entries = MIN(ie->ie_max, length / type_size);
	else
		n_entries = MIN(ie->ie_max * ie->ie_gran, length / type_size);

	if (n_entries == 0)
		n_entries = length / type_size;

	if ((length % type_size) != 0) {
		inittab_msg("inittab_decode: length of string not compatible "
		    "with option type `%i'", ie->ie_type);
		return (NULL);
	}

	switch (ie->ie_type) {

	case ASCII:

		result = malloc(n_entries + 1);
		if (result == NULL)
			return (NULL);

		(void) memcpy(result, payload, n_entries);
		result[n_entries] = '\0';
		break;

	case OCTET:

		result = malloc(n_entries * (sizeof ("0xNN") + 1));
		if (result == NULL)
			return (NULL);

		for (resultp = result; n_entries != 0; n_entries--)
			resultp += sprintf(resultp, "0x%02X ", *payload++);

		resultp[-1] = '\0';
		break;

	case IP:

		if ((length / sizeof (ipaddr_t)) % ie->ie_gran != 0) {
			inittab_msg("inittab_decode: number of entries "
			    "not compatible with option granularity");
			return (NULL);
		}

		result = malloc(n_entries * (sizeof ("aaa.bbb.ccc.ddd") + 1));
		if (result == NULL)
			return (NULL);

		for (resultp = result; n_entries != 0; n_entries--) {
			(void) memcpy(&in_addr.s_addr, payload,
			    sizeof (ipaddr_t));
			resultp += sprintf(resultp, "%s ", inet_ntoa(in_addr));
			payload += sizeof (ipaddr_t);
		}

		resultp[-1] = '\0';
		break;

	case NUMBER:					/* FALLTHRU */
	case UNUMBER8:					/* FALLTHRU */
	case SNUMBER8:					/* FALLTHRU */
	case UNUMBER16:					/* FALLTHRU */
	case SNUMBER16:					/* FALLTHRU */
	case UNUMBER32:					/* FALLTHRU */
	case SNUMBER32:					/* FALLTHRU */
	case UNUMBER64:					/* FALLTHRU */
	case SNUMBER64:

		if (ie->ie_type == SNUMBER64 || ie->ie_type == SNUMBER32 ||
		    ie->ie_type == SNUMBER16 || ie->ie_type == SNUMBER8)
			is_signed = B_TRUE;
		else
			is_signed = B_FALSE;

		result = malloc(n_entries * ITAB_MAX_NUMBER_LEN);
		if (result == NULL)
			return (NULL);

		if (decode_number(n_entries, type_size, is_signed, ie->ie_gran,
		    payload, result) == B_FALSE) {
			free(result);
			return (NULL);
		}
		break;

	default:
		inittab_msg("inittab_decode: unsupported type `%d'",
		    ie->ie_type);
		break;
	}

	return (result);
}

/*
 * inittab_msg(): prints diagnostic messages if INITTAB_DEBUG is set
 *
 *	    const char *: a printf-like format string
 *	    ...: arguments to the format string
 *  output: void
 */

static void
inittab_msg(const char *fmt, ...)
{
	enum { INITTAB_MSG_CHECK, INITTAB_MSG_RETURN, INITTAB_MSG_OUTPUT };

	va_list		ap;
	char		buf[512];
	static int	action = INITTAB_MSG_CHECK;

	/*
	 * check DHCP_INITTAB_DEBUG the first time in; thereafter, use
	 * the the cached result (stored in `action').
	 */

	switch (action) {

	case INITTAB_MSG_CHECK:

		if (getenv("DHCP_INITTAB_DEBUG") == NULL) {
			action = INITTAB_MSG_RETURN;
			return;
		}

		action = INITTAB_MSG_OUTPUT;

		/* FALLTHRU into INITTAB_MSG_OUTPUT */

	case INITTAB_MSG_OUTPUT:

		va_start(ap, fmt);

		(void) snprintf(buf, sizeof (buf), "inittab: %s\n", fmt);
		(void) vfprintf(stderr, buf, ap);

		va_end(ap);
		break;

	case INITTAB_MSG_RETURN:

		return;
	}
}

/*
 * decode_number(): decodes a sequence of numbers from binary into ascii;
 *		    binary is coming off of the network, so it is in nbo
 *
 *   input: uint8_t: the number of "granularity" numbers to decode
 *	    uint8_t: the length of each number
 *	    boolean_t: whether the numbers should be considered signed
 *	    uint8_t: the number of numbers per granularity
 *	    const uint8_t *: where to decode the numbers from
 *	    char *: where to decode the numbers to
 *  output: boolean_t: true on successful conversion, false on failure
 */

static boolean_t
decode_number(uint8_t n_entries, uint8_t size, boolean_t is_signed,
    uint8_t granularity, const uint8_t *from, char *to)
{
	uint16_t	uint16;
	uint32_t	uint32;
	uint64_t	uint64;

	if (granularity != 0) {
		if ((granularity % n_entries) != 0) {
			inittab_msg("decode_number: number of entries "
			    "not compatible with option granularity");
			return (B_FALSE);
		}
	}

	for (; n_entries != 0; n_entries--, from += size) {

		switch (size) {

		case 1:
			to += sprintf(to, is_signed ? "%d " : "%u ", *from);
			break;

		case 2:
			(void) memcpy(&uint16, from, 2);
			to += sprintf(to, is_signed ? "%hd " : "%hu ",
			    ntohs(uint16));
			break;

		case 4:
			(void) memcpy(&uint32, from, 4);
			to += sprintf(to, is_signed ? "%ld " : "%lu ",
			    ntohl(uint32));
			break;

		case 8:
			(void) memcpy(&uint64, from, 8);
			to += sprintf(to, is_signed ? "%lld " : "%llu ",
			    dhcp_ntohll(uint64));
			break;

		default:
			inittab_msg("decode_number: unknown integer size `%d'",
			    size);
			return (B_FALSE);
		}
	}

	to[-1] = '\0';
	return (B_TRUE);
}

/*
 * encode_number(): encodes a sequence of numbers from ascii into binary;
 *		    number will end up on the wire so it needs to be in nbo
 *
 *   input: uint8_t: the number of "granularity" numbers to encode
 *	    uint8_t: the length of each number
 *	    boolean_t: whether the numbers should be considered signed
 *	    uint8_t: the number of numbers per granularity
 *	    const uint8_t *: where to encode the numbers from
 *	    char *: where to encode the numbers to
 *  output: boolean_t: true on successful conversion, false on failure
 */

static boolean_t /* ARGSUSED */
encode_number(uint8_t n_entries, uint8_t size, boolean_t is_signed,
    uint8_t granularity, const char *from, uint8_t *to)
{
	uint8_t		i;
	uint16_t	uint16;
	uint32_t	uint32;
	uint64_t	uint64;

	if (granularity != 0) {
		if ((granularity % n_entries) != 0) {
			inittab_msg("encode_number: number of entries "
			    "not compatible with option granularity");
			return (B_FALSE);
		}
	}

	for (i = 0; i < n_entries; i++, from++) {

		/*
		 * totally obscure c factoid: it is legal to pass a
		 * string representing a negative number to strtoul().
		 * in this case, strtoul() will return an unsigned
		 * long that if cast to a long, would represent the
		 * negative number.  we take advantage of this to
		 * cut down on code here.
		 */

		switch (size) {

		case 1:
			to[i] = strtoul(from, 0, 0);
			break;

		case 2:
			uint16 = htons(strtoul(from, 0, 0));
			(void) memcpy(to + (i * 2), &uint16, 2);
			break;

		case 4:
			uint32 = htonl(strtoul(from, 0, 0));
			(void) memcpy(to + (i * 4), &uint32, 4);
			break;

		case 8:
			uint64 = dhcp_htonll(strtoull(from, 0, 0));
			(void) memcpy(to + (i * 8), &uint64, 8);
			break;

		default:
			inittab_msg("encode_number: unknown integer "
			    "size `%d'", size);
			return (B_FALSE);
		}

		from = strchr(from, ' ');
		if (from == NULL)
			break;
	}

	return (B_TRUE);
}

/*
 * ie_type_to_size(): given an inittab entry, returns size of one entry of
 *		      its type
 *
 *   input: inittab_entry_t *: an entry of the given type
 *  output: uint8_t: the size in bytes of an entry of that type
 */

static uint8_t
ie_type_to_size(inittab_entry_t *ie)
{
	switch (ie->ie_type) {

	case ASCII:
	case OCTET:
	case SNUMBER8:
	case UNUMBER8:

		return (1);

	case SNUMBER16:
	case UNUMBER16:

		return (2);

	case SNUMBER32:
	case UNUMBER32:
	case IP:

		return (4);

	case SNUMBER64:
	case UNUMBER64:

		return (8);

	case NUMBER:

		return (ie->ie_gran);
	}

	return (0);
}

/*
 * category_to_code(): maps a category name to its numeric representation
 *
 *   input: const char *: the category name
 *  output: uchar_t: its internal code (numeric representation)
 */

static uchar_t
category_to_code(const char *category)
{
	/*
	 * valid values for the `category' field.  must be updated as
	 * we add new ITAB_CAT_* definitions.  note that the ith
	 * position in this array corresponds with a category using
	 * the (i + 1)th bit.
	 */

	static const char *ife_categories[] = {
		"STANDARD", "FIELD", "INTERNAL", "VENDOR", "SITE", NULL
	};

	unsigned int	i;

	for (i = 0; ife_categories[i] != NULL; i++)
		if (strcasecmp(ife_categories[i], category) == 0)
			return (1 << i);

	return (0);
}

/*
 * dhcp_htonll(): converts a 64-bit number from host to network byte order
 *
 *   input: uint64_t: the number to convert
 *  output: uint64_t: its value in network byte order
 */

static uint64_t
dhcp_htonll(uint64_t uint64_hbo)
{
	return (dhcp_ntohll(uint64_hbo));
}

/*
 * dhcp_ntohll(): converts a 64-bit number from network to host byte order
 *
 *   input: uint64_t: the number to convert
 *  output: uint64_t: its value in host byte order
 */

static uint64_t
dhcp_ntohll(uint64_t uint64_nbo)
{
#ifdef	_LITTLE_ENDIAN
	return ((uint64_t)ntohl(uint64_nbo & 0xffffffff) << 32 |
	    ntohl(uint64_nbo >> 32));
#else
	return (uint64_nbo);
#endif
}

/*
 * our internal table of DHCP option values, used by inittab_verify()
 */

static inittab_entry_t inittab_table[] =
{
{ ITAB_CAT_INTERNAL,	1024,	"Hostname",	BOOL,		0,	0 },
{ ITAB_CAT_INTERNAL,	1025,	"LeaseNeg",	BOOL,		0,	0 },
{ ITAB_CAT_INTERNAL,	1026,	"EchoVC",	BOOL,		0,	0 },
{ ITAB_CAT_INTERNAL,	1027,	"BootPath",	ASCII,		1,	0 },
{ ITAB_CAT_FIELD,	0,	"Opcode",	UNUMBER8,	1,	1 },
{ ITAB_CAT_FIELD,	1,	"Htype",	UNUMBER8,	1,	1 },
{ ITAB_CAT_FIELD,	2,	"HLen",		UNUMBER8,	1,	1 },
{ ITAB_CAT_FIELD,	3,	"Hops",		UNUMBER8,	1,	1 },
{ ITAB_CAT_FIELD,	4,	"Xid",		UNUMBER32,	1,	1 },
{ ITAB_CAT_FIELD,	8,	"Secs",		UNUMBER16,	1,	1 },
{ ITAB_CAT_FIELD,	10,	"Flags",	OCTET,		1,	2 },
{ ITAB_CAT_FIELD,	12,	"Ciaddr",	IP,		1,	1 },
{ ITAB_CAT_FIELD,	16,	"Yiaddr",	IP,		1,	1 },
{ ITAB_CAT_FIELD,	20,	"BootSrvA",	IP,		1,	1 },
{ ITAB_CAT_FIELD,	24,	"Giaddr",	IP,		1,	1 },
{ ITAB_CAT_FIELD,	28,	"Chaddr",	OCTET, 		1,	16 },
{ ITAB_CAT_FIELD,	44,	"BootSrvN",	ASCII,		1,	64 },
{ ITAB_CAT_FIELD,	108,	"BootFile",	ASCII,		1,	128 },
{ ITAB_CAT_FIELD,	236,	"Cookie",	OCTET,		1,	4 },
{ ITAB_CAT_FIELD,	240,	"Options",	OCTET,		1,	60 },
{ ITAB_CAT_STANDARD,	1,	"Subnet",	IP,		1,	1 },
{ ITAB_CAT_STANDARD,	2,	"UTCoffst",	SNUMBER32,	1,	1 },
{ ITAB_CAT_STANDARD,	3,	"Router",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	4,	"Timeserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	5,	"IEN116n",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	6,	"DNSserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	7,	"Logserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	8,	"Cookie",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	9,	"Lprserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	10,	"Impres",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	11,	"Resource",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	12,	"Hostname",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	13,	"Bootsize",	UNUMBER16,	1,	1 },
{ ITAB_CAT_STANDARD,	14,	"Dumpfile",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	15,	"DNSdmain",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	16,	"Swapserv",	IP,		1,	1 },
{ ITAB_CAT_STANDARD,	17,	"Rootpath",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	18,	"ExtendP",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	19,	"IpFwdF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	20,	"NLrouteF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	21,	"PFilter",	IP,		2,	0 },
{ ITAB_CAT_STANDARD,	22,	"MaxIpSiz",	UNUMBER16,	1,	1 },
{ ITAB_CAT_STANDARD,	23,	"IpTTL",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	24,	"PathTO",	UNUMBER32,	1,	1 },
{ ITAB_CAT_STANDARD,	25,	"PathTbl",	UNUMBER16,	1,	0 },
{ ITAB_CAT_STANDARD,	26,	"MTU",		UNUMBER16,	1,	1 },
{ ITAB_CAT_STANDARD,	27,	"SameMtuF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	28,	"Broadcst",	IP,		1,	1 },
{ ITAB_CAT_STANDARD,	29,	"MaskDscF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	30,	"MaskSupF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	31,	"RDiscvyF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	32,	"RSolictS",	IP,		1,	1 },
{ ITAB_CAT_STANDARD,	33,	"StaticRt",	IP,		2,	0 },
{ ITAB_CAT_STANDARD,	34,	"TrailerF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	35,	"ArpTimeO",	UNUMBER32,	1,	1 },
{ ITAB_CAT_STANDARD,	36,	"EthEncap",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	37,	"TcpTTL",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	38,	"TcpKaInt",	UNUMBER32,	1,	1 },
{ ITAB_CAT_STANDARD,	39,	"TcpKaGbF",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	40,	"NISdmain",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	41,	"NISserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	42,	"NTPserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	43,	"Vendor",	OCTET,		1,	0 },
{ ITAB_CAT_STANDARD,	44,	"NetBNm",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	45,	"NetBDst",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	46,	"NetBNdT",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	47,	"NetBScop",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	48,	"XFontSrv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	49,	"XDispMgr",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	50,	"ReqIP",	IP,		1,	1 },
{ ITAB_CAT_STANDARD,	51,	"LeaseTim",	UNUMBER32,	1,	1 },
{ ITAB_CAT_STANDARD,	52,	"OptOvrld",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	53,	"DHCPType",	UNUMBER8,	1,	1 },
{ ITAB_CAT_STANDARD,	54,	"ServerID",	IP,		1,	1 },
{ ITAB_CAT_STANDARD,	55,	"ReqList",	OCTET,		1,	0 },
{ ITAB_CAT_STANDARD,	56,	"Message",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	57,	"DHCP_MTU",	UNUMBER16,	1,	1 },
{ ITAB_CAT_STANDARD,	58,	"T1Time",	UNUMBER32,	1,	1 },
{ ITAB_CAT_STANDARD,	59,	"T2Time",	UNUMBER32,	1,	1 },
{ ITAB_CAT_STANDARD,	60,	"ClassID",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	61,	"ClientID",	OCTET,		1,	0 },
{ ITAB_CAT_STANDARD,	62,	"NW_dmain",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	63,	"NWIPOpt",	OCTET,		1,	128 },
{ ITAB_CAT_STANDARD,	64,	"NIS+dom",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	65,	"NIS+serv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	66,	"TFTPsrvN",	ASCII,		1,	64 },
{ ITAB_CAT_STANDARD,	67,	"OptBootF",	ASCII,		1,	128 },
{ ITAB_CAT_STANDARD,	68,	"MblIPAgt",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	69,	"SMTPserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	70,	"POP3serv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	71,	"NNTPserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	72,	"WWWserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	73,	"Fingersv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	74,	"IRCserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	75,	"STserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	76,	"STDAserv",	IP,		1,	0 },
{ ITAB_CAT_STANDARD,	77,	"UserClas",	ASCII,		1,	0 },
{ ITAB_CAT_STANDARD,	78,	"SLP_DA",	OCTET,		1,	0 },
{ ITAB_CAT_STANDARD,	79,	"SLP_DS",	OCTET,		1,	0 },
{ ITAB_CAT_STANDARD,	82,	"AgentOpt",	OCTET,		1,	0 },
{ ITAB_CAT_STANDARD,	89,	"FQDN",		OCTET,		1,	0 },
{ ITAB_CAT_STANDARD,	93,	"PXEarch",	UNUMBER16,	1,	1 },
{ ITAB_CAT_STANDARD,	94,	"PXEnii",	OCTET,		1,	13 },
{ ITAB_CAT_STANDARD,	95,	"PXEcid",	OCTET,		1,	17 },
{ ITAB_CAT_STANDARD,	107,	"Multicst",	OCTET,		1,	0 },
{ 0,			0,	"",		0,		0,	0 }
};
