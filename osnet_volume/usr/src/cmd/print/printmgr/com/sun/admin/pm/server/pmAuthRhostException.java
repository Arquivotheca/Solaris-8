/*
 * ident	"@(#)pmAuthRhostException.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmAuthRhostException class
 * A .rhosts entry has not been set up allowing updates of
 * pre-2.6 nis servers.
 *
 */

package com.sun.admin.pm.server;

public class pmAuthRhostException extends pmAuthException
{
    public pmAuthRhostException()
    {
	super();
    }

    public pmAuthRhostException(String s)
    {
	super(s);
    }
}
