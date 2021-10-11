#pragma ident	"@(#)DoPrinterNS.c	1.1	99/03/16 SMI"
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* LINTLIBRARY */

#include <stdio.h>
#include <strings.h>
#include <libintl.h>
#include "com_sun_admin_pm_server_DoPrinterNS.h"

jstring	glob_jstdout = NULL;
jstring glob_jstderr = NULL;

extern char glob_stdout[];
extern char glob_stderr[];

void set_stderr(JNIEnv *env);
void set_stdout(JNIEnv *env);
extern _dorexec(
	const char *host,
	const char *user,
	const char *passwd,
	const char *cmd,
	const char *locale);

extern _updateoldyp(
	const char *action,
	const char *printername,
	const char *printserver,
	const char *extensions,
	const char *comment,
	const char *isdefault);

JNIEXPORT jint JNICALL
Java_com_sun_admin_pm_server_DoPrinterNS_dorexec(
	JNIEnv *env,
	jclass class,
	jstring jhost,
	jstring juser,
	jstring jpasswd,
	jstring jcmd,
	jstring jlocale)
{
	const char *host;
	const char *user;
	const char *passwd;
	const char *cmd;
	const char *locale;

	int ret = 0;
	jstring empty = (*env)->NewStringUTF(env, "");
	glob_jstdout = (*env)->NewGlobalRef(env, empty);
	glob_jstderr = (*env)->NewGlobalRef(env, empty);

	host = (*env)->GetStringUTFChars(env, jhost, 0);
	user = (*env)->GetStringUTFChars(env, juser, 0);
	passwd = (*env)->GetStringUTFChars(env, jpasswd, 0);
	cmd = (*env)->GetStringUTFChars(env, jcmd, 0);
	locale = (*env)->GetStringUTFChars(env, jlocale, 0);

	ret = _dorexec(host, user, passwd, cmd, locale);

	if (ret != 0) {
		(void) set_stderr(env);
		return (ret);
	}
	(void) set_stderr(env);
	(void) set_stdout(env);
	return (ret);
}

JNIEXPORT jstring JNICALL
Java_com_sun_admin_pm_server_DoPrinterNS_getstderr(JNIEnv *env, jclass class)
{
	return (glob_jstderr);
}

JNIEXPORT jstring JNICALL
Java_com_sun_admin_pm_server_DoPrinterNS_getstdout(JNIEnv *env, jclass class)
{
	return (glob_jstdout);
}

void
set_stderr(JNIEnv *env)
{
	static jstring jerrstr;
	static char errbuf[BUFSIZ];

	if (*glob_stderr == '\0') {
		(void) strcpy(errbuf, "");
	} else {
		(void) strcpy(errbuf, glob_stderr);
	}

	jerrstr = (*env)->NewStringUTF(env, "");
	if (glob_jstderr != NULL) {
		(*env)->DeleteGlobalRef(env, glob_jstderr);
	}

	jerrstr = (*env)->NewStringUTF(env, errbuf);
	glob_jstderr = (*env)->NewGlobalRef(env, jerrstr);
}

void
set_stdout(JNIEnv *env)
{
	static jstring joutstr;
	static char outbuf[BUFSIZ];

	if (*glob_stdout == '\0') {
		(void) strcpy(outbuf, "");
	} else {
		(void) strcpy(outbuf, glob_stdout);
	}

	joutstr = (*env)->NewStringUTF(env, "");
	if (glob_jstdout != NULL) {
		(*env)->DeleteGlobalRef(env, glob_jstdout);
	}

	joutstr = (*env)->NewStringUTF(env, outbuf);
	glob_jstdout = (*env)->NewGlobalRef(env, joutstr);
}

JNIEXPORT jint JNICALL
Java_com_sun_admin_pm_server_DoPrinterNS_updateoldyp(
	JNIEnv *env,
	jclass class,
	jstring jaction,
	jstring jprintername,
	jstring jprintserver,
	jstring jextensions,
	jstring jcomment,
	jstring jisdefault)
{
	const char *action = NULL;
	const char *printername = NULL;
	const char *printserver = NULL;
	const char *extensions = NULL;
	const char *comment = NULL;
	const char *isdefault = NULL;

	int ret = 0;
	jstring empty = (*env)->NewStringUTF(env, "");
	glob_jstdout = (*env)->NewGlobalRef(env, empty);
	glob_jstderr = (*env)->NewGlobalRef(env, empty);

	action = (*env)->GetStringUTFChars(env, jaction, 0);
	printername = (*env)->GetStringUTFChars(env, jprintername, 0);
	if (jprintserver != NULL) {
		printserver =
			(*env)->GetStringUTFChars(env, jprintserver, 0);
	}
	if (jextensions != NULL) {
		extensions =
			(*env)->GetStringUTFChars(env, jextensions, 0);
	}
	if (jcomment != NULL) {
		comment =
			(*env)->GetStringUTFChars(env, jcomment, 0);
	}
	isdefault = (*env)->GetStringUTFChars(env, jisdefault, 0);

	ret = _updateoldyp(action, printername, printserver,
		extensions, comment, isdefault);

	if (ret != 0) {
		(void) set_stderr(env);
		return (ret);
	}
	(void) set_stderr(env);
	(void) set_stdout(env);
	return (ret);
}
