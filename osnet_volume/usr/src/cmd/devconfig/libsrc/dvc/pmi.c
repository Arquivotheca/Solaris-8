/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)pmi.c 1.13 99/03/26 SMI"

#include <stdlib.h>
#include <libintl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "dev.h"
#include "util.h"
#include "win.h"

/*
 * The pmi module allows a particular card to be associated with several
 * pmi files.  This allows support for different resolutions not to
 * be duplicated in every pmi file.  For example the <mach32> card
 * support both 1280x1024 and 1024x768 resolutions.  The support
 * for the lower resolutions however is provides in the <ati> pmi file.
 *
 * A node in the pmi tree is accessed by two keys representing the
 * type and the sub_type for the card represented by a pmi file.
 * For a "generic" card of a certain type that does not have any
 * particular properties, the sub_type key will be NULL.
 * Note that there exists separate <cfinfo> files to support the type
 * and the sub_type.
 *
 * In this example <i8514> and <mach32> correspond to type and sub_type
 * fields in a pmi node respectively:
 *
 *	pmi file	key (type)	key2 (sub_type)	cfinfo file
 *	--------	----------	--------------	-----------
 *
 *	mach32_60.pmi	i8514		mach32		mach32.cfinfo
 *	ati.pmi		i8514		NULL		i8514.cfinfo
 *
 * pmi files are grouped together using the "__gourp__" and "__members__"
 * entries in a pmi file.  For example, the mach32.cfinfo file is
 * organized as follows:
 *
 *	__group__="mach32"
 *	__members__="mach32_60,mach32_70,mach32_76,mach32_87,mach32_95"
 *
 * Once a resolution has been selected the particular pmi file supporting
 * that resolution is selected using the "__pmimap__" attribute in the
 * cfinfo file as in the following example:
 *
 *	__resolution__="string,1280x1024,1024x768"
 *	__pmimap__="string,mach32,ati"
 *
 */
typedef struct node {
	char*		key;
	char*		key2;
	void*		val;
	struct node*	l;
	struct node*	r;
} node_t;

static node_t*		by_pmi;
static node_t*		tmp_by_pmi;
static node_t*		by_vda;
static node_t*		tmp_by_vda;

extern int		strneq(char *a, char *b, size_t n);

static void*
find(node_t* tree, char* key)
{
	int x;

	if ( tree == NULL )
		return NULL;

	x = strcmp(tree->key, key);
	if ( x == 0 )
		return tree->val;
	if ( x < 0 )
		return find(tree->l, key);
	return find(tree->r, key);
}

static void
insert(node_t** tree, char* key, char* key2, void* val)
{
	node_t* t = *tree;

	if ( t == NULL ) {
		t = xzmalloc(sizeof(node_t));
		t->key = xstrdup(key);
		if (key2)
			t->key2 = xstrdup(key2);
		t->val = val;
		*tree = t;
		return;
	}
	if ( strcmp(t->key, key) < 0 )
		insert(&t->l, key, key2, val);
	else
		insert(&t->r, key, key2, val);
}

static char*
last_char(char* str, char c)
{
	char* last = str;

	while ( *str ) {
		if ( *str == c )
			last = str + 1;
		++str;
	}
	return last;
}

static char*
idx(char* str, char c)
{
	while ( *str ) {
		if ( *str == c )
			return str;
		++str;
	}
	return NULL;
}

void
strip_crap_from_tail(char* str, char* chars)
{
	char* cp;
	int cnt;
	int len;

	for ( cp=str+strlen(str)-1; cp > str ; --cp ) {
		if ( idx(chars, *cp) == NULL )
			return;
		*cp = '\0';
	}
	return;
}


