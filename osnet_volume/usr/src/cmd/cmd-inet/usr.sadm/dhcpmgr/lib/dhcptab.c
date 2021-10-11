/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dhcptab.c	1.3	99/05/07 SMI"

#include <libintl.h>
#include <string.h>
#include <malloc.h>
#include <jni.h>
#include <dhcdata.h>
#include <com_sun_dhcpmgr_bridge_Bridge.h>
#include <exception.h>

/*
 * Retrieve a record, either a macro or an option, from the dhcptab.  Returns
 * the record as a new instance of DhcptabRecord
 */
/*ARGSUSED*/
JNIEXPORT jobject JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getDhcptabRecord(
    JNIEnv *env,
    jobject obj,
    jstring key,
    jstring flag)
{
	int ns = TBL_NS_DFLT;
	int tbl_err;
	Tbl tbl = { NULL, 0 };
	int err;
	jclass dtrclass;
	jmethodID dtrcons;
	jobject jobj = NULL;
	const char *skey;
	const char *sflag;

	dtrclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcptabRecord");
	if (dtrclass == NULL) {
		return (jobj);
	}
	dtrcons = (*env)->GetMethodID(env, dtrclass, "<init>",
	    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
	if (dtrcons == NULL) {
		return (jobj);
	}

	skey = (*env)->GetStringUTFChars(env, key, NULL);
	sflag = (*env)->GetStringUTFChars(env, flag, NULL);
	if (skey == NULL || sflag == NULL) {
		throw(env, gettext("Unable to read arguments"));
		return (jobj);
	}

	err = list_dd(TBL_DHCPTAB, ns, NULL, NULL, &tbl_err, &tbl, skey, sflag);
	(*env)->ReleaseStringUTFChars(env, key, skey);
	(*env)->ReleaseStringUTFChars(env, flag, sflag);
	if (err == TBL_SUCCESS) {
		jobj = (*env)->NewObject(env, dtrclass, dtrcons,
		    (*env)->NewStringUTF(env, tbl.ra[0]->ca[0]),
		    (*env)->NewStringUTF(env, tbl.ra[0]->ca[1]),
		    (*env)->NewStringUTF(env, tbl.ra[0]->ca[2]));
	} else {
		throw(env, gettext("Error in list_dd"));
	}

	free_dd(&tbl);
	return (jobj);
}

/*
 * Return all options (aka symbols) currently defined in the dhcptab.
 * The options are returned as an array of Options.
 */
/*ARGSUSED*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getOptions(
    JNIEnv *env,
    jobject obj)
{

	jclass oclass;
	jmethodID ocons;
	jobjectArray jlist = NULL;
	jobject rec;
	int ns = TBL_NS_DFLT;
	int tbl_err;
	Tbl tbl = { NULL, 0 };
	int err;
	int i;

	/* Find the Option class and its constructor */
	oclass = (*env)->FindClass(env,
	    "com/sun/dhcpmgr/data/Option");
	if (oclass == NULL) {
		return (NULL);
	}
	ocons = (*env)->GetMethodID(env, oclass, "<init>",
	    "(Ljava/lang/String;Ljava/lang/String;)V");
	if (ocons == NULL) {
		return (NULL);
	}

	/* Search criteria match all symbols */
	err = list_dd(TBL_DHCPTAB, ns, NULL, NULL, &tbl_err, &tbl, "", "s");
	if (err != TBL_SUCCESS) {
		if (tbl_err == TBL_NO_ENTRY) {
			/* Found none - return a zero-length array */
			jlist = (*env)->NewObjectArray(env, 0, oclass, NULL);
			return (jlist);
		}
		/* Some other error */
		throw(env, gettext("Error in list_dd"));
		return (NULL);
	}
	/*
	 * For each row in the table, construct an Option object and add it to
	 * the array we'll return.
	 */
	for (i = 0; i < tbl.rows; ++i) {
		rec = (*env)->NewObject(env, oclass, ocons,
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[0]),
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[2]));
		if ((*env)->ExceptionOccurred(env) != NULL) {
			free_dd(&tbl);
			return (NULL);
		}
		if (i == 0) {
			/* First record; construct array */
			jlist = (*env)->NewObjectArray(env, tbl.rows, oclass,
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
 * Return all macros defined in the dhcptab.  Returned as an array of
 * Macro objects.
 */
/*ARGSUSED*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_getMacros(
    JNIEnv *env,
    jobject obj)
{

	jclass mclass;
	jmethodID mcons;
	jobjectArray jlist = NULL;
	jobject rec;
	int ns = TBL_NS_DFLT;
	int tbl_err;
	Tbl tbl = { NULL, 0 };
	int err;
	int i;

	/* Locate the Macro class and its constructor */
	mclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/Macro");
	if (mclass == NULL) {
		return (NULL);
	}
	mcons = (*env)->GetMethodID(env, mclass, "<init>",
	    "(Ljava/lang/String;Ljava/lang/String;)V");
	if (mcons == NULL) {
		return (NULL);
	}

	/* Search for all dhcptab records which are macros */
	err = list_dd(TBL_DHCPTAB, ns, NULL, NULL, &tbl_err, &tbl, "", "m");
	if (err != TBL_SUCCESS) {
		if (tbl_err == TBL_NO_ENTRY) {
			/* Found none - return a zero-length array */
			jlist = (*env)->NewObjectArray(env, 0, mclass, NULL);
			return (jlist);
		}
		/* Some other error */
		throw(env, gettext("Error in list_dd"));
		return (NULL);
	}
	/* Each record we found is returned as an element in the array */
	for (i = 0; i < tbl.rows; ++i) {
		rec = (*env)->NewObject(env, mclass, mcons,
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[0]),
		    (*env)->NewStringUTF(env, tbl.ra[i]->ca[2]));
		if ((*env)->ExceptionOccurred(env) != NULL) {
			free_dd(&tbl);
			return (NULL);
		}
		if (i == 0) {
			/* First record; construct the array */
			jlist = (*env)->NewObjectArray(env, tbl.rows, mclass,
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
 * Function to create a new macro
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_createDhcptabRecord(
    JNIEnv *env,
    jobject obj,
    jobject jrec)
{

	int tbl_err, result;
	jclass dtrclass;
	jmethodID dtrgetkey, dtrgetflag, dtrgetvalue;
	jstring jkey, jflag, jvalue;
	const char *key, *flag, *value;
	char *tkey = NULL, *tflag = NULL, *tvalue = NULL;

	/* Locate the class and methods we need */
	dtrclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcptabRecord");
	if (dtrclass == NULL) {
		return;
	}

	dtrgetkey = (*env)->GetMethodID(env, dtrclass, "getKey",
	    "()Ljava/lang/String;");
	if (dtrgetkey == NULL) {
		return;
	}

	dtrgetflag = (*env)->GetMethodID(env, dtrclass, "getFlag",
	    "()Ljava/lang/String;");
	if (dtrgetflag == NULL) {
		return;
	}

	dtrgetvalue = (*env)->GetMethodID(env, dtrclass, "getValue",
	    "()Ljava/lang/String;");
	if (dtrgetvalue == NULL) {
		return;
	}

	/* Pull the fields out of the record passed in */
	jkey = (*env)->CallObjectMethod(env, jrec, dtrgetkey);
	jflag = (*env)->CallObjectMethod(env, jrec, dtrgetflag);
	jvalue = (*env)->CallObjectMethod(env, jrec, dtrgetvalue);

	/* Convert from Java Strings to char *'s */
	key = (*env)->GetStringUTFChars(env, jkey, NULL);
	if (key == NULL) {
		throw(env, gettext("Null key passed"));
		return;
	}

	flag = (*env)->GetStringUTFChars(env, jflag, NULL);
	if (flag == NULL) {
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		throw(env, gettext("Null flag passed"));
		return;
	}

	value = (*env)->GetStringUTFChars(env, jvalue, NULL);
	if (value == NULL) {
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		(*env)->ReleaseStringUTFChars(env, jflag, flag);
		throw(env, gettext("Null value passed"));
		return;
	}

	/*
	 * Have to do these strdup's as add_dd_entry doesn't have const
	 * declarations on its arguments.
	 */
	tkey = strdup(key);
	tflag = strdup(flag);
	tvalue = strdup(value);
	if ((tkey == NULL) || (tflag == NULL) || (tvalue == NULL)) {
		free(tkey);
		free(tflag);
		free(tvalue);
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		(*env)->ReleaseStringUTFChars(env, jflag, flag);
		(*env)->ReleaseStringUTFChars(env, jvalue, value);
		throw_memory_exception(env);
		return;
	}

	/* Now add the record */
	result = add_dd_entry(TBL_DHCPTAB, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    tkey, tflag, tvalue);


	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_ENTRY_EXISTS) {
			throw_exists_exception(env, key);
		} else {
			throw(env, gettext("Error in add_dd_entry"));
		}
	}

	/* Clean up after all those allocations */
	free(tkey);
	free(tflag);
	free(tvalue);

	(*env)->ReleaseStringUTFChars(env, jkey, key);
	(*env)->ReleaseStringUTFChars(env, jflag, flag);
	(*env)->ReleaseStringUTFChars(env, jvalue, value);
}


/*
 * Modify a dhcptab record.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_modifyDhcptabRecord(
    JNIEnv *env,
    jobject obj,
    jobject joldrec,
    jobject jnewrec)
{
	int tbl_err, result;
	jclass dtrclass;
	jmethodID dtrgetkey, dtrgetflag, dtrgetvalue;
	jstring jkey, jflag, jvalue, joldkey;
	const char *key, *flag, *value, *oldkey;
	char *tkey = NULL, *tflag = NULL, *tvalue = NULL, *toldkey = NULL;

	/* Get the class and methods we need */
	dtrclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcptabRecord");
	if (dtrclass == NULL) {
		return;
	}

	dtrgetkey = (*env)->GetMethodID(env, dtrclass, "getKey",
	    "()Ljava/lang/String;");
	if (dtrgetkey == NULL) {
		return;
	}

	dtrgetflag = (*env)->GetMethodID(env, dtrclass, "getFlag",
	    "()Ljava/lang/String;");
	if (dtrgetflag == NULL) {
		return;
	}

	dtrgetvalue = (*env)->GetMethodID(env, dtrclass, "getValue",
	    "()Ljava/lang/String;");
	if (dtrgetvalue == NULL) {
		return;
	}

	/* Now retrieve the field values */
	jkey = (*env)->CallObjectMethod(env, jnewrec, dtrgetkey);
	jflag = (*env)->CallObjectMethod(env, jnewrec, dtrgetflag);
	jvalue = (*env)->CallObjectMethod(env, jnewrec, dtrgetvalue);
	joldkey = (*env)->CallObjectMethod(env, joldrec, dtrgetkey);

	/* Convert the fields from Java Strings to char *'s */
	key = (*env)->GetStringUTFChars(env, jkey, NULL);
	if (key == NULL) {
		throw(env, gettext("Null key passed"));
		return;
	}

	oldkey = (*env)->GetStringUTFChars(env, joldkey, NULL);
	if (oldkey == NULL) {
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		throw(env, gettext("Null old key passed"));
		return;
	}

	flag = (*env)->GetStringUTFChars(env, jflag, NULL);
	if (flag == NULL) {
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		(*env)->ReleaseStringUTFChars(env, joldkey, oldkey);
		throw(env, gettext("Null flag passed"));
		return;
	}

	value = (*env)->GetStringUTFChars(env, jvalue, NULL);
	if (value == NULL) {
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		(*env)->ReleaseStringUTFChars(env, joldkey, oldkey);
		(*env)->ReleaseStringUTFChars(env, jflag, flag);
		throw(env, gettext("Null value passed"));
		return;
	}

	/*
	 * Make working copies needed as mod_dd_entry doesn't declare its
	 * arguments as const.
	 */
	tkey = strdup(key);
	toldkey = strdup(oldkey);
	tflag = strdup(flag);
	tvalue = strdup(value);
	if ((tkey == NULL) || (toldkey == NULL) || (tflag == NULL) ||
	    (tvalue == NULL)) {
		free(tkey);
		free(toldkey);
		free(tflag);
		free(tvalue);
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		(*env)->ReleaseStringUTFChars(env, joldkey, oldkey);
		(*env)->ReleaseStringUTFChars(env, jflag, flag);
		(*env)->ReleaseStringUTFChars(env, jvalue, value);
		throw_memory_exception(env);
		return;
	}

	/* Now do the modify */
	result = mod_dd_entry(TBL_DHCPTAB, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    toldkey, tflag, tkey, tflag, tvalue);

	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_NO_ENTRY) {
			throw_noent_exception(env, oldkey);
		} else if (tbl_err == TBL_ENTRY_EXISTS) {
			throw_exists_exception(env, key);
		} else {
			throw(env, gettext("Error in mod_dd_entry"));
		}
	}

	/* Clean up all the allocations we just did */
	free(tkey);
	free(toldkey);
	free(tflag);
	free(tvalue);

	(*env)->ReleaseStringUTFChars(env, jkey, key);
	(*env)->ReleaseStringUTFChars(env, joldkey, oldkey);
	(*env)->ReleaseStringUTFChars(env, jflag, flag);
	(*env)->ReleaseStringUTFChars(env, jvalue, value);
}

