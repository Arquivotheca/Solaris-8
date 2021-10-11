/*
 * ident	"@(#)ServiceLocationAttributeDescriptor.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)ServiceLocationAttributeDescriptor.java	1.2	05/10/99
//  ServiceLocationAttributeDescriptor.java: Describes an SLP attribute.
//  Author:           James Kempf
//  Created On:       Thu Jun 19 10:38:01 1997
//  Last Modified By: James Kempf
//  Last Modified On: Fri May 22 13:01:18 1998
//  Update Count:     16
//

package com.sun.slp;

import java.util.*;

/**
 * Objects implementing the <b>ServiceLocationAttributeDescriptor</b>
 * interface return information on a particular service location attribute.
 * This information is primarily for GUI tools. Programmatic attribute
 * verification should be done through the
 * <b>ServiceLocationAttributeVerifier</b>.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 *
 */

public interface ServiceLocationAttributeDescriptor {

    /**
     * Return the attribute's id.
     *
     * @return A <b>String</b> for the attribute's id.
     */

    public String getId();		

    /**
     * Return the fully qualified Java type of the attribute. SLP types
     * are translated into Java types as follows:<br>
     *<ol>
     *	<li><i>STRING</i> -- <i>"java.lang.String"</i></li>
     *	<li><i>INTEGER</i> -- <i>"java.lang.Integer"</i></li>
     *	<li><i>BOOLEAN</i> -- <i>"java.lang.Boolean"</i></li>
     *	<li><i>OPAQUE</i> --  <i>"[B"</i> (i.e. array of byte,
     *			      <b>byte[]</b>)</li>
     *	<li><i>KEYWORD</i> -- null string, <i>""</i></li>
     *</ol>
     *
     * @return A <b>String</b> containing the Java type name for the
     *	      attribute values.
     */

    public String getValueType();		

    /**
     * Return attribute's help text.
     *
     * @return A <b>String</b> containing the attribute's help text.
     */

    public String getDescription();	

    /**
     * Return an <b>Enumeration</b> of allowed values for the attribute type.
     * For keyword attributes returns null. For no allowed values
     * (i.e. unrestricted) returns an empty <b>Enumeration</b>. Small memory
     * implementations may want to parse values on demand rather
     * than at the time the descriptor is created.
     *
     * @return An <b>Enumeration</b> of allowed values for the attribute,
     *         or null if the attribute is keyword.
     */

    public Enumeration getAllowedValues();

    /**
     * Return an <b>Enumeration</b> of default values for the attribute type.
     * For keyword attributes returns null. For no allowed values
     * (i.e. unrestricted) returns an empty <b>Enumeration</b>. Small memory
     * implementations may want to parse values on demand rather
     * than at the time the descriptor is created.
     *
     * @return An <b>Enumeration</b> of default values for the attribute or
     *         null if the attribute is keyword.
     */

    public Enumeration getDefaultValues();

    /**
     * Returns true if the <i>"M"</i> flag is set.
     *
     * @return True if the <i>"M"</i> flag is set.
     */

    public boolean getIsMultivalued();	

    /**
     * Returns true if the <i>"O"</i>" flag is set.
     *
     * @return True if the <i>"O"</i>" flag is set.
     */

    public boolean getIsOptional();		

    /**
     * Returns true if the <i>"X"</i>" flag is set.
     *
     * @return True if the <i>"X"</i> flag is set.
     */

    public boolean getRequiresExplicitMatch();

    /**
     * Returns true if the <i>"L"</i> flag is set.
     *
     * @return True if the <i>"L"</i> flag is set.
     */

    public boolean getIsLiteral();		

    /**
     * Returns <i>true</i> if the attribute is a keyword attribute.
     *
     * @return <i>true</i> if the attribute is a keyword attribute
     */

    public boolean getIsKeyword();		

}
