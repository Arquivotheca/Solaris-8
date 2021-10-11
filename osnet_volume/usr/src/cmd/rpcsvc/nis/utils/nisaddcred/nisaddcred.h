/*
 *      nisaddcred.h
 *
 *      Copyright (c) 1988-1995 Sun Microsystems, Inc.
 *      All Rights Reserved.
 */
 
#pragma ident   "@(#)nisaddcred.h 1.11     97/11/19 SMI"

#define	MAXIPRINT	(11)	/* max length of printed integer */
#define	CRED_TABLE "cred.org_dir"

extern nis_object *init_entry(void);
extern char *default_principal(char *);
extern char *program_name;
extern uid_t my_uid;
extern nis_name my_nisname;
extern char *my_host;
extern char *my_group;
extern char nispasswd[];
extern int explicit_domain;
extern struct passwd *getpwuid_nisplus_master(uid_t, nis_error *);
extern int addonly;
