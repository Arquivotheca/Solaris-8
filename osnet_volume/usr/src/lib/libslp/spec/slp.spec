#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)slp.spec	1.2	99/10/08 SMI"
#
# lib/libslp/spec/slp.spec

function	SLPOpen
include		<slp.h>
declaration	SLPError SLPOpen(const char *pcLang, \
				 SLPBoolean isAsync, \
				 SLPHandle *phSLP)
version		SUNW_1.1
end

function	SLPClose
include		<slp.h>
declaration	void SLPClose(SLPHandle hSLP)
version		SUNW_1.1
end

function	SLPReg
declaration	SLPError SLPReg(SLPHandle hSLP, \
				const char *pcSrvURL, \
				const unsigned short usLifetime, \
				const char *pcSrvType, \
				const char *pcAttrs, \
				SLPBoolean fresh, \
				SLPRegReport callback, \
				void *pvCookie)
include		<slp.h>
version		SUNW_1.1
end

function	SLPDereg
declaration	SLPError SLPDereg(SLPHandle hSLP, \
				  const char *pcURL, \
				  SLPRegReport callback, \
				  void *pvCookie)
include		<slp.h>
version		SUNW_1.1
end

function	SLPDelAttrs
declaration	SLPError SLPDelAttrs(SLPHandle	hSLP, \
				     const char *pcURL, \
				     const char *pcAttrs, \
				     SLPRegReport callback, \
				     void *pvCookie)
include		<slp.h>
version		SUNW_1.1
end

function	SLPFindSrvTypes
declaration	SLPError SLPFindSrvTypes(SLPHandle hSLP, \
					 const char *pcNamingAuthority, \
					 const char *pcScopeList, \
					 SLPSrvTypeCallback callback, \
					 void *pvCookie)
include		<slp.h>
version		SUNW_1.1
end

function	SLPFindSrvs
declaration	SLPError SLPFindSrvs(SLPHandle hSLP, \
				     const char *pcServiceType, \
				     const char *pcScopeList, \
				     const char *pcSearchFilter, \
				     SLPSrvURLCallback callback, \
				     void *pvCookie)
include		<slp.h>
version		SUNW_1.1
end

function	SLPFindAttrs
declaration	SLPError SLPFindAttrs(SLPHandle hSLP, \
				      const char *pcURL, \
				      const char *pcScopeList, \
				      const char *pcAttrIds, \
				      SLPAttrCallback callback, \
				      void *pvCookie)
include		<slp.h>
version		SUNW_1.1
end

function	SLPGetRefreshInterval
declaration	unsigned short SLPGetRefreshInterval()
include		<slp.h>
version		SUNW_1.1
end

function	SLPFindScopes
declaration	SLPError SLPFindScopes(SLPHandle hSLP, char **ppcScopeList)
include		<slp.h>
version		SUNW_1.1
end

function	SLPParseSrvURL
declaration	SLPError SLPParseSrvURL(char *pcSrvURL, \
					SLPSrvURL **ppSrvURL)
include		<slp.h>
version		SUNW_1.1
end

function	SLPFree
declaration	void SLPFree(void *pvMem)
version		SUNW_1.1
end

function	SLPEscape
declaration	SLPError SLPEscape(const char *pcInbuf, \
				   char **ppcOutBuf, \
				   SLPBoolean isTag)
include		<slp.h>
version		SUNW_1.1
end

function	SLPUnescape
declaration	SLPError SLPUnescape(const char *pcInbuf, \
				     char **ppcOutbuf, \
				     SLPBoolean isTag)
include		<slp.h>
version		SUNW_1.1
end

function	SLPGetProperty
declaration	const char *SLPGetProperty(const char *pcName)
exception	$return == NULL
version		SUNW_1.1
end

function	SLPSetProperty
declaration	void SLPSetProperty(const char *pcName, const char *pcValue)
version		SUNW_1.1
end

function	slp_strerror
declaration	const char *slp_strerror(SLPError err_code)
include		<slp.h>
version		SUNW_1.1
end

function	Java_com_sun_slp_Syslog_syslog
declaration	void Java_com_sun_slp_Syslog_syslog(JNIEnv *env, \
						    jobject jobj, \
						    jint priority, \
						    jstring jmsg)
include		<jni.h>
version		SUNWprivate_1.1
end
