/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)service.c	1.9	99/10/21 SMI"

#include <string.h>
#include <malloc.h>
#include <sys/signal.h>
#include <libintl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dhcdata.h>
#include <jni.h>
#include <com_sun_dhcpmgr_bridge_Bridge.h>
#include <exception.h>
#include <dd_misc.h>

#define	DHCPD_FNAME	"in.dhcpd"
#define	DEFAULTS_MODE	0644

/*
 * Retrieve the list of data stores available for DHCP.  Returns an array of
 * DhcpDatastore objects.
 */
/*ARGSUSED*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getDataStores(
    JNIEnv *env,
    jobject obj)
{
	jclass dsclass;
	jmethodID dscons;
	jobjectArray jdsl;
	jobject jobj;
	struct data_store **dsl;
	int i, len;

	/* Make sure we have the classes & methods we need */
	dsclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcpDatastore");
	if (dsclass == NULL) {
		return (NULL);
	}
	dscons = (*env)->GetMethodID(env, dsclass, "<init>",
	    "(ILjava/lang/String;)V");
	if (dscons == NULL) {
		return (NULL);
	}

	/* Get the list */
	dsl = dd_data_stores();
	if (dsl == NULL) {
		throw(env, gettext("Error in dd_data_stores"));
		return (NULL);
	}

	/* Compute the length of the array, store in len */
	ARRAY_LENGTH(dsl, len);

	/* For each store, make an object and add it to the array */
	for (i = 0; i < len; ++i) {
		jobj = (*env)->NewObject(env, dsclass, dscons, dsl[i]->code,
		    (*env)->NewStringUTF(env, dsl[i]->name));
		if (jobj == NULL) {
			dd_free_data_stores(dsl);
			return (NULL);
		}
		if (i == 0) {
			/* First element, construct the array */
			jdsl = (*env)->NewObjectArray(env, len, dsclass, jobj);
			if (jdsl == NULL) {
				dd_free_data_stores(dsl);
				return (NULL);
			}
		} else {
			(*env)->SetObjectArrayElement(env, jdsl, i, jobj);
			if ((*env)->ExceptionOccurred(env) != NULL) {
				dd_free_data_stores(dsl);
				return (NULL);
			}
		}
	}

	dd_free_data_stores(dsl);
	return (jdsl);
}

/*
 * Read the defaults file for DHCP and return its contents as a DhcpdOptions
 * object.
 */
/*ARGSUSED*/
JNIEXPORT jobject JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_readDefaults(
    JNIEnv *env,
    jobject obj)
{
	jclass defclass;
	jmethodID defcons, defset;
	jobject defobj;
	dhcp_defaults_t *defs, *tdefs;

	/* Make sure we have the classes & methods we need */
	defclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcpdOptions");
	if (defclass == NULL) {
		return (NULL);
	}
	defcons = (*env)->GetMethodID(env, defclass, "<init>", "()V");
	if (defcons == NULL) {
		return (NULL);
	}
	defset = (*env)->GetMethodID(env, defclass, "set",
	    "(Ljava/lang/String;Ljava/lang/String;Z)V");
	if (defset == NULL) {
		return (NULL);
	}

	/* Get the data */
	if (read_dhcp_defaults(&defs) != 0) {
		throw(env, strerror(errno));
	} else {
		/* Construct returned options object */
		defobj = (*env)->NewObject(env, defclass, defcons);
		if (defobj == NULL) {
			free_dhcp_defaults(defs);
			return (NULL);
		}

		/* Load the option settings into the options object */
		tdefs = defs;
		for (;;) {
			if (defs->def_type == DHCP_COMMENT) {
				(*env)->CallVoidMethod(env, defobj, defset,
				    (*env)->NewStringUTF(env, defs->def_key),
				    (*env)->NewStringUTF(env, ""), JNI_TRUE);
			} else {
				if (defs->def_key == NULL) {
					break;
				}
				(*env)->CallVoidMethod(env, defobj, defset,
				    (*env)->NewStringUTF(env, defs->def_key),
				    (*env)->NewStringUTF(env, defs->def_value),
				    JNI_FALSE);
			}
			if ((*env)->ExceptionOccurred(env) != NULL) {
				free_dhcp_defaults(tdefs);
				return (NULL);
			}
			++defs;
		}
		free_dhcp_defaults(tdefs);
	}
	return (defobj);
}

