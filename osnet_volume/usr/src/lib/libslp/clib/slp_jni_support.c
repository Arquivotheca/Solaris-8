/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)slp_jni_support.c	1.1	99/04/02 SMI"

/*
 * This file contains native methods for the Java SLP implementation.
 * So far this is just the syslog function.
 */

#include <jni.h>
#include <syslog.h>

/*
 * Class:     com_sun_slp_Syslog
 * Method:    syslog
 * Signature: (ILjava/lang/String;)V
 */
/* ARGSUSED */
JNIEXPORT
void JNICALL Java_com_sun_slp_Syslog_syslog(JNIEnv *env,
					    jobject obj,
					    jint priority,
					    jstring jmsg) {

	const char *msg = (*env)->GetStringUTFChars(env, jmsg, 0);

	openlog("slpd", LOG_PID, LOG_DAEMON);
	syslog(priority, msg);
	closelog();

	(*env)->ReleaseStringUTFChars(env, jmsg, msg);
}
