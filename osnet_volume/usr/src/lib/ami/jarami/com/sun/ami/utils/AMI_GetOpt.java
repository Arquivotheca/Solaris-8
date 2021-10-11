/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_GetOpt.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;

// OVERVIEW:
//
// GetOpt provides a general means for a Java program to parse command
// line arguments in accordance with the standard Unix conventions;
// it is analogous to, and based on, getopt(3) for C programs.
// (The following documentation is based on the man page for getopt(3).)

// DESCRIPTION:
//
// GetOpt is a Java class that provides one method, getopt,
// and some variables that control behavior of or return additional
// information from getopt.
//
// GetOpt interprets command arguments in accordance with the standard
// Unix conventions: option arguments of a command are introduced by "-"
// followed by a key character, and a non-option argument terminates
// the processing of options.  GetOpt's option interpretation is controlled
// by its parameter optString, which specifies what characters designate
// legal options and which of them require associated values.
//
// The getopt method returns the next, moving left to right, option letter
// in the command line arguments that matches a letter in optString.
// optString must contain the option letters the command using getopt
// will recognize.  For example, getopt("ab") specifies that the command
// line should contain no options, only "-a", only "-b", or both "-a" and
// "-b" in either order.  (The command line can also contain non-option
// arguments after any option arguments.)  Multiple options per argument
// are allowed, e.g., "-ab" for the last case above.
//
// If a letter in optString is followed by a colon, the option is expected
// to have an argument.  The argument may or may not be separated by
// whitespace from the option letter.  For example, getopt("w:") allows
// either "-w 80" or "-w80".  The variable optArg is set to the option
// argument, e.g., "80" in either of the previous examples.  Conversion
// functions such as Integer.parseInt(), etc., can then be applied to
// optArg.
//
// getopt places in the variable optIndex the index of the next command
// line argument to be processed; optIndex is automatically initialized
// to 1 before the first call to getopt.
//
// When all options have been processed (that is, up to the first
// non-option argument), getopt returns optEOF (-1).  getopt recognizes the
// command line argument "--" (i.e., two dashes) to delimit the end of
// the options; getopt returns optEOF and skips "--".  Subsequent,
// non-option arguments can be retrieved using the String array passed to
// main(), beginning with argument number optIndex.

// DIAGNOSTICS:
//
// getopt prints an error message on System.stderr and returns a question
// mark ('?') when it encounters an option letter in a command line argument
// that is not included in optString.  Setting the variable optErr to
// false disables this error message.

// NOTES:
//
// The following notes describe GetOpt's behavior in a few interesting
// or special cases; these behaviors are consistent with getopt(3)'s
// behaviors.
// -- A '-' by itself is treated as a non-option argument.
// -- If optString is "a:" and the command line arguments are "-a -x",
//    then "-x" is treated as the argument associated with the "-a".
// -- Duplicate command line options are allowed; it is up to user to
//    deal with them as appropriate.
// -- A command line option like "-b-" is considered as the two options
//    "b" and "-" (so "-" should appear in option string); this differs
//    from "-b --".
// -- Sun and DEC getopt(3)'s differ w.r.t. how "---" is handled.
//    Sun treats "---" (or anything starting with "--") the same as "--"
//    DEC treats "---" as two separate "-" options
//    (so "-" should appear in option string).
//    Java GetOpt follows the DEC convention.
// -- An option `letter' can be a letter, number, or most special character.
//    Like getopt(3), GetOpt disallows a colon as an option letter.

public class AMI_GetOpt {

   private String[] theArgs = null;
   private int argCount = 0;
   private String optString = null;
   private int optIndex = 0;
   private String optArg = null;
   private int optPosition = 1;

   public static final int optEOF = -1;
   public int optIndexGet() {return optIndex;}

   public AMI_GetOpt(String[] args, String opts) {
      theArgs = args;
      argCount = theArgs.length;
      optString = opts;
   }

