/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)smartcard_authenticate.c 1.44     99/10/27 SMI"

#include "smartcard_headers.h"

/* ocf_conv() call back function */
static uint32_t
ocf_conv(OCF_AuthOpTag_t, void*, OCF_AuthOpBuffer_t,
    OCF_AuthOpBuffer_t *);

/* ocf event callback function */
static uint32_t eventHandler(OCF_Event_t, OCF_EventData_t);

static int MAX_RETRY_PIN = 3;

/* client-private data passed to ocf_conv() */
typedef struct sc_appdata_t {
	void		*appdata_ptr;	/* PAM's appdata_ptr */
	int32_t		retryPIN;	/* PIN retry count */
	int32_t		retryPassword;	/* Password retry count */
	int32_t		pamStatus;	/* PAM return code */
	pam_handle_t	*pamh;
	/* XXX add in your own data here such as retry counts... XXX */
} sc_appdata_t;

/* eventHandler data passed to event handler eventHandler */
typedef struct event_handler_data_t {
	int debug;
	int verbose;
	pam_handle_t *pamh;
	OCF_CardHandle_t cardHandle;
	OCF_CardSpec_t *cardSpec;
} event_handler_data_t;

/* Initialize CardSpec structure */
extern void cardSpec_init(OCF_CardSpec_t *);

/* OCF event, client deregistration. */
extern void ocf_cleanup(OCF_ClientHandle_t);

/*
 * pam_sm_authenticate		- Authenticate smartcard user
 */
