#!/bin/sh

#--------------------------------------------------------------------------
#
# Name:     batch.k
#
# Purpose:  McIDAS ROUTE PostProcess BATCH file execution script runs
#           McIDAS-X BATCH files in a McIDAS-X v7.8+ environment
#
# Notes:    The sequence of events is as follows:
#
#           o the site administrator has built or installed the binary
#             version of the ldm-mcidas decoders that are compatible
#             with (i.e. linked against the library of) Unidata McIDAS-X
#             v7.8+
#
#           o A McIDAS-encoded product is received in the IDD-broadcast
#             Unidata-Wisconsin datastream
#
#           o the site administrator has setup his/her LDM pqact file
#             to run the ldm-mcidas decoder pnga2area to convert the
#             PNG-compressed product into a McIDAS-compatible AREA file
#
#           o the site McIDAS administrator has setup file routing for the
#             various products that are in the Unidata-Wisconsin datastream,
#             and there are some ROUTE PostProcesses that are to be 
#             executed upon successful receipt of products.  The routing
#             and PostProcess BATCH file information is stored in the
#             McIDAS-X routing table, ROUTE.SYS that is located in the
#             directory in which the ldm-mcidas decoders create their
#             output data products.
#
#           o the site administrator has made sure that the McIDAS-X routing
#             table, ROUTE.SYS, file has been copied to the directory in
#             which the ldm-mcidas decoder(s) will create their output
#
#             *** NOTE ***
#             ROUTE.SYS must be readable AND writable by the user running
#             the LDM processes
#
#           o the site administrator has made sure that the McIDAS-X System
#             Key Table, SYSKEY.TAB, has been copied to the directory in
#             which the ldm-mcidas decoder(s) will create their output
#
#             *** NOTE ***
#             SYSKEY.TAB must be readable AND writable by the user running
#             the LDM processes
#
#           o this file, batch.k, is located in a directory which is in
#             the search PATH of the user running the LDM processes
#
#             *** NOTE ***
#             if the directory containing the McIDAS executables is also
#             in PATH, then the directory containing this file (batch.k )
#             must be placed before the directory containing the McIDAS
#             commands (this is crutial)
#
#           o the site administrator configuring this script needs to
#             define the following environment variables used by this
#             script:
#
#             MCHOME - the home directory of the user 'mcidas'
#
#             MCDATA - the working directory of the user 'mcidas'
#                      (default is /home/mcidas/workdata)
#
#             MCPATH - a colon-separated list of directories that McIDAS-X
#                      processes will search when looking for auxiliary
#                      data files, help files, and "real" data files.
#                      The first directory in MCPATH must be the directory
#                      defined in MCDATA
#
#   NOTE:     The combination of MCDATA and MCPATH will insure that the
#             ROUTE PP BATCH files will "inherit" the environment setup
#             by the user 'mcidas'.  In particular, the 'mcidas' REDIRECTions
#             are needed in order to locate McIDAS image data (AREA) files.
#             The directories in MCPATH are needed so that the ROUTE PP
#             BATCH files can locate auxiliary data files (map databases,
#             BATCH and McBASI scripts, enhancements, stretches, etc.).
#
#             MCLOG  - fully qualified pathname of the file in which 
#                      BATCH commands will be logged
#
#             PATH -   a search PATH that contains the directory of
#                      McIDAS executables.  NOTE that in this PATH
#                      the McIDAS bin directory MUST be before the
#                      directory in which this file, batch.k, exists
#                      (do not confuse this with the seemingly contrary
#                      statement about PATHs above)
#
#             LD_LIBRARY_PATH - the search path for sharable libraries;
#                      this should be the same search path as the one
#                      used by the McIDAS session
#
#             NOTE: AIX and HP-UX users need to change LD_LIBRARY_PATH
#                   everywhere in this file to what is appropriate on
#                   their systems
#
#                   AIX:  LD_LIBRARY_PATH -> LIBPATH
#                   HPUX: LD_LIBRARY_PATH -> SHLIB_PATH
#
# History:  19960722 - Adapted from V2.102 batch.mx kludge for running
#                      McIDAS-X 7.10 processes from ldm-mcidas decoders
#                      linked against the McIDAS-X 2.102 library
#           19970109 - Updated for ldm-mcidas-7.10 and McIDAS-7.10
#           20020405 - Updated for ldm-mcidas-7.8.0
#           20021220 - Updated for ldm-mcidas-2002b
#           20040914 - Changed default location for log directory to ~ldm/logs
#                      Changed specification of PATH to be platform-independent
#                      Changed LD_LIBRARY_PATH specification to account for OS
#
#--------------------------------------------------------------------------

# Make sure that everything run from this script executes in the Bourne Shell
SHELL=/bin/sh
export SHELL

# Define environment variables needed by McIDAS

MCHOME=/home/mcidas
MCDATA=$MCHOME/workdata
MCGUI=$MCHOME/bin
# MCLOG=$MCDATA/ROUTEPP.LOG
MCLOG=/usr/local/ldm/logs/ROUTEPP.LOG
MCPATH=${MCDATA}:$MCHOME/data:$MCHOME/help
export MCPATH

# Add the MCGUI directory to the front of the PATH
#
# NOTE:  PATH _must_ include the directory containing 'sed'.  If the
#        PATH set for the user running 'batch.k' does not contain this
#        directory, add it to the specification below

PATH=${MCGUI}:$PATH
export PATH

# Set LD_LIBRARY_PATH

case `uname -s` in
   AUX)
      LIBPATH=$MCHOME/lib:/usr/lib:/lib
      export LIBPATH
   ;;
   HP-UX)
      SHLIB_PATH=$MCHOME/lib:/usr/lib:/lib
      export SHLIB_PATH
   ;;
   SunOS)
      LD_LIBRARY_PATH=/usr/openwin/lib:/opt/SUNWspro/lib:$MCHOME/lib:/usr/ucblib:/usr/lib:/lib
      export LD_LIBRARY_PATH
   ;;
   *)
      LD_LIBRARY_PATH=$MCHOME/lib:/usr/lib:/lib
      export LD_LIBRARY_PATH
   ;;
esac

# Send the McIDAS command line to the log file
echo "$@" >> $MCLOG

# Now eliminate the first command line parameter (which is 'BATCH')
shift

# cd to the $MCDATA directory
cd $MCDATA

# Send all messages to the log file
exec 2>> $MCLOG 1>&2

# Run the BATCH command passing along the needed environment
#
# NOTE: if your ROUTE PostProcess BATCH file needs to access frames
#       other than frame #1, and/or if you want to specify use of more
#       than the default number of image or graphics levels, you must
#       modify the line executing batch.k:
#
#       change:
#              mcenv -f 1@480x640 batch.k "$@"
#
#       to:
#              mcenv -f num@LINxELE <-i num> <-g num> batch.k "$@"
#
#              -f values:
#              num - number of frames to create
#              LIN - number of LINes in each frame
#              ELE - number of ELEments in each frame
#
#              -g value:
#              num - number of graphic levels (default = 8)
#
#              -i value:
#              num - number of image levels (default = 48)
#
#       Examples:
#
#              mcenv -f 16@480x640 batch.k "$@"
#              mcenv -f 16@480x640 -g 32 -i 128 batch.k "$@"
#

mcenv -f 1@480x640 batch.k "$@"

# All done!
exit
