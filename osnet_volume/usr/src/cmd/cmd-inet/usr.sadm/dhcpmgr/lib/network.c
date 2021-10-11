/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)network.c	1.4	99/05/07 SMI"

#include <string.h>
#include <dhcdata.h>
#include <libintl.h>
#include <malloc.h>
#include <arpa/inet.h>
#include <jni.h>
#include <com_sun_dhcpmgr_bridge_Bridge.h>
#include <exception.h>

extern int getnetmaskbyaddr(const struct in_addr, struct in_addr *);

/*
 * List the networks currently under DHCP management.  Return as an array
 * of Network objects including the subnet mask for each network.
 */
/*ARGSUSED*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getNetworks(
    JNIEnv *env,
    jobject obj)
{
	jclass netclass;
	jmethodID netcons;
	jobject net;
	jobjectArray jlist = NULL;
	jstring jstr;
	jsize len;
	int ns = TBL_NS_DFLT;
	int tbl_err;
	char **list;
	int i;
	char *cp;
	struct in_addr mask, addr;

	/* Locate the class and methods we need */
	netclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/Network");
	if (netclass == NULL) {
		return (NULL);
	}
	netcons = (*env)->GetMethodID(env, netclass, "<init>",
	    "(Ljava/lang/String;I)V");
	if (netcons == NULL) {
		return (NULL);
	}

	/* Get the list of network tables */
	list = dd_ls(ns, NULL, &tbl_err);
	if (list == NULL) {
		throw(env, gettext("Error in dd_ls"));
		return (NULL);
	}
	/* Compute length of list */
	ARRAY_LENGTH(list, len);
	/*
	 * Convert each table name to a network number and then look up
	 * the associated netmask and then create a Network object from it.
	 */
	for (i = 0; i < len; ++i) {
		/* Transform underscores to periods */
		for (cp = list[i]; *cp != '\0'; ++cp) {
			if (*cp == '_') {
				*cp = '.';
			}
		}
		addr.s_addr = inet_addr(list[i]);
		(void) getnetmaskbyaddr(addr, &mask);
		jstr = (*env)->NewStringUTF(env, list[i]);
		net = (*env)->NewObject(env, netclass, netcons, jstr,
		    htonl(mask.s_addr));
		if (net == NULL) {
			while (i < len) {
				free(list[i]);
				++i;
			}
			free(list);
			return (NULL);
		}

		if (i == 0) {
			/* First element; construct the array */
			jlist = (*env)->NewObjectArray(env, len, netclass,
			    net);
			if (jlist == NULL) {
				while (i < len) {
					free(list[i]);
					++i;
				}
				free(list);
				return (NULL);
			}
		} else {
			(*env)->SetObjectArrayElement(env, jlist, i, net);
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

/*
 * Retrieve all of the records in a particular network table.  Returns an
 * array of DhcpClientRecord.
 */
/*ARGSUSED*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_loadNetwork(
    JNIEnv *env,
    jobject obj,
    jstring net)
{
	jclass dcrclass;
	jmethodID dcrcons;
	jobjectArray jlist = NULL;
	jobject rec;
	int ns = TBL_NS_DFLT;
	int tbl_err;
	Tbl tbl = { NULL, 0 };
	int err;
	int i;
	char *cnet;
	char *comment;

	/* Locate the class and constructor we need */
	dcrclass = (*env)->FindClass(env,
	    "com/sun/dhcpmgr/data/DhcpClientRecord");
	if (dcrclass == NULL) {
		return (NULL);
	}
	dcrcons = (*env)->GetMethodID(env, dcrclass, "<init>",
	    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"\
	    "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"\
	    "Ljava/lang/String;)V");
	if (dcrcons == NULL) {
		return (NULL);
	}

	/* Get table name, and list its contents */
	cnet = (char *)(*env)->GetStringUTFChars(env, net, NULL);

	err = list_dd(TBL_DHCPIP, ns, cnet, NULL, &tbl_err, &tbl, "", "", "",
	    "", "");
	(*env)->ReleaseStringUTFChars(env, net, cnet);

	if (err != TBL_SUCCESS) {
		throw(env, gettext("Error in list_dd"));
		return (NULL);
	}

	/* For each row, construct a client record */
	for (i = 0; i < tbl.rows; ++i) {
		if (tbl.ra[i]->ca[6] == NULL) {
			comment = "";
		} else {
			comment = tbl.ra[i]->ca[6];
		}
		rec = (*env)->NewObject(env, dcrclass, dcrcons,
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[0]),
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[1]),
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[2]),
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[3]),
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[4]),
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[5]),
		    (*env)->NewStringUTF(env, comment));
		if (i == 0) {
			/* First record; construct array */
			jlist = (*env)->NewObjectArray(env, tbl.rows, dcrclass,
			    rec);
			if (jlist == NULL) {
				free_dd(&tbl);
				return (NULL);
			}
		} else {
			(*env)->SetObjectArrayElement(env, jlist, i, rec);
			if ((*env)->ExceptionOccurred(env) != NULL) {
				free_dd(&tbl);
				return (NULL);
			}
		}
	}
	free_dd(&tbl);
	return (jlist);
}