/*
 * Write the DHCP defaults file.  Takes a DhcpdOptions object as input
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_writeDefaults(
    JNIEnv *env,
    jobject obj,
    jobject jdefs)
{
	jobjectArray resArray;
	jsize reslen;
	jobject jobj, resobj;
	jclass defclass, resclass;
	jmethodID getkey, getvalue, getall, iscomment;
	dhcp_defaults_t *defs;
	int i;
	jboolean comment;
	const char *tmpstr;

	/* Make sure we can get at the classes we need */
	defclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcpdOptions");
	if (defclass == NULL) {
		return;
	}
	getall = (*env)->GetMethodID(env, defclass, "getAll",
	    "()[Ljava/lang/Object;");
	if (getall == NULL) {
		return;
	}
	resclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcpResource");
	if (resclass == NULL) {
		return;
	}
	getkey = (*env)->GetMethodID(env, resclass, "getKey",
	    "()Ljava/lang/String;");
	getvalue = (*env)->GetMethodID(env, resclass, "getValue",
	    "()Ljava/lang/String;");
	iscomment = (*env)->GetMethodID(env, resclass, "isComment", "()Z");
	if (getkey == NULL || getvalue == NULL || iscomment == NULL) {
		return;
	}

	/* Get the resource array from the defaults object */
	resArray = (*env)->CallObjectMethod(env, jdefs, getall);
	reslen = (*env)->GetArrayLength(env, resArray);
	/* Allocate array to convert into; extra zero'd item to signal end */
	defs = calloc(reslen+1, sizeof (dhcp_defaults_t));
	if (defs == NULL) {
		throw_memory_exception(env);
		return;
	}

	/* Now copy data into local array */
	for (i = 0; i < reslen; ++i) {
		jobj = (*env)->GetObjectArrayElement(env, resArray, i);
		if (jobj == NULL) {
			free_dhcp_defaults(defs);
			return;
		}
		/* Set record type */
		comment = (*env)->CallBooleanMethod(env, jobj, iscomment);
		if (comment == JNI_TRUE) {
			defs[i].def_type = DHCP_COMMENT;
		} else {
			defs[i].def_type = DHCP_KEY;
		}
		/*
		 * Get the key from the object, convert to a char *,
		 * and then duplicate into the defs array so that 
		 * free_dhcp_defaults can be used correctly.
		 * Do the same thing for the value.
		 */
		resobj = (*env)->CallObjectMethod(env, jobj, getkey);
		tmpstr = (*env)->GetStringUTFChars(env, resobj, NULL);
		if (tmpstr == NULL) {
			free_dhcp_defaults(defs);
			throw(env, gettext("Error converting key"));
			return;
		}
		defs[i].def_key = strdup(tmpstr);
		(*env)->ReleaseStringUTFChars(env, resobj, tmpstr);
		if (defs[i].def_key == NULL) {
			/* Out of memory, fail */
			free_dhcp_defaults(defs);
			throw_memory_exception(env);
			return;
		}
		resobj = (*env)->CallObjectMethod(env, jobj, getvalue);
		tmpstr = (*env)->GetStringUTFChars(env, resobj, NULL);
		if (tmpstr == NULL) {
			free_dhcp_defaults(defs);
			throw(env, gettext("Error converting value"));
			return;
		}
		defs[i].def_value = strdup(tmpstr);
		(*env)->ReleaseStringUTFChars(env, resobj, tmpstr);
		if (defs[i].def_value == NULL) {
			/* Out of memory, fail */
			free_dhcp_defaults(defs);
			throw_memory_exception(env);
			return;
		}
	}

	/* Now write the new data */
	if (write_dhcp_defaults(defs, DEFAULTS_MODE) != 0) {
		throw(env, strerror(errno));
	}
	free_dhcp_defaults(defs);
}