int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int		i, retcode;
	int		debug = 0;
	int		verbose = 0;
	int		max_retry_pin = 0;
	int		max_retry_passwd = 0;
	int		dual_login_mode = 0;
	char		*service, *user = NULL;
	char		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	struct pam_response *ret_resp = (struct pam_response *) 0;

	OCF_ClientHandle_t	clientHandle;
	void*	clientData = NULL;
	OCF_CardHandle_t	cardHandle;
	OCF_CardSpec_t		cardSpec;
	OCF_EventHandle_t	eventHandle;
	uint32_t		ocfReturnCode;
	uint32_t	ocfAuthReturnCode;
	getUserInfo_t		userinfo;
	event_handler_data_t	eventHandlerData;
	sc_appdata_t		scad;
	char			*pamClientVersion;
	char			*pam_user = 0;
	int			isVersion2Client = 0;
	struct pam_conv		*pam_convp;

	/* set appdata_ptr from client */
	if ((retcode = pam_get_item(pamh, PAM_CONV, (void **)&pam_convp))
	    != PAM_SUCCESS) {
		return (retcode);
	}
	scad.appdata_ptr = pam_convp->appdata_ptr;
	scad.pamh = pamh;

	/* set default retry counts */
	scad.retryPIN = 0;
	scad.retryPassword = 0;

	/* set default PAM return status */
	scad.pamStatus = PAM_SUCCESS;

	/* get the client version */
	if ((pam_get_item(pamh, PAM_MSG_VERSION, (void **) &pamClientVersion)
	    == PAM_SUCCESS) && (pamClientVersion != NULL)) {
		if (strcmp(pamClientVersion, PAM_MSG_VERSION_V2) == 0)
			isVersion2Client = 1;
	}
	if (verbose) {
		if (isVersion2Client)
			printf("PAM: Version 2 client\n");
		else
			printf("PAM: Version 1 client\n");
	}

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "debug") == 0) {
			debug = 1;
		} else if (strcasecmp(argv[i], "nowarn") == 0) {
			flags = flags | PAM_SILENT;
		} else if (strcmp(argv[i], "max_retry_PIN") == 0) {
			max_retry_pin = atoi(argv[i+1]);
			scad.retryPIN = max_retry_pin;
			i++;
		} else if (strcmp(argv[i], "max_retry_passwd") == 0) {
			max_retry_passwd = atoi(argv[i+1]);
			scad.retryPassword = max_retry_passwd;
			i++;
		} else if (strcmp(argv[i], "verbose") == 0) {
			verbose = 1;
		} else if (strcmp(argv[i], "dual_login_mode") == 0) {
			dual_login_mode = 1;
		} else {
			syslog(LOG_ERR, "illegal option %s", argv[i]);
		}
	}

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **) &service))
	    != PAM_SUCCESS) {
		return (retcode);
	} else if (verbose)
		printf("PAM: Service name: %s\n", service);

	if (debug) {
		syslog(LOG_DEBUG, "SC pam_sm_authenticate(%s %s), flags = %x ",
		    service, (user)?user:"no user", flags);
	}

	/* Initialize cardSpec */
	cardSpec_init(&cardSpec);

	/* set event handler data */
	eventHandlerData.debug = debug;
	eventHandlerData.verbose = verbose;
	eventHandlerData.pamh = pamh;
	eventHandlerData.cardSpec = &cardSpec;

	/* register OCF client */
	if (verbose)
		printf("PAM: Calling OCF_Register with %s\n", service);
	if ((ocfReturnCode = OCF_RegisterClient(service, "1.0",
	    &clientHandle)) != OCF_Success) {
		if (verbose)
			printf("PAM: OCF_RegisterClient failed\n");
		return (PAM_AUTHINFO_UNAVAIL);
	}
	if (verbose)
		printf("PAM: Client Handle: %d\n", (int) clientHandle);

	/* register OCF event handler */
	ocfReturnCode = OCF_RegisterEventHandler(clientHandle,
	    (void*) &eventHandlerData, eventHandler);

	/* Get the user name if set in pam handle */
	if (((retcode = pam_get_item(pamh, PAM_USER, (void **) &pam_user))
	    != PAM_SUCCESS) || (pam_user == 0) || pam_user[0] == '\0')
		pam_user = 0;
	else if (verbose)
		printf("PAM: Username in PAM handle: %s\n", pam_user);

	/* Check if card is already present */
	if (OCF_CardPresent(clientHandle, cardSpec) != OCF_Success
	    /* OCF_NoCard */) {
		if (verbose)
			printf("PAM initial check: card NOT-PRESENT\n");

		/* If pam_user is set, return AUTH_ERR */
		if (pam_user != 0) {
			/*
			 * This hack is mainly for dtsession
			 * which does not want PAM smart card
			 * module to prompt for insert card
			 * when user name is set
			 */
			return (PAM_AUTH_ERR);
		}

		/* Register for card inserted event */
		if (isVersion2Client &&
		    ((ocfReturnCode = OCF_RegisterForEvent(clientHandle,
		    OCF_Event_CardInserted, &cardSpec, NULL, &eventHandle))
		    != OCF_Success)) {
			if (verbose)
				printf("PAM: OCF_RegisterForEvent failed %d\n",
				    ocfReturnCode);
			ocf_cleanup(clientHandle);
			return (PAM_CONV_ERR);
		}

		/* Message to insert smart card */
		if (dual_login_mode) {
			if (isVersion2Client) {
				strcpy(messages[0],
				    PAM_MSG(pamh, 30,
				    "Please Insert Smart Card (or) "
				    "Enter Username: "));
			} else {
				strcpy(messages[0],
				    PAM_MSG(pamh, 31,
				    "Please Insert Smart Card (or) "
				    "Enter Username and Press Enter: "));
			}
			retcode = pam_get_user(pamh, &user, messages[0]);
		} else {
			if (isVersion2Client)
				strcpy(messages[0], PAM_MSG(pamh, 32, 
				"Please Insert Smart Card: "));
			else
				strcpy(messages[0], PAM_MSG(pamh, 33,
				    "Please Insert Smart Card and Press Enter: "));
			__pam_display_msg(pamh, PAM_MSG_NOCONF, 1,
			    messages, NULL);
		}

		/* Check if the user has entered username */
		if ((user != NULL) && (user[0] != '\0')) {
			int scstatus;
			pam_handle_t *scpamh;
			struct pam_conv *pam_convp;

			if (verbose)
				printf("Entered user name: %s\n", user);

			/* Cleanup smartcard */
			ocf_cleanup(clientHandle);

			/* Get the conversation function */
			if ((scstatus = pam_get_item(pamh, PAM_CONV,
			    (void **) &pam_convp)) != PAM_SUCCESS)
				return (scstatus);

			/* Call UNIX module */
			if ((scstatus = pam_start("smartcard_unix", user,
			    pam_convp, &scpamh)) != PAM_SUCCESS)
				return (scstatus);
			scstatus = pam_authenticate(scpamh, 0);
			pam_end(scpamh, PAM_SUCCESS);
			return (scstatus);
		}

		/* if no card, must be client returning due to UI change */
		if (OCF_CardPresent(clientHandle, cardSpec) != OCF_Success
		    /* OCF_NoCard */) {
			if (verbose)
				printf("PAM: returning due to UI change\n");
			ocf_cleanup(clientHandle);
			return (PAM_CONV_ERR);
		}

		if (isVersion2Client) {
			/* Get the card handle from the eventHandler data */
			cardHandle = eventHandlerData.cardHandle;
		} else {
			/* Get the card handle, by waitingForCard */
			if ((ocfReturnCode = OCF_WaitForCardInserted(
			    clientHandle, cardSpec, &cardHandle))
			    != OCF_Success) {
				if (verbose)
					printf("PAM: OCF_WaitForCardInserted "
					    "returned error %d\n", ocfReturnCode);
				ocf_cleanup(clientHandle);
				return (PAM_AUTH_ERR);
			}
		}
	} else {
		/* Get the card handle */
		if ((ocfReturnCode = OCF_WaitForCardInserted(clientHandle,
		    cardSpec, &cardHandle)) != OCF_Success) {
			if (verbose)
				printf("PAM: OCF_WaitForCardInserted "
				    "returned error %d\n", ocfReturnCode);
			ocf_cleanup(clientHandle);
			return (PAM_AUTH_ERR);
		}

		if (isVersion2Client &&
		    ((ocfReturnCode = OCF_RegisterForEvent(clientHandle,
		    OCF_Event_CardRemoved, &cardHandle, NULL,
		    &eventHandle)) != OCF_Success)) {
			if (verbose)
				printf("PAM: OCF_RegisterForEvent failed %d\n",
				    ocfReturnCode);
			ocf_cleanup(clientHandle);
			return (PAM_CONV_ERR);
		}
	}

	if (verbose)
		printf("PAM: Card Handle: %d\n", (int)cardHandle);

	/* Get user info */
	if (OCF_UserInfoCardService(OCF_UserInfoCardService_GetUserInfo,
	    clientHandle, cardHandle, "dtlogin", &userinfo) != OCF_Success) {
		if (verbose)
			printf("PAM: can't get user info\n");
		ocf_cleanup(clientHandle);
		return (PAM_AUTH_ERR);
	}

	/* If pam_user is set, compare with the name in the smart card */
	if ((pam_user != 0) &&
	    (strcasecmp(pam_user, userinfo.userName) != 0))
		return (PAM_AUTH_ERR);

	/* Set the username in pam handle */
	pam_set_item(pamh, PAM_USER, userinfo.userName);

	/* Pass the conversation function ocf_conv() pointer.  */
	ocfAuthReturnCode = OCF_Authenticate(clientHandle, cardHandle,
	    (void*) &scad, ocf_conv);

	if (ocfAuthReturnCode != OCFAuth_Success) {
		if (verbose)
			printf("PAM: OCF_Authenticate FAILED: %d\n",
			    ocfAuthReturnCode);
		ocf_cleanup(clientHandle);
		if (scad.pamStatus != PAM_SUCCESS)
			return (scad.pamStatus);
		else
			return (PAM_AUTH_ERR);
	}

	/* deregister OCF client */
	ocf_cleanup(clientHandle);
	return (PAM_SUCCESS);
}

