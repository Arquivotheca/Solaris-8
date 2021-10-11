/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ICONV_TM_HASH_H
#define	_ICONV_TM_HASH_H

#pragma ident	"@(#)hash.h	1.2	99/05/25 SMI"


#ifdef	__cplusplus
extern "C" {
#endif



static itm_size_t	hash(const char *, itm_size_t, itm_size_t);
static itm_num_t	hash_dense_encoding(const unsigned char *, itm_size_t,
				    const unsigned char *,
				    const unsigned char *);


static itm_size_t
hash(const char *ptr, itm_size_t size, itm_size_t hash_size)
{
	itm_size_t	value;

	value = *(ptr++);
	--size;
	for (; 0 < size; --size) {
		value *= 27239;
		value += *(ptr++);
	}
	return (value % hash_size);
}

static itm_num_t
hash_dense_encoding(
	const unsigned char	*byte_seq,
	itm_size_t		length,
	const unsigned char	*byte_seq_min,
	const unsigned char	*byte_seq_max)
{
	long		i;
	itm_num_t	num;

	num = (*byte_seq - *byte_seq_min);
	byte_seq_min++;
	byte_seq_max++;
	for (i = 1, byte_seq++; i < length;
	    i++, byte_seq++, byte_seq_min++, byte_seq_max++) {
		if ((*byte_seq < *byte_seq_min) ||
		    (*byte_seq_max < *byte_seq)) {
			return (-1);
		}
		num *= (*byte_seq_max - *byte_seq_min + 1);
		num += (*byte_seq - *byte_seq_min);
	}
	return (num);
}

#ifdef	__cplusplus
}
#endif

#endif /* !_ICONV_TM_HASH_H */
