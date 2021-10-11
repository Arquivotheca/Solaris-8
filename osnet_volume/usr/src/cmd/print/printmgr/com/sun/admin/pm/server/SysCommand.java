/*
 * ident	"@(#)SysCommand.java	1.2	99/08/04 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * SysCommand
 * Execute a command and capture stdout/stderr.
 *
 */

package com.sun.admin.pm.server;

import java.io.*;

public class SysCommand
{

    private Process p = null;
    private String out = null;
    private String err = null;
    private int status = 0;

    public static void main(String[] args) {
	SysCommand syscmd = new SysCommand();
	String cmd = "ypcat hosts";
	String o = "";
	try {
		syscmd.exec(cmd);
	}
	catch (Exception e) {
		System.out.println(e);
	}
	o = syscmd.getOutput();
	System.out.println(o);
    }
	
    /*
     * Execute a system command.
     * @param String cmd The command to be executed.
     */
    public void exec(String cmd) throws Exception
    {
	if (cmd == null) {
		throw new pmInternalErrorException(
		    "SysCommand.exec(): null command");
	}

	Debug.message("SVR: " + cmd);

	p = Runtime.getRuntime().exec(cmd);
	if (p == null) {
		throw new pmInternalErrorException(
		    "SysCommand.exec(): null process");
	}
	out = readOut();
	err = readErr();
	p.waitFor();
	status = getStatus();
	dispose();
    }

    public void exec(String[] cmd) throws Exception
    {
	if (cmd == null) {
		throw new pmInternalErrorException(
		    "SysCommand.exec(): null command");
	}

	// Trim command arrays with nulls at the end.
	int i;
	for (i = 0; i < cmd.length; i++) {
		if (cmd[i] == null) {
			break;
		}
	}
	if (i != cmd.length) {
		String[] newcmd = new String[i];

		for (i = 0; i < newcmd.length; i++) {
			newcmd[i] = cmd[i];
		}
		Debug.message("SVR: " + PrinterDebug.arr_to_str(newcmd));
		p = Runtime.getRuntime().exec(newcmd);
	} else {
		Debug.message("SVR: " + PrinterDebug.arr_to_str(cmd));
		p = Runtime.getRuntime().exec(cmd);
	}
	if (p == null) {
		throw new pmInternalErrorException(
		    "SysCommand.exec(): null process");
	}
	out = readOut();
	err = readErr();
	p.waitFor();
	status = getStatus();
	dispose();
    }


    public void exec(String cmd, String locale) throws Exception
    {
	if (cmd == null) {
		throw new pmInternalErrorException(
		    "SysCommand.exec(): null command");
	}

	Debug.message("SVR: " + locale + "; " + cmd);

	String [] envp = new String[1];
	envp[0] = locale;
	p = Runtime.getRuntime().exec(cmd, envp);
	if (p == null) {
		throw new pmInternalErrorException(
		    "SysCommand.exec(): null process");
	}
	out = readOut();
	err = readErr();
	p.waitFor();
	status = getStatus();
	dispose();
    }

    public String getOutput() {
	if (out == null)
		return (null);
	return (new String(out));
    }
    public String getError() {
	if (err == null)
		return (null);
	return (new String(err));
    }
    public int getExitValue() {
	return (status);
    }


    private String readOut() throws Exception
    {
	String result = null;
	String line = null;
	BufferedReader out = null;

	out = new BufferedReader(
	    new InputStreamReader(p.getInputStream()));
	while ((line = out.readLine()) != null) {
		if (result == null)
			result = line;
		else
			result = result.concat("\n" + line);
	}
	return (result);
    }

    private String readErr() throws Exception
    {
	String errstr = null;
	String line = null;
	BufferedReader err = null;

	err = new BufferedReader(
	    new InputStreamReader(p.getErrorStream()));
	while ((line = err.readLine()) != null) {
		if (errstr == null) {
			errstr = line;
		} else {
			errstr = errstr.concat("\n" + line);
		}
	}
	return (errstr);
    }

    private int getStatus() throws Exception
    {
	return (p.exitValue());
    }

    /*
     * Clean up opened file descriptors.
     */
    private void dispose() {

	try {
		p.getInputStream().close();
		p.getOutputStream().close();
		p.getErrorStream().close();
		p.destroy();
	}
	catch (Exception e) {
		Debug.message("SVR:" + e.getMessage());
	}
    }
}
