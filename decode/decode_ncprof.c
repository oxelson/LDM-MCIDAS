#include <stdio.h>
#include <string.h>
#include <netcdf.h>

#include "profiler.h"
#include "ulog.h"

extern void *decode_fsl1( int, long ,int * );
extern void *decode_fsl2( int, long, int * );

void *decode_ncprof( char *infilnam, long miss, int *iret )
{

    int          ier;
    int          cdfid;
    int          attnum;
    int          tmpint[2];
    prof_struct *head;

    ier = nc_open( infilnam, 0, &cdfid );
    if ( ier != 0 ) {
      *iret = -1;
      uerror( "Could not open %s\0",infilnam );
      return( NULL );
    }

    udebug("decode_ncprof:: %s miss %ld ier %d cdfid %d",
           infilnam, miss, ier, cdfid );

    ier = nc_inq_attid( cdfid,NC_GLOBAL,"avgTimePeriod", &attnum );
    if ( ier == 0 ) {

      /* FSL new format */
      head = (void *) decode_fsl2( cdfid,miss,&ier );

    } else {

      udebug( "Old style Unidata/Wisconsin file\0" );
      head = (void *) decode_fsl1(cdfid,miss,&ier );

    }

    ier = nc_close( cdfid );

    *iret = 0;
    return( head );
}
