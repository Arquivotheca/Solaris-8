/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfga.c	1.10	99/12/02 SMI"

/*
 *	Plugin Library for PCI Hot-Plug Controller
 */

#include <stddef.h>
#include <locale.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <sys/param.h>
#include <varargs.h>
#include <libdevinfo.h>
#include <libdevice.h>

#define	CFGA_PLUGIN_LIB

#include <config_admin.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/dditypes.h>
#include <sys/devctl.h>
#include <sys/modctl.h>
#include <sys/hotplug/hpctrl.h>
#include <sys/pci.h>
#include <libintl.h>

/*
 * Set the version number
 */
int cfga_version = CFGA_HSL_V2;

#ifdef	DEBUG
#define	PCIHP_DBG	1
#endif

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

/*
 *	DEBUGING LEVEL
 *
 * 	External routines:  1 - 2
 *	Internal routines:  3 - 4
 */
#ifdef	PCIHP_DBG
int	pcihp_debug = 1;
#define	DBG(level, args) \
	{ if (pcihp_debug >= (level)) printf args; }
#define	DBG_F(level, args) \
	{ if (pcihp_debug >= (level)) fprintf args; }
#else
#define	DBG(level, args)	/* nothing */
#define	DBG_F(level, args)	/* nothing */
#endif

#define	CMD_ACQUIRE		0
#define	CMD_GETSTAT		1
#define	CMD_LIST		2
#define	CMD_SLOT_CONNECT	3
#define	CMD_SLOT_DISCONNECT	4
#define	CMD_SLOT_CONFIGURE	5
#define	CMD_SLOT_UNCONFIGURE	6
#define	CMD_SLOT_INSERT		7
#define	CMD_SLOT_REMOVE		8
#define	CMD_OPEN		9
#define	CMD_FSTAT		10
#define	ERR_CMD_INVAL		11
#define	ERR_AP_INVAL		12
#define	ERR_AP_ERR		13
#define	ERR_OPT_INVAL		14

static char *
cfga_errstrs[] = {
	/* n */ "acquire ",
	/* n */ "get-status ",
	/* n */ "list ",
	/* n */ "connect ",
	/* n */ "disconnect ",
	/* n */ "configure ",
	/* n */ "unconfigure ",
	/* n */ "insert ",
	/* n */ "remove ",
	/* n */ "open ",
	/* n */ "fstat ",
	/* y */ "invalid command ",
	/* y */ "invalid attachment point ",
	/* y */ "invalid transition ",
	/* y */ "invalid option ",
		NULL
};

#define	HELP_HEADER		1
#define	HELP_CONFIG		2
#define	HELP_ENABLE_SLOT	3
#define	HELP_DISABLE_SLOT	4
#define	HELP_ENABLE_AUTOCONF	5
#define	HELP_DISABLE_AUTOCONF	6
#define	HELP_LED_CNTRL		7
#define	HELP_UNKNOWN		8
#define	SUCCESS			9
#define	FAILED			10
#define	UNKNOWN			11

#define	TYPESIZE		12

extern int errno;

static void cfga_err(char **errstring, ...);

static char *
cfga_strs[] = {
NULL,
"\nPCI hotplug specific commands:",
"\t-c [connect|disconnect|configure|unconfigure|insert|remove] "
"ap_id [ap_id...]",
"\t-x enable_slot  ap_id [ap_id...]",
"\t-x disable_slot ap_id [ap_id...]",
"\t-x enable_autoconfig  ap_id [ap_id...]",
"\t-x disable_autoconfig ap_id [ap_id...]",
"\t-x led[=[fault|power|active|attn],mode=[on|off|blink]] ap_id [ap_id...]",
"\tunknown command or option: ",
"success   ",
"failed   ",
"unknown",
NULL
};


/*
 * PCI CLASS CODE/SUBCLASS CODE
 */


/*
 * when adding subclasses, update this to the max number of strings
 * and padd the rest of them out to that length with "unknown"
 */

#define	PCISO_MAX_SUBCLASS 9

static char *
pci_masstrg []  = {
	/* n */ "scsi",
	/* n */ "ide",
	/* n */ "flpydisk",
	/* n */ "ipi",
	/* n */ "raid",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */  NULL
};

static char *
pci_network [] = {
	/* n */ "ethernet",
	/* n */ "tokenrg",
	/* n */ "fddi",
	/* n */ "atm",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ NULL
};

static char *
pci_display [] = {
	/* n */ "vgs8514",
	/* n */ "xga",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */  NULL
};

static char *
pci_multimd [] = {
	/* n */ "video",
	/* n */ "audio",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ NULL
};

static char *
pci_memory [] = {
	/* n */ "ram",
	/* n */ "flash",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ "unknown",
	/* n */ NULL
};

static char *
pci_bridge [] = {
	/* n */ "host-pci",
	/* n */ "pci-isa",
	/* n */ "pci-eisa",
	/* n */ "pci-mca",
	/* n */ "pci-pci",
	/* n */ "pci-pcmc",
	/* n */ "pci-nubu",
	/* n */ "pci-card",
	/* n */ "pci-race",
	/* n */ "stpcipci",
	/* n */ NULL
};

/*
 * Board Type
 */
static char *
board_strs[] = {
	/* n */ "???",	/* HPC_BOARD_UNKNOWN */
	/* n */ "hp",	/* HPC_BOARD_PCI_HOTPLUG */
	/* n */ "nhs",	/* HPC_BOARD_CPCI_NON_HS */
	/* n */ "bhs",  /* HPC_BOARD_CPCI_BASIC_HS */
	/* n */ "fhs",	/* HPC_BOARD_CPCI_FULL_HS */
	/* n */ "hs",	/* HPC_BOARD_CPCI_HS */
	/* n */ NULL
};