/*
 * Remove the DHCP defaults file
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_removeDefaults(
    JNIEnv *env,
    jobject obj)
{
	if (delete_dhcp_defaults() != 0) {
		throw(env, strerror(errno));
	}
}

/*
 * Create the links from the various runlevel directories to the script
 * located in /etc/init.d
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_createLinks(
    JNIEnv *env,
    jobject obj)
{
	int err;

	if ((err = dd_create_links()) != 0) {
		throw(env, strerror(err));
	}
}


/*
 * Delete the links from the various runlevel directories to the script
 * located in /etc/init.d
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_removeLinks(
    JNIEnv *env,
    jobject obj)
{
	int err;

	if ((err = dd_remove_links()) != 0) {
		throw(env, strerror(err));
	}
}

/*
 * Start up the daemon.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_startup(
    JNIEnv *env,
    jobject obj)
{
	int err;

	if ((err = dd_startup(1)) != 0) {
		throw(env, strerror(err));
	}
}

/*
 * Shut down the dameon.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_shutdown(
    JNIEnv *env,
    jobject obj)
{
	int err;

	if ((err = dd_startup(0)) != 0) {
		throw(env, strerror(err));
	}
}

/*
 * Tell the daemon to re-read the dhcptab.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_reload(
    JNIEnv *env,
    jobject obj)
{
	int err;

	if ((err = dd_signal(DHCPD_FNAME, SIGHUP)) != 0) {
		if (err == -1) {
			/* dd_signal couldn't find in.dhcpd running */
			throw_not_running_exception(env);
		} else {
			throw(env, strerror(err));
		}
	}
}


/*
 * Check if the server is running; returns true if so, false if not.
 */
/*ARGSUSED*/
JNIEXPORT jboolean JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_isServerRunning(
    JNIEnv *env,
    jobject obj)
{
	if (dd_getpid(DAEMON_FNAME) != (pid_t)-1) {
		return (JNI_TRUE);
	} else {
		return (JNI_FALSE);
	}
}

/*
 * Check if the server is set to run at startup.
 */
/*ARGSUSED*/
JNIEXPORT jboolean JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_isServerEnabled(
    JNIEnv *env,
    jobject obj)
{

	if (dd_check_links()) {
		return (JNI_TRUE);
	} else {
		return (JNI_FALSE);
	}
}

/*
 * Retrieve the list of interfaces on the system which are candidates for
 * use by the DHCP daemon.  Returns an array of IPInterface objects.
 */
/*ARGSUSED*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getInterfaces(
    JNIEnv *env,
    jobject obj)
{
	jclass ipifclass;
	jmethodID ipifcons;
	jobjectArray jlist = NULL;
	jobject jobj;
	jsize len;
	struct ip_interface **list;
	int i;

	/* Locate the class and constructor we need */
	ipifclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/IPInterface");
	if (ipifclass == NULL) {
		return (NULL);
	}
	ipifcons = (*env)->GetMethodID(env, ipifclass, "<init>",
	    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
	if (ipifcons == NULL) {
		return (NULL);
	}

	/* Retrieve interface list */
	list = dd_get_interfaces();
	if (list == NULL) {
		throw(env, "Error in dd_get_interfaces");
		return (NULL);
	}
	/* Compute length of list */
	ARRAY_LENGTH(list, len);

	/* For each interface, construct an object and add to the array */
	for (i = 0; i < len; ++i) {
		jobj = (*env)->NewObject(env, ipifclass, ipifcons,
		    (*env)->NewStringUTF(env, list[i]->name),
		    (*env)->NewStringUTF(env, inet_ntoa(list[i]->addr)),
		    (*env)->NewStringUTF(env, inet_ntoa(list[i]->mask)));
		if (i == 0) {
			/* First object, create the array */
			jlist = (*env)->NewObjectArray(env, len, ipifclass,
			    jobj);
			if (jlist == NULL) {
				while (i < len) {
					free(list[i]);
					++i;
				}
				free(list);
				return (NULL);
			}
		} else {
			(*env)->SetObjectArrayElement(env, jlist, i, jobj);
			if ((*env)->ExceptionOccurred(env) != NULL) {
				while (i < len) {
					free(list[i]);
					++i;
				}
				free(list);
				return (NULL);
			}
		}
		free(list[i]);

	}

	free(list);
	return (jlist);
}
