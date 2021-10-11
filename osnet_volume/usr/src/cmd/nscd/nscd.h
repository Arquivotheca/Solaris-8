/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _NSCD_H
#define	_NSCD_H

#pragma ident	"@(#)nscd.h	1.7	99/04/07 SMI"

extern char * getcacheopt(char * s);
extern nsc_stat_t * getcacheptr(char * s);
extern int nsc_ping(void);
extern void switcher(void *cookie, char *argp, size_t arg_size,
    door_desc_t *dp, uint_t n_desc);
extern void usage(char * s);
extern int nscd_set_lf(admin_t * ptr, char * s);
extern int logit(char * format, ...);
extern void do_update(nsc_call_t * in);
extern int launch_update(nsc_call_t * in);
extern int nsc_calllen(nsc_call_t * in);
extern int load_admin_defaults(admin_t * ptr, int will_become_server);
extern int client_getadmin(admin_t * ptr);
extern int getadmin(nsc_return_t * out, int size, nsc_call_t * ptr);
extern int setadmin(nsc_return_t * out, int size, nsc_call_t * ptr);
extern int client_setadmin(admin_t * ptr);
extern int client_showstats(admin_t * ptr);
extern int getpw_init(void);
extern void getpw_revalidate(void);
extern int getpw_uidkeepalive(int keep, int interval);
extern int getpw_invalidate(void);
extern int getpw_lookup(nsc_return_t *out, int maxsize, nsc_call_t * in,
    time_t now);
extern void getgr_init(void);
extern void getgr_revalidate(void);
extern void getgr_invalidate(void);
extern int getgr_lookup(nsc_return_t *out, int maxsize, nsc_call_t * in,
    time_t now);
extern int gethost_init(void);
extern void gethost_revalidate(void);
extern int gethost_invalidate(void);
extern int gethost_lookup(nsc_return_t *out, int maxsize, nsc_call_t * in,
    time_t now);
extern int getnode_init(void);
extern void getnode_revalidate(void);
extern int getnode_invalidate(void);
extern int getnode_lookup(nsc_return_t *out, int maxsize, nsc_call_t * in,
    time_t now);
extern hash_t * make_hash(int size);
extern hash_t * make_ihash(int size);
extern char ** get_hash(hash_t *tbl, char *key);
extern char ** find_hash(hash_t *tbl, char *key);
extern char * del_hash(hash_t *tbl, char *key);
extern int operate_hash(hash_t *tbl, void (*ptr)(), char *usr_arg);
extern int operate_hash_addr(hash_t *tbl, void (*ptr)(), char *usr_arg);
extern void destroy_hash(hash_t *tbl, int (*ptr)(), char *usr_arg);
extern int * maken(int n);
extern int insertn(int * table, int n, int data);
extern int nscd_parse(char * progname, char * filename);
extern int strbreak(char * field[], char *s, char *sep);
extern int nscd_yesno(char * s);
extern int nscd_set_integer(int * addr, char * facility, char * cachename,
    int value, int min, int max);
extern int nscd_set_short(short * addr, char * facility, char * cachename,
    int value, int min, int max);
extern int nscd_setyesno(int * addr, char * facility, char * cachename,
    int value);
extern int nscd_setyesno_sh(short * addr, char * facility, char * cachename,
    int value);
extern int nscd_set_dl(admin_t * ptr, int value);
extern int nscd_set_ec(nsc_stat_t * cache, char * name, int value);
extern int nscd_set_cf(nsc_stat_t * cache, char * name, int value);
extern int nscd_set_khc(nsc_stat_t * cache, char * name, int value);
extern int nscd_set_odo(nsc_stat_t * cache, char * name, int value);
extern int nscd_set_ss(nsc_stat_t * cache, char * name, int value);
extern int nscd_set_ttl_positive(nsc_stat_t * cache, char * name, int value);
extern int nscd_set_ttl_negative(nsc_stat_t * cache, char * name, int value);
extern int nscd_wait(waiter_t * wchan, mutex_t * lock, char ** key);
extern int nscd_signal(waiter_t * wchan, char ** key);
extern int get_clearance(int callnumber);
extern int release_clearance(int callnumber);

extern void getexec_init ( void );
extern void getexec_revalidate ( void );
extern void getexec_invalidate ( void );
extern int getexec_lookup ( nsc_return_t *out, int maxsize, nsc_call_t * in, time_t now );
extern void getprof_init ( void );
extern void getprof_revalidate ( void );
extern void getprof_invalidate ( void );
extern int getprof_lookup ( nsc_return_t *out, int maxsize, nsc_call_t * in, time_t now );
extern void getuser_init ( void );
extern void getuser_revalidate ( void );
extern void getuser_invalidate ( void );
extern int getuser_lookup ( nsc_return_t *out, int maxsize, nsc_call_t * in, time_t now );

#endif	/* _NSCD_H */
