/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _NS_H
#define _NS_H

#pragma ident	"@(#)ns.h	1.11	99/05/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *		Name Service Common Keys/types for lookup
 */
#define NS_KEY_BSDADDR			"bsdaddr"
#define NS_KEY_USE		 	"use"
#define NS_KEY_ALL		 	"all"
#define NS_KEY_GROUP		 	"group"
#define NS_KEY_LIST		 	"list"

#define NS_KEY_PRINTER_TYPE             "printer-type"
#define NS_KEY_DESCRIPTION              "description"

/*
 *		Name Service reserved names for lookup
 */
#define NS_NAME_DEFAULT		"_default"
#define NS_NAME_ALL		"_all"

/*
 *		Name Services supported
 */
#define NS_SVC_USER		"user"
#define NS_SVC_PRINTCAP		"printcap"
#define NS_SVC_ETC		"etc"
#define NS_SVC_NIS		"nis"
#define NS_SVC_NISPLUS		"nisplus"
#define NS_SVC_XFN		"xfn"

/*
 *		Known Protocol Extensions
 */
#define NS_EXT_SOLARIS		"solaris"
#define NS_EXT_GENERIC		"extensions" /* same as SOLARIS */
#define NS_EXT_HPUX		"hpux"
#define NS_EXT_DEC		"dec"

/*
 *	get unique or full list of printer bindings
 */
#define	NOTUNIQUE	0
#define	UNIQUE		1

/*  BSD binding address structure */
struct ns_bsd_addr {
  char	*server;        /* server name */
  char	*printer;       /* printer name or NULL */
  char	*extension;     /* RFC-1179 conformance */
  char  *pname;		/* Local printer name */	
};
typedef struct ns_bsd_addr ns_bsd_addr_t;

/* Key/Value pair structure */
struct ns_kvp {
  char *key;              /* key */
  char *value;            /* value string */
};
typedef struct ns_kvp ns_kvp_t;

/* Printer Object structure */
struct ns_printer {
  char     *name;         /* primary name of printer */
  char     **aliases;     /* aliases for printer */
  char     *source;       /* name service derived from */
  ns_kvp_t **attributes;  /* key/value pairs. */
};
typedef struct ns_printer ns_printer_t ;

/* functions to get/put printer objects */
extern ns_printer_t *ns_printer_create(char *, char **, char *, ns_kvp_t **);
extern ns_printer_t *ns_printer_get_name(const char *, const char *);
extern ns_printer_t **ns_printer_get_list(const char *);
extern int          ns_printer_put(const ns_printer_t *);
extern void         ns_printer_destroy(ns_printer_t *);

extern int setprinterentry(int, char *);
extern int endprinterentry();
extern int getprinterentry(char *, int, char *);
extern int getprinterbyname(char *, char *, int, char *);

extern char *_cvt_printer_to_entry(ns_printer_t *, char *, int);

extern ns_printer_t *_cvt_nss_entry_to_printer(char *, char *);
extern ns_printer_t *posix_name(const char *);



/* functions to manipulate key/value pairs */
extern void         *ns_get_value(const char *, const ns_printer_t *);
extern char         *ns_get_value_string(const char *, const ns_printer_t *);
extern int          ns_set_value(const char *, const void *, ns_printer_t *);
extern int          ns_set_value_from_string(const char *, const char *,
						ns_printer_t *);
extern ns_kvp_t	*ns_kvp_create(const char *, const char *);

/* for BSD bindings only */
extern ns_bsd_addr_t *ns_bsd_addr_get_default();
extern ns_bsd_addr_t *ns_bsd_addr_get_name(char *name);
extern ns_bsd_addr_t **ns_bsd_addr_get_all(int);
extern ns_bsd_addr_t **ns_bsd_addr_get_list(int);

/* others */
extern int ns_printer_match_name(ns_printer_t *, const char *);
extern char *ns_printer_name_list(const ns_printer_t *);
extern char *value_to_string(const char *, void *);
extern void *string_to_value(const char *, char *);
extern char *normalize_ns_name(char *);
extern char * strncat_escaped(char *, char *, int, char *);



#ifdef __cplusplus
}
#endif

#endif /* _NS_H */