   // user can toggle this to control printing of error messages
   public boolean optErr = false;

   public int processArg(String arg, int n) {
      int value;
      try {
         value = Integer.parseInt(arg);
      } catch (NumberFormatException e) {
         if (optErr)
            System.err.println("processArg cannot process " + arg
               + " as an integer");
         return n;
      }
      return value;
   }

   public int tryArg(int k, int n) {
      int value;
      try {
         value = processArg(theArgs[k], n);
      } catch (ArrayIndexOutOfBoundsException e) {
         if (optErr)
            System.err.println("tryArg: no theArgs[" + k + "]");
            return n;
      }
      return value;
   }

   public long processArg(String arg, long n) {
      long value;
      try {
         value = Long.parseLong(arg);
      } catch (NumberFormatException e) {
         if (optErr)
            System.err.println("processArg cannot process " + arg
               + " as a long");
         return n;
      }
      return value;
   }

   public long tryArg(int k, long n) {
      long value;
      try {
         value = processArg(theArgs[k], n);
      } catch (ArrayIndexOutOfBoundsException e) {
         if (optErr)
            System.err.println("tryArg: no theArgs[" + k + "]");
            return n;
      }
      return value;
   }

   public double processArg(String arg, double d) {
      double value;
      try {
         value = Double.valueOf(arg).doubleValue();
      } catch (NumberFormatException e) {
         if (optErr)
            System.err.println("processArg cannot process " + arg
               + " as a double");
         return d;
      }
      return value;
   }

   public double tryArg(int k, double d) {
      double value;
      try {
         value = processArg(theArgs[k], d);
      } catch (ArrayIndexOutOfBoundsException e) {
         if (optErr)
            System.err.println("tryArg: no theArgs[" + k + "]");
            return d;
      }
      return value;
   }

   public float processArg(String arg, float f) {
      float value;
      try {
         value = Float.valueOf(arg).floatValue();
      } catch (NumberFormatException e) {
         if (optErr)
            System.err.println("processArg cannot process " + arg
               + " as a float");
         return f;
      }
      return value;
   }

   public float tryArg(int k, float f) {
      float value;
      try {
         value = processArg(theArgs[k], f);
      } catch (ArrayIndexOutOfBoundsException e) {
         if (optErr)
            System.err.println("tryArg: no theArgs[" + k + "]");
            return f;
      }
      return value;
   }

   public boolean processArg(String arg, boolean b) {
      // `true' in any case mixture is true; anything else is false
      return Boolean.valueOf(arg).booleanValue();
   }

   public boolean tryArg(int k, boolean b) {
      boolean value;
      try {
         value = processArg(theArgs[k], b);
      } catch (ArrayIndexOutOfBoundsException e) {
         if (optErr)
            System.err.println("tryArg: no theArgs[" + k + "]");
            return b;
      }
      return value;
   }

   public String tryArg(int k, String s) {
      String value;
      try {
         value = theArgs[k];
      } catch (ArrayIndexOutOfBoundsException e) {
         if (optErr)
            System.err.println("tryArg: no theArgs[" + k + "]");
            return s;
      }
      return value;
   }

   private static void writeError(String msg, char ch) {
      System.err.println("AMI_GetOpt: " + msg + " -- " + ch);
   }

   public void incrementOptIndex() {optIndex++;}

   public String optArgGet() {return optArg;}

