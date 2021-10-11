/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ADAPTOR_H
#define	_ADAPTOR_H

#pragma ident	"@(#)adaptor.h	1.9	98/07/22 SMI"

/*
 * The API used by the BSD print protocol adaptor to glue the front end
 * request receiving code to the back end request fufilling code.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	ADAPTOR_PATH "/etc/print/bsd-adaptor,/usr/lib/print/bsd-adaptor"
#define	NS_KEY_ADAPTOR_PATH	"spooling-type-path"
#define	NS_KEY_ADAPTOR_NAME	"spooling-type"
#define	CASCADE			"cascade"
#define	LPSCHED			"lpsched"

extern	int  adaptor_available(const char *printer);
extern	int  adaptor_spooler_available(const char *printer);
extern	int  adaptor_spooler_accepting_jobs(const char *printer);
extern  int  adaptor_client_access(const char *printer, const char *host);
extern  int  adaptor_restart_printer(const char *printer);
extern  char *adaptor_temp_dir(const char *printer, const char *host);
extern  int  adaptor_submit_job(const char *printer, const char *host,
				char *cf, char **df_list);
extern  int  adaptor_show_queue(const char *printer, FILE *ofp,
				const int type, char **list);
extern  int  adaptor_cancel_job(const char *printer, FILE *ofp,
				const char *user, const char *host,
				char **list);

#ifdef __cplusplus
}
#endif

#endif /* _ADAPTOR_H */
