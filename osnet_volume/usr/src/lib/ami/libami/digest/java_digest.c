/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)java_digest.c	1.3 99/07/16 SMI"

/*
 * This is the "C" wrapper functions file for AMI digest method.
 */

#include <malloc.h>
#include <jni.h>

#include <security/ami.h>
#include <ami_local.h>
#include <ami_proto.h>

#include "com_sun_ami_digest_AMI_0005fDigest.h"

static void
java2ami_digest(JNIEnv * env, jobject obj,
    jbyteArray toBeDigested, jint toBeDigestedLen,
    ami_mechanism digestAlgId)
{
	jfieldID fid;
	int ret_val;
	jbyteArray jarray;
	jbyte *body;
	char msg[50];
	uchar_t *digestValue = 0;
	size_t digestLen = 0;
	ami_handle_t amih;
	jclass cls = (*env)->GetObjectClass(env, obj);

	/*
	 * Set flag value to AMI_END_DATA .. to digest
	 */
	int flag = AMI_END_DATA;

	uchar_t *c_toBeDigested = 0;
	if (toBeDigestedLen > 0)
		c_toBeDigested = (uchar_t *)
		    (*env)->GetByteArrayElements(env,
		    toBeDigested, 0);

	/* Perform digest operation */
	if ((ret_val = __ami_init(&amih, NULL, NULL, 0, 0, NULL) != AMI_OK) ||
	    ((ret_val = ami_digest(amih, c_toBeDigested, toBeDigestedLen,
	    flag, digestAlgId, &digestValue, &digestLen)) != AMI_OK)) {
		jclass newExcCls = (*env)->FindClass(env,
		    "com/sun/ami/digest/AMI_DigestException");
		ami_end(amih);
		if (newExcCls == 0)
			/* Unable to find the new exception class, give up. */
			return;
		sprintf(msg, "Unable to Digest the data  : %d", ret_val);
		(*env)->ThrowNew(env, newExcCls, msg);
		return;
	}
	ami_end(amih);

	/*
	 * Digest was successful ..
	 * Set the digest back in the calling class
	 */
	fid = (*env)->GetFieldID(env, cls, "_digest", "[B");
	if (fid == 0)
		return;

	jarray = (*env)->GetObjectField(env, obj, fid);

	jarray = (*env)->NewByteArray(env, digestLen);
	body = (*env)->GetByteArrayElements(env, jarray, 0);

	memcpy(body, digestValue, digestLen);
	(*env)->SetObjectField(env, obj, fid, jarray);

	(*env)->ReleaseByteArrayElements(env, jarray, body, 0);

	/*
	 * Release the memory for the digest
	 * allocated in the ami API call
	 */
	free(digestValue);
}

/* MD2 digest data  */
JNIEXPORT void JNICALL
Java_com_sun_ami_digest_AMI_1Digest_ami_1md2_1digest(
JNIEnv * env, jobject obj, jbyteArray toBeDigested, jint toBeDigestedLen)
{
	/* Set Digest algo to MD2 */
	java2ami_digest(env, obj, toBeDigested, toBeDigestedLen, AMI_MD2);
}

JNIEXPORT void JNICALL
Java_com_sun_ami_digest_AMI_1Digest_ami_1md5_1digest(
JNIEnv * env, jobject obj, jbyteArray toBeDigested, jint toBeDigestedLen)
{
	/*  Set Digest algo to MD5  */
	java2ami_digest(env, obj, toBeDigested, toBeDigestedLen, AMI_MD5);
}

JNIEXPORT void JNICALL
Java_com_sun_ami_digest_AMI_1Digest_ami_1sha1_1digest(
JNIEnv * env, jobject obj, jbyteArray toBeDigested, jint toBeDigestedLen)
{
	/*  Set Digest algo to SHA1  */
	java2ami_digest(env, obj, toBeDigested, toBeDigestedLen, AMI_SHA1);
}
