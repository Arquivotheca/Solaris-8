#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)dhcpmgr.spec	1.1	99/03/19 SMI"
#
# lib/libdhcpmgr/spec/dhcpmgr.spec

function	Java_com_sun_dhcpmgr_bridge_Bridge_createDhcpClientRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_createDhcpClientRecord( \
		    JNIEnv *env, jobject obj, jobject jrec, jstring jtable)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_createDhcpNetwork
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_createDhcpNetwork( \
		    JNIEnv *env, jobject obj, jstring jnet)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_createDhcptab
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_createDhcptab( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_createDhcptabRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_createDhcptabRecord( \
		    JNIEnv *env, jobject obj, jobject jrec)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_createHostsRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_createHostsRecord( \
		    JNIEnv *env, jobject obj, jstring jaddr, jstring jname, \
		    jstring jcomment)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_createLinks
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_createLinks( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcpClientRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcpClientRecord( \
		    JNIEnv *env, jobject obj, jobjoect jrec, jstring jtable)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcpNetwork
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcpNetwork( \
		    JNIEnv *env, jobject obj, jstring jnet)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcptab
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcptab( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcptabRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_deleteDhcptabRecord( \
		    JNIENV *env, jobject obj, jobject jrec)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_deleteHostsRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_deleteHostsRecord( \
		    JNIEnv *env, jobject obj, jstring jaddr)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getDataStores
include		<jni.h>
declaration	JNIEXPORT jobjectArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getDataStores( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getDhcptabRecord
include		<jni.h>
declaration	JNIEXPORT jobject JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getDhcptabRecord( \
		    JNIEnv *env, jobject obj, jstring key, jstring flag)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getIPOption
include		<jni.h>
declaration	JNIEXPORT jobjectArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getIPOption( \
		    JNIEnv *env, jobject obj), jshort code, jstring jarg)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getInterfaces
include		<jni.h>
declaration	JNIEXPORT jobjectArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getInterfaces( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getMacros
include		<jni.h>
declaration	JNIEXPORT jobjectArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getMacros( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getNetworks
include		<jni.h>
declaration	JNIEXPORT jobjectArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getNetworks( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getNumberOption
include		<jni.h>
declaration	JNIEXPORT jlongArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getNumberOption( \
		    JNIEnv *env, jobject obj, jshort code, jstring jarg)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getOptions
include		<jni.h>
declaration	JNIEXPORT jobjectArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getOptions( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_getStringOption
include		<jni.h>
declaration	JNIEXPORT jstring JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_getStringOption( \
		    JNIEnv *env, jobject obj, jshort code, jstring jarg)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_isServerEnabled
include		<jni.h>
declaration	JNIEXPORT jboolean JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_isServerEnabled( \
			JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_isServerRunning
include		<jni.h>
declaration	JNIEXPORT jboolean JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_isServerRunning( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_loadNetwork
include		<jni.h>
declaration	JNIEXPORT jobjectArray JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_loadNetwork( \
		    JNIEnv *env, jobject obj, jstring net)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_modifyDhcpClientRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_modifyDhcpClientRecord( \
		    JNIEnv *env, jobject obj, jobject joldrec, \
		    jobject jnewrec, jstring jtable)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_modifyDhcptabRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_modifyDhcptabRecord( \
		    JNIEnv *env, jobject obj, jobject joldrec, jobject jnewrec)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_modifyHostsRecord
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_modifyHostsRecord( \
		    JNIEnv *env, jobject obj, jstring joldaddr, \
		    jstring jnewaddr, jstring jname, jstring jcomment)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_readDefaults
include		<jni.h>
declaration	JNIEXPORT jobject JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_readDefaults( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_readOptions
include		<jni.h>
declaration	JNIEXPORT jobject JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_readOptions( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_reload
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_reload( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_removeDefaults
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_removeDefaults( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_removeLinks
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_removeLinks( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_shutdown
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_shutdown( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_startup
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_startup( \
		    JNIEnv *env, jobject obj)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_writeDefaults
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_writeDefaults( \
		    JNIEnv *env, jobject obj, jobject jdefs)
version		SUNWprivate_1.1
end

function	Java_com_sun_dhcpmgr_bridge_Bridge_writeOptions
include		<jni.h>
declaration	JNIEXPORT void JNICALL \
		    Java_com_sun_dhcpmgr_bridge_Bridge_writeOptions( \
		    JNIEnv *env, jobject obj, jobject jopts)
version		SUNWprivate_1.1
end

