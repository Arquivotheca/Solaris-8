/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nscd_parse.c	1.3	95/02/21 SMI"

/*
 *   routine to parse configuration file
 *
 *   returns -1 on error, 0 on sucess.  Error messages to log.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <synch.h>
#include <sys/door.h>
#include <unistd.h>

#include "getxby_door.h"
#include "server_door.h"
#include "nscd.h"

extern admin_t current_admin;

int
nscd_parse(char * progname, char * filename)
{
	FILE * in;
	char buffer[255];
	char * fields [128];
	int errflg;
	int linecnt;
	int fieldcnt;

	if((in = fopen(filename, "r")) == NULL) {
		logit("%s: open of configuration file %s failed: %s\n",
		      progname, filename, strerror(errno));
		return(-1);
	}

	errflg = 0;
	linecnt = 0;
	while(fgets(buffer, sizeof(buffer), in)!=NULL && !errflg) {
		nsc_stat_t * cache;
		
		linecnt++;

		if((fieldcnt = strbreak(fields, buffer, " \t\n")) == 0)  /* blank */
			continue;

			

		switch(*fields[0]) {
			


		case '#':		/* comment ignore it */
			break;
		case 'p':

			if((strcmp("positive-time-to-live", fields[0]) != 0)
			    ||
			    (fieldcnt != 3)
			    ||
			    !(cache = getcacheptr(fields[1]))) {
				errflg++;
				break;
			}

			if(nscd_set_ttl_positive(cache, fields[1], atoi(fields[2])) < 0)
				errflg++;

			break;

		case 'n':

			if((strcmp("negative-time-to-live", fields[0]) != 0)
			    ||
			    (fieldcnt != 3)
			    ||
			    !(cache = getcacheptr(fields[1]))) {
				errflg++;
				break;
			}

			if(nscd_set_ttl_negative(cache, fields[1], atoi(fields[2])) < 0)
				errflg++;

			break;

		case 's':

			if((strcmp("suggested-size", fields[0]) != 0)
			   ||
			   (fieldcnt != 3)
			   ||
			   !(cache = getcacheptr(fields[1])))  {
				errflg++;
				break;
			}

			if(nscd_set_ss(cache, fields[1], atoi(fields[2])) < 0)
				errflg++;

			break;


		case 'k':

			if((strcmp("keep-hot-count", fields[0]) != 0)
			    ||
			    (fieldcnt != 3)
			    ||
			    !(cache = getcacheptr(fields[1])))  {
				errflg++;
				break;
			}

			if(nscd_set_khc(cache, fields[1], atoi(fields[2])) < 0)
				errflg++;

			break;
				
		case 'o':

			if((strcmp("old-data-ok", fields[0]) != 0)
			   ||
			   (fieldcnt != 3)
			   ||
			   !(cache = getcacheptr(fields[1])))  {
				errflg++;
				break;
			}

			if(nscd_set_odo(cache, fields[1], nscd_yesno(fields[2])) <  0) {
				errflg++;
			}

			break;

		case 'e':
			if((strcmp("enable-cache", fields[0]) != 0)			   
			   ||
			   (fieldcnt != 3)
			   ||
			   !(cache = getcacheptr(fields[1]))) {
				errflg++;
				break;
			}

			if(nscd_set_ec(cache, fields[1], nscd_yesno(fields[2])) <  0)
				errflg++;
			break;

		case 'c':

			if((strcmp("check-files", fields[0]) != 0) 
			    ||
			    (fieldcnt != 3)
			    ||
			    !(cache = getcacheptr(fields[1]))) {
				errflg++;
				break;
			}

			if(nscd_set_cf(cache, fields[1], nscd_yesno(fields[2])) < 0  )
				errflg++;
			break;


		case 'l':

			if(strcmp("logfile", fields[0])) {
				errflg++;
				break;
			}
			printf("fields[1] = %s\n", fields[1]);

			if(nscd_set_lf(&current_admin, fields[1]) < 0)
				errflg++;

			break;

		case 'd':
			if(strcmp("debug-level", fields[0])) {
				errflg++;
				break;
			}

			if(nscd_set_dl(&current_admin, atoi(fields[1])) < 0)
				errflg++;
			break;



		default:
			errflg++;
			break;
				
		}

		if(errflg) {
			logit("Syntax error line %d of configuration file %s\n",
			      linecnt, filename);
			return(-1);
		}

	}
	
	fclose(in);
	return(errflg?-1:0);
}

