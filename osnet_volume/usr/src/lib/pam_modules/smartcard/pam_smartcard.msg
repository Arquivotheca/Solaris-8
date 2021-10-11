$ *************************************<+>*************************************
$ *****************************************************************************
$ **
$ **  File:        pam_smartcard.msg
$ **
$ **  Project:     smartcard pam module
$ **
$ **  Description:
$ **  -----------
$ **  This file is the source for the pam_smartcard message catalog.
$ **
$ **  It contains messages for the pam_smartcard service module (set 1).
$ **
$ *****************************************************************************
$ **
$ **     (c) Copyright 1998 Sun Microsystems, Inc.
$ **     All Rights reserved
$ **
$ *****************************************************************************
$ *************************************<+>*************************************

$ *****************************************************************************
$
$ ***** NOTE FOR MESSAGE CATALOG TRANSLATORS *****
$
$ There may be three types of messages in this file:
$
$  1. Messages that appear in dialogs or are displayed to the user.
$
$     These messages are the default and they should ALL BE LOCALIZED.
$     Note that these messages do NOT have any identification (see the
$     comments for type 2 and 3 below).
$
$  2. Messages that only appear in the error log file.
$
$     The localization of these messages is OPTIONAL.  These messages are
$     identified by the following:
$
$  MESSAGES xx-yy IN SET zz WILL ONLY APPEAR IN THE DT ERRORLOG FILE
$
$  3. Messages that should not be localized.
$
$     These messages are identified by the following:
$
$  DO NOT TRANSLATE or CHANGE or LOCALIZE MESSAGES xx-yy from set zz
$
$ ***** END (NOTE FOR MESSAGE CATALOG TRANSLATORS) *****
$
$ ******************************************************************************

$ smartcard module messages
$set 1

$ smartcard_authenticate.c
30 Please Insert Smart Card (or) Enter Username:
31 Please Insert Smart Card (or) Enter Username and Press Enter: 
32 Please Insert Smart Card: 
33 Please Insert Smart Card and Press Enter: 
34 Please Insert Smart Card or user name:
35 Invalid Card, Please remove
36 Enter PIN on Card Reader
37 Enter PIN:
38 Invalid PIN
39 Enter Password:
40 Invalid Password
41 Enter user name:
42 Invalid user name

$ smartcard_acct_mgmt.c
20 Your password has been expired for too long.
21 Please contact the system administrator
22 Your password will expire within 24 hours.
23 Your password will expire in %d day.
24 Your password will expire in %d days.

$ smartcard_setcred.c
50 removing root credentials would break the rpc services that
51 use secure rpc on this host!
52 root may use keylogout -f to do this (at your own risk)!
53 Could not unset your secret key(s).
54 Maybe the keyserver is down?
55 Warning: NFS credentials not destroyed
56 Password does not decrypt any secret keys for %s.
57 Could not set secret key(s) for %s. The key server may be down.