/*
 * HW functions
 */
static char *
func_strs[] = {
	/* n */	"enable_slot",
	/* n */ "disable_slot",
	/* n */ "enable_autoconfig",
	/* n */ "disable_autoconfig",
	/* n */ "led",
	/* n */ "mode",
	/* n */ NULL
};

#define	PCISO_SUBCLASS_OTHER 0x80 /* generic subclass */

/*
 * other subclass types
 */

static char *
other_strs[] = {
	/* n */	"none",
	/* n */	"storage",
	/* n */	"network",
	/* n */	"display",
	/* n */	"mmedia",
	/* n */	"memory",
	/* n */	"bridge",
	/* n */ NULL
};


#define	ENABLE_SLOT	0
#define	DISABLE_SLOT	1
#define	ENABLE_AUTOCNF	2
#define	DISABLE_AUTOCNF	3
#define	LED		4
#define	MODE		5

/*
 * LED strings
 */
static char *
led_strs[] = {
	/* n */ "fault",	/* HPC_FAULT_LED */
	/* n */ "power",	/* HPC_POWER_LED */
	/* n */ "attn",		/* HPC_ATTN_LED */
	/* n */ "active",	/* HPC_ACTIVE_LED */
	/* n */ NULL
};

#define	FAULT	0
#define	POWER	1
#define	ATTN	2
#define	ACTIVE	3

static char *
mode_strs[] = {
	/* n */ "off",		/* HPC_LED_OFF */
	/* n */ "on",		/* HPC_LED_ON */
	/* n */ "blink",	/* HPC_LED_BLINK */
	/* n */	NULL
};

#define	OFF	0
#define	ON	1
#define	BLINK	2

#define	cfga_errstrs(i)		cfga_errstrs[(i)]

#define	cfga_eid(a, b)		(((a) << 8) + (b))
#define	MAXSLOTS		12
#define	MAXDEVS			32

struct searcharg {
	char	*devpath;
	char	slotnames[MAXDEVS][MAXNAMELEN];
	int	minor;
	di_prom_handle_t	promp;
};

void *	private_check;

static int
ap_idx(const char *ap_id)
{
	int id;
	char *s;
	static char *slot = "slot";

	DBG(3, ("ap_idx(%s)\n", ap_id));

	if ((s = strstr(ap_id, slot)) == NULL)
		return (-1);
	else {
		int n;

		s += strlen(slot);
		n = strlen(s);

		switch (n) {
		case 2:
			if (!isdigit(s[1]))
				return (-1);
		/* FALLTHROUGH */
		case 1:
			if (!isdigit(s[0]))
				return (-1);
		break;
		default:
			return (-1);
		}
	}

	if ((id = atoi(s)) > (1 << MAXSLOTS))
		return (-1);

	return (id);
}

/*
 * This routine accepts a variable number of message IDs and constructs
 * a corresponding error string which is printed via the message print routine
 * argument.
 */
/*ARGSUSED*/
static void
cfga_msg(struct cfga_msg *msgp, const char *str)
{
	DBG(2, ("<%s>", str));
	fprintf(stdout, "%s\n", str);
}

/*
 * Transitional Diagram:
 *
 *  empty		unconfigure
 * (remove)	^|  (physically insert card)
 *			|V
 * disconnect	configure
 * "-c DISCONNECT"	^|	"-c CONNECT"
 *				|V	"-c CONFIGURE"
 * connect	unconfigure	->	connect    configure
 *						<-
 *					"-c UNCONFIGURE"
 *
 */
/*ARGSUSED*/
cfga_err_t
cfga_change_state(cfga_cmd_t state_change_cmd, const char *ap_id,
    const char *options, struct cfga_confirm *confp,
    struct cfga_msg *msgp, char **errstring, cfga_flags_t flags)
{
	int rv;
	devctl_hdl_t		dcp;
	devctl_ap_state_t	state;
	ap_rstate_t		rs;
	ap_ostate_t		os;

	if (errstring != NULL)
		*errstring = NULL;

	rv = CFGA_OK;
	DBG(1, ("cfga_change_state:(%s)\n", ap_id));

	if (ap_idx(ap_id) == -1) {
		cfga_err(errstring, ERR_AP_INVAL, ap_id, 0);
		rv = CFGA_ERROR;
		return (rv);
	}

	if ((dcp = devctl_ap_acquire((char *)ap_id, 0)) == NULL) {
		if (rv == EBUSY) {
			cfga_err(errstring, CMD_ACQUIRE, ap_id, 0);
			DBG(1, ("cfga_change_state: device is busy\n"));
			rv = CFGA_BUSY;
		} else
			rv = CFGA_ERROR;
		return (rv);
	}

	if (devctl_ap_getstate(dcp, &state) == -1) {
		DBG(2, ("cfga_change_state: devctl ap getstate failed\n"));
		cfga_err(errstring, CMD_GETSTAT, ap_id, 0);
		if (rv == EBUSY)
			rv = CFGA_BUSY;
		else
			rv = CFGA_ERROR;
		return (rv);
	}

	rs = state.ap_rstate;
	os = state.ap_ostate;

