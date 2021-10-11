/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_asn1.c	1.2 99/07/13 SMI"

#include <stdio.h>
#include "ami.h"
#include "ami_local.h"
#include "ami_asn.h"
#include "ami_proto.h"

/* AMISERV AUX ROUTINES */

AMI_STATUS
encode_printable_string(char *printableString,
    uchar_t **paramValue, size_t * paramLength)
{
	return (__ami_rpc_encode_printable_string(printableString,
	    paramValue, paramLength));
}

AMI_STATUS
decode_printable_string(uchar_t * encodedData, size_t length,
    ami_printable_string * pData)
{
	return (__ami_rpc_decode_printable_string(encodedData, length, pData));
}

AMI_STATUS
encode_IA5_string(ami_email_addr str,
    uchar_t ** paramValue, size_t * paramLength)
{
	return (__ami_rpc_encode_IA5_string(str, paramValue, paramLength));
}

AMI_STATUS
decode_IA5_string(uchar_t * encodedData, size_t length,
    ami_email_addr * pData)
{
	return (__ami_rpc_decode_IA5_string(encodedData, length, pData));
}

AMI_STATUS
encode_octet_string(ami_octetstring * iv, uchar_t ** paramValue,
    size_t * paramLength)
{
	return (__ami_rpc_encode_octect_string(iv, paramValue, paramLength));
}

AMI_STATUS
decode_octet_string(uchar_t * encodedData, size_t length,
    ami_octetstring ** pIv)
{
	return (__ami_rpc_decode_octect_string(encodedData, length, pIv));
}


AMI_STATUS
encode_digested_data(uchar_t * data, size_t length, uchar_t * algo,
    uchar_t ** paramValue, size_t * paramLength)
{
	return (__ami_rpc_encode_digested_data(data, length, algo,
	    paramValue, paramLength));
}

AMI_STATUS
encode_rc2_params(ami_octetstring * iv, int effectiveKeySize,
    uchar_t ** paramValue, size_t * paramLength)
{
	return (__ami_rpc_encode_rc2_params(iv, effectiveKeySize,
		paramValue, paramLength));
}


AMI_STATUS
decode_rc2_params(uchar_t * encodedData, size_t length,
    ami_octetstring ** pIv, int *effectiveKeySize)
{
	return (__ami_rpc_decode_rc2_params(encodedData, length,
		pIv, effectiveKeySize));
}


AMI_STATUS
encode_certificate(void *certInfo, uchar_t ** paramValue, size_t *paramLength)
{
	return (__ami_rpc_encode_certificate(certInfo, paramValue,
	    paramLength));
}

AMI_STATUS
ami_random_gen(int length, uchar_t ** random)
{
	return (__ami_rpc_ami_random_gen(length, random));
}
