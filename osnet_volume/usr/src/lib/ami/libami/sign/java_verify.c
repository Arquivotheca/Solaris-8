/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)java_verify.c	1.2 99/07/16 SMI"

/*
 * This is the "C" wrapper functions file for AMI verify method.
 */

#include <jni.h>
#include <security/ami.h>
#include <ami_local.h>
#include "com_sun_ami_sign_AMI_0005fSignature.h"
#include "ami_proto.h"

/* Sign the data  */
JNIEXPORT jboolean JNICALL
Java_com_sun_ami_sign_AMI_1Signature_ami_1rsa_1verify(
    JNIEnv * env,
    jobject obj,
    jbyteArray signature,
    jbyteArray toBeVerified,
    jbyteArray key,
    jstring sigAlg,
    jstring keyAlg)
{
	int ret_val;
	char msg[50];
	ami_handle_t amih;
	ami_public_key c_publicKey;

	/* Set flag value to AMI_END_DATA .. to verify only */
	int flag = AMI_END_DATA;

	uchar_t *c_toBeVerified = (uchar_t *)
	    (*env)->GetByteArrayElements(env, toBeVerified, 0);
	uchar_t *c_sigValue = (uchar_t *)
	    (*env)->GetByteArrayElements(env, signature, 0);
	uchar_t *c_key = (uchar_t *)
	    (*env)->GetByteArrayElements(env, key, 0);
	size_t c_toBeVerifiedLen = (size_t) (*env)->GetArrayLength(
	    env, toBeVerified);
	size_t c_keyLen = (size_t) (*env)->GetArrayLength(env, key);
	size_t c_sigLen = (size_t) (*env)->GetArrayLength(env, signature);

	/* These are hardcodeded as ami only supports the following algos */
	const char *c_sigAlg = (*env)->GetStringUTFChars(env, sigAlg, 0);
	ami_mechanism sigAlgId;

	if ((strcmp(c_sigAlg, "MD5/RSA") == 0) ||
	    (strcmp(c_sigAlg, "MD5withRSA") == 0))
		sigAlgId = AMI_MD5WithRSAEncryption;
	else if ((strcmp(c_sigAlg, "MD2/RSA") == 0) ||
	    (strcmp(c_sigAlg, "MD2withRSA") == 0))
		/* Set Sign algo to MD2/RSA */
		sigAlgId = AMI_MD2WithRSAEncryption;
	else if ((strcmp(c_sigAlg, "SHA1/RSA") == 0) ||
	    (strcmp(c_sigAlg, "SHA1withRSA") == 0) ||
	    (strcmp(c_sigAlg, "SHA/RSA") == 0) ||
	    (strcmp(c_sigAlg, "SHAwithRSA") == 0))
		/* Set Sign algo to SHA1/RSA */
		sigAlgId = AMI_SHA1WithRSAEncryption;

	/* Invoke the ami method */
	__ami_init(&amih, NULL, NULL, 0, 0, NULL);
	c_publicKey.key = c_key;
	c_publicKey.length = c_keyLen;
	c_publicKey.mech = AMI_RSA;
	if ((ret_val = ami_verify(amih, c_toBeVerified, c_toBeVerifiedLen,
	    flag, AMI_RSA, &c_publicKey, sigAlgId, c_sigValue, c_sigLen))
	    != AMI_OK) {
		ami_end(amih);
		if (ret_val == AMI_VERIFY_ERR)
			return (JNI_FALSE);
		else {
			jclass newExcCls = (*env)->FindClass(env,
			    "com/sun/ami/sign/AMI_SignatureException");
			if (newExcCls == 0) {
				/*
				 * Unable to find the new
				 * exception class, give up.
				 */
				return (JNI_FALSE);
			}
			sprintf(msg,
			    "Unable to Verify the data ! Error :%d", ret_val);
			(*env)->ThrowNew(env, newExcCls, msg);
			return (JNI_FALSE);
		}
	}
	ami_end(amih);

	/* Verification was successful .. */
	return (JNI_TRUE);
}