   public int getopt() {
      optArg = null;

      if (theArgs == null || optString == null) 
	    return optEOF;
      if (optIndex < 0 || optIndex >= argCount) 
	    return optEOF;

      String thisArg = theArgs[optIndex];
      int argLength = thisArg.length();
      // handle special cases
      if (argLength <= 1 || thisArg.charAt(0) != '-') {
         // e.g., "", "a", "abc", or just "-"
         return optEOF;
      } else if (thisArg.equals("--")) { // end of non-option args
         optIndex++;
         return optEOF;
      }

      // get next "letter" from option argument
      char ch = thisArg.charAt(optPosition);
      // find this option in optString
      int pos = optString.indexOf(ch);
      if (pos == -1 || ch == ':') {
         if (optErr) writeError("illegal option", ch);
         ch = '?';
      } else { // handle colon, if present
         if (pos < optString.length()-1 && optString.charAt(pos+1) == ':') {
            if (optPosition != argLength-1) {
               // take rest of current arg as optArg
               optArg = thisArg.substring(optPosition+1);
               optPosition = argLength-1; // force advance to next arg below
            } else { // take next arg as optArg
               optIndex++;
               if (optIndex < argCount
                     && (theArgs[optIndex].charAt(0) != '-' ||
                     theArgs[optIndex].length() >= 2 &&
                     (optString.indexOf(theArgs[optIndex].charAt(1)) == -1
                     || theArgs[optIndex].charAt(1) == ':'))) {
                  optArg = theArgs[optIndex];
               } else {
                  if (optErr) writeError("option requires an argument", ch);
                  optArg = null;
                  ch = ':'; // Linux man page for getopt(3) says : not ?
               }
            }
         }
      }
      // advance to next option argument,
      // which might be in thisArg or next arg
      optPosition++;
      if (optPosition >= argLength) {
         optIndex++;
         optPosition = 1;
      }
      return ch;
   }

  public static void main(String[] args) {  // test the class
      AMI_GetOpt go = new AMI_GetOpt(args, "Uab:f:h:w:");
      go.optErr = true;
      int ch = -1;
      // process options in command line arguments
      boolean usagePrint = false;                 // set
      int aflg = 0;                               // default
      boolean bflg = false;                       // values
      String filename = "out";                    // of
      int width = 80;                             // options
      double height = 1;                          // here
      while ((ch = go.getopt()) != go.optEOF) {
         if      ((char)ch == 'U') usagePrint = true;
         else if ((char)ch == 'a') aflg++;
         else if ((char)ch == 'b')
            bflg = go.processArg(go.optArgGet(), bflg);
         else if ((char)ch == 'f') filename = go.optArgGet();
         else if ((char)ch == 'h')
            height = go.processArg(go.optArgGet(), height);
         else if ((char)ch == 'w')
            width = go.processArg(go.optArgGet(), width);
         else System.exit(1);                     // undefined option
      }                                           // getopt() returns '?'
      if (usagePrint) {
         System.out.println("Usage: -a -b bool -f file -h height -w width");
         System.exit(0);
      }
      System.out.println("These are all the command line arguments " +
         "before processing with AMI_GetOpt:");
      for (int i=0; i<args.length; i++) System.out.print(" " + args[i]);
      System.out.println();
      System.out.println("-U " + usagePrint);
      System.out.println("-a " + aflg);
      System.out.println("-b " + bflg);
      System.out.println("-f " + filename);
      System.out.println("-h " + height);
      System.out.println("-w " + width);
      // process non-option command line arguments
      for (int k = go.optIndexGet(); k < args.length; k++) {
         System.out.println("normal argument " + k + " is " + args[k]);
      }
   }
}

/* ............... Example compile and run(s)

D:\>javac AMI_GetOpt.java

D:\>java AMI_GetOpt -aaa -b true -f theFile -w -80 -h3.33 arg1 arg2

These are all the command line arguments before processing with AMI_GetOpt:
 -aaa -b true -f theFile -w -80 -h3.33 arg1 arg2
 -U false
 -a 3
 -b true
 -f theFile
 -h 3.33
 -w -80
 normal argument 8 is arg1
 normal argument 9 is arg2

D:\>java AMI_GetOpt -aaa -x -w90

AMI_GetOpt: illegal option -- x

D:\>java AMI_GetOpt -af theFile -w -b true

AMI_GetOpt: option requires an argument -- w
                                            ... end of example run(s)  */