static uint32_t
eventHandler(OCF_Event_t event, OCF_EventData_t eventData)
{
	OCF_ClientHandle_t clientHandle;
	OCF_EventHandle_t  eventHandle;
	event_handler_data_t *eventHandlerData;
	pam_handle_t	    *pamh;
	int		    status, debug, verbose;

	/* Get PAM parameters */
	eventHandlerData = (event_handler_data_t *) (eventData.clientdata);
	debug = eventHandlerData->debug;
	verbose = eventHandlerData->verbose;
	pamh = eventHandlerData->pamh;

	if (verbose)
		printf("PAM: eventHandler OCF_Event_t = 0x%x\n", event);

	/* Get EventHandler Data */
	clientHandle = eventData.clienthandle;

	/* Register for further events */
	if (event == OCF_Event_CardInserted) {
		if (verbose)
			printf("PAM: Card inserted event. Card Handle: %d\n",
			    (int) eventData.cardhandle);

		eventHandlerData->cardHandle = eventData.cardhandle;
		status = OCF_RegisterForEvent(clientHandle,
		    OCF_Event_CardRemoved, &(eventHandlerData->cardHandle),
		    NULL, &eventHandle);
	} else if (event == OCF_Event_CardRemoved) {
		if (verbose)
			printf("PAM: Card removed event\n");

		status = OCF_RegisterForEvent(clientHandle,
		    OCF_Event_CardInserted, eventHandlerData->cardSpec,
		    NULL, &eventHandle);
	}

	/* Delete the displayed message */
	if (event == OCF_Event_CardInserted ||
	    event == OCF_Event_CardRemoved)
		__pam_display_msg(pamh, PAM_CONV_INTERRUPT, 1, NULL, NULL);

	if (verbose)
		printf("PAM: Return from event handler. Status=%d\n", status);

	return (0);
}

