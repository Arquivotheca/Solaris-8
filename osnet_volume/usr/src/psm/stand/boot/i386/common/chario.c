/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)chario.c	1.10	99/05/04 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/bootlink.h>
#include <sys/machine.h>
#include <sys/salib.h>
#include "chario.h"

extern void serial_init();
extern int doint(void);

int chario_mark(char *buf, int len, int flag);
int chario_parse_mode(int port, char *buf, int len, short *val);
int chario_mark_one(char *name, int flag);

struct _chario_storage_ {
	struct _chario_storage_		*next;
	void				*name;
	char				*value;
};

typedef struct _chario_storage_ _chario_storage_t, *_chario_storage_p;

_chario_storage_p chario_storage_head;
#define	NULL_STORAGE ((_chario_storage_p)0)

_chario_storage_p
chario_alloc(char *buf, int len, void *cookie)
{
	_chario_storage_p p;

	p = (_chario_storage_p)bkmem_alloc(sizeof (_chario_storage_t));
	if (p == (_chario_storage_p)0) {
		printf("Failed to alloc storage for %s\n", cookie);
		return (NULL_STORAGE);
	}
	p->name = cookie;
	p->value = (char *)bkmem_alloc(len + 1);
	if (p->value == (char *)0) {
		printf("Failed to alloc storage for %s\n", buf);
		bkmem_free((caddr_t)p, sizeof (_chario_storage_t));
		return (NULL_STORAGE);
	}
	bcopy(buf, p->value, len);
	if (chario_storage_head)
		p->next = chario_storage_head;
	chario_storage_head = p;

	return (p);
}

int
chario_getprop(char *buf, int len, void *cookie)
{
	_chario_storage_p p;
	int valuelen;

	/*
	 * First we must find the values associated with the requested
	 * cookie. If not found report it as an error.
	 */
	for (p = chario_storage_head; p; p = p->next)
		if (strcmp(cookie, p->name) == 0)
			break;

	if (p == NULL_STORAGE) {
		return (BOOT_FAILURE);
	}

	/*
	 * Actually copy the information if there's space.
	 */
	valuelen = strlen(p->value) + 1;
	if (buf) {
		if (valuelen > len) {
			printf("Not enough storage for %s\n", cookie);
			return (BOOT_FAILURE);
		}
		bcopy(p->value, buf, valuelen);
	}
	return (valuelen);
}

struct dnode;

/*ARGSUSED*/
int
chario_put_dev(struct dnode *node, char *buf, int len, void *cookie)
{
	if (chario_alloc(buf, len, cookie) == NULL_STORAGE)
		return (BOOT_FAILURE);

	if (strcmp(cookie, "input-device") == 0) {
		return (chario_mark(buf, len, CHARIO_IN_ENABLE));
	} else if (strcmp(cookie, "output-device") == 0) {
		return (chario_mark(buf, len, CHARIO_OUT_ENABLE));
	}
	return (BOOT_SUCCESS);		/* XXX is this right? */
}

/*ARGSUSED*/
int
chario_get_dev(struct dnode *node, char *buf, int len, void *cookie)
{
	return (chario_getprop(buf, len, cookie));
}

/*ARGSUSED*/
int
chario_put_mode(struct dnode *node, char *buf, int len, void *cookie)
{
	int rtn;
	char *altcookie = (char *)0;

	if (strcmp(cookie, "ttya-mode") == 0) {
		rtn = chario_parse_mode(0, buf, len, 0);
	} else if (strcmp(cookie, "ttyb-mode") == 0 ||
	    strcmp(cookie, "ttyb-mode") == 0) {
		rtn = chario_parse_mode(1, buf, len, 0);
	} else if (strcmp(cookie, "com1-mode") == 0) {
		rtn = chario_parse_mode(0, buf, len, 0);
		altcookie = "ttya-mode";
	} else if (strcmp(cookie, "com2-mode") == 0) {
		rtn = chario_parse_mode(1, buf, len, 0);
		altcookie = "ttyb-mode";
	}

	/*
	 * set ttyx-mode if comx-mode set.
	 */
	if (altcookie) {
		if ((rtn == BOOT_SUCCESS) &&
		    (chario_alloc(buf, len, altcookie) == NULL_STORAGE))
			return (BOOT_FAILURE);
	}

	if ((rtn == BOOT_SUCCESS) &&
	    (chario_alloc(buf, len, cookie) == NULL_STORAGE))
		return (BOOT_FAILURE);


	return (BOOT_SUCCESS);
}

