$ *************************************<+>*************************************
$ *****************************************************************************
$ **
$ **  File:        pam_ami.msg
$ **
$ **  Project:     ami pam module
$ **
$ **  Description:
$ **  -----------
$ **  This file is the source for the pam_ami message catalog.
$ **
$ **  It contains messages for the pam_ami service module (set 1).
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

$ ami module messages
$set 1

$ ami_chauthtok.c
1 Enter old AMI password: 
2 New AMI password: 
3 Re-enter new AMI password: 
4 A user cannot change AMI password for another user
5 %s (AMI): passwd successfully changed for %s

$ ami_authenticate.c
10 AMI Password: 