	DBG(1, ("cfga_change_state: rs is %d\n", state.ap_rstate));
	DBG(1, ("cfga_change_state: os is %d\n", state.ap_ostate));
	switch (state_change_cmd) {
	case CFGA_CMD_CONNECT:
		if ((rs == AP_RSTATE_CONNECTED) ||
		    (os == AP_OSTATE_CONFIGURED)) {
			cfga_err(errstring, ERR_AP_ERR, 0);
			rv = CFGA_INVAL;
		} else {
			/* Lets connect the slot */
			if (devctl_ap_connect(dcp) == -1) {
				rv = CFGA_ERROR;
				cfga_err(errstring,
				CMD_SLOT_CONNECT, 0);
			}
		}

		break;

	case CFGA_CMD_DISCONNECT:
		if (os == AP_OSTATE_CONFIGURED) {
			if (devctl_ap_unconfigure(dcp) == -1) {
				if (errno == EBUSY)
					rv = CFGA_BUSY;
				else
					rv = CFGA_ERROR;
				cfga_err(errstring, CMD_SLOT_DISCONNECT, 0);
				break;
			}
		}

		if (rs == AP_RSTATE_CONNECTED) {
			if (devctl_ap_disconnect(dcp) == -1) {
				rv = CFGA_ERROR;
				cfga_err(errstring, CMD_SLOT_DISCONNECT, 0);
				break;
			}
		} else {
			cfga_err(errstring, ERR_AP_ERR, 0);
			rv = CFGA_INVAL;
		}

		break;

	case CFGA_CMD_CONFIGURE:
		if (rs == AP_RSTATE_DISCONNECTED) {
			if (devctl_ap_connect(dcp) == -1) {
				rv = CFGA_ERROR;
				cfga_err(errstring, CMD_SLOT_CONNECT, 0);
				break;
			}
		}

		/*
		 * for multi-func device we allow multiple
		 * configure on the same slot because one
		 * func can be configured and other one won't
		 */
		if (devctl_ap_configure(dcp) == -1) {
			rv = CFGA_ERROR;
			cfga_err(errstring, CMD_SLOT_CONFIGURE, 0);
			if (devctl_ap_disconnect(dcp) == -1) {
				rv = CFGA_ERROR;
				cfga_err(errstring,
				CMD_SLOT_CONFIGURE, 0);
			}
			break;
		}

		break;

	case CFGA_CMD_UNCONFIGURE:
		if (os == AP_OSTATE_CONFIGURED) {
			if (devctl_ap_unconfigure(dcp) == -1) {
				if (errno == EBUSY)
					rv = CFGA_BUSY;
				else {
					if (errno == ENOTSUP)
						rv = CFGA_OPNOTSUPP;
					else
						rv = CFGA_ERROR;
				}
				cfga_err(errstring, CMD_SLOT_UNCONFIGURE, 0);
			}
		} else {
			cfga_err(errstring, ERR_AP_ERR, 0);
			rv = CFGA_INVAL;
		}

		break;

	case CFGA_CMD_LOAD:
		if ((os == AP_OSTATE_UNCONFIGURED) &&
		    (rs == AP_RSTATE_DISCONNECTED)) {
			if (devctl_ap_insert(dcp) == -1) {
				rv = CFGA_ERROR;
				cfga_err(errstring, CMD_SLOT_INSERT, 0);
			}
		} else {
			cfga_err(errstring, ERR_AP_ERR, 0);
			rv = CFGA_INVAL;
		}

		break;

	case CFGA_CMD_UNLOAD:
		if ((os == AP_OSTATE_UNCONFIGURED) &&
		    (rs == AP_RSTATE_DISCONNECTED)) {
			if (devctl_ap_remove(dcp) == -1) {
				rv = CFGA_ERROR;
				cfga_err(errstring, CMD_SLOT_REMOVE, 0);
			}
		} else {
				cfga_err(errstring, ERR_AP_ERR, 0);
				rv = CFGA_INVAL;
			}

		break;

	default:
		rv = CFGA_OPNOTSUPP;
		break;
	}

	return (rv);
}

/*
 * Building iocdatat to pass it to nexus
 *
 *	iocdata->cmd ==  HPC_CTRL_ENABLE_SLOT/HPC_CTRL_DISABLE_SLOT
 *			HPC_CTRL_ENABLE_AUTOCFG/HPC_CTRL_DISABLE_AUTOCFG
 *			HPC_CTRL_GET_LED_STATE/HPC_CTRL_SET_LED_STATE
 *			HPC_CTRL_GET_SLOT_STATE/HPC_CTRL_GET_SLOT_INFO
 *			HPC_CTRL_DEV_CONFIGURE/HPC_CTRL_DEV_UNCONFIGURE
 *			HPC_CTRL_GET_BOARD_TYPE
 *
 */
void
build_control_data(struct hpc_control_data *iocdata, uint_t cmd,
    void *retdata)
{
	iocdata->cmd = cmd;
	iocdata->data = retdata;
}

/*
 * building logical name from ap_id
 */
/*ARGSUSED2*/
void
get_logical_name(const char *ap_id, char *buf, dev_t rdev)
{
	char *bufptr, *bufptr2, *pci, *apid;

	if ((apid = malloc(MAXPATHLEN)) == NULL) {
		DBG(1, ("malloc failed\n"));
		return;
	}

	memset(apid, 0, MAXPATHLEN);
	strncpy(apid, ap_id, strlen(ap_id));

	bufptr = strrchr(apid, '/');
	bufptr2 = strrchr(apid, ':');
	pci = ++bufptr;
	bufptr = strchr(pci, ',');
	*bufptr = '\0';
	bufptr = strchr(pci, '@');
	*bufptr = '\0'; bufptr++;
	strcat(buf, pci);
	strcat(buf, bufptr);
	strcat(buf, bufptr2);
	free(apid);
}

