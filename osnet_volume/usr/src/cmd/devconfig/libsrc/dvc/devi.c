/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)devi.c 1.5 93/12/14 SMI"

#include <assert.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>		/* TEST_FRAME */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/dditypes.h>
#include <sys/ddidmareq.h>
#include <sys/ddipropdefs.h>
#include <sys/ddi_impldefs.h>
#include <kvm.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "dev.h"
#include "util.h"
#include "win.h"

static conf_list_t*	fake_head;
conf_list_t*		devi_head;
int			dvc_fake_data;

void
add_devi_node(conf_list_t* devi)
{
	devi->next = devi_head;
	devi_head = devi;
}

/*
 * Copy a zero terminated string from the kernel to user memory.
 */
static char*
getkstring(kvm_t* kd, char* kaddr)
{
	static char* buf;
	static int   bufsize = 30;

	char c;
	int  i = 0;

	if ( kaddr == NULL ) {
		static char noname[] = "<null>";
		return strcpy((char*)xmalloc(sizeof(noname)), noname);
	}

	if ( buf == NULL )
		buf = (char*)xmalloc(bufsize);

	do {
		if ( kvm_read(kd, (u_long)kaddr++, &c, 1) != 1 )
			error_exit(MSG(KVM_STRERR));

		if ( i == bufsize )
			buf = (char*)xrealloc(buf, bufsize *= 2);

	} while ( (buf[i++] = c) != '\0' );

	return strcpy((char*)xmalloc(i), buf);
}


/*
 * Copy a fixed size object from the kernel to user memory.
 */

static void*
getkobj(kvm_t* kd, void* kaddr, size_t n)
{
	void* p = xmalloc(n);

	if ( kvm_read(kd, (u_long)kaddr, (char*)p, n) != n )
		error_exit(MSG(KVM_RD_ERR));

	return p;
}


/*
 * Get routines copy dev_info nodes from the kernel to
 * local data structures.
 */

static void
get_prop_list(kvm_t* kd, ddi_prop_t** head)
{
	ddi_prop_t* kp;
	ddi_prop_t* np = 0;
	ddi_prop_t* prevp = 0;

	for ( kp=*head; kp; kp=np->prop_next )  {
		np = (ddi_prop_t*)getkobj(kd, kp, sizeof(ddi_prop_t));

		/* Get prop data. */
		np->prop_name = getkstring(kd, np->prop_name);
		if ( np->prop_len )
			np->prop_val = (caddr_t)getkobj(kd, np->prop_val, np->prop_len);

		if ( prevp )
			prevp->prop_next = np;
		else
			*head = np;
		prevp = np;
	}
}

static void
get_subtree(kvm_t* kd, struct dev_info* pdev, struct dev_info* dp)
{
	dp->devi_parent = pdev;

	if ( dp->devi_name )
		dp->devi_name = getkstring(kd, dp->devi_name);

	/* To get the prop lists for objects: */
	get_prop_list(kd, &(dp->devi_sys_prop_ptr));
	get_prop_list(kd, &(dp->devi_drv_prop_ptr));

	/* Skip devices that are not devices. */
	if ( strcmp(dp->devi_name, "pseudo") == 0 )
		dp->devi_child = 0;

	/* Recurse on children. */
	if ( dp->devi_child ) {
		dp->devi_child = (struct dev_info*)getkobj(kd, dp->devi_child, sizeof(struct dev_info));
		get_subtree(kd, dp, dp->devi_child);
	}

	/* Recurse on siblings/ */
	if (dp->devi_sibling) {
		dp->devi_sibling = (struct dev_info*)getkobj(kd, dp->devi_sibling, sizeof(struct dev_info));
		get_subtree(kd, pdev, dp->devi_sibling);
	}
}


/*
 * Find every node in the tree and call the action routine for it.
 * The tree is walked from the bottom-up so that the action function
 * can free the memory for a node without breaking this function.
 */

static void
walk_tree(struct dev_info* dp, void (*action)(struct dev_info*))
{
	if ( dp->devi_child )
		walk_tree(dp->devi_child, action);
	if ( dp->devi_sibling )
		walk_tree(dp->devi_sibling, action);
	action(dp);
}


