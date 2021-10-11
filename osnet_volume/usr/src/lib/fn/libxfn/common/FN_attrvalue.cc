/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_attrvalue.cc	1.6	94/08/03 SMI"

#include <xfn/FN_attrvalue.hh>
#include <string.h>
#include <stdlib.h>

/* test whether two attributes are equal */

FN_attrvalue::FN_attrvalue(const unsigned char *str)
{
	value.length = strlen((const char *)str);
	value.contents = strdup((const char *)str);
}

FN_attrvalue::FN_attrvalue(const unsigned char *str, size_t len)
{
	value.length = len;
	value.contents = strdup((const char *)str);
}

FN_attrvalue::FN_attrvalue(const void *str, size_t len)
{
	value.length = len;
	value.contents = malloc(len);
	memcpy(value.contents, str, len);
}

FN_string *
FN_attrvalue::string() const
{
	return (new FN_string((unsigned char *)value.contents, value.length));
}

int
FN_attrvalue::operator==(const FN_attrvalue &val2) const
{

	if (value.length != val2.length())
	    return (0);

	size_t i;
	unsigned char *buf1 = (unsigned char *)value.contents;
	unsigned char *buf2 = (unsigned char *)val2.contents();

	for (i = 0; i < value.length; i++)
	    if (buf1[i] != buf2[i])
		return (0);

	return (1);
}

int FN_attrvalue::operator!=(const FN_attrvalue &val2) const
{

	if (value.length != val2.length())
	    return (1);

	size_t i;
	unsigned char *buf1 = (unsigned char *)value.contents;
	unsigned char *buf2 = (unsigned char *)val2.contents();

	for (i = 0; i < value.length; i++)
	    if (buf1[i] != buf2[i])
		return (1);

	return (0);
}

FN_attrvalue &
FN_attrvalue::operator=(const FN_attrvalue &val2)
{
	value.length = val2.length();
	if (value.contents)
		free(value.contents);
	value.contents = malloc(value.length);
	memcpy(value.contents, val2.contents(), value.length);

	return (*this);
}

FN_attrvalue::FN_attrvalue(const FN_attrvalue &val2)
{
	value.length = val2.length();
	value.contents = malloc(value.length);
	memcpy(value.contents, val2.contents(), value.length);
}

FN_attrvalue::FN_attrvalue(const FN_attrvalue_t &val2)
{
	value.length = val2.length;
	value.contents = malloc(value.length);
	memcpy(value.contents, val2.contents, value.length);
}

FN_attrvalue::FN_attrvalue(const FN_string &str)
{
	value.length = str.bytecount();
	value.contents = malloc(value.length);
	memcpy(value.contents, str.contents(), value.length);
}

FN_attrvalue::FN_attrvalue()
{
	value.length = 0;
	value.contents = 0;
}

FN_attrvalue::~FN_attrvalue()
{
	if (value.contents)
		free(value.contents);
}