char*
extract_title(char* filename, char* active)
{
	FILE*	fp;
	char	bfr[100];

	if ( (fp = fopen(filename, "r")) == NULL )
		return NULL;


	while ( fgets(bfr, sizeof(bfr), fp) ) {
		if ( memcmp(bfr, active, sizeof(active)-1) == 0 ) {
			/* Next nonblank line is the title. */
			do {
				if ( fgets(bfr, sizeof(bfr), fp) == NULL ) {
					fclose(fp);
					return NULL;
				}
			} while ( strlen(bfr) == 0 );

			/* Strip off leading tab. for pmi files */
			if ( bfr[0] == '\t' ) {
				int	i;

				for ( i=1; bfr[i]; ++i )
					bfr[i-1] = bfr[i];
				bfr[i-1] = '\0';
			}
			/* assume .xqa file strip accordingly   */
			else {	
			    int i,j;

			    for (i=0;bfr[i]!='"';i++)
				;
			    i++;
			    for ( j=i; bfr[j] != '"'; j++)
			        bfr[j-i] = bfr[j];
			    bfr[j-i] = '\0';
			}

			     
			strip_crap_from_tail(bfr, "\n;");

			fclose(fp);
			return (char *)dgettext(DVC_MSGS_TEXTDOMAIN, xstrdup(bfr));
		}
	}

	fclose(fp);
	return NULL;
}


char*
extract_keyword(char* filename, char* active)
{
        FILE*   fp;
        char    bfr[100];
	char* 	c;
 
        if ( (fp = fopen(filename, "r")) == NULL )
                return NULL;
 
        while ( fgets(bfr, sizeof(bfr), fp) ) {
		if ((c=strtok(bfr," ")) == NULL )
		    	continue;
		if ( !strneq(c,active,sizeof(active)) ) 
			continue;
		if ((c=strtok(NULL," ")) == NULL )
			continue;
		if ((c=strtok(NULL,"\"")) == NULL ) 
			continue;
		strcpy(bfr,c);	
                strip_crap_from_tail(bfr, "\"\n;");
                fclose(fp);
                return (char *)dgettext(DVC_MSGS_TEXTDOMAIN, xstrdup(bfr));
        }
        fclose(fp);
        return NULL;
}

char*
extract_depth(char* filename)
{
        FILE*   fp;
        char    bfr[100];
        char*   c;
        char    active[] = "Depth";
        int     depth_found = 0;
	int	len;
        char    depth0[512] = {'\0'};
        char    depth1[512] = "string,";
 
        if ( (fp = fopen(filename, "r")) == NULL )
                return NULL;
 
 
        while ( fgets(bfr, sizeof(bfr),fp) ) {
                if ((c=strtok(bfr," ")) == NULL )
                        continue;
                if ( !strneq(c,active,sizeof(active)) )
                        continue;
                if ((c=strtok(NULL," ")) == NULL )
                        continue;
                if ((c=strtok(NULL," ")) == NULL )
                        continue;
                strip_crap_from_tail(c,"\n;");
		if(!strncmp(c,"8",strlen(c)) || !strncmp(c,"24",strlen(c))){
			strcat(depth0,c);
			strcat(depth0,",");
		}
                continue;
        }        
	len = strlen(depth0);
	depth0[len-1] = '\0';		/* get rid of extra ,	*/
        fclose(fp);
        if(*depth0 != '\0' ){
                strcat(depth1,depth0);
                return (char *)dgettext(DVC_MSGS_TEXTDOMAIN, xstrdup(depth1));
        }else{
                return NULL;
        }
}


/*
 * gets resolution and virtual desk top values give a 
 * color depth number (color bit plane)
 */
