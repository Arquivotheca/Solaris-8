/*
 * Copyright (c) 1989-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)console.c	1.22	98/11/30 SMI"

#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/console.h>
#include <sys/consdev.h>
#include <sys/promif.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/taskq.h>
#include <sys/log.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>

#define	MINLINES	10
#define	MAXLINES	48
#define	LOSCREENLINES	34
#define	HISCREENLINES	48

#define	MINCOLS		10
#define	MAXCOLS		120
#define	LOSCREENCOLS	80
#define	HISCREENCOLS	120

vnode_t *console_vnode;
taskq_t *console_taskq;
vnode_t *ltemvp = NULL;
dev_t ltemdev;

static void
console_getprop(dev_t dev, dev_info_t *dip, char *name, ushort_t *sp)
{
	uchar_t *data;
	uint_t len;
	uint_t i;

	*sp = 0;
	if (ddi_prop_lookup_byte_array(dev, dip, 0, name, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		for (i = 0; i < len; i++) {
			if (data[i] < '0' || data[i] > '9')
				break;
			*sp = *sp * 10 + data[i] - '0';
		}
		ddi_prop_free(data);
	}
}

/*
 * Gets the number of rows and columns (in char's) and the
 * width and height (in pixels) of the console.
 */
void
console_get_size(ushort_t *r, ushort_t *c, ushort_t *x, ushort_t *y)
{
	int rel_needed = 0;
	dev_info_t *dip;
	dev_t dev;

	/*
	 * If we have loaded the console IO stuff, then ask for the screen
	 * size properties from the layered terminal emulator.  Else ask for
	 * them from the root node, which will eventually fall through to the
	 * options node and get them from the prom.
	 */
	if (ltemvp == NULL) {
		dip = ddi_root_node();
		dev = DDI_DEV_T_ANY;
	} else {
		dip = e_ddi_get_dev_info(ltemdev, VCHR);
		dev = ltemdev;
		rel_needed = 1;
	}

	/*
	 * If we have not initialized a console yet and don't have a root
	 * node (ie. we have not initialized the DDI yet) return our default
	 * size for the screen.
	 */
	if (dip == NULL) {
		*r = LOSCREENLINES;
		*c = LOSCREENCOLS;
		*x = *y = 0;
		return;
	}

	console_getprop(dev, dip, "screen-#columns", c);
	console_getprop(dev, dip, "screen-#rows", r);
	console_getprop(dev, dip, "screen-width", x);
	console_getprop(dev, dip, "screen-height", y);

	if (*c < MINCOLS)
		*c = LOSCREENCOLS;
	else if (*c > MAXCOLS)
		*c = HISCREENCOLS;

	if (*r < MINLINES)
		*r = LOSCREENLINES;
	else if (*r > MAXLINES)
		*r = HISCREENLINES;

	if (rel_needed)
		ddi_rele_driver(getmajor(ltemdev));
}

typedef struct console_msg {
	size_t	cm_size;
	char	cm_text[1];
} console_msg_t;

static void
console_putmsg(console_msg_t *cm)
{
	ssize_t resid;

	ASSERT(RW_LOCK_HELD(taskq_lock(console_taskq)));

	if (rconsvp == NULL || panicstr ||
	    vn_rdwr(UIO_WRITE, console_vnode, cm->cm_text, strlen(cm->cm_text),
	    0, UIO_SYSSPACE, FAPPEND, (rlim64_t)LOG_HIWAT, kcred, &resid) != 0)
		prom_printf("%s", cm->cm_text);
	kmem_free(cm, cm->cm_size);
}

void
console_vprintf(const char *fmt, va_list adx)
{
	console_msg_t *cm;
	size_t len = vsnprintf(NULL, 0, fmt, adx);

	if (console_taskq != NULL && rconsvp != NULL && panicstr == NULL &&
	    (cm = kmem_alloc(sizeof (*cm) + len, KM_NOSLEEP)) != NULL) {
		cm->cm_size = sizeof (*cm) + len;
		(void) vsnprintf(cm->cm_text, len + 1, fmt, adx);
		if (taskq_dispatch(console_taskq, (task_func_t *)console_putmsg,
		    cm, KM_NOSLEEP) != 0)
			return;
		kmem_free(cm, cm->cm_size);
	}

	prom_vprintf(fmt, adx);
}

/*PRINTFLIKE1*/
void
console_printf(const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	console_vprintf(fmt, adx);
	va_end(adx);
}