cfga_err_t
prt_led_mode(const char *ap_id, int repeat, char **errstring)
{
	hpc_led_info_t	power_led_info;
	hpc_led_info_t	fault_led_info;
	hpc_led_info_t	attn_led_info;
	hpc_led_info_t	active_led_info;
	struct hpc_control_data iocdata;
	struct stat	statbuf;
	char  *buff;
	int	fd;

	DBG(1, ("prt_led_mod function\n"));
	if (!repeat)
		fprintf(stdout, "Ap_Id\t\t\tLed\n");

	if ((fd = open(ap_id, O_RDWR)) == -1) {
		DBG(2, ("open = ap_id%s, fd%d\n", ap_id, fd));
		DBG_F(2, (stderr, "open on %s failed\n", ap_id));
		cfga_err(errstring, CMD_OPEN,  ap_id, 0);
		return (CFGA_ERROR);
	}

	if (fstat(fd, &statbuf) == -1) {
		DBG(2, ("fstat = ap_id%s, fd%d\n", ap_id, fd));
		DBG_F(2, (stderr, "fstat on %s failed\n", ap_id));
		cfga_err(errstring, CMD_FSTAT, ap_id, 0);
		return (CFGA_ERROR);
	}

	if ((buff = malloc(MAXPATHLEN)) == NULL) {
		cfga_err(errstring, "malloc ", 0);
		return (CFGA_ERROR);
	}

	memset(buff, 0, MAXPATHLEN);

	get_logical_name(ap_id, buff, statbuf.st_rdev);

	fprintf(stdout, "%s\t\t", buff);

	free(buff);

	power_led_info.led = HPC_POWER_LED;
	build_control_data(&iocdata, HPC_CTRL_GET_LED_STATE, &power_led_info);
	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		fprintf(stdout, "%s=%s,", led_strs[power_led_info.led],
		    cfga_strs[UNKNOWN]);
	} else {
		fprintf(stdout, "%s=%s,", led_strs[power_led_info.led],
		    mode_strs[power_led_info.state]);
	}

	DBG(1, ("%s:%d\n", led_strs[power_led_info.led], power_led_info.state));

	fault_led_info.led = HPC_FAULT_LED;
	build_control_data(&iocdata, HPC_CTRL_GET_LED_STATE, &fault_led_info);
	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		fprintf(stdout, "%s=%s,", led_strs[fault_led_info.led],
		    cfga_strs[UNKNOWN]);
	} else {
		fprintf(stdout, "%s=%s,", led_strs[fault_led_info.led],
			mode_strs[fault_led_info.state]);
	}
	DBG(1, ("%s:%d\n", led_strs[fault_led_info.led], fault_led_info.state));

	attn_led_info.led = HPC_ATTN_LED;
	build_control_data(&iocdata, HPC_CTRL_GET_LED_STATE, &attn_led_info);
	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		fprintf(stdout, "%s=%s,", led_strs[attn_led_info.led],
		    cfga_strs[UNKNOWN]);
	} else {
		fprintf(stdout, "%s=%s,", led_strs[attn_led_info.led],
		    mode_strs[attn_led_info.state]);
	}
	DBG(1, ("%s:%d\n", led_strs[attn_led_info.led], attn_led_info.state));

	active_led_info.led = HPC_ACTIVE_LED;
	build_control_data(&iocdata, HPC_CTRL_GET_LED_STATE, &active_led_info);
	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		fprintf(stdout, "%s=%s,", led_strs[active_led_info.led],
		    cfga_strs[UNKNOWN]);
	} else {
		fprintf(stdout, "%s=%s\n",
		    led_strs[active_led_info.led],
		    mode_strs[active_led_info.state]);
	}
	DBG(1, ("%s:%d\n", led_strs[active_led_info.led],
	    active_led_info.state));

	close(fd);

	return (CFGA_OK);
}

/*ARGSUSED*/
cfga_err_t
cfga_private_func(const char *function, const char *ap_id,
    const char *options, struct cfga_confirm *confp,
    struct cfga_msg *msgp, char **errstring, cfga_flags_t flags)
{
	char *str;
	int   len, fd, i = 0, repeat = 0;
	char buf[MAXNAMELEN];
	char ptr;
	hpc_led_info_t	led_info;
	struct hpc_control_data	iocdata;

	DBG(1, ("cfgadm_private_func: ap_id:%s\n", ap_id));
	DBG(2, ("  options: %s\n", (options == NULL)?"null":options));
	DBG(2, ("  confp: %x\n", confp));
	DBG(2, ("  cfga_msg: %x\n", cfga_msg));
	DBG(2, ("  flag: %d\n", flags));

	if (private_check == confp)
		repeat = 1;
	else
		private_check = (void*)confp;

	/* XXX change const 6 to func_str[i] != NULL */
	for (i = 0, str = func_strs[i], len = strlen(str); i < 6; i++) {
		str = func_strs[i];
		len = strlen(str);
		if (strncmp(function, str, len) == 0)
			break;
	}