/*
 * Create a client record
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_createDhcpClientRecord(
    JNIEnv *env,
    jobject obj,
    jobject jrec,
    jstring jtable)
{
	int tbl_err, result;
	jclass dcrclass;
	jmethodID dcrmeth;
	jstring jstr;
	const char *str;
	char *methods[] = { "getClientId", "getFlagString",
	    "getClientIPAddress", "getServerIPAddress", "getExpirationTime",
	    "getMacro", "getComment" };
	char *args[sizeof (methods) / sizeof (char *)], *table;
	int i, j;

	/* Retrieve the table argument */
	str = (*env)->GetStringUTFChars(env, jtable, NULL);
	if (str == NULL) {
		throw(env, gettext("Null table passed"));
		return;
	}
	table = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jtable, str);
	if (table == NULL) {
		throw_memory_exception(env);
		return;
	}

	/* Locate the class we need */
	dcrclass = (*env)->FindClass(env,
	    "com/sun/dhcpmgr/data/DhcpClientRecord");
	if (dcrclass == NULL) {
		free(table);
		return;
	}

	/*
	 * For each field, locate the method to retrieve the field
	 * and then stuff it into the args list we'll pass to add_dd_entry
	 */
	for (i = 0; i < sizeof (methods) / sizeof (char *); ++i) {
		dcrmeth = (*env)->GetMethodID(env, dcrclass, methods[i],
		    "()Ljava/lang/String;");
		if (dcrmeth == NULL) {
			for (j = i-1; j >= 0; --j) {
				free(args[j]);
			}
			free(table);
			return;
		}

		jstr = (*env)->CallObjectMethod(env, jrec, dcrmeth);
		/* Convert from Java String to char * */
		str = (*env)->GetStringUTFChars(env, jstr, NULL);
		if (str == NULL) {
			throw(env, gettext("Null argument passed"));
			for (j = i-1; j >= 0; --j) {
				free(args[j]);
			}
			free(table);
			return;
		}
		/* Make a copy; add_dd_entry declares arguments as non-const */
		args[i] = strdup(str);
		(*env)->ReleaseStringUTFChars(env, jstr, str);
		if (args[i] == NULL) {
			throw_memory_exception(env);
			for (j = i-1; j >= 0; --j) {
				free(args[j]);
			}
			free(table);
			return;
		}
	}

	/* Now add the record */
	result = add_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, table, NULL, &tbl_err,
	    args[0], args[1], args[2], args[3], args[4], args[5], args[6]);


	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_ENTRY_EXISTS) {
			throw_exists_exception(env, args[2]);
		} else {
			throw(env, gettext("Error in add_dd_entry"));
		}
	}

	/* Clean up allocated memory */
	for (i = 0; i < sizeof (methods) / sizeof (char *); ++i) {
		free(args[i]);
	}
	free(table);
}