/*ARGSUSED*/
int
chario_get_mode(struct dnode *node, char *buf, int len, void *cookie)
{
	return (chario_getprop(buf, len, cookie));
}

/*ARGSUSED*/
int
chario_put_cd(struct dnode *node, char *buf, int len, void *cookie)
{
	/* need to handle carrier detect */
	if (chario_alloc(buf, len, cookie) == NULL_STORAGE)
		return (BOOT_FAILURE);

	return (BOOT_SUCCESS);
}

/*ARGSUSED*/
int
chario_get_cd(struct dnode *node, char *buf, int len, void *cookie)
{
	return (chario_getprop(buf, len, cookie));
}

/*ARGSUSED*/
int
chario_put_rts(struct dnode *node, char *buf, int len, void *cookie)
{
	/* the existence of this property is checked in serial_init() */
	if (chario_alloc(buf, len, cookie) == NULL_STORAGE)
		return (BOOT_FAILURE);

	return (BOOT_SUCCESS);
}

/*ARGSUSED*/
int
chario_get_rts(struct dnode *node, char *buf, int len, void *cookie)
{
	return (chario_getprop(buf, len, cookie));
}

int
chario_mark(char *buf, int len, int flag)
{
	_char_io_p p;
	char *vp, *vp1, *value;
	int current_set = 0, current_idx = 0, current_valid = 0;
	extern _char_io_t console;

	if ((value = (char *)bkmem_alloc(len + 1)) == 0) {
		printf("Can't alloc space for %s\n", buf);
		return (BOOT_FAILURE);
	} else {
		bcopy(buf, value, len);
	}

	/*
	 * clear the flag in all of the output devices. Only those devices
	 * that match the buf values will be enabled or created. Save the
	 * current devices which are enabled in case the input value does
	 * match anything so that we can reset the values.
	 */
	for (p = &console; p; p = p->next, current_idx++) {
		if (p->flags & flag)
			BSET(current_set, current_idx);
		p->flags &= ~flag;
	}

	vp = value;
	do {
		if ((vp1 = strchr(vp, ' ')) != 0)
			*vp1++ = '\0';
		if (chario_mark_one(vp, flag) == 0)  {
			/* Failed to find device in current list, create it */
			if ((strncmp(vp, "ttya", 4) == 0) ||
			    (strncmp(vp, "com1", 4) == 0)) {
				serial_init(vp, 0,
					S9600|DATA_8|STOP_1,
					flag);
				current_valid = 1;
			} else if ((strncmp(vp, "ttyb", 4) == 0) ||
			    (strncmp(vp, "com2", 4) == 0)) {
				serial_init(vp, 1,
					S9600|DATA_8|STOP_1,
					flag);
				current_valid = 1;
			}
		} else {
			current_valid = 1;
		}
		vp = vp1;
	} while (vp);

	/*
	 * If we didn't find a valid device reset the structures to their
	 * orignal state
	 */
	if (!current_valid) {
		for (current_idx = 0, p = &console; p;
		    p = p->next, current_idx++)
			if (BISSET(current_set, current_idx))
				p->flags |= flag;
	}

	bkmem_free((caddr_t)value, len + 1);
	return (BOOT_SUCCESS);
}

int
chario_mark_one(char *name, int flag)
{
	_char_io_p p;
	extern _char_io_t console;

	for (p = &console; p; p = p->next)  {
		if (strcmp(name, p->name) == 0) {
			p->flags |= flag;
			return (1);
		}
	}
	return (0);
}

/*
 * chario_mark_port: append flags, replace vals on serial port
 * This works around problems with ordering of -mode and asy-direct
 * properties in bootenv.rc file
 */
int
chario_mark_port(int port, int flags, int vals)
{
	_char_io_p p;
	extern _char_io_t console;

	for (p = &console; p; p = p->next)  {
		if ((int)p->cookie == port &&
		    (strncmp(p->name, "com", 3) == 0 ||
		    strncmp(p->name, "tty", 3) == 0)) {
			p->flags |= flags;
			p->vals = vals;
			return (1);
		}
	}
	return (0);
}