/*
 * Conversation function for smartcard_unix module
 */
static int
smartcard_unix_conv(int num_msg, struct pam_message **msg,
    struct pam_response **response, void *appdata_ptr)
{
	struct pam_message	*m;
	struct pam_response	*r;
	char 			*temp;
	int			k, i;

	if (num_msg <= 0)
		return (PAM_CONV_ERR);

	*response = calloc(num_msg, sizeof (struct pam_response));
	if (*response == NULL)
		return (PAM_BUF_ERR);

	k = num_msg;
	m = *msg;
	r = *response;
	while (k--) {
		switch (m->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			temp = (char *) appdata_ptr;
			if (temp != NULL) {
				r->resp = strdup(temp);
				if (r->resp == NULL) {
					/* free responses */
					r = *response;
					for (i = 0; i < num_msg; i++, r++) {
						if (r->resp)
							free(r->resp);
					}
					free(*response);
					*response = NULL;
					return (PAM_BUF_ERR);
				}
			}

			m++;
			r++;
			break;

		case PAM_PROMPT_ECHO_ON:
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
		default:
			if (m->msg != NULL) {
				(void) fputs(m->msg, stdout);
				(void) fputs("\n", stdout);
			}
			m++;
			r++;
			break;


		}
	}
	return (PAM_SUCCESS);
}	

static int
unix_authenticate(const char *service, const char *username,
    const char *passwd)
{
	int status;
	pam_handle_t *pamh;
	struct pam_conv pam_convp;

	pam_convp.conv = &smartcard_unix_conv;
	pam_convp.appdata_ptr = (void *) passwd;
	if ((status = pam_start(service, username, &pam_convp,
	    &pamh)) != PAM_SUCCESS)
		return (status);

	status = pam_authenticate(pamh, 0);

	pam_end(pamh, PAM_SUCCESS);
	return (status);
}

/*
 * Part of pam_smartcard.so
 */