/*
 * Modify a client record.  Supply both old and new record and table in
 * which they're to be modified.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_modifyDhcpClientRecord(
    JNIEnv *env,
    jobject obj,
    jobject joldrec,
    jobject jnewrec,
    jstring jtable)
{
	int tbl_err, result;
	jclass dcrclass;
	jmethodID dcrmeth;
	jstring jstr;
	const char *str;
	char *methods[] = { "getClientId", "getFlagString",
	    "getClientIPAddress", "getServerIPAddress", "getExpirationTime",
	    "getMacro", "getComment" };
	char *args[sizeof (methods) / sizeof (char *)], *table, *oldaddr = NULL;
	int i, j;

	/* Retrieve table to be edited */
	str = (*env)->GetStringUTFChars(env, jtable, NULL);
	if (str == NULL) {
		throw(env, gettext("Null table passed"));
		return;
	}
	/* Make a non-const copy */
	table = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jtable, str);
	if (table == NULL) {
		throw_memory_exception(env);
		return;
	}

	/* Find the class we need */
	dcrclass = (*env)->FindClass(env,
	    "com/sun/dhcpmgr/data/DhcpClientRecord");
	if (dcrclass == NULL) {
		free(table);
		return;
	}

	/* For each field, retrieve and stuff into args list */
	for (i = 0; i < sizeof (methods) / sizeof (char *); ++i) {
		/* Lookup method for this field */
		dcrmeth = (*env)->GetMethodID(env, dcrclass, methods[i],
		    "()Ljava/lang/String;");
		if (dcrmeth == NULL) {
			for (j = i-1; j >= 0; --j) {
				free(args[j]);
			}
			free(table);
			return;
		}

		jstr = (*env)->CallObjectMethod(env, jnewrec, dcrmeth);
		/* Convert from Java String to char */
		str = (*env)->GetStringUTFChars(env, jstr, NULL);
		if (str == NULL) {
			throw(env, gettext("Null argument passed"));
			for (j = i-1; j >= 0; --j) {
				free(args[j]);
			}
			free(table);
			return;
		}
		/* Make a non-const copy and stuff into args array */
		args[i] = strdup(str);
		(*env)->ReleaseStringUTFChars(env, jstr, str);
		if (args[i] == NULL) {
			throw_memory_exception(env);
			for (j = i-1; j >= 0; --j) {
				free(args[j]);
			}
			free(table);
			return;
		}
		if (i == 2) {
			/* Get IP address identifying old record to modify */
			jstr = (*env)->CallObjectMethod(env, joldrec, dcrmeth);
			str = (*env)->GetStringUTFChars(env, jstr, NULL);
			if (str == NULL) {
				throw(env, gettext("Null argument passed"));
				for (j = i; j >= 0; --j) {
					free(args[j]);
				}
				free(table);
				return;
			}
			oldaddr = strdup(str);
			(*env)->ReleaseStringUTFChars(env, jstr, str);
			if (oldaddr == NULL) {
				throw_memory_exception(env);
				for (j = i; j >= 0; --j) {
					free(args[j]);
				}
				free(table);
				return;
			}
		}
	}

	/* Now do the modify */
	result = mod_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, table, NULL, &tbl_err,
	    NULL, oldaddr, NULL, NULL, NULL, args[0], args[1], args[2], args[3],
	    args[4], args[5], args[6]);


	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_ENTRY_EXISTS) {
			throw_exists_exception(env, args[2]);
		} else if (tbl_err == TBL_NO_ENTRY) {
			throw_noent_exception(env, oldaddr);
		} else {
			throw(env, gettext("Error in mod_dd_entry"));
		}
	}

	for (i = 0; i < sizeof (methods) / sizeof (char *); ++i) {
		free(args[i]);
	}
	free(oldaddr);
	free(table);
}

