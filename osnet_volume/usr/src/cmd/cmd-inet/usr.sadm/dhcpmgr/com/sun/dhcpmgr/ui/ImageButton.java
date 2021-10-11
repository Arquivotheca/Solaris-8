/*
 * @(#)ImageButton.java	1.2	99/05/05 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import javax.swing.JButton;
import javax.swing.ImageIcon;
import java.io.InputStream;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

/**
 * A button with an image loaded from a gif file.  To use this class, extend it
 * and in your constructor call setImage().
 */
public abstract class ImageButton extends JButton {
    
    /**
     * Sets the button's icon to the image loaded from the file; falls back the 
     * specified text if for some reason the icon can't be loaded.  Base class
     * is used to find the gif image in the same directory as the class that's
     * using it, a convention we use.
     * @param baseClass the name of the class we're doing this on behalf of
     * @param file the name of the file the gif image is stored in
     * @param text the text to use if the icon fails for some reason
     */
    public void setImage(Class baseClass, String file, String text) {
	try {
	    InputStream resource = baseClass.getResourceAsStream(file);
	    if (resource != null) {
		BufferedInputStream in = new BufferedInputStream(resource);
		ByteArrayOutputStream out = new ByteArrayOutputStream(1024);
		byte [] buffer = new byte[1024];
		int n;
		while ((n = in.read(buffer)) > 0) {
		    out.write(buffer, 0, n);
		}
		in.close();
		out.flush();
		buffer = out.toByteArray();
		setIcon(new ImageIcon(buffer));
	    }
	} catch (IOException ioe) {
	    setText(text);  // Fall back to text
	}
    }
}