	switch (i) {
		case ENABLE_SLOT:
			build_control_data(&iocdata,
				HPC_CTRL_ENABLE_SLOT, 0);
			break;
		case DISABLE_SLOT:
			build_control_data(&iocdata,
				HPC_CTRL_DISABLE_SLOT, 0);
			break;
		case ENABLE_AUTOCNF:
			build_control_data(&iocdata,
				HPC_CTRL_ENABLE_AUTOCFG, 0);
			break;
		case DISABLE_AUTOCNF:
			build_control_data(&iocdata,
				HPC_CTRL_DISABLE_AUTOCFG, 0);
			break;
		case LED:
			/* set mode */
			ptr = function[len++];
			if (ptr == '=') {
				str = (char *)function;
				for (str = (str+len++), i = 0; *str != ',';
				    i++, str++) {
					buf[i] = *str;
					DBG_F(2, (stdout, "%c\n", buf[i]));
				}
				buf[i] = '\0'; str++;
				DBG(2, ("buf = %s\n", buf));

				/* ACTIVE=3,ATTN=2,POWER=1,FAULT=0 */
				if (strcmp(buf, led_strs[POWER]) == 0)
					led_info.led = HPC_POWER_LED;
				else if (strcmp(buf, led_strs[FAULT]) == 0)
					led_info.led = HPC_FAULT_LED;
				else if (strcmp(buf, led_strs[ATTN]) == 0)
					led_info.led = HPC_ATTN_LED;
				else if (strcmp(buf, led_strs[ACTIVE]) == 0)
					led_info.led = HPC_ACTIVE_LED;
				else return (CFGA_INVAL);

				len = strlen(func_strs[MODE]);
				if ((strncmp(str, func_strs[MODE], len) == 0) &&
				    (*(str+(len)) == '=')) {
				    for (str = (str+(++len)), i = 0;
					*str != NULL; i++, str++) {
						buf[i] = *str;

				    }
				}
				buf[i] = '\0';
				DBG(2, ("buf_mode= %s\n", buf));

				/* ON = 1, OFF = 0 */
				if (strcmp(buf, mode_strs[ON]) == 0)
					led_info.state = HPC_LED_ON;
				else if (strcmp(buf, mode_strs[OFF]) == 0)
					led_info.state = HPC_LED_OFF;
				else if (strcmp(buf, mode_strs[BLINK]) == 0)
					led_info.state = HPC_LED_BLINK;
				else return (CFGA_INVAL);

				/* sendin  */
				build_control_data(&iocdata,
				    HPC_CTRL_SET_LED_STATE,
				    (void *)&led_info);
				break;
			} else if (ptr == '\0') {
				/* print mode */
				DBG(1, ("Print mode\n"));
				return (prt_led_mode(ap_id, repeat, errstring));
			}
		default:
			DBG(1, ("default\n"));
			errno = EINVAL;
			return (CFGA_INVAL);
	}

	if ((fd = open(ap_id, O_RDWR)) == -1) {
		DBG(1, ("open failed\n"));
		return (CFGA_ERROR);
	}

	DBG(1, ("open = ap_id=%s, fd=%d\n", ap_id, fd));

	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		DBG(1, ("ioctl failed\n"));
		close(fd);
		return (CFGA_ERROR);
	}

	close(fd);

	return (CFGA_OK);
}

/*ARGSUSED*/
cfga_err_t cfga_test(const char *ap_id, const char *options,
    struct cfga_msg *msgp, char **errstring, cfga_flags_t flags)
{
	if (errstring != NULL)
		*errstring = NULL;

	DBG(1, ("cfga_test:(%s)\n", ap_id));
	/* will need to implement pci CTRL command */
	return (CFGA_NOTSUPP);
}

int
find_slotname(di_node_t din, di_minor_t dim, void *arg)
{
	struct searcharg *slotarg = (struct searcharg *)arg;
	di_prom_handle_t ph = (di_prom_handle_t)slotarg->promp;
	di_prom_prop_t	prop;
	int *intp, rval;
	char *devname;
	char fulldevname[MAXNAMELEN];

	slotarg->minor = dim->dev_minor % 256;

	DBG(2, ("minor number:(%i)\n", slotarg->minor));
	DBG(2, ("hot plug slots found so far:(%i)\n", 0));

	if ((devname = di_devfs_path(din)) != NULL) {
		sprintf(fulldevname, "/devices%s:%s",
			devname, di_minor_name(dim));
		di_devfs_path_free(devname);
	}

	if (strcmp(fulldevname, slotarg->devpath) == 0) {
		if ((prop = di_prom_prop_next(ph, din, DI_PROM_PROP_NIL))
			== DI_PROM_PROP_NIL) {
			*slotarg->slotnames[0] = '\0';
			return (DI_WALK_TERMINATE);
		}
		while (prop != DI_PROM_PROP_NIL) {
			if (strcmp("slot-names", di_prom_prop_name(prop))
			    == 0) {
				rval = di_prom_prop_lookup_ints(ph,
				    din, di_prom_prop_name(prop), &intp);
				if (rval == -1) {
					DBG(2, ("di_prom_prop_lookup_ints"));
				} else {
					int i;
					char *tmptr = (char *)(intp+1);

					DBG(1, ("slot-bitmask:"
					    " %x \n", *intp));

					rval = (rval -1) * 4;

					for (i = 0; i <= slotarg->minor; i++) {
						DBG(2, ("curr slot-name: "
						    "%s \n", tmptr));

						if (i >= MAXDEVS)
							return
							    (DI_WALK_TERMINATE);

						if ((*intp >> i) & 1) {
						/* assign tmptr */
							DBG(2, ("slot-name:"
							    " %s \n", tmptr));
							if (i == slotarg->minor)
								strcpy(slotarg->
								    slotnames
								    [i], tmptr);
						/* wind tmptr to next \0 */
							while (*tmptr != '\0') {
								tmptr++;
							}
							tmptr++;
						} else {
						/* point at unknown string */
							if (i == slotarg->minor)
								strcpy(slotarg->
								    slotnames
								    [i],
								    "unknown");
						}
					}
				}
				return (DI_WALK_TERMINATE);
			}
			prop = di_prom_prop_next(ph, din, prop);
		}
		return (DI_WALK_TERMINATE);
	} else
		return (DI_WALK_CONTINUE);
}

