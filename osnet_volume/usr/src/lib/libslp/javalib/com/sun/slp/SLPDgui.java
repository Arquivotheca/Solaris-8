/*
 * ident	"@(#)SLPDgui.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SLPDgui.java	1.2	05/10/99
//  %M : The service location daemon GUI.
//  Author:           Erik Guttman
//

package com.sun.slp;

/**
 * This GUI will allow the user of the slpd to monitor the daemon,
 * shut it off, manually enter services and stuff like that.
 *
 * @version 1.2 00/10/14
 * @author Erik Guttman
 */

import java.awt.*;
import java.util.*;

class SLPDgui extends Frame {
    public SLPDgui(String configFile) {

        super("slpd");

	Font font = new Font("SansSerif", Font.BOLD, 12);

	setFont(font);

	configFilename = configFile;

	setLayout(new BorderLayout());

	// Set up a panel displaying config file.
	Panel configPanel = new Panel();
	configPanel.setLayout(new FlowLayout());

	configPanel.add(new Label("Configuration file:", Label.CENTER));
	configPanel.add(new Label(configFile == null ? "":configFile));
	add("North", configPanel);

	taLog = new TextArea();
	taLog.setEditable(false);
	add("Center", taLog);

	Panel pS = new Panel();
	pS.setLayout(new FlowLayout());
	pS.add(new Button("Quit"));

	add("South", pS);

	// Use JDK 1.1 event model in compatibility mode w. 1.0.

	enableEvents(AWTEvent.WINDOW_EVENT_MASK);

	setSize(WIDTH, HEIGHT);
    }

    public void processEvent(AWTEvent evt) {
	if (evt.getID() == Event.WINDOW_DESTROY) {
	  try {
	    slpd.stop();

	  } catch (ServiceLocationException ex) {
	    SLPConfig conf = SLPConfig.getSLPConfig();
	    ResourceBundle bundle = conf.getMessageBundle(conf.getLocale());

	    slpd.errorExit(bundle, ex);

	  }
	}

	super.processEvent(evt);
    }

    TextArea getTALog() {

	return taLog;

    }

    // Size of main window.

    static private int WIDTH = 780;
    static private int HEIGHT = 750;

    private String configFilename;
    private TextArea taLog;

}
