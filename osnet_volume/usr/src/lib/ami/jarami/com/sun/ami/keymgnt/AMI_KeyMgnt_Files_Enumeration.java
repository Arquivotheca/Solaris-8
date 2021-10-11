/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgnt_Files_Enumeration.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.util.Enumeration;
import java.io.*;

public class AMI_KeyMgnt_Files_Enumeration implements Enumeration {
	boolean Completed;
	int entryPointer;
	String entries;

	public AMI_KeyMgnt_Files_Enumeration(String filename)
	    throws AMI_KeyMgntException {
		int data_len = 0;
		entries = null;
		try {
			int data_read;
			byte line[] = new byte[1024];
			FileInputStream file = new FileInputStream(filename);
			while ((data_read = file.read(line)) != -1) {
				data_len += data_read;
				if (entries == null)
					entries =
					    (new String(line)).toLowerCase();
				else
					entries = entries +
					    (new String(line)).toLowerCase();
			}
		} catch (FileNotFoundException e) {
			throw new AMI_KeyMgntException(
			    "File name not provided");
		} catch (IOException i) {
			throw new AMI_KeyMgntException(
			    "Error while reading file: " + filename);
		}
		if (entries != null) {
			byte data[] = new byte[data_len];
			byte string_byte[] = entries.getBytes();
			for (int i = 0; i < data_len; i++)
				data[i] = string_byte[i];
			entries = new String(data);
			Completed = false;
		} else
			Completed = true;
		entryPointer = 0;
	}

	public boolean hasMoreElements() {
		if ((Completed) || (entryPointer+1 >= entries.length()))
			return false;
		else
			return true;
	}

	private void removeComments() {
		char c;
		while ((entryPointer < entries.length()) &&
		    ((c = entries.charAt(entryPointer)) == '#')) {
			// Advance until new line ie., \n
			int newLine = entries.indexOf('\n', entryPointer);
			if (newLine != -1)
				entryPointer = newLine + 1;
			else
				entryPointer = entries.length();
		}
	}

	public Object nextElement() {
		String answer = null;
		char c = '\0';
		boolean inQuotes = false;

		// Removed empty feilds
		while (entryPointer < entries.length()) {
			c = entries.charAt(entryPointer);
			if ((c != ' ') && (c != '\t') &&
			    (c != '\n')) {
				if (c == '#')
					removeComments();
				else
					break;
			} else
				entryPointer++;
		}

		// Get the string
		while (entryPointer < entries.length()) {
			c = entries.charAt(entryPointer);
			if (c == '"') {
				inQuotes = !inQuotes;
				entryPointer++;
				continue;
			}
			if (!inQuotes && ((c == ' ') || (c == '\t') ||
			    (c == '\n') || (c == '#')))
				break;
			if (answer == null)
				answer = (new Character(c)).toString();
			else
				answer = answer + (new Character(c)).toString();
			entryPointer++;
		}

		// Remove the comment lines, if any
		removeComments();
		return (answer);
	}
}
