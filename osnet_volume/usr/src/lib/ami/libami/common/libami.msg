$ *************************************<+>*************************************
$ *****************************************************************************
$ **
$ **  File:        libami.msg
$ **
$ **  Project:     libami
$ **
$ **  Description:
$ **  -----------
$ **  This file is the source for the message catalog for libami library.
$ **
$ **
$ *****************************************************************************
$ **
$ **     (c) Copyright 1996 Sun Microsystems, Inc.
$ **     All Rights reserved
$ **
$ **
$ *****************************************************************************
$ **
$ **
$ *****************************************************************************
$ *************************************<+>*************************************

$ *****************************************************************************
$
$ ***** NOTE FOR MESSAGE CATALOG TRANSLATORS *****
$
$ There may be three types of messages in this file:
$
$  1. Messages that appear in dialogs or are displayed to the user.
$
$     These messages are the default and they should ALL BE LOCALIZED.
$     Note that these messages do NOT have any identification (see the
$     comments for type 2 and 3 below).
$
$  2. Messages that only appear in the error log file. 
$
$     The localization of these messages is OPTIONAL.  These messages are
$     identified by the following:
$
$  MESSAGES xx-yy IN SET zz WILL ONLY APPEAR IN THE DT ERRORLOG FILE
$
$  3. Messages that should not be localized.
$
$     These messages are identified by the following:
$
$  DO NOT TRANSLATE or CHANGE or LOCALIZE MESSAGES xx-yy from set zz
$
$ ***** END (NOTE FOR MESSAGE CATALOG TRANSLATORS) *****
$
$ ******************************************************************************

$set 2

$ AMI_OK 
1 Success

$ AMI_EBUFSIZE 
2 Invalid buffer size

$ AMI_ENOMEM 
3 Out of memory

$ AMI_BAD_FILE 
4 Bad file format

$ AMI_FILE_NOT_FOUND
5 The named file does not exist or is the null pathname

$ AMI_FILE_IO_ERR 
6 File I/O error

$ AMI_BAD_PASSWD 
7 Bad password

$ AMI_UNKNOWN_USER 
8 Unknown user

$ AMI_ALGORITHM_UNKNOWN 
9 Unsupported algorithm

$ AMI_ASN1_ENCODE_ERR 
10 ASN1 Encoding error

$ AMI_ASN1_DECODE_ERR 
11 ASN1 Decoding error

$ AMI_BAD_KEY 
12 Bad key material

$ AMI_KEYGEN_ERR 
13 Key generation error

$ AMI_ENCRYPT_ERR 
14 Encryption error

$ AMI_DECRYPT_ERR 
15 Decryption error

$ AMI_SIGN_ERR 
16 Signing error

$ AMI_VERIFY_ERR 
17 Verification error

$ AMI_DIGEST_ERROR 
18 Digest error

$ AMI_OUTPUT_FORMAT_ERR 
19 Error during outputting of data

$ AMI_SYSTEM_ERR 
20 General AMI failure

$ AMI_ATTRIBUTE_UNKNOWN 
21 Unsupported attribute type

$ AMI_AMILOGIN_ERR 
22 Unable to amilogin

$ AMI_AMILOGOUT_ERR 
23 Unable to amilogout

$ AMI_NO_SUCH_ENTRY 
24 Entry does not exist

$ AMI_ENTRY_ALREADY_EXISTS 
25 Entry already exists

$ AMI_AMISERV_DECRYPT_ERR 
26 amiserv decryption error

$ AMI_AMISERV_SIGN_ERR 
27 amiserv signing error

$ AMI_USER_DID_NOT_AMILOGIN 
28 User has not done a amilogin

$ AMI_AMISERV_CONNECT 
29 Error connecting to amiserv

$ AMI_KEYPKG_NOT_FOUND 
30 Key package not found

$ AMI_TIME_INVALID 
31 Public key has expired or is not currently valid

$ AMI_UNTRUSTED_PUBLIC_KEY 
32 Issuer not trusted

$ AMI_EPARM 
33 Invalid function parameter

$ AMI_BINARY_TO_RFC1421_ERR 
34 Conversion from BINARY to RFC1421 format failed

$ AMI_RFC1421_TO_BINARY_ERR 
35 Conversion from RFC1421 to BINARY format failed

$ AMI_RANDOM_NUM_ERR 
36 Random number generation error

$ AMI_XFN_ERR 
37 Error reading from or writing to the public repository using XFN

$ AMI_CERT_CHAIN_ERR 
38 Cannot establish certificate chain

$ AMI_RDN_MISSING_EQUAL 
39 Equals missing in printable name representation

$ AMI_AVA_TYPE_MISSING 
40 Attribute type missing in printable name encoding

$ AMI_AVA_VALUE_MISSING 
41 Attribute value missing in printable name encoding

$ AMI_CERT_NOT_FOUND 
42 Certificate not found

$ AMI_DN_NOT_FOUND 
43 Distinguished Name not found

$ AMI_CRITICAL_EXTNS_ERR 
44 Certificate has critical extensions

$ AMI_ASN1_INIT_ERROR 
45 Cannot initialize compiler-generated encoder/decoder data

$ AMI_WRAP_ERROR 
46 Cannot wrap session key

$ AMI_UNWRAP_ERROR 
47 Cannot unwrap session key

$ AMI_UNSUPPORTED_KEY_TYPE 
48 Unsupported key type

$ AMI_DH_PART1_ERR 
49 Cannot complete Diffie-Hellman Phase 1

$ AMI_DH_PART2_ERR 
50 Cannot complete Diffie-Hellman Phase 2

$ AMI_DOUBLE_ENCRYPT 
51 Cannot double encrypt data

$ AMI_AMISERV_KEYPKG_UPDATE 
52 Unable to update key package with amiserv

$ AMI_AMISERV_STAT_ERR 
53 Unable to stat private key registered with amiserv

$ AMI_EXPORT_ERR
54 Export restriction violated

$ AMI_TRUSTED_KEY_EXPIRED
55 Trusted public key in key package expired

$ AMI_OPEN_ERR
56 Cannot open file

$ Unknown Error
100 Unknown Error