static int
walk_tree_match(void*			vp,
		int			cnt,
		char*			name,
		struct dev_info*	dp,
		void			(*action)(void*, struct dev_info*))
{
	if ( dp->devi_child )
		cnt = walk_tree_match(vp, cnt, name, dp->devi_child, action);
	if ( dp->devi_sibling )
		cnt = walk_tree_match(vp, cnt, name, dp->devi_sibling, action);
	if ( DDI_CF2(dp) && ((name==NULL) || streq(name, dp->devi_name)) ) {
		++cnt;
		action(vp, dp);
	}
	return cnt;
}

/*
 * Recursively walk the devinfo tree performing "action" on each node with a
 * name that matches the name arguement.  Return the number of nodes matched.
 */
static int
find_nodes(void*		vp,
	   char*		name,
	   struct dev_info*	dp,
	   void			(*action)(void*, struct dev_info*))
{
	if ( dp )
		return walk_tree_match(vp, 0, name, dp, action);

	return 0;
}

static struct dev_info*
get_tree()
{
	static struct nlist list[] = { { "top_devinfo" }, 0 };

	kvm_t*           kd;
	dev_info_t*      rnodep;
	struct dev_info* rootp;

	if ( (kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL))==NULL ) {
		char *msg = strcats(MSG(KVM_OPNERR),": ", strerror(errno),NULL);

		ui_notice(msg);
		xfree(msg);
		return NULL;
	}

	if ( kvm_nlist(kd, &list[0]) ) {
		error_exit(MSG(BADOP));
		return NULL;
	}

	/* Read the root node. */
	if ( kvm_read(kd, list[0].n_value, (char*)&rnodep, sizeof rnodep) != sizeof rnodep ) {
		char *msg = strcats(MSG(KVM_RDFAIL),": ",strerror(errno),NULL);

		ui_notice(msg);
		xfree(msg);
		return NULL;
	}
	rootp = (struct dev_info*)getkobj(kd, rnodep, sizeof(struct dev_info));

	get_subtree(kd, NULL, rootp);

	kvm_close(kd);

	return rootp;
}

static void
make_obj(struct val_list **vlist, caddr_t val, int len)
{
	char *s;
	val_list_t *vp;

	vp = (val_list_t *)xzmalloc(sizeof (val_list_t));
	vp->val_type = VAL_STRING;
	s = (caddr_t)xzmalloc(len + 1);
	memccpy((void *)s, (const void *)val, 1, len);
	vp->val.string = s;
	vp->next = *vlist;
	*vlist = vp;
}

/*ARGSUSED*/
static void
parse_node(void* vp, struct dev_info* dp)
{
	ddi_prop_t*		pp;
	struct conf_list**	head = (struct conf_list**)vp;
	struct conf_list*	cf = (struct conf_list*)xmalloc(sizeof(struct conf_list));
	struct attr_list*	next = NULL;

	memset(cf, 0, sizeof *cf);

	cf->next = *head;
	*head = cf;

	cf->alist = (struct attr_list*)xmalloc(sizeof *(cf->alist));
	memset(cf->alist, 0, sizeof *(cf->alist));

	cf->alist->name = xstrdup("name");
	make_val(&(cf->alist->vlist), VAL_STRING, dp->devi_name);

	next = cf->alist;
	cf->alist = (struct attr_list*)xmalloc(sizeof *(cf->alist));
	memset(cf->alist, 0, sizeof *(cf->alist));
	cf->alist->next = next;

	cf->alist->name = xstrdup(INSTANCE_ATTR);
	make_val(&(cf->alist->vlist), VAL_NUMERIC, dp->devi_instance);

	for ( pp=dp->devi_sys_prop_ptr; pp!=NULL; pp=pp->prop_next ) {

		if (streq(pp->prop_name, "interrupts") ||
		    streq(pp->prop_name, "reg"))
			continue;

		next = cf->alist;
		cf->alist = (struct attr_list*)xmalloc(sizeof *(cf->alist));
		memset(cf->alist, 0, sizeof *(cf->alist));
		cf->alist->next = next;

		/* Attribute name. */
		cf->alist->name = xstrdup(pp->prop_name);

		/* Don't know what it is, assume it is a string. */
		make_obj(&(cf->alist->vlist), pp->prop_val, pp->prop_len);
	}

	for ( pp=dp->devi_drv_prop_ptr; pp!=NULL; pp=pp->prop_next ) {
		char *name;

		/*
		 * We only care about the chosen-* attributes for now.
		 */
		if (streq(pp->prop_name, "chosen-interrupt"))
			name = xstrdup("interrupts");
		else if (streq(pp->prop_name, "chosen-reg"))
			name = xstrdup("reg");
		else
			continue;

		next = cf->alist;
		cf->alist = (struct attr_list*)xmalloc(sizeof *(cf->alist));
		memset(cf->alist, 0, sizeof *(cf->alist));
		cf->alist->next = next;

		/* Attribute name. */
		cf->alist->name = name;

		/* Don't know what it is, assume it is a string. */
		make_obj(&(cf->alist->vlist), pp->prop_val, pp->prop_len);
	}
}

