/*
 * ident	"@(#)Advertiser.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)Advertiser.java	1.2	05/10/99
//  ServiceAgent.java: Interface for the SLP Service Agent operations
//  Author:           James Kempf
//  Created On:       Mon Jul  7 09:05:40 1997
//  Last Modified By: James Kempf
//  Last Modified On: Thu Jan  7 14:17:12 1999
//  Update Count:     16
//

package com.sun.slp;

import java.util.*;

/**
 * The Advertiser interface allows clients to register new service
 * instances with SLP and to change the attributes of existing services.
 *
 * @see ServiceLocationManager
 *
 * @version 1.2 00/10/14
 * @author James Kempf, Erik Guttman
 */

public interface Advertiser {

    /**
     * Return the Advertiser's locale object. 
     *
     * @return The Locale object.
     */

    Locale getLocale();

    /**
     * Register a new service with the service location protocol in
     * the Advertiser's locale.
     *
     * @param URL	The service URL for the service.
     * @param serviceLocationAttributes A vector of ServiceLocationAttribute
     *				       objects describing the service.
     * @exception ServiceLocationException An exception is thrown if the
     *					  registration fails.  
     * @exception IllegalArgumentException A  parameter is null or
     *					  otherwise invalid.
     *
     */

    public void register(ServiceURL URL,
			 Vector serviceLocationAttributes)
	throws ServiceLocationException;

    /**
     * Deregister a service with the service location protocol.
     * This has the effect of deregistering the service from <b>every</b>
     * Locale and scope under which it was registered.
     *
     * @param URL	The service URL for the service.
     * @exception ServiceLocationException An exception is thrown if the
     *					  deregistration fails.
     */

    public void deregister(ServiceURL URL)
	throws ServiceLocationException;

    /**
     * Add attributes to a service URL in the locale of the Advertiser.
     *
     * Note that due to SLP v1 update semantics, the URL will be registered
     * if it is not already.
     *
     *
     * @param URL	The service URL for the service.
     * @param serviceLocationAttributes A vector of ServiceLocationAttribute
     *				       objects to add.
     * @exception ServiceLocationException An exception is thrown if the
     *					  operation fails.
     */

    public void addAttributes(ServiceURL URL,
			      Vector serviceLocationAttributes)
	throws ServiceLocationException;

    /**
     * Delete the attributes from a service URL in the locale of
     * the Advertiser. The deletions are made for all scopes in
     * which the URL is registered.
     *
     *
     * @param URL	The service URL for the service.
     * @param attributeIds A vector of Strings indicating
     *			  the attributes to remove.
     * @exception ServiceLocationException An exception is thrown if the
     *					  operation fails.
     */

    public void deleteAttributes(ServiceURL URL,
				 Vector attributeIds)
	throws ServiceLocationException;
}
