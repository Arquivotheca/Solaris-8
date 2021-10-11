/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_asn.h	1.1 99/07/11 SMI"

/* AMISERV AUX ROUTINES */

AMI_STATUS
encode_printable_string(char *printableString,
    uchar_t **paramValue, size_t * paramLength);

AMI_STATUS
decode_printable_string(uchar_t * encodedData, size_t length,
    ami_printable_string * pData);

AMI_STATUS
encode_IA5_string(ami_email_addr str,
    uchar_t ** paramValue, size_t * paramLength);

AMI_STATUS
decode_IA5_string(uchar_t * encodedData, size_t length,
    ami_email_addr * pData);

AMI_STATUS
encode_octet_string(ami_octetstring * iv, uchar_t ** paramValue,
    size_t * paramLength);

AMI_STATUS
decode_octet_string(uchar_t * encodedData, size_t length,
    ami_octetstring ** pIv);

AMI_STATUS
encode_digested_data(uchar_t * data, size_t length, uchar_t * algo, uchar_t
    ** paramValue,
    size_t * paramLength);

AMI_STATUS
encode_rc2_params(ami_octetstring * iv, int effectiveKeySize,
    uchar_t ** paramValue, size_t * paramLength);

AMI_STATUS
decode_rc2_params(uchar_t * encodedData, size_t length,
    ami_octetstring ** pIv, int *effectiveKeySize);

AMI_STATUS
encode_certificate(void *certInfo, uchar_t ** paramValue, size_t *paramLength);

AMI_STATUS
ami_random_gen(int length, uchar_t ** random);