int
find_physical_slot_names(const char *devcomp, struct searcharg *slotarg)
{
	di_node_t root_node;

	if ((root_node = di_init("/", DINFOSUBTREE|DINFOMINOR))
		== DI_NODE_NIL) {
		DBG(1, ("di_init() failed\n"));
		return (NULL);
	}

	slotarg->devpath = (char *)devcomp;

	if ((slotarg->promp = di_prom_init()) == DI_PROM_HANDLE_NIL) {
		DBG(1, ("di_prom_init() failed\n"));
		di_fini(root_node);
		return (NULL);
	}

	(void) di_walk_minor(root_node, "ddi_ctl:attachment_point:pci",
		0, (void *)slotarg, find_slotname);

	di_prom_fini(slotarg->promp);
	di_fini(root_node);
	if (slotarg->slotnames[0] != NULL)
		return (0);
	else
		return (-1);
}





int
get_type(hpc_board_type_t boardtype, hpc_card_info_t cardinfo, char * buf)
{
	if (cardinfo.sub_class == PCISO_SUBCLASS_OTHER) {
		strcat(buf, other_strs[cardinfo.base_class]);
	} else {
		if (cardinfo.sub_class > PCISO_MAX_SUBCLASS) {
			strcat(buf, "unknown");
		} else {
			if (cardinfo.header_type != PCI_HEADER_MULTI) {
				switch (cardinfo.base_class) {
				case PCI_CLASS_MASS:
					strcat(buf,
					    pci_masstrg[cardinfo.sub_class]);
					break;
				case PCI_CLASS_NET:
					strcat(buf,
					    pci_network[cardinfo.sub_class]);
					break;
				case PCI_CLASS_DISPLAY:
					strcat(buf,
					    pci_display[cardinfo.sub_class]);
					break;
				case PCI_CLASS_MM:
					strcat(buf,
					    pci_multimd[cardinfo.sub_class]);
					break;
				case PCI_CLASS_MEM:
					strcat(buf,
					    pci_memory[cardinfo.sub_class]);
					break;
				case PCI_CLASS_BRIDGE:
					strcat(buf,
					    pci_bridge[cardinfo.sub_class]);
					break;
				case PCI_CLASS_NONE:
				default:
					strcat(buf, "unknown");
					return (0);
				}
			} else
				strcat(buf, "mult");
		}
	}

	strcat(buf + strlen(buf), "/");
	switch (boardtype) {
	case HPC_BOARD_PCI_HOTPLUG:
	case HPC_BOARD_CPCI_NON_HS:
	case HPC_BOARD_CPCI_BASIC_HS:
	case HPC_BOARD_CPCI_FULL_HS:
	case HPC_BOARD_CPCI_HS:
	    strcat(buf, board_strs[boardtype]);
	    break;
	case HPC_BOARD_UNKNOWN:
	default:
	    strcat(buf, board_strs[HPC_BOARD_UNKNOWN]);
	    break;
	}
	return (0);
}