/*
 * Value of this string is in the form of "9600,8,n,1,-"
 * 1) speed: 9600, 4800, ...
 * 2) data bits
 * 3) parity: n(none), e(even), o(odd)
 * 4) stop bits
 * 5) handshake: -(none), h(hardware: rts/cts), s(software: xon/off)
 *    we don't support handshaking
 *
 * This parsing came from a SPARCstation eeprom.
 */
int
chario_parse_mode(int port, char *buf, int len, short *val)
{
	short port_vals;
	char *p, *p1, *values;
	extern struct int_pb	ic;

	if ((values = (char *)bkmem_alloc(len + 1)) == 0) {
		printf("Failed to alloc space for %s\n", buf);
		return (BOOT_FAILURE);
	} else {
		bcopy(buf, values, len);
	}
	p = values;

	/* ---- baud rate ---- */
	port_vals = S9600;
	if (p && (p1 = strchr(p, ',')) != 0) {
		*p1++ = '\0';
	} else {
		port_vals |= DATA_8|STOP_1;
		goto error_exit;
	}

	if (strcmp(p, "110") == 0)
		port_vals = S110;
	else if (strcmp(p, "150") == 0)
		port_vals = S150;
	else if (strcmp(p, "300") == 0)
		port_vals = S300;
	else if (strcmp(p, "600") == 0)
		port_vals = S600;
	else if (strcmp(p, "1200") == 0)
		port_vals = S1200;
	else if (strcmp(p, "2400") == 0)
		port_vals = S2400;
	else if (strcmp(p, "4800") == 0)
		port_vals = S4800;
	else if (strcmp(p, "9600") == 0)
		port_vals = S9600;
	else if (strcmp(p, "19200") == 0)
		port_vals = S19200|SDIRECT;
	else if (strcmp(p, "38400") == 0)
		port_vals = S38400|SDIRECT;
	else if (strcmp(p, "57600") == 0)
		port_vals = S57600|SDIRECT;
	else if (strcmp(p, "76800") == 0)
		port_vals = S76800|SDIRECT;
	else if (strcmp(p, "115200") == 0)
		port_vals = S115200|SDIRECT;
	else if (strcmp(p, "153600") == 0)
		port_vals = S153600|SDIRECT;
	else if (strcmp(p, "230400") == 0)
		port_vals = S230400|SDIRECT;
	else if (strcmp(p, "307200") == 0)
		port_vals = S307200|SDIRECT;
	else if (strcmp(p, "460800") == 0)
		port_vals = S460800|SDIRECT;

	/* ---- Next item is data bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		port_vals |= DATA_8|STOP_1;
		goto error_exit;
	}
	switch (*p) {
		default:
		case '8': port_vals |= DATA_8; break;
		case '7': port_vals |= DATA_7; break;
		case '6': port_vals |= DATA_6; break;
		case '5': port_vals |= DATA_5; break;
	}

	/* ---- Parity info ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		goto error_exit;
	}
	switch (*p)  {
		default:
		case 'n': break;
		case 'e': port_vals |= PARITY_EVEN; break;
		case 'o': port_vals |= PARITY_ODD; break;
	}

	/* ---- Find stop bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		port_vals |= STOP_1;
		goto error_exit;
	}
	if (*p == '1')
		port_vals |= STOP_1;
	else
		port_vals |= STOP_2;

	/* ---- handshake is next ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';

		if (*p == 'h')
			port_vals |= SHARD;
		else if (*p == 's')
			port_vals |= SSOFT;
	}

	/* ---- direct serial port ---- */
	if (p1 && *p1 == 'd')
		port_vals |= SDIRECT;

error_exit:
	/* ---- save parsed values ---- */
	if (val)
		*val = port_vals;

	/* ---- Now setup the serial port with our values ---- */
	if (port_vals & SDIRECT) {
		/* reset port values to workaround property ordering problems */
		(void) chario_mark_port(port, CHARIO_INIT, port_vals);
	} else {
		ic.ax = port_vals & 0xFF;
		ic.dx = (short)port;
		ic.intval = 0x14;

		(void) doint();
	}

	bkmem_free((caddr_t)values, len + 1);
	return (BOOT_SUCCESS);
}