/*
 * Delete a client record
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcpClientRecord(
    JNIEnv *env,
    jobject obj,
    jobject jrec,
    jstring jtable)
{
	int tbl_err, result;
	jclass dcrclass;
	jmethodID dcrmeth;
	jstring jstr;
	const char *str;
	char *addr, *table;

	/* Get table name, convert to C */
	str = (*env)->GetStringUTFChars(env, jtable, NULL);
	if (str == NULL) {
		throw(env, gettext("Null table passed"));
		return;
	}
	table = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jtable, str);
	if (table == NULL) {
		throw_memory_exception(env);
		return;
	}

	/* Find the class and method we need */
	dcrclass = (*env)->FindClass(env,
	    "com/sun/dhcpmgr/data/DhcpClientRecord");
	if (dcrclass == NULL) {
		free(table);
		return;
	}

	dcrmeth = (*env)->GetMethodID(env, dcrclass, "getClientIPAddress",
		"()Ljava/lang/String;");
	if (dcrmeth == NULL) {
		free(table);
		return;
	}

	/* Get the address from the record */
	jstr = (*env)->CallObjectMethod(env, jrec, dcrmeth);
	/* Convert from Java String to char * */
	str = (*env)->GetStringUTFChars(env, jstr, NULL);
	if (str == NULL) {
		throw(env, gettext("Null argument passed"));
		free(table);
		return;
	}
	/* Convert to a non-const version */
	addr = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jstr, str);
	if (addr == NULL) {
		throw_memory_exception(env);
		free(table);
		return;
	}

	/* Delete the record */
	result = rm_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, table, NULL, &tbl_err,
	    NULL, addr, NULL, NULL, NULL);

	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_NO_ENTRY) {
			throw_noent_exception(env, addr);
		} else {
			throw(env, gettext("Error in rm_dd_entry"));
		}
	}

	free(addr);
	free(table);
}

/*
 * Create a hosts record
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_createHostsRecord(
    JNIEnv *env,
    jobject obj,
    jstring jaddr,
    jstring jname,
    jstring jcomment)
{
	const char *str;
	char *taddr, *tname, *tcomment;
	int result, tbl_err;

	/* Retrieve address, name, comment arguments, convert from Java */
	str = (*env)->GetStringUTFChars(env, jaddr, NULL);
	if (str == NULL) {
		return;
	}
	taddr = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jaddr, str);
	if (taddr == NULL) {
		throw_memory_exception(env);
		return;
	}

	str = (*env)->GetStringUTFChars(env, jname, NULL);
	if (str == NULL) {
		free(taddr);
		return;
	}
	tname = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jname, str);
	if (tname == NULL) {
		throw_memory_exception(env);
		free(taddr);
		return;
	}

	str = (*env)->GetStringUTFChars(env, jcomment, NULL);
	if (str == NULL) {
		free(taddr);
		free(tname);
		return;
	}
	tcomment = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jcomment, str);
	if (tcomment == NULL) {
		throw_memory_exception(env);
		free(tname);
		free(taddr);
		return;
	}

	/* Add the record */
	result = add_dd_entry(TBL_HOSTS, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    taddr, tname, NULL, tcomment);

	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_ENTRY_EXISTS) {
			throw_host_exists_exception(env, taddr);
		} else {
			throw(env, gettext("Error in add_dd_entry"));
		}
	}

	free(tcomment);
	free(tname);
	free(taddr);
}