/*
 * Delete a record from the dhcptab
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcptabRecord(
    JNIEnv *env,
    jobject obj,
    jobject jrec)
{
	int tbl_err, result;
	jclass dtrclass;
	jmethodID dtrgetkey, dtrgetflag;
	jstring jkey, jflag;
	const char *key, *flag;
	char *tkey = NULL, *tflag = NULL;

	/* Locate the class and methods we need */
	dtrclass = (*env)->FindClass(env, "com/sun/dhcpmgr/data/DhcptabRecord");
	if (dtrclass == NULL) {
		return;
	}

	dtrgetkey = (*env)->GetMethodID(env, dtrclass, "getKey",
	    "()Ljava/lang/String;");
	if (dtrgetkey == NULL) {
		return;
	}

	dtrgetflag = (*env)->GetMethodID(env, dtrclass, "getFlag",
	    "()Ljava/lang/String;");
	if (dtrgetflag == NULL) {
		return;
	}

	/* Get the field values from the object */
	jkey = (*env)->CallObjectMethod(env, jrec, dtrgetkey);
	jflag = (*env)->CallObjectMethod(env, jrec, dtrgetflag);

	/* Convert them from Java Strings to char *'s */
	key = (*env)->GetStringUTFChars(env, jkey, NULL);
	if (key == NULL) {
		throw(env, gettext("Null key passed"));
		return;
	}

	flag = (*env)->GetStringUTFChars(env, jflag, NULL);
	if (flag == NULL) {
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		throw(env, gettext("Null flag passed"));
		return;
	}

	/*
	 * Make working copies needed because rm_dd_entry's arguments are
	 * not declared as const.
	 */
	tkey = strdup(key);
	tflag = strdup(flag);
	if ((tkey == NULL) || (tflag == NULL)) {
		free(tkey);
		free(tflag);
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		(*env)->ReleaseStringUTFChars(env, jflag, flag);
		throw(env, gettext("Unable to duplicate key or flag"));
		return;
	}

	result = rm_dd_entry(TBL_DHCPTAB, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    tkey, tflag);

	if (result != TBL_SUCCESS) {
		if (tbl_err == TBL_NO_ENTRY) {
			throw_noent_exception(env, key);
		} else {
			throw(env, gettext("Error in rm_dd_entry"));
		}
	}

	/* Clean up the allocations done above */
	free(tkey);
	free(tflag);

	(*env)->ReleaseStringUTFChars(env, jkey, key);
	(*env)->ReleaseStringUTFChars(env, jflag, flag);
}

/*
 * Create the dhcptab.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_createDhcptab(
    JNIEnv *env,
    jobject obj)
{
	int result, tbl_err;

	result = make_dd(TBL_DHCPTAB, TBL_NS_DFLT, NULL, NULL, &tbl_err, NULL,
	    NULL);

	if (result != TBL_SUCCESS) {
		throw(env, gettext("Error creating dhcptab"));
	}
}

/*
 * Delete the dhcptab.
 */
/*ARGSUSED*/
JNIEXPORT void JNICALL
Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcptab(
    JNIEnv *env,
    jobject obj)
{
	int result, tbl_err;

	result = del_dd(TBL_DHCPTAB, TBL_NS_DFLT, NULL, NULL, &tbl_err);

	if (result != TBL_SUCCESS) {
		throw(env, gettext("Error deleting dhcptab"));
	}
}