static void
free_node(struct dev_info* dp)
{
	xfree(dp->devi_name);
	xfree(dp);
}


void
free_tree(struct dev_info* rootp)
{
	if ( rootp )
		walk_tree(rootp, free_node);
	xfree(rootp);
}

static conf_list_t*
parse_tree(void)
{
	struct conf_list*	cf = NULL;
	struct dev_info*	rootp;

	rootp = get_tree();
	if ( rootp == NULL )
		return NULL;

	find_nodes(&cf, NULL, rootp, parse_node);

	return cf;
}

/*ARGSUSED*/
static void
read_fake_data(char* path, char* unused)
{
	conf_list_t*		cf;
	conf_list_t*		last;
	FILE*			fp;

	errno = 0;
	if ( (fp = fopen(path, "r")) == NULL ) {
		vrb(MSG(READERR), path, strerror(errno));
		return;
	}

	if ( (cf = read_conf(fp)) != NULL ) {
		last = cf;
		while ( last->next )
			last = last->next;
		last->next = fake_head;
		fake_head = cf;
	}

	fclose(fp);
}

void
fetch_devi_info(void)
{
	if ( dvc_fake_data ) {
		scan_dir(".", ".*\\.testdata$", read_fake_data);
		devi_head = fake_head;
	} else
		devi_head = parse_tree();

	read_win_conf();
}

#ifdef DEVI_TEST_FRAME

void
write_prop(ddi_prop_t* head, char* name)
{
	ddi_prop_t* pp;

	if ( head )
		printf(" %s props:\n", name);

	for ( pp=head; pp!=NULL; pp=pp->prop_next ) {
		printf("  name <%s> length <%d>", pp->prop_name, pp->prop_len);

		if ( pp->prop_undef )
			printf(" undefined.\n");
		else if ( pp->prop_len == 0 )
			printf(" no value.\n");
		else {
			int   i;

			if ( pp->prop_len == sizeof(int) )
				printf(" 0x%x", *(int*)pp->prop_val);

			printf(" value string ");
			printf("\"%s\" ", pp->prop_val);
			for ( i=0; i<pp->prop_len; ++i )
				printf("%2.2x", (unsigned)((unsigned char)*(pp->prop_val+i)));
			printf(".\n");
		}
	}
	printf("\n");
}

void
write_node(struct dev_info* dp)
{
	/* If a driver is attached (node is in canonical form 2) then dump the node. */
	if ( DDI_CF2(dp) ) {
		printf("\n");
		printf("%s, unit #%d\n", dp->devi_name, dp->devi_instance);
		write_prop(dp->devi_sys_prop_ptr, "System");
		write_prop(dp->devi_drv_prop_ptr, "Driver");
	}
}

void
ui_notice(char* text)
{
	fprintf(stderr, "NOTICE: ");
	fprintf(stderr, text);
	fflush(stderr);
}

void
ui_error_exit(char* text)
{
	fprintf(stderr, "ERROR: ");
	fprintf(stderr, text);
	fflush(stderr);
	exit(1);
}

int
main(int ac, char* av[])
{
	struct dev_info* rootp;

	setlocale(LC_ALL, "");
	textdomain("SUNW_INSTALL_DEVCFG");

	if (streq(av[1], "-p")) {
		devi_head = parse_tree();
		write_conf(stdout, devi_head);
	} else if ( (rootp = get_tree()) != NULL ) {
		walk_tree(rootp, write_node);
		free_tree(rootp);
	}

	return 0;
}

#endif /* DEVI_TEST_FRAME */
