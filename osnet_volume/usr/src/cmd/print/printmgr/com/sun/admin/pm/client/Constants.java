/*
 *
 * ident	"@(#)Constants.java	1.2	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Constants.java
 * Common constants for Printer Manager
 */

package com.sun.admin.pm.client;

/*
 * pmConstants.java
 * 	defines constants used with print manager
 */

public interface Constants
{
	// Buttons
	int OK = 1;
	int APPLY = 2;
	int RESET = 3;
	int CANCEL = 4;
	int HELP = 5;

	// Buttons for user access list
	int ADD = 6;
	int DELETE = 7;

	// Printer type to add/modify 
	int ADDLOCAL = 1; 
	int ADDNETWORK = 2;
	int MODIFYATTACHED = 3; 
	int MODIFYREMOTE = 4; 
	int MODIFYNETWORK = 5; 

	// Printer connection types
	int ATTACHED = 1;
	int NETWORK = 2;

	// Useful Constants
	int MAXPNAMELEN = 20;

	// Combo Listener 
	int PORT = 1;
	int TYPE = 2;

}
