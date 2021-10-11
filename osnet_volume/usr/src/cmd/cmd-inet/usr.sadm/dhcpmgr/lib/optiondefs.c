/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)optiondefs.c	1.5	99/05/11 SMI"

#include <libintl.h>
#include <arpa/inet.h>
#include <jni.h>
#include <com_sun_dhcpmgr_bridge_Bridge.h>
#include <exception.h>
#include <dd_opt.h>

/*
 * Retrieve default value for an option with a string value.  Returns a
 * single String.
 */
/*ARGSUSED*/
JNIEXPORT jstring JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getStringOption(
    JNIEnv *env,
    jobject obj,
    jshort code,
    jstring jarg)
{
	jstring jstr;
	struct dhcp_option *opt;
	ushort_t scode = (ushort_t)code;
	const char *arg;

	/* Get the option whose default value we want to generate. */
	arg = (*env)->GetStringUTFChars(env, jarg, NULL);
	if (arg == NULL) {
		return (NULL);
	}

	/* Get the option data */
	opt = dd_getopt(scode, arg, NULL);
	(*env)->ReleaseStringUTFChars(env, jarg, arg);

	if (opt->error_code != 0) {
		throw(env, opt->u.msg);
		dd_freeopt(opt);
		return (NULL);
	}

	/* Set the return value */
	jstr = (*env)->NewStringUTF(env, opt->u.ret.data.strings[0]);
	dd_freeopt(opt);
	return (jstr);
}

/*
 * Get the default value for an option whose value is one or more IP
 * addresses.  Returns an array of IPAddress objects.
 */
/*ARGSUSED*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getIPOption(
    JNIEnv *env,
    jobject obj,
    jshort code,
    jstring jarg)
{
	jclass ipclass;
	jmethodID ipcons;
	jobjectArray jlist = NULL;
	jobject jaddr;
	jstring jstr;
	struct dhcp_option *opt;
	ushort_t scode = (ushort_t)code;
	int i;
	const char *arg;

	/* Get classes and methods we need */
	ipclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/IPAddress");
	if (ipclass == NULL) {
		return (NULL);
	}
	ipcons = (*env)->GetMethodID(env, ipclass, "<init>",
	    "(Ljava/lang/String;)V");
	if (ipcons == NULL) {
		return (NULL);
	}

	/* Retrieve option to generate value for */
	arg = (*env)->GetStringUTFChars(env, jarg, NULL);
	if (arg == NULL) {
		return (NULL);
	}

	/* Go get the default value */
	opt = dd_getopt(scode, arg, NULL);
	(*env)->ReleaseStringUTFChars(env, jarg, arg);

	if (opt->error_code != 0) {
		throw(env, opt->u.msg);
		dd_freeopt(opt);
		return (NULL);
	}

	/*
	 * For each address returned, make an IPAddress object out of the
	 * in_addr structures returned.
	 */
	for (i = 0; i < opt->u.ret.count; ++i) {
		jstr = (*env)->NewStringUTF(env,
		    inet_ntoa(*opt->u.ret.data.addrs[i]));
		if (jstr == NULL) {
			dd_freeopt(opt);
			return (NULL);
		}
		jaddr = (*env)->NewObject(env, ipclass, ipcons, jstr);
		if ((*env)->ExceptionOccurred(env) != NULL) {
			dd_freeopt(opt);
			return (NULL);
		}
		if (i == 0) {
			/* First object; construct the array */
			jlist = (*env)->NewObjectArray(env, opt->u.ret.count,
			    ipclass, jaddr);
			if (jlist == NULL) {
				dd_freeopt(opt);
				return (NULL);
			}
		} else {
			(*env)->SetObjectArrayElement(env, jlist, i, jaddr);
			if ((*env)->ExceptionOccurred(env) != NULL) {
				dd_freeopt(opt);
				return (NULL);
			}
		}
	}
	dd_freeopt(opt);
	return (jlist);
}

/*
 * Generate the default value for an option whose value is a list of numbers.
 * Returns an array of longs.
 */
/*ARGSUSED*/
JNIEXPORT jlongArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getNumberOption(
    JNIEnv *env,
    jobject obj,
    jshort code,
    jstring jarg)
{
	jlongArray list;
	struct dhcp_option *opt;
	const char *arg;
	ushort_t scode = (ushort_t)code;
	jlong *listel;
	int i;

	/* Get option to retrieve */
	arg = (*env)->GetStringUTFChars(env, jarg, NULL);
	if (arg == NULL) {
		return (NULL);
	}

	opt = dd_getopt(scode, arg, NULL);
	(*env)->ReleaseStringUTFChars(env, jarg, arg);

	if (opt->error_code != 0) {
		throw(env, opt->u.msg);
		dd_freeopt(opt);
		return (NULL);
	}

	/* Allocate return array */
	list = (*env)->NewLongArray(env, opt->u.ret.count);
	if (list == NULL) {
		dd_freeopt(opt);
		return (NULL);
	}

	/* Get access to elements of return array, then copy data in */
	listel = (*env)->GetLongArrayElements(env, list, NULL);
	if (listel == NULL) {
		dd_freeopt(opt);
		return (NULL);
	}

	for (i = 0; i < opt->u.ret.count; ++i) {
		listel[i] = opt->u.ret.data.numbers[i];
	}

	/* Tell VM we're done so it can finish putting data back */
	(*env)->ReleaseLongArrayElements(env, list, listel, 0);

	dd_freeopt(opt);
	return (list);
}