char*
extract_resdesk_depth(char* filename, char *depth, char **resp, char **deskp)
{
	FILE* 	fp;
	char 	bfr[100];
	char*	c;
	char	active[] = "Depth";
	char	resolution[]= "[RESOLUTIONS]";
	char	desktop[] = "[DESKTOPS]";
	int	depth_found = 0;
	char	res0[512] = {'\0'};
	char	res1[512] = "string,";
	char	desk0[512] = {'\0'};
	char    desk1[512] = "string,";

	if ( (fp = fopen(filename, "r")) == NULL )
		return NULL;


	while ( fgets(bfr, sizeof(bfr),fp) ) {
		if(!depth_found) {
			if ((c=strtok(bfr," ")) == NULL )
                        	continue;
                	if ( !strneq(c,active,sizeof(active)) )
                        	continue;
                	if ((c=strtok(NULL," ")) == NULL )
                        	continue;
                	if ((c=strtok(NULL," ")) == NULL )
                        	continue;
			strip_crap_from_tail(c,"\n;");
			if(!strncmp(c,depth,strlen(c))){
				depth_found=1;
				continue;
			}
		}
		if ((c=strtok(bfr," ")) == NULL)
			continue;
		if(strncmp(c,resolution,strlen(resolution)-1) == 0){
		    while( fgets(bfr, sizeof(bfr),fp ) ) {
			if ((c=strtok(bfr,"\t \n")) == NULL )
		 	    continue;
			while ( *c == '\t' )
			    c++;
			if (*c>= '1' && *c<='9')
			    strcat(res0,c);
			else
			    break;
		    }
		}
		if (strncmp(c,desktop,strlen(desktop)-1) == 0){
		    while (fgets(bfr, sizeof(bfr), fp)){
			if ((c=strtok(bfr,"\t \n")) == NULL)
			    continue;
			while ( *c == '\t' )
			    c++;
			if (*c >='1' && *c<='9')
			    strcat(desk0,c);
			else
			    break;
		    }
		    break;
		}
	}
	fclose(fp);
	if(*res0 != '\0' ){
		strcat(res1,res0);
		*resp = (char *)dgettext(DVC_MSGS_TEXTDOMAIN, xstrdup(res1));
	}
	if (*desk0 != '\0' ){
		strcat(desk1,desk0);
		*deskp = (char *)dgettext(DVC_MSGS_TEXTDOMAIN, xstrdup(desk1));
	}
}

char*
get_depth(device_info_t* dp, char* bd)
{
	char* path;
	char* board;
	char* depth;

	if((board=find_attr_str(dp->dev_alist,"pmifile")) == NULL){
		return NULL;
	}

        path = strcats(win_home_path(),"/etc/devdata/SUNWaccel/boards/",board,NULL);
	depth = extract_depth(path);
	return(depth);
}
	
void
get_resdepth(device_info_t* dp, char* depth, char** resp, char** deskp)
{
	char* path;
	char* board;
	char* res;


	if((board=find_attr_str(dp->dev_alist,"pmifile")) == NULL){
		return;
	}
	
	path = strcats(win_home_path(),"/etc/devdata/SUNWaccel/boards/",board,NULL);
	extract_resdesk_depth(path, depth, resp, deskp);
}
 
 
char*
xqa_path()
{
	return strcats(win_home_path(), "/etc/devdata/SUNWaccel/boards", NULL);
}

char*
vda_path()
{
	return strcats(win_home_path(), "/etc/devdata/SUNWaccel/monitors",NULL);
}

char*
pmi_path()
{
	return strcats(win_home_path(), "/etc/devdata/svpmi", NULL);
}

static void
load_pmi(char* path, char* name)
{
        char  adapter_str_pmi[] = "[ACTIVE_ADAPTER]";
        char  adapter_str_xqa[] = "[ADAPTER]";
	char  monitor_str_xqa[] = "Module";

	char* title;
	char* module;
	/* char* pmi = xstrdup(name); */
	char* pmi = get_pmi_name(name); 
	char* sub_type = get_typ_group(pmi);
        static char *re1,*re2;
        

	if ( re1 == NULL )
                re1 = regcmp(".*\\.pmi$", NULL);
	if ( re2 == NULL ) 
		re2 = regcmp(".*\\.xqa$",NULL);
	
        if ( regex(re1, path) ){
                title=extract_title(path, adapter_str_pmi);
		insert(&tmp_by_pmi, pmi, sub_type, title);
	}
	else if (regex(re2, path) ) {
		title=extract_title(path, adapter_str_xqa);
		module=extract_keyword(path, monitor_str_xqa);
		strip_crap_from_tail(module,".ddx");
		insert(&tmp_by_pmi, pmi, sub_type, title);
	}
	else
	    	exit(1);

}

