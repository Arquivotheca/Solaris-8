/*
 * ident	"@(#)NameService.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * NameService class
 * Methods and state associated with a name service.
 */

package com.sun.admin.pm.server;

import java.io.*;

public class NameService
{
    private String nameservice = null;
    private String nshost = null;
    private String user = null;
    private String passwd = null;
    private boolean boundtonisslave = false;
    private boolean isauth = false;

    //
    // Constructors
    //
    // This constructor is used internally in the server package.
    public NameService()
    {
	nameservice = "system";
	isauth = true;
    }
    // This constructor should always be used by the client.
    public NameService(String nsname) throws Exception
    {
	if ((nsname.equals("system")) ||
	    (nsname.equals("nis")) ||
	    (nsname.equals("nisplus")) ||
	    (nsname.equals("xfn")) ||
	    (nsname.equals("ldap"))) {
		nameservice = nsname;
	} else {
		throw new pmInternalErrorException(
			"Unknown name service: " + nsname);
	}

	Host h = new Host();
	h.isNSConfigured(nameservice);

	if (nsname.equals("nis")) {
		String nm = h.getNisHost("master");
		String nb = h.getNisHost("bound");
		if (!nm.equals(nb)) {
			boundtonisslave = true;
		}
		setUser("root");
		setNameServiceHost(nm);
		setPasswd("");
	}
    }

    public void setNameServiceHost(String arg)
    {
	nshost = arg;
    }
    public void setUser(String arg)
    {
	user = arg;
    }
    public void setPasswd(String arg)
    {
	passwd = arg;
    }

    public String getNameService()
    {
	return (nameservice);
    }
    public String getNameServiceHost()
    {
	return (nshost);
    }
    public String getUser()
    {
	return (user);
    }
    public String getPasswd()
    {
	return (passwd);
    }
    public boolean getBoundToNisSlave()
    {
	return (boundtonisslave);
    }
    public boolean isAuth()
    {
	return (isauth);
    }

    public void checkAuth() throws Exception
    {
	Debug.message("SVR: NameService.checkAuth()");

	DoPrinterNS.doAuth(this);
	isauth = true;
    }
}