/*ARGSUSED*/
cfga_err_t
cfga_list_ext(const char *ap_id, cfga_list_data_t **cs,
    int *nlist, const char *options, const char *listopts, char **errstring,
    cfga_flags_t flags)
{
	devctl_hdl_t		dcp;
	struct hpc_control_data	iocdata;
	devctl_ap_state_t	state;
	hpc_board_type_t	boardtype;
	hpc_card_info_t		cardinfo;
	hpc_slot_info_t		slot_info;
	char			*buf;
	struct stat		statbuf;
	struct	searcharg	slotname_arg;
	int			idx, fd;
	int			rv = CFGA_OK;

	if (errstring != NULL)
		*errstring = NULL;

	memset(&slot_info, 0, sizeof (hpc_slot_info_t));

	DBG(1, ("cfga_list_ext:(%s)\n", ap_id));

	if (cs == NULL || nlist == NULL) {
		rv = CFGA_ERROR;
		return (rv);
	}

	*nlist = 1;

	if ((*cs = malloc(sizeof (cfga_list_data_t))) == NULL) {
		cfga_err(errstring, "malloc ", 0);
		DBG(1, ("malloc failed\n"));
		rv = CFGA_ERROR;
		return (rv);
	}

	if ((idx = ap_idx(ap_id)) == -1) {
		cfga_err(errstring, ERR_AP_INVAL, ap_id, 0);
		rv = CFGA_ERROR;
		return (rv);
	}

	DBG(1, ("cfga_list_ext::(%d)\n", idx));

	if ((dcp = devctl_ap_acquire((char *)ap_id, 0)) == NULL) {
		cfga_err(errstring, CMD_GETSTAT, 0);
		DBG(2, ("cfga_list_ext::(devctl_ap_acquire())\n"));
		rv = CFGA_ERROR;
		return (rv);
	}

	if (devctl_ap_getstate(dcp, &state) == -1) {
		cfga_err(errstring, ERR_AP_ERR, ap_id, 0);
		devctl_release((devctl_hdl_t)dcp);
		DBG(2, ("cfga_list_ext::(devctl_ap_getstate())\n"));
		rv = CFGA_ERROR;
		return (rv);
	}

	switch (state.ap_rstate) {
		case AP_RSTATE_EMPTY:
			(*cs)->ap_r_state = CFGA_STAT_EMPTY;
			DBG(2, ("ap_rstate = CFGA_STAT_EMPTY\n"));
			break;
		case AP_RSTATE_DISCONNECTED:
			(*cs)->ap_r_state = CFGA_STAT_DISCONNECTED;
			DBG(2, ("ap_rstate = CFGA_STAT_DISCONNECTED\n"));
			break;
		case AP_RSTATE_CONNECTED:
			(*cs)->ap_r_state = CFGA_STAT_CONNECTED;
			DBG(2, ("ap_rstate = CFGA_STAT_CONNECTED\n"));
			break;
	default:
		cfga_err(errstring, CMD_GETSTAT, ap_id, 0);
		rv = CFGA_ERROR;
		return (rv);
	}

	switch (state.ap_ostate) {
		case AP_OSTATE_CONFIGURED:
			(*cs)->ap_o_state = CFGA_STAT_CONFIGURED;
			DBG(2, ("ap_ostate = CFGA_STAT_CONFIGURED\n"));
			break;
		case AP_OSTATE_UNCONFIGURED:
			(*cs)->ap_o_state = CFGA_STAT_UNCONFIGURED;
			DBG(2, ("ap_ostate = CFGA_STAT_UNCONFIGURED\n"));
			break;
	default:
		cfga_err(errstring, CMD_GETSTAT, ap_id, 0);
		rv = CFGA_ERROR;
		return (rv);
	}

	switch (state.ap_condition) {
		case AP_COND_OK:
			(*cs)->ap_cond = CFGA_COND_OK;
			DBG(2, ("ap_cond = CFGA_COND_OK\n"));
			break;
		case AP_COND_FAILING:
			(*cs)->ap_cond = CFGA_COND_FAILING;
			DBG(2, ("ap_cond = CFGA_COND_FAILING\n"));
			break;
		case AP_COND_FAILED:
			(*cs)->ap_cond = CFGA_COND_FAILED;
			DBG(2, ("ap_cond = CFGA_COND_FAILED\n"));
			break;
		case AP_COND_UNUSABLE:
			(*cs)->ap_cond = CFGA_COND_UNUSABLE;
			DBG(2, ("ap_cond = CFGA_COND_UNUSABLE\n"));
			break;
		case AP_COND_UNKNOWN:
			(*cs)->ap_cond = CFGA_COND_UNKNOWN;
			DBG(2, ("ap_cond = CFGA_COND_UNKNOW\n"));
			break;
	default:
		cfga_err(errstring, CMD_GETSTAT, ap_id, 0);
		rv = CFGA_ERROR;
		return (rv);
	}
	(*cs)->ap_busy = (int) state.ap_in_transition;

	devctl_release((devctl_hdl_t)dcp);

	if ((fd = open(ap_id, O_RDWR)) == -1) {
		cfga_err(errstring, ERR_AP_ERR, ap_id, 0);
		(*cs)->ap_status_time = 0;
		boardtype = HPC_BOARD_UNKNOWN;
		cardinfo.base_class = PCI_CLASS_NONE;
		get_logical_name(ap_id, slot_info.pci_slot_name, 0);
		DBG(2, ("open on %s failed\n", ap_id));
		goto cont;
	}
	DBG(1, ("open = ap_id=%s, fd=%d\n", ap_id, fd));

	if (fstat(fd, &statbuf) == -1) {
		cfga_err(errstring, ERR_AP_ERR, ap_id, 0);
		DBG(1, ("fstat on %s failed\n", ap_id));
		(*cs)->ap_status_time = 0;
	} else
		(*cs)->ap_status_time = (time32_t)statbuf.st_atime;

	/* need board type and a way to get to hpc_slot_info */
	build_control_data(&iocdata, HPC_CTRL_GET_BOARD_TYPE,
	    (void *)&boardtype);

	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		boardtype = HPC_BOARD_UNKNOWN;
	}
	DBG(1, ("ioctl boardtype\n"));

	build_control_data(&iocdata, HPC_CTRL_GET_SLOT_INFO,
	    (void *)&slot_info);

	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		get_logical_name(ap_id, slot_info.pci_slot_name, 0);
		DBG(1, ("ioctl failed slotinfo: %s\n",
		    slot_info.pci_slot_name));
	} else {
		/*
		 * the driver will report back things like hpc0_slot0
		 * this needs to be changed to things like pci1:hpc0_slot0
		 */
		(*cs)->ap_log_id[0] = '\0';
		strcat((*cs)->ap_log_id, "pci");
		strcat((*cs)->ap_log_id, ap_id+13); /* |ap_id| <= MAXPATHLEN */
		buf = strstr((*cs)->ap_log_id, ",");
		*buf = '\0';
		strcat((*cs)->ap_log_id, ":");
		strcat((*cs)->ap_log_id, slot_info.pci_slot_name);
		DBG(1, ("ioctl slotinfo: %s\n", (*cs)->ap_log_id));
	}

	build_control_data(&iocdata, HPC_CTRL_GET_CARD_INFO,
	    (void *)&cardinfo);

	if (ioctl(fd, DEVCTL_AP_CONTROL, &iocdata) == -1) {
		cardinfo.base_class = PCI_CLASS_NONE;
	}

	DBG(1, ("ioctl cardinfo: %d\n", cardinfo.base_class));
	DBG(1, ("ioctl subclass: %d\n", cardinfo.sub_class));
	DBG(1, ("ioctl headertype: %d\n", cardinfo.header_type));

	close(fd);

