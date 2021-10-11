/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)java_sign.c	1.1 99/07/11 SMI"

/*
 * This is the "C" wrapper functions file for AMI sign method.
 */

#include <jni.h>
#include <security/ami.h>
#include <ami_local.h>
#include "com_sun_ami_sign_AMI_0005fSignature.h"
#include <malloc.h>
#include "ami_proto.h"

/*
 * Sign the data: Since java code does digest and encode
 * the following code encrypts with RSA keys
 */
JNIEXPORT void JNICALL
Java_com_sun_ami_sign_AMI_1Signature_ami_1rsa_1sign(JNIEnv * env,
    jobject obj,
    jbyteArray toBeSigned,
    jbyteArray key,
    jstring sigAlg,
    jstring keyAlg,
    jint keyLen)
{
	/* AMI variables */
	int ret_val;
	ami_handle_t amih = 0;

	/* Signature and length to be returned */
	uchar_t *sigValue = 0;
	uint sigLen = 0;

	/* Key and signature algo to RSA */
	const ami_algid *c_keyAlg = AMI_RSA_ENCR_AID;
	const ami_algid *c_sigAlg = AMI_RSA_ENCR_AID;

	/* Exception message */
	char msg[50];

	/* Java varibales */
	jfieldID fid;
	jbyteArray jarray;
	jbyte *body;
	uchar_t *c_key;
	size_t c_keyLen;
	uchar_t *c_toBeSigned;
	size_t c_toBeSignedLen;

	/* Obtain the java class */
	jclass cls = (*env)->GetObjectClass(env, obj);


	/*
	 * If key is not provided throw exception
	 */
	if ((c_keyLen = (size_t) keyLen) == 0) {
		/* Throw key not found exception */
		jclass newExcCls = (*env)->FindClass(env,
		    "com/sun/ami/sign/AMI_SignatureException");
		ami_end(amih);
		if (newExcCls == 0) {
			/* Unable to find the new exception class */
			return;
		}
		sprintf(msg, "key to sign not provided");
		(*env)->ThrowNew(env, newExcCls, msg);
		return;
	}
	c_key = (uchar_t *) (*env)->GetByteArrayElements(
	    env, key, 0);
	c_keyAlg = AMI_RSA_ENCR_AID;

	/*
	 * If data to sign not provided throw exception
	 */
	if (toBeSigned == 0) {
		/* Throw key not found exception */
		jclass newExcCls = (*env)->FindClass(env,
		    "com/sun/ami/sign/AMI_SignatureException");
		ami_end(amih);
		if (newExcCls == 0) {
			/* Unable to find the new exception class */
			return;
		}
		sprintf(msg, "Data to sign not provided");
		(*env)->ThrowNew(env, newExcCls, msg);
		return;
	}
	c_toBeSigned = (uchar_t *)
	    (*env)->GetByteArrayElements(env, toBeSigned, 0);
	c_toBeSignedLen = (size_t) (*env)->GetArrayLength(env,
	    toBeSigned);

	/* Invoke the ami methods: ami_init and ami_sign */
	if (((ret_val = __ami_init(&amih, NULL, NULL, 0, 0, NULL))
	    != AMI_OK) ||
	    (ret_val = __ami_sign1(amih, c_toBeSigned, c_toBeSignedLen,
	    c_keyAlg, c_key, c_keyLen, c_sigAlg, &sigValue,
	    &sigLen)) != AMI_OK) {
		jclass newExcCls = (*env)->FindClass(env,
		    "com/sun/ami/sign/AMI_SignatureException");
		ami_end(amih);
		if (newExcCls == 0) {
			/* Unable to find the new exception class */
			return;
		}
		sprintf(msg, "Unable to Sign the data: %d", ret_val);
		(*env)->ThrowNew(env, newExcCls, msg);
		return;
	}
	ami_end(amih);

	/*
	 * Signature was successful .. Set the signature back in
	 * the calling class
	 */

	/* Set Signature */
	fid = (*env)->GetFieldID(env, cls, "_sign", "[B");
	if (fid == 0)
		return;
	jarray = (*env)->NewByteArray(env, sigLen);
	body = (*env)->GetByteArrayElements(env, jarray, 0);
	memcpy(body, sigValue, sigLen);
	(*env)->SetObjectField(env, obj, fid, jarray);
	(*env)->ReleaseByteArrayElements(env, jarray, body, 0);

	/* Release bytes for toBeSignedData and key */
	(*env)->ReleaseByteArrayElements(env, key, (jbyte *) c_key, 0);
	(*env)->ReleaseByteArrayElements(env, toBeSigned,
	    (jbyte *) c_toBeSigned, 0);

	/*
	 * Release the memory allocated in the ami API call
	 */
	free(sigValue);
}
