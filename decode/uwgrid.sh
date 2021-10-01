#!/bin/sh

#--------------------------------------------------------------------------
#
# Name:     uwgrid.sh
#
# Purpose:  Bourne shell script used to create Unidata-Wisconsin GRID files
#
# Syntax:   uwgrid.sh pcode <date> <time>
#
#           pcode - NG - North American NGM
#                   NF - Global AVN initialization
#
#           date  - CCYYDDD -- if not specified then current date is used
#           time  - HH      -- if not specified then current time is used
#
# Notes:    What this script does, and how it works
#
#             MCHOME - the home directory for the McIDAS-X installation
#
#             MCDATA - the directory in which the XCD decoders will
#                      create/update data and ancillary data files
#                      NOTE: data files (e.g. MD and GRID) will probably be
#                      located in other directories but can be located
#                      by REDIRECTions contained in $MCDATA/LWPATH.NAM.
#                      MCDATA is typically the working directory for
#                      the user 'mcidas'.
#
#             MCPATH - a colon separated list of directories that contain
#                      McIDAS data files, ancillary data files, and help
#                      files
#
#             PATH -   a search PATH that contains the directory of
#                      McIDAS executables.  NOTE that in this PATH
#                      the mcidas/bin directory MUST be before the
#                      directory in which this file, xcd_run, exists
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
#
# History:  19990721 - Written for Unidata McIDAS-X, -XCD 7.60
#           20040914 - Changed default location for log directory to ~ldm/logs
#                      Changed specification of PATH to be platform-independent
#                      Changed LD_LIBRARY_PATH specification to account for OS
#
#--------------------------------------------------------------------------

SHELL=sh
export SHELL

# Define macros needed for McIDAS-7.X+ environment
#
# Set MCHOME to be the home directory of your 'mcidas' installation.

# Set MCDATA to be the working directory of your 'mcidas' installation.
# For this example, I will assume that this is /home/mcidas/workdata
#
# Set MCPATH to include MCDATA as its first directory.  I assume
# in this example that McIDAS-X was installed in /home/mcidas.

MCHOME=/home/mcidas
MCDATA=$MCHOME/workdata
# MCLOG=$MCDATA/uwgrid.log
MCLOG=/usr/local/ldm/logs/uwgrid.log
MCPATH=${MCDATA}:$MCHOME/data:$MCHOME/help
export MCPATH MCLOG

# Setup PATH so that the McIDAS-X executables can be found

PATH=$MCHOME/bin:$PATH
export PATH

# Set LD_LIBRARY_PATH to include all directories (other than those searched
# by default) that are needed to be searched to find shared libraries.

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

# Send all textual output to the log file
exec 2>$MCLOG 1>&2

# cd to the MCDATA directory
cd $MCDATA

# Get the current date and time
ccyyddd=`TZ=UTC date +%Y%j`
hhmmss=`TZ=UTC date +%H%M%S`

# Set the day and time for the model run
if test $2
then
    day='DAY='$2
else
    day=''
fi

if test $3
then
    run='RUN='$3
else
    run=''
fi

# Announce and start the appropriate action
echo Starting uwgrid.sh $1 $day $run at $ccyyddd/$hhmmss

case $1 in
    NG) uwgrid.k RTGRIDS/NGM X NGM $day $run DIAL=NG $ccyyddd $hhmmss SCR=MYDATA/GRIDS.9999 ROU=YES;;
    NF) uwgrid.k RTGRIDS/AVN X AVN $day $run DIAL=NF $ccyyddd $hhmmss SCR=MYDATA/GRIDS.9999 ROU=YES;;
    *)  echo "uwgrid action $1 incorrectly specified, failing..."
        ;;
esac

# Log the ending time.
ccyyddd=`TZ=UTC date +%Y%j`
hhmmss=`TZ=UTC date +%H%M%S`

echo Ending uwgrid.sh $1 $day $run at $ccyyddd/$hhmmss

# Done
exit 0