cont:
	strcpy((*cs)->ap_phys_id, ap_id);	/* physical path of AP */
	if ((*cs)->ap_log_id[0] == '\0')
		strcpy((*cs)->ap_log_id, slot_info.pci_slot_name);

	/* slot_names of bus node  */
	if (find_physical_slot_names(ap_id, &slotname_arg) != -1)
		strcpy((*cs)->ap_info,
		    slotname_arg.slotnames[slotname_arg.minor]);

	if ((buf = malloc(TYPESIZE)) == NULL) {
		cfga_err(errstring, "malloc ", 0);
		DBG(1, ("malloc failed\n"));
		rv = CFGA_ERROR;
		return (rv);
	}

	memset(buf, 0, TYPESIZE);
	/* class_code/subclass/boardtype */
	get_type(boardtype, cardinfo, buf);
	strcpy((*cs)->ap_type, buf);
	free(buf);

	DBG(1, ("cfga_list_ext return success\n"));
	rv = CFGA_OK;

	return (rv);
}


/*ARGSUSED*/
cfga_err_t
cfga_help(struct cfga_msg *msgp, const char *options, cfga_flags_t flags)
{
	int help = 0;
	if (options) {
		cfga_msg(msgp, dgettext(TEXT_DOMAIN, cfga_strs[HELP_UNKNOWN]));
		cfga_msg(msgp, options);
	}
	DBG(1, ("cfga_help\n"));

	cfga_msg(msgp, dgettext(TEXT_DOMAIN, cfga_strs[HELP_HEADER]));
	cfga_msg(msgp, cfga_strs[HELP_CONFIG]);
	cfga_msg(msgp, cfga_strs[HELP_ENABLE_SLOT]);
	cfga_msg(msgp, cfga_strs[HELP_DISABLE_SLOT]);
	cfga_msg(msgp, cfga_strs[HELP_ENABLE_AUTOCONF]);
	cfga_msg(msgp, cfga_strs[HELP_DISABLE_AUTOCONF]);
	cfga_msg(msgp, cfga_strs[HELP_LED_CNTRL]);
	return (CFGA_OK);
}



/*
 * cfga_err() accepts a variable number of message IDs and constructs
 * a corresponding error string which is returned via the errstring argument.
 * cfga_err() calls gettext() to internationalize proper messages.
 */
static void
cfga_err(char **errstring, ...)
{
	int a;
	int i;
	int n;
	int len;
	int flen;
	char *p;
	char *q;
	char *s[32];
	char *failed;
	va_list ap;

	/*
	 * If errstring is null it means user in not interested in getting
	 * error status. So we don't do all the work
	 */
	if (errstring == NULL) {
		return;
	}
	va_start(ap);

	failed = dgettext(TEXT_DOMAIN, cfga_strs[FAILED]);
	flen = strlen(failed);

	for (n = len = 0; (a = va_arg(ap, int)) != 0; n++) {
		switch (a) {
		case CMD_GETSTAT:
		case CMD_LIST:
		case CMD_SLOT_CONNECT:
		case CMD_SLOT_DISCONNECT:
		case CMD_SLOT_CONFIGURE:
		case CMD_SLOT_UNCONFIGURE:
			p =  cfga_errstrs(a);
			len += (strlen(p) + flen);
			s[n] = p;
			s[++n] = cfga_strs[FAILED];

			DBG(2, ("<%s>", p));
			DBG(2, (cfga_strs[FAILED]));
			break;

		case ERR_CMD_INVAL:
		case ERR_AP_INVAL:
		case ERR_OPT_INVAL:
		case ERR_AP_ERR:
			switch (a) {
			case ERR_CMD_INVAL:
				p = dgettext(TEXT_DOMAIN,
				    cfga_errstrs[ERR_CMD_INVAL]);
				break;
			case ERR_AP_INVAL:
				p = dgettext(TEXT_DOMAIN,
				    cfga_errstrs[ERR_AP_INVAL]);
				break;
			case ERR_OPT_INVAL:
				p = dgettext(TEXT_DOMAIN,
				    cfga_errstrs[ERR_OPT_INVAL]);
				break;
			case ERR_AP_ERR:
				p = dgettext(TEXT_DOMAIN,
				    cfga_errstrs[ERR_AP_ERR]);
				break;
			}

			if ((q = va_arg(ap, char *)) != NULL) {
				len += (strlen(p) + strlen(q));
				s[n] = p;
				s[++n] = q;
				DBG(2, ("<%s>", p));
				DBG(2, ("<%s>", q));
				break;
			} else {
				len += strlen(p);
				s[n] = p;

			}
			DBG(2, ("<%s>", p));
			break;

		default:
			n--;
			break;
		}
	}

	DBG(2, ("\n"));
	va_end(ap);

	if ((p = (char *)calloc(len, 1)) == NULL)
		return;

	for (i = 0; i < n; i++) {
		(void) strcat(p, s[i]);
		DBG(2, ("i:%d, %s\n", i, s[i]));
	}

	*errstring = p;
#ifdef	DEBUG
	printf("%s\n", *errstring);
	free(*errstring);
#endif
}

/*ARGSUSED*/
int
cfga_ap_id_cmp(const cfga_ap_log_id_t ap_id1, const cfga_ap_log_id_t ap_id2)
{
	return (ap_idx(ap_id1) - ap_idx(ap_id2));
}