/*
 * Modify a hosts record
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_modifyHostsRecord(
    JNIEnv *env,
    jobject obj,
    jstring joldaddr,
    jstring jnewaddr,
    jstring jname,
    jstring jcomment)
{
	const char *str;
	char *toldaddr, *tnewaddr, *tname, *tcomment;
	int result, tbl_err;

	/* Retrieve each argument, convert from Java, make a non-const copy */
	str = (*env)->GetStringUTFChars(env, joldaddr, NULL);
	if (str == NULL) {
		return;
	}
	toldaddr = strdup(str);
	(*env)->ReleaseStringUTFChars(env, joldaddr, str);
	if (toldaddr == NULL) {
		throw_memory_exception(env);
		return;
	}

	str = (*env)->GetStringUTFChars(env, jnewaddr, NULL);
	if (str == NULL) {
		free(toldaddr);
		return;
	}
	tnewaddr = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jnewaddr, str);
	if (tnewaddr == NULL) {
		throw_memory_exception(env);
		free(toldaddr);
		return;
	}

	str = (*env)->GetStringUTFChars(env, jname, NULL);
	if (str == NULL) {
		free(toldaddr);
		free(tnewaddr);
		return;
	}
	tname = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jname, str);
	if (tname == NULL) {
		throw_memory_exception(env);
		free(tnewaddr);
		free(toldaddr);
		return;
	}

	str = (*env)->GetStringUTFChars(env, jcomment, NULL);
	if (str == NULL) {
		free(toldaddr);
		free(tnewaddr);
		free(tname);
		return;
	}
	tcomment = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jcomment, str);
	if (tcomment == NULL) {
		throw_memory_exception(env);
		free(tname);
		free(tnewaddr);
		free(toldaddr);
		return;
	}

	/* Modify the record */
	result = mod_dd_entry(TBL_HOSTS, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    toldaddr, NULL, tnewaddr, tname, NULL, tcomment);

	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_ENTRY_EXISTS) {
			throw_host_exists_exception(env, tnewaddr);
		} else if (tbl_err == TBL_NO_ENTRY) {
			throw_host_noent_exception(env, toldaddr);
		} else {
			throw(env, gettext("Error in mod_dd_entry"));
		}
	}

	free(tcomment);
	free(tname);
	free(tnewaddr);
	free(toldaddr);
}

/*
 * Delete a hosts record
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_deleteHostsRecord(
    JNIEnv *env,
    jobject obj,
    jstring jaddr)
{
	const char *str;
	char *taddr;
	int result, tbl_err;

	/* Get address, convert from Java to char * */
	str = (*env)->GetStringUTFChars(env, jaddr, NULL);
	if (str == NULL) {
		return;
	}
	/* Make a non-const copy */
	taddr = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jaddr, str);
	if (taddr == NULL) {
		throw_memory_exception(env);
		return;
	}

	/* Delete the record */
	result = rm_dd_entry(TBL_HOSTS, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    taddr, NULL);

	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_NO_ENTRY) {
			throw_host_noent_exception(env, taddr);
		} else {
			throw(env, gettext("Error in rm_dd_entry"));
		}
	}

	free(taddr);
}

/*
 * Create a network table.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_createDhcpNetwork(
    JNIEnv *env,
    jobject obj,
    jstring jnet)
{
	const char *str;
	char *tnet;
	int result, tbl_err;

	/* Get network table name */
	str = (*env)->GetStringUTFChars(env, jnet, NULL);
	if (str == NULL) {
		return;
	}
	/* Make a non-const copy */
	tnet = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jnet, str);
	if (tnet == NULL) {
		throw_memory_exception(env);
		return;
	}


	/* Create the table */
	result = make_dd(TBL_DHCPIP, TBL_NS_DFLT, tnet, NULL, &tbl_err, NULL,
	    NULL);
	free(tnet);

	if (result != TBL_SUCCESS) {
		throw(env, gettext("Error creating network table"));
	}
}

/*
 * Delete a network table.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcpNetwork(
    JNIEnv *env,
    jobject obj,
    jstring jnet)
{
	const char *str;
	char *tnet;
	int result, tbl_err;

	/* Get network table number as a char * */
	str = (*env)->GetStringUTFChars(env, jnet, NULL);
	if (str == NULL) {
		return;
	}
	/* Make a non-const copy */
	tnet = strdup(str);
	(*env)->ReleaseStringUTFChars(env, jnet, str);
	if (tnet == NULL) {
		throw_memory_exception(env);
		return;
	}


	/* Delete the table */
	result = del_dd(TBL_DHCPIP, TBL_NS_DFLT, tnet, NULL, &tbl_err);
	free(tnet);

	if (result != TBL_SUCCESS) {
		throw(env, gettext("Error deleting network table"));
	}
}