int strbreak(field,s, sep)
	char * field[], *s, *sep;
{
	register int i;
	char * lasts;

	for(i=0; field[i]=strtok_r((i?(char *)NULL:s),sep, &lasts);i++)
		;
	return(i);
}

nscd_yesno(char * s) 
{
	if(strcmp(s,"yes") == 0) 
		return(1);

	if(strcmp(s,"no") == 0) 
		return(0);
	return(-1);
}

nscd_set_integer(int * addr, char * facility, char * cachename, int value, int min, int max)
{
	if(value < min || value > max) {
		logit("attempted to set value of %s for %s to %d, which is not %d <= x <= %d\n",
		      facility, cachename, value, min,max);
		return(-1);
	}

	if(*addr != value) {
		if(current_admin.debug_level) 
		    logit("Setting %s for %s to %d\n", 
			  facility, cachename, value);
		*addr = value;
		return(1);
	}
	return(0);
}

nscd_set_short(short * addr, char * facility, char * cachename, int value, int min, int max)
{
	if(value < min || value > max) {
		logit("attempted to set value of %s for %s to %d, which is not %d <= x <= %d\n",
		      facility, cachename, value, min,max);
		return(-1);
	}

	if(*addr != value) {
		if(current_admin.debug_level) 
		    logit("Setting %s for %s to %d\n", 
			  facility, cachename, value);
		*addr = value;
		return(1);
	}
	return(0);
}


nscd_setyesno(int * addr, char * facility, char * cachename, int value)
{

	int yn;

	switch(yn = value) {
	case 1:
	case 0:
		if(*addr != yn) {
			if(current_admin.debug_level) 
			    logit("%s now %s for %s\n",
				  facility, (yn?"enabled":"disabled"), cachename);
			*addr = yn;
			return(1);
		}
		else
			return(0);
	}
	return(-1);
}

nscd_setyesno_sh(short * addr, char * facility, char * cachename, int value)
{

	int yn;

	switch(yn = value) {
		
	case 1:
	case 0:
		if(*addr != yn) {
			if(current_admin.debug_level) 
			    logit("%s now %s for %s\n",
				  facility, (yn?"enabled":"disabled"), cachename);
			*addr = yn;
			return(1);
		}
		else
			return(0);
	}
	return(-1);
}

nscd_set_dl(admin_t * ptr, int value)
{
	return(nscd_set_integer(&(ptr->debug_level),
			       "Debug level",
			       "nscd",
			       value,
			       0,
			       10));
}

nscd_set_ec(nsc_stat_t * cache, char * name, int value)
{
	return(nscd_setyesno(&(cache->nsc_enabled), "Caching", name, value));
}

nscd_set_cf(nsc_stat_t * cache, char * name, int value)
{
	return(nscd_setyesno_sh(&(cache->nsc_check_files), "Checking files", name, value));
}




nscd_set_khc(nsc_stat_t * cache, char * name, int value)
{
	if(cache->nsc_pos_ttl < 600 && cache->nsc_keephot) {
		logit("ttl less than 600 seconds - disabling keep warm for %s cache\n",
		      name);
		return(0);
	}
	else 
		return(nscd_set_short(&(cache->nsc_keephot),
				      "Number of entries to keep hot",
				      name,
				      value,
				      0,
				      200));
}

nscd_set_odo(nsc_stat_t * cache, char * name, int value)
{
	return(nscd_setyesno_sh(&(cache->nsc_old_data_ok), "Allowing return of old data", 
			     name, value));
}

nscd_set_ss(nsc_stat_t * cache, char * name, int value)
{
	return(nscd_set_integer(&(cache->nsc_suggestedsize),
			       "Suggested size",
			       name,
			       value,
			       37,
			       10001));
}

nscd_set_ttl_positive(nsc_stat_t * cache, char * name, int value)
{
	int result = 
		nscd_set_integer(&(cache->nsc_pos_ttl), 
				 "Time to live for positive cache entries",
				 name, 
				 value,
				 0,
				 1<<30);
	if(cache->nsc_pos_ttl < 600  && cache->nsc_keephot) {
		cache->nsc_keephot = 0;
		logit("Disabling keephot for cache %s since ttl is less than 600 seconds\n",
		      name);
	}
	return(result);
}


nscd_set_ttl_negative(nsc_stat_t * cache, char * name, int value)
{
	int result = 
		nscd_set_integer(&(cache->nsc_neg_ttl), 
				 "Time to live for negative cache entries",
				 name, 
				 value,
				 0,
				 1<<30);
	return(result);
}









