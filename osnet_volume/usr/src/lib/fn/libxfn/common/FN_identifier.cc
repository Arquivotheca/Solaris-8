/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_identifier.cc	1.7	96/03/31 SMI"

#include <string.h>	/* memcpy */

#include <xfn/FN_identifier.hh>
#include <xfn/FN_status.h>


struct FN_identifier_rep {
	unsigned char	*p_str;
};


void
FN_identifier::common_init(unsigned int format, size_t length,
    const void *contents)
{
	rep = new FN_identifier_rep;
	rep->p_str = 0;
	info.format = format;
	if (length > 0) {
		info.contents = new char[length];
		if (info.contents)
			memcpy(info.contents, contents, length);
	} else
		info.contents = 0;
	info.length = length;
}


FN_identifier::FN_identifier()
{
	common_init(0, 0, 0);
}

// %%% this is non standard

FN_identifier::FN_identifier(const unsigned char *str)
{
	if (str)
		common_init(FN_ID_STRING, strlen((const char *)str), str);
	else
		common_init(FN_ID_STRING, 0, 0);
}

FN_identifier::FN_identifier(unsigned int format, size_t length,
    const void *contents)
{
	common_init(format, length, contents);
}


FN_identifier::FN_identifier(const FN_identifier &rval)
{
	common_init(rval.info.format, rval.info.length, rval.info.contents);
}


FN_identifier::FN_identifier(const FN_identifier_t &rval)
{
	common_init(rval.format, rval.length, rval.contents);
}


FN_identifier::~FN_identifier()
{
	delete rep->p_str;
	delete rep;
	delete[] info.contents;
}

FN_identifier &
FN_identifier::operator=(const FN_identifier &rhs)
{
	// should find a way to avoid "delete rep"
	delete rep->p_str;
	delete rep;
	delete[] info.contents;

	common_init(rhs.info.format, rhs.info.length, rhs.info.contents);
	return (*this);
}


unsigned int
FN_identifier::format() const
{
	return (info.format);
}

size_t
FN_identifier::length() const
{
	return (info.length);
}

const void *
FN_identifier::contents() const
{
	return (info.contents);
}

/*
 * this is a convenience routine routine.
 */

const unsigned char *
FN_identifier::str(unsigned int *status) const
{
	// %%% should check format first
	switch (info.format) {
	case FN_ID_STRING:
	case FN_ID_DCE_UUID:
	case FN_ID_ISO_OID_STRING:
		if (rep->p_str == 0) {
			rep->p_str = new unsigned char[info.length + 1];
			if (rep->p_str == 0) {
				if (status)
					*status = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
			memcpy(rep->p_str, info.contents, info.length);
			rep->p_str[info.length] = '\0';
		}
		if (status)
			*status = FN_SUCCESS;
		return ((const unsigned char *)rep->p_str);
	default:
		if (status)
			*status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	}
}

int
FN_identifier::operator==(const FN_identifier &rhs) const
{
	return (info.format == rhs.info.format &&
		info.length == rhs.info.length &&
		memcmp(info.contents, rhs.info.contents, info.length) == 0);
}
