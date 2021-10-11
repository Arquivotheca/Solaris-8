/*
 * ident	"@(#)pmNSNotConfiguredException.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmNSNotConfiguredException class
 * The selected name service does not appear to be configured
 * on the host.
 */

package com.sun.admin.pm.server;

public class pmNSNotConfiguredException extends pmException
{
    public pmNSNotConfiguredException()
    {
	super();
    }

    public pmNSNotConfiguredException(String s)
    {
	super(s);
    }
}
