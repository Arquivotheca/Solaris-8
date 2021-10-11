/*
 * ident	"@(#)Host.java	1.4	99/05/04 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Host class
 * Methods associated with a host.
 */

package com.sun.admin.pm.server;

import java.io.*;

public class Host
{
    public static void main(String[] args)
    {
	try {
		System.out.println(getLocalHostName());
		System.out.println(getDomainName());
		System.out.println(getNisHost("master"));
	}
	catch (Exception e) {
		System.out.println(e);
	}
	System.exit(0);
    }

    //
    // Get the local hostname
    // Return an empty string if we don't find one.
    //
    public synchronized static String getLocalHostName()
	throws Exception
    {
	Debug.message("SVR: Host.getLocalHostName()");

	String cmd = "/usr/bin/hostname";
	SysCommand syscmd = new SysCommand();
	syscmd.exec(cmd);

	if (syscmd.getExitValue() != 0) {
		String err = syscmd.getError();
		syscmd = null;
		throw new pmCmdFailedException(err);
	}
	String o = syscmd.getOutput();
	syscmd = null;

	if (o == null)
		return (new String(""));
	return (new String(o));
    }

    //
    // Get the domainname
    // Return an empty string if we don't find one.
    //
    public synchronized static String getDomainName()
	throws Exception
    {
	Debug.message("SVR: Host.getDomainName()");

	String cmd = "/usr/bin/domainname";
	SysCommand syscmd = new SysCommand();
	syscmd.exec(cmd);
	if (syscmd.getExitValue() != 0) {
		String err = syscmd.getError();
		syscmd = null;
		throw new pmCmdFailedException(err);
	}

	String o = syscmd.getOutput();
	syscmd = null;

	if (o == null)
		return (new String(""));
	return (new String(o));
    }

    public synchronized static void pingHost(String host)
	throws Exception
    {
	int exitvalue;

	Debug.message("SVR: Host.pingHost()");

	SysCommand syscmd = new SysCommand();
	syscmd.exec("/usr/sbin/ping " + host);
	exitvalue = syscmd.getExitValue();
	syscmd = null;

	if (exitvalue != 0) {
		String err = syscmd.getError();
		throw new pmHostNotPingableException(err);
	}
    }

    public synchronized static String getNisMaster()
	throws Exception
    {
	return (getNisHost("master"));
    }

    //
    // Look for the nis server.
    // If we are looking for the master server first try
    // the printers.conf.byname map. If that fails
    // look for passwd.
    //
    public synchronized static String getNisHost(String type)
	throws Exception
    {
	Debug.message("SVR: Host.getNisHost() " + type);

	SysCommand syscmd = null;
	String cmd = null;
	int exitvalue = 0;

	if (type.equals("master")) {
		cmd = "/usr/bin/ypwhich -m printers.conf.byname";
	} else {
		cmd = "/usr/bin/ypwhich";
	}
	syscmd = new SysCommand();
	syscmd.exec(cmd);
	exitvalue = syscmd.getExitValue();
	if ((exitvalue != 0) && (type.equals("master"))) {
		Debug.message("SVR: printers.conf NIS host not found.");
		Debug.message("SVR: Looking for NIS passwd host.");
		cmd = "/usr/bin/ypwhich -m passwd";

		syscmd = new SysCommand();
		syscmd.exec(cmd);
		exitvalue = syscmd.getExitValue();
	}
	if (exitvalue != 0) {
		Debug.error("SVR: NIS server could not be found");
		String err = syscmd.getError();
		syscmd = null;
		throw new pmNSNotConfiguredException(err);
	}

	String o = syscmd.getOutput();
	syscmd = null;

	if (o == null) {
		throw new pmCmdFailedException(syscmd.getError());
	}
	o = o.trim();
	return (new String(o));
    }

    //
    // Check to see if a name service is configured
    //
    public synchronized static void isNSConfigured(String ns)
	throws Exception
    {
	Debug.message("SVR: Host.isNSConfigured() " + ns);

	int exitvalue;
	String cmd = null;
	String err = null;
	SysCommand syscmd = null;

	if (ns.equals("system")) {
		return;
	} else if (ns.equals("nis")) {
		cmd = "/usr/bin/ypwhich";
		syscmd = new SysCommand();
		syscmd.exec(cmd);
		exitvalue = syscmd.getExitValue();
		err = syscmd.getError();
		syscmd = null;

		if (exitvalue != 0) {
			throw new pmNSNotConfiguredException(err);
		}

		cmd = "/usr/bin/ypcat cred";
		syscmd = new SysCommand();
		syscmd.exec(cmd);
		exitvalue = syscmd.getExitValue();
		syscmd = null;
		if (exitvalue == 0) {
			Debug.warning(
			    "SVR: Discovered NIS+ server in yp compat mode.");
			Debug.warning(
			    "SVR: Unable to update this configuration.");
			throw new pmNSNotConfiguredException();
		}

	} else if (ns.equals("xfn")) {
		String dom = null;
		try {
			dom = getDomainName();
		}
		catch (Exception e) {
			throw new pmNSNotConfiguredException(e.getMessage());
		}
		cmd = "/usr/bin/niscat -o fns.ctx_dir." + dom;
		syscmd = new SysCommand();
		syscmd.exec(cmd);
		if (syscmd.getExitValue() != 0) {
			err = syscmd.getError();
			syscmd = null;
			throw new pmNSNotConfiguredException(err);
		}
		syscmd = null;
	} else if (ns.equals("nisplus")) {
		cmd = "/usr/bin/grep printers: /etc/nsswitch.conf";
		syscmd = new SysCommand();
		syscmd.exec(cmd);
		if (syscmd.getExitValue() != 0) {
			syscmd = null;
			Debug.message(
			    "SVR: nisplus is not supported for this system");
			throw new pmNSNotConfiguredException();
		}
		cmd = "/usr/bin/nisls";
		syscmd = new SysCommand();
		syscmd.exec(cmd);
		if (syscmd.getExitValue() != 0) {
			err = syscmd.getError();
			syscmd = null;
			throw new pmNSNotConfiguredException(err);
		}
		syscmd = null;
	} else if (ns.equals("ldap")) {
		throw new pmNSNotConfiguredException("ldap not supported");
	} else {
		throw new pmInternalErrorException(
		    "Unkown name service " + ns);
	}
    }
}