static void
load_vda(char* path, char* name)
{
        char  manufact_str[] = "Manufacturer";
        char  model_str[]    = "ModelNumber";
 
        char* monitor,*model,*title;
        char* pmi = get_pmi_name(name);
        static char *re1;
         
 
        if ( re1 == NULL )
                re1 = regcmp(".*\\.vda$", NULL);
 
        
        monitor=extract_keyword(path, manufact_str);
	model=extract_keyword(path, model_str);
	title=strcats(monitor,"-",model,NULL);
 
        insert(&tmp_by_vda, pmi, NULL , title);
}



static void 
load_type_vda(char* path, char* type)
{
	scan_dir(path,".*\\.vda$",load_vda);
	insert(&by_vda, type, NULL, tmp_by_vda);
	tmp_by_vda = NULL;
}

static void
load_type_pmi(char* path, char* type)
{
	scan_dir(path, ".*\\.[px][mq][ia]$", load_pmi);
	insert(&by_pmi, type, NULL, tmp_by_pmi);
	tmp_by_pmi = NULL;
}


void
fetch_pmi_info(void)
{
	ONCE();

	scan_dir(pmi_path(), "[a-zA-Z0-9].*", load_type_pmi);
	scan_dir(xqa_path(), "[a-zA-Z0-9].*", load_type_pmi);
	scan_dir(vda_path(), "[a-zA-Z0-9].*", load_type_vda);
	
}


static void
each(node_t* list, char* name, void (*action)(char* pmi_base, char* title))
{
	if ( list ) {
		if ((!name && !list->key2) ||
		    (name && list->key2 && streq(name, list->key2))){
			action(list->key, (char*)list->val);
		}
		each(list->l, name, action);
		each(list->r, name, action);
	}
}

void
each_pmi_title(char* name,char* type,void (*action)(char* pmi_base,char* title))
{
	node_t* list;
 	list = (node_t*)find(by_pmi, type);
	if ( list == NULL){
	    return;
	}
	each(list, streq(name, type) ? NULL : name, action);
}

void
each_vda_title(char* name,char* type,void (*action)(char* pmi_base,char* title))
{
	node_t* list;
        list = (node_t*)find(by_vda, type);
        if ( list == NULL)
	    return;
        each(list, streq(name, type) ? NULL : name, action);
}

char*
make_pmi_fname(char* type, char* pmi_base,char *isxin)
{
	if ( isxin == NULL )
	    return strcats(pmi_path(), "/", type, "/", pmi_base, ".pmi", NULL);
	else
	    return strcats(type,"/",pmi_base,".xqa",NULL);
}

char*
make_vda_fname(char* type, char* vda_base)
{
	char *c;
        c=strtok(type,"/");
	c=strcats(c,"/",vda_base,".vda",NULL);
	return c;
	
}

char*
get_pmi_name(char* pmi_file)
{
	char*	bfr;
	char*	cp;
	int	len;

	static char* re1,*re2,*re3;
	if ( re1 == NULL )
		re1 = regcmp(".*\\.pmi$", NULL);
	if ( re2 == NULL )
		re2 = regcmp(".*\\.xqa$",NULL);
	if ( re3 == NULL )
		re3 = regcmp(".*\\.vda$",NULL);
	if ( !regex(re1,pmi_file) && !regex(re2,pmi_file) && 
       	     !regex(re3,pmi_file))
		return pmi_file;

	/* extract and return the name part of the pmi file. */
	cp = last_char(pmi_file, '/');
	len = strlen(cp) - 4;

	bfr = (char*)xzmalloc(len+1);
	memcpy(bfr, cp, len);

	return bfr;
}

char*
get_pmi_type(char* name)
{
	char	c;
	char*	p;
	char*	q;
	char*	type;

	p = &name[strlen(pmi_path()) + 1];
	q = strchr(p, '/');
	c = *q;
	*q = NULL;

	type = xstrdup(p);
	*q = c;

	return type;
}
