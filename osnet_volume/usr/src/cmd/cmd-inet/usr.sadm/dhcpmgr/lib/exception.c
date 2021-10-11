/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)exception.c	1.6	99/05/07 SMI"

#include <libintl.h>
#include <jni.h>
#include <exception.h>

#define	BRIDGE_EXCEPTION	"com/sun/dhcpmgr/bridge/BridgeException"
#define	MEMORY_EXCEPTION	"com/sun/dhcpmgr/bridge/MemoryException"
#define	EXISTS_EXCEPTION	"com/sun/dhcpmgr/bridge/ExistsException"
#define	NOENT_EXCEPTION		"com/sun/dhcpmgr/bridge/NoEntryException"
#define	HOST_EXISTS_EXCEPTION	"com/sun/dhcpmgr/bridge/HostExistsException"
#define	HOST_NOENT_EXCEPTION	"com/sun/dhcpmgr/bridge/NoHostsEntryException"
#define	NOT_RUNNING_EXCEPTION	"com/sun/dhcpmgr/bridge/NotRunningException"

/* Throw a generic bridge exception with a specified message */
void
throw(JNIEnv *env, const char *msg)
{
	jclass exceptclass = (*env)->FindClass(env, BRIDGE_EXCEPTION);
	if (exceptclass != NULL) {
		(*env)->ThrowNew(env, exceptclass, msg);
	}
}

/* Throw an out of memory exception */
void
throw_memory_exception(JNIEnv *env)
{
	jclass exceptclass = (*env)->FindClass(env, MEMORY_EXCEPTION);
	if (exceptclass != NULL) {
		(*env)->ThrowNew(env, exceptclass,
		    gettext("Out of memory"));
	}
}

/* Throw an exception indicating record or file exists */
void
throw_exists_exception(JNIEnv *env, const char *obj)
{
	jclass exceptclass = (*env)->FindClass(env, EXISTS_EXCEPTION);
	if (exceptclass != NULL) {
		(*env)->ThrowNew(env, exceptclass, obj);
	}
}

/* Throw an exception indicating record or file does not exist */
void
throw_noent_exception(JNIEnv *env, const char *obj)
{
	jclass exceptclass = (*env)->FindClass(env, NOENT_EXCEPTION);
	if (exceptclass != NULL) {
		(*env)->ThrowNew(env, exceptclass, obj);
	}
}

/* Throw an exception indicating a hosts record exists */
void
throw_host_exists_exception(JNIEnv *env, const char *obj)
{
	jclass exceptclass = (*env)->FindClass(env, HOST_EXISTS_EXCEPTION);
	if (exceptclass != NULL) {
		(*env)->ThrowNew(env, exceptclass, obj);
	}
}

/* Throw an exception indicating hosts record does not exist */
void
throw_host_noent_exception(JNIEnv *env, const char *obj)
{
	jclass exceptclass = (*env)->FindClass(env, HOST_NOENT_EXCEPTION);
	if (exceptclass != NULL) {
		(*env)->ThrowNew(env, exceptclass, obj);
	}
}

/* Throw an exception indicating that the service is not currently running */
void
throw_not_running_exception(JNIEnv *env)
{
	jclass exceptclass = (*env)->FindClass(env, NOT_RUNNING_EXCEPTION);
	if (exceptclass != NULL) {
		(*env)->ThrowNew(env, exceptclass, "");
	}
}
