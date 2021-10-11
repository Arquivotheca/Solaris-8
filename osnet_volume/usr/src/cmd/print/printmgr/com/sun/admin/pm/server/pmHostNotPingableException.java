/*
 * ident	"@(#)pmHostNotPingableException.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmHostNotPingableException class
 * A host is not accessible using ping.
 */

package com.sun.admin.pm.server;

public class pmHostNotPingableException extends pmException
{
    public pmHostNotPingableException()
    {
	super();
    }

    public pmHostNotPingableException(String s)
    {
	super(s);
    }
}
