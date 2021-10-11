/*
 *  ap_config_auto.h -- Automatically determined configuration stuff
 *  THIS FILE WAS AUTOMATICALLY GENERATED - DO NOT EDIT!
 */

#ifndef AP_CONFIG_AUTO_H
#define AP_CONFIG_AUTO_H

/* check: #include <dlfcn.h> */
#ifndef HAVE_DLFCN_H
#define HAVE_DLFCN_H 1
#endif

/* check: #include <dl.h> */
#ifdef HAVE_DL_H
#undef HAVE_DL_H
#endif

/* check: #include <bstring.h> */
#ifdef HAVE_BSTRING_H
#undef HAVE_BSTRING_H
#endif

/* check: #include <crypt.h> */
#ifndef HAVE_CRYPT_H
#define HAVE_CRYPT_H 1
#endif

/* check: #include <unistd.h> */
#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H 1
#endif

/* check: #include <sys/resource.h> */
#ifndef HAVE_SYS_RESOURCE_H
#define HAVE_SYS_RESOURCE_H 1
#endif

/* check: #include <sys/select.h> */
#ifndef HAVE_SYS_SELECT_H
#define HAVE_SYS_SELECT_H 1
#endif

/* check: #include <sys/processor.h> */
#ifndef HAVE_SYS_PROCESSOR_H
#define HAVE_SYS_PROCESSOR_H 1
#endif

/* determine: longest possible integer type */
#ifndef AP_LONGEST_LONG
#define AP_LONGEST_LONG long long
#endif

/* determine: byte order of machine (12: little endian, 21: big endian) */
#ifndef AP_BYTE_ORDER
#define AP_BYTE_ORDER 21
#endif

/* determine: is off_t a quad */
#ifndef AP_OFF_T_IS_QUAD
#undef AP_OFF_T_IS_QUAD
#endif

/* determine: is void * a quad */
#ifndef AP_VOID_P_IS_QUAD
#undef AP_VOID_P_IS_QUAD
#endif

/* build flag: -DSOLARIS2=280 */
#ifndef SOLARIS2
#define SOLARIS2 280
#endif

/* build flag: -DMOD_PERL */
#ifndef MOD_PERL
#define MOD_PERL 1
#endif

/* build flag: -DUSE_EXPAT */
#ifndef USE_EXPAT
#define USE_EXPAT 1
#endif

#endif /* AP_CONFIG_AUTO_H */