static uint32_t
ocf_conv(OCF_AuthOpTag_t tag, void* cd,
    OCF_AuthOpBuffer_t inbuf, OCF_AuthOpBuffer_t *outbuf)
{

	int		retcode = 0;
	int 		cnt_pin = 0, cnt_passwd = 0;
	char		*service, *user, *password;
	pam_handle_t    *pamh;
	void		*appdata_ptr;
	sc_appdata_t	*scd;
	validateUserInfo_t	*userInfo;

	char    messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	struct pam_response *ret_resp = (struct pam_response *)0;

	/* Get the appdata pointer and pam handle */
	scd = (sc_appdata_t *) cd;
	appdata_ptr = scd->appdata_ptr;
	pamh = scd->pamh;

	switch (tag) {
	case OCFAuth_Insert_Card:
		strcpy(messages[0], PAM_MSG(pamh, 34, 
		"Please Insert Smart Card or user name:"));
		retcode = __pam_display_msg(pamh, PAM_MSG_NOCONF, 1, messages,
		    appdata_ptr);
		if (scd->pamStatus == PAM_SUCCESS)
			scd->pamStatus = retcode;
		if (retcode == PAM_SUCCESS) {
			return (OCFAuth_Success);
		} else if (retcode == PAM_CONV_ERR) {
			return (OCFAuth_Conversation_Interrupt);
		} else
			return (OCFAuth_Internal_Error);

	case OCFAuth_Invalid_Card:
		strcpy(messages[0], PAM_MSG(pamh, 35, 
		"Invalid Card, Please remove"));
		retcode = __pam_display_msg(pamh, PAM_MSG_NOCONF, 1,
		    messages, appdata_ptr);
		if (scd->pamStatus == PAM_SUCCESS)
			scd->pamStatus = retcode;
		if (retcode == PAM_SUCCESS)
			return (OCFAuth_Retry_Operation);
		else if (retcode == PAM_CONV_ERR)
			return (OCFAuth_Conversation_Interrupt);
		else
			return (OCFAuth_Internal_Error);

	case OCFAuth_Enter_PIN_External:
		strcpy(messages[0], PAM_MSG(pamh, 36, 
		"Enter PIN on Card Reader"));
		retcode = __pam_display_msg(pamh, PAM_MSG_NOCONF, 1, messages,
		    appdata_ptr);
		if (scd->pamStatus == PAM_SUCCESS)
			scd->pamStatus = retcode;
		if (retcode == PAM_SUCCESS)
			return (OCFAuth_Success);
		else if (retcode == PAM_CONV_ERR)
			return (OCFAuth_Conversation_Interrupt);
		else
			return (OCFAuth_Internal_Error);

	case OCFAuth_Enter_PIN:
		/*
		 * Get the PIN from user, use PAM_AUTHTOK field to store PIN
		 * in pamh. Already assumed PAM_PROMT_ECHO_OFF message type
		 * here by calling __pam_get_authtok.
		 */

		strcpy(messages[0], PAM_MSG(pamh, 37, "Enter PIN:"));
		retcode = __pam_get_authtok(pamh, PAM_PROMPT, PAM_AUTHTOK,
		    PIN_LEN, messages[0], (char **) outbuf, appdata_ptr);
		if (scd->pamStatus == PAM_SUCCESS)
			scd->pamStatus = retcode;
		if (retcode == PAM_SUCCESS)
			return (OCFAuth_Success);
		else if (retcode == PAM_CONV_ERR)
			return (OCFAuth_Conversation_Interrupt);
		else
			return (OCFAuth_Internal_Error);

	case OCFAuth_Invalid_PIN:
		strcpy(messages[0], PAM_MSG(pamh, 38, "Invalid PIN"));
		if (++scd->retryPIN < MAX_RETRY_PIN) {
			retcode = __pam_display_msg(pamh, PAM_ERROR_MSG, 1,
			    messages, appdata_ptr);
			if (scd->pamStatus == PAM_SUCCESS)
				scd->pamStatus = retcode;
			if ((retcode == PAM_SUCCESS) ||
			    (retcode == PAM_CONV_ERR))
				return (OCFAuth_Retry_Operation);
			else
				return (OCFAuth_Internal_Error);
		} else {
			scd->pamStatus = PAM_AUTH_ERR;
			return (OCFAuth_Success);
		}

	case OCFAuth_Enter_Password:
		/* Get password from user, assumes OCF will do authentication */
		strcpy(messages[0], PAM_MSG(pamh, 39, "Enter Password:"));
		retcode = __pam_get_authtok(pamh, PAM_PROMPT, PAM_AUTHTOK,
		    PASSWORD_LEN, messages[0], (char **) outbuf, appdata_ptr);
		if (retcode != PAM_SUCCESS) {
			if (password) {
				memset(password, 0, strlen(password));
				free(password); 
			}
		}
		if (scd->pamStatus == PAM_SUCCESS)
			scd->pamStatus = retcode;
		if (retcode == PAM_SUCCESS)
			return (OCFAuth_Success);
		else if (retcode == PAM_CONV_ERR)
			return (OCFAuth_Conversation_Interrupt);
		else
			return (OCFAuth_Internal_Error);

	case OCFAuth_Invalid_Password:
		/*
		 * OCF does the authentication. If fails, ask login_conv to
		 * display error message.
		 *
		 * Since no retry is supported, return OK, ie., OCFAuth_Success
		 */
		/*  strcpy(messages[0], PAM_MSG(pamh, 40, "Invalid Password"));
		    retcode = __pam_display_msg(pamh, PAM_ERROR_MSG, 1, messages,
		    appdata_ptr);
		 */

		scd->pamStatus = PAM_AUTH_ERR;
		return (OCFAuth_Success);

	case OCFAuth_Verify_Password:
		/* Get user information */
		userInfo = (validateUserInfo_t *) inbuf;
		service = "smartcard_unix";
		user = userInfo->userName;
		password = userInfo->password;

		/* Perform unix authentication */
		retcode = unix_authenticate(service, user, password);

		if (scd->pamStatus == PAM_SUCCESS)
			scd->pamStatus = retcode;

		switch (retcode) {
                case PAM_MAXTRIES:
                        cnt_passwd = scd->retryPassword;
                case PAM_AUTH_ERR:
			return (OCFAuth_Password_Verify_ERROR);

                case PAM_AUTHINFO_UNAVAIL:
                case PAM_USER_UNKNOWN:
			return (OCFAuth_Password_Verify_UNABLE);

                case PAM_SUCCESS:
			return (OCFAuth_Password_Verify_OK);

                case PAM_NEW_AUTHTOK_REQD:
			/*
			 * pam_authenticate() shouldn't
			 * return this but might do in
			 * some cases.
			 */
			return (OCFAuth_Password_Verify_ERROR);

		case PAM_ABORT:
                        /*NOTREACHED*/

                default:        /* Some other PAM error */
			return (OCFAuth_Password_Verify_UNABLE);
                        /*NOTREACHED*/
                }

	case OCFAuth_Enter_Username:
		/*
		 * Get username from user, will this ever happen for smart card?
		 * Would the card will always has user name on it? Do we compare
		 * the card user name with user's input? - Need more explaination
		 */
		strcpy(messages[0], PAM_MSG(pamh, 41, "Enter user name:"));
		retcode = __pam_get_input(pamh, PAM_PROMPT_ECHO_ON, 1,
		    messages, appdata_ptr, &ret_resp);
		if (scd->pamStatus == PAM_SUCCESS)
			scd->pamStatus = retcode;
		if (retcode == PAM_SUCCESS)
			return (OCFAuth_Success);
		else if (retcode == PAM_CONV_ERR)
			return (OCFAuth_Conversation_Interrupt);
		else
			return (OCFAuth_Internal_Error);

		/*
		 * Or, in alternative, we can simple call pam_get_user() to
		 * get user name. But it does a lot more... we can combine
		 * OCFAuth_Invalid_Username into one if call pam_get_user()...
		 */

	case OCFAuth_Invalid_Username:
		/* Since no retry option is supported, it is commented */
		/*
		  strcpy(messages[0], PAM_MSG(pamh, 42, "Invalid user name"));
		  retcode = __pam_display_msg(pamh, PAM_ERROR_MSG, 1,
		  messages, appdata_ptr);
		 */
		scd->pamStatus = PAM_AUTH_ERR;
		return (OCFAuth_Success);

	case OCFAuth_Auth_Failure:
	case OCFAuth_Auth_Error:
	case OCFAuth_No_Card:
	case OCFAuth_TextMsg:
	case OCFAuth_TextMsg_NOCONF:
	case OCFAuth_TextMsg_INPUT:
	default:
		scd->pamStatus = PAM_AUTH_ERR;
		return (OCFAuth_Success);
	}
}

/* XXX dummy PAM code XXX */
int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{

	return (PAM_SUCCESS);
}

int
pam_sm_chauthtok(
	pam_handle_t		*pamh,
	int			flags,
	int			argc,
	const char		**argv)
{
	return (PAM_SUCCESS);
}

int
pam_sm_close_session(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	return (PAM_SUCCESS);
}

int
pam_sm_open_session(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	return (PAM_SUCCESS);
}

int
pam_sm_setcred(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	return (PAM_SUCCESS);
}
