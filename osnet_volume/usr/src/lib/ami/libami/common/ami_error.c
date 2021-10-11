/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_error.c	1.2 99/07/19 SMI"

#include <thread.h>
#include <synch.h>

#include "ami.h"
#include "ami_local.h"
#include "ami_proto.h"

/*
 * do not forget to call "gencat libami.cat libami.msg"
 * if you add an error code or the message catalog
 */

#define	AMI_I18N	"libami"
#define	AMI_I18N_SET	2

#if !defined(NL_CAT_LOCALE)
#define	NL_CAT_LOCALE 0
#endif

static char *ami_errors[AMI_TOTAL_ERRNUM];

/* Initialization for I18N */
static int i18n_initialized = 0;
static mutex_t i18n_init_lock = DEFAULTMUTEX;

static void
ami_i18n_initialize(nl_catd nlmsg_fd)
{
	if (i18n_initialized)
		return;

	mutex_lock(&i18n_init_lock);
	if (i18n_initialized) {
		mutex_unlock(&i18n_init_lock);
		return;
	}

	ami_errors[0] = catgets(nlmsg_fd, AMI_I18N_SET, 1,
	    "Success"); /* AMI_OK */
	ami_errors[1] = catgets(nlmsg_fd, AMI_I18N_SET, 2,
	    "Invalid buffer size"); /* AMI_EBUFSIZE */
	ami_errors[2] = catgets(nlmsg_fd, AMI_I18N_SET, 3,
	    "Out of memory"); /* AMI_ENOMEM */
	ami_errors[3] = catgets(nlmsg_fd, AMI_I18N_SET, 4,
	    "Bad file format"); /* AMI_BAD_FILE */
	ami_errors[4] = catgets(nlmsg_fd, AMI_I18N_SET, 5,
	    "The named file does not exist or is the null pathname");
	    /* AMI_FILE_NOT_FOUND */
	ami_errors[5] = catgets(nlmsg_fd, AMI_I18N_SET, 6,
	    "File I/O error"); /* AMI_FILE_IO_ERR */
	ami_errors[6] = catgets(nlmsg_fd, AMI_I18N_SET, 7,
	    "Bad password"); /* AMI_BAD_PASSWD */
	ami_errors[7] = catgets(nlmsg_fd, AMI_I18N_SET, 8,
	    "Unknown user"); /* AMI_UNKNOWN_USER */
	ami_errors[8] = catgets(nlmsg_fd, AMI_I18N_SET, 9,
	    "Unsupported algorithm"); /* AMI_ALGORITHM_UNKNOWN */
	ami_errors[9] = catgets(nlmsg_fd, AMI_I18N_SET, 10,
	    "ASN1 Encoding error"); /* AMI_ASN1_ENCODE_ERR */
	ami_errors[10] = catgets(nlmsg_fd, AMI_I18N_SET, 11,
	    "ASN1 Decoding error"); /* AMI_ASN1_DECODE_ERR */
	ami_errors[11] = catgets(nlmsg_fd, AMI_I18N_SET, 12,
	    "Bad key material"); /* AMI_BAD_KEY */
	ami_errors[12] = catgets(nlmsg_fd, AMI_I18N_SET, 13,
	    "Key generation error"); /* AMI_KEYGEN_ERR */
	ami_errors[13] = catgets(nlmsg_fd, AMI_I18N_SET, 14,
	    "Encryption error"); /* AMI_ENCRYPT_ERR */
	ami_errors[14] = catgets(nlmsg_fd, AMI_I18N_SET, 15,
	    "Decryption error"); /* AMI_DECRYPT_ERR */
	ami_errors[15] = catgets(nlmsg_fd, AMI_I18N_SET, 16,
	    "Signing error"); /* AMI_SIGN_ERR */
	ami_errors[16] = catgets(nlmsg_fd, AMI_I18N_SET, 17,
	    "Verification error"); /* AMI_VERIFY_ERR */
	ami_errors[17] = catgets(nlmsg_fd, AMI_I18N_SET, 18,
	    "Digest error"); /* AMI_DIGEST_ERROR */
	ami_errors[18] = catgets(nlmsg_fd, AMI_I18N_SET, 19,
	    "Error during outputting of data"); /* AMI_OUTPUT_FORMAT_ERR */
	ami_errors[19] = catgets(nlmsg_fd, AMI_I18N_SET, 20,
	    "General AMI failure"); /* AMI_SYSTEM_ERR */
	ami_errors[20] = catgets(nlmsg_fd, AMI_I18N_SET, 21,
	    "Unsupported attribute type"); /* AMI_ATTRIBUTE_UNKNOWN */
	ami_errors[21] = catgets(nlmsg_fd, AMI_I18N_SET, 22,
	    "Unable to amilogin"); /* AMI_AMILOGIN_ERR */
	ami_errors[22] = catgets(nlmsg_fd, AMI_I18N_SET, 23,
	    "Unable to amilogout"); /* AMI_AMILOGOUT_ERR */
	ami_errors[23] = catgets(nlmsg_fd, AMI_I18N_SET, 24,
	    "Entry does not exist"); /* AMI_NO_SUCH_ENTRY */
	ami_errors[24] = catgets(nlmsg_fd, AMI_I18N_SET, 25,
	    "Entry already exists"); /* AMI_ENTRY_ALREADY_EXISTS */
	ami_errors[25] = catgets(nlmsg_fd, AMI_I18N_SET, 26,
	    "amiserv decryption error"); /* AMI_AMISERV_DECRYPT_ERR */
	ami_errors[26] = catgets(nlmsg_fd, AMI_I18N_SET, 27,
	    "amiserv signing error"); /* AMI_AMISERV_SIGN_ERR */
	ami_errors[27] = catgets(nlmsg_fd, AMI_I18N_SET, 28,
	    "User has not done a amilogin"); /* AMI_USER_DID_NOT_AMILOGIN */
	ami_errors[28] = catgets(nlmsg_fd, AMI_I18N_SET, 29,
	    "Error connecting to amiserv"); /* AMI_AMISERV_CONNECT */
	ami_errors[29] = catgets(nlmsg_fd, AMI_I18N_SET, 30,
	    "Key package not found"); /* AMI_KEYPKG_NOT_FOUND */
	ami_errors[30] = catgets(nlmsg_fd, AMI_I18N_SET, 31,
	    "Public key has expired or is not currently valid");
	    /* AMI_TIME_INVALID */
	ami_errors[31] = catgets(nlmsg_fd, AMI_I18N_SET, 32,
	    "Issuer not trusted"); /* AMI_UNTRUSTED_PUBLIC_KEY */
	ami_errors[32] = catgets(nlmsg_fd, AMI_I18N_SET, 33,
	    "Invalid function parameter"); /* AMI_EPARM */
	ami_errors[33] = catgets(nlmsg_fd, AMI_I18N_SET, 34,
	    "Conversion from BINARY to RFC1421 format failed");
	    /* AMI_BINARY_TO_RFC1421_ERR */
	ami_errors[34] = catgets(nlmsg_fd, AMI_I18N_SET, 35,
	    "Conversion from RFC1421 to BINARY format failed");
	    /* AMI_RFC1421_TO_BINARY_ERR */
	ami_errors[35] = catgets(nlmsg_fd, AMI_I18N_SET, 36,
	    "Random number generation error");
	    /* AMI_RANDOM_NUM_ERR */
	ami_errors[36] = catgets(nlmsg_fd, AMI_I18N_SET, 37,
	    "Error reading from or writing to the public repository using XFN");
	    /* AMI_XFN_ERR */
	ami_errors[37] = catgets(nlmsg_fd, AMI_I18N_SET, 38,
	    "Cannot establish certificate chain");
	    /* AMI_CERT_CHAIN_ERR */
	ami_errors[38] = catgets(nlmsg_fd, AMI_I18N_SET, 39,
	    "Equals missing in printable name representation");
	    /* AMI_RDN_MISSING_EQUAL */
	ami_errors[39] = catgets(nlmsg_fd, AMI_I18N_SET, 40,
	    "Attribute type missing in printable name encoding");
	    /* AMI_AVA_TYPE_MISSING */
	ami_errors[40] = catgets(nlmsg_fd, AMI_I18N_SET, 41,
	    "Attribute value missing in printable name encoding");
	    /* AMI_AVA_VALUE_MISSING */
	ami_errors[41] = catgets(nlmsg_fd, AMI_I18N_SET, 42,
	    "Certificate not found"); /* AMI_CERT_NOT_FOUND */
	ami_errors[42] = catgets(nlmsg_fd, AMI_I18N_SET, 43,
	    "Distinguished Name not found"); /* AMI_DN_NOT_FOUND */
	ami_errors[43] = catgets(nlmsg_fd, AMI_I18N_SET, 44,
	    "Certificate has critical extensions");
	    /* AMI_CRITICAL_EXTNS_ERR */
	ami_errors[44] = catgets(nlmsg_fd, AMI_I18N_SET, 45,
	    "Cannot initialize compiler-generated encoder/decoder data");
	    /* AMI_ASN1_INIT_ERROR */
	ami_errors[45] = catgets(nlmsg_fd, AMI_I18N_SET, 46,
	    "Cannot wrap session key"); /* AMI_WRAP_ERROR */
	ami_errors[46] = catgets(nlmsg_fd, AMI_I18N_SET, 47,
	    "Cannot unwrap session key"); /* AMI_UNWRAP_ERROR */
	ami_errors[47] = catgets(nlmsg_fd, AMI_I18N_SET, 48,
	    "Unsupported key type"); /* AMI_UNSUPPORTED_KEY_TYPE */
	ami_errors[48] = catgets(nlmsg_fd, AMI_I18N_SET, 49,
	    "Cannot complete Diffie-Hellman Phase 1");
	    /* AMI_DH_PART1_ERR */
	ami_errors[49] = catgets(nlmsg_fd, AMI_I18N_SET, 50,
	    "Cannot complete Diffie-Hellman Phase 2");
	    /* AMI_DH_PART2_ERR */
	ami_errors[50] = catgets(nlmsg_fd, AMI_I18N_SET, 51,
	    "Cannot double encrypt data");
	    /* AMI_DOUBLE_ENCRYPT */
	ami_errors[51] = catgets(nlmsg_fd, AMI_I18N_SET, 52,
	    "Unable to update key package with amiserv");
	    /* AMI_AMISERV_KEYPKG_UPDATE */
	ami_errors[52] = catgets(nlmsg_fd, AMI_I18N_SET, 53,
	    "Unable to stat private key registered with amiserv");
	    /* AMI_AMISERV_STAT_ERR */
	ami_errors[53] = catgets(nlmsg_fd, AMI_I18N_SET, 54,
	    "Export restriction violated"); /* AMI_GLOBAL_ERR */
	ami_errors[54] = catgets(nlmsg_fd, AMI_I18N_SET, 55,
	    "Trusted public key in key package expired");
	    /* AMI_TRUSTED_KEY_EXPIRED */
	ami_errors[55] = catgets(nlmsg_fd, AMI_I18N_SET, 56,
	    "Cannot open file"); /* AMI_OPEN_ERR */

	i18n_initialized = 1;
	mutex_unlock(&i18n_init_lock);
}

char *
ami_strerror(ami_handle_t amih, const AMI_STATUS err)
{
	char *msg;
	nl_catd nlmsg_fd;

	if (amih && amih->fd == 0)
		amih->fd = catopen(AMI_I18N, NL_CAT_LOCALE);

	nlmsg_fd = ((amih && amih->fd) ? amih->fd : 0);
	ami_i18n_initialize(nlmsg_fd);

	/*
	 * The reason we must pass err + 1 to catgets is because
	 * the message catalog does not accept 0 as a valid errnum.
	 * So the message catalog actually starts with 1 (AMI_OK).
	 *
	 * Also, make sure that "Unknown Error" is #100, or some other
	 * huge number that will not conflict with the regular AMI errors
	 */
	if (err < AMI_TOTAL_ERRNUM)
		msg = ami_errors[err];
	else
		msg = catgets(nlmsg_fd, AMI_I18N_SET, 100, "Unknown Error");

	return (msg);
}
