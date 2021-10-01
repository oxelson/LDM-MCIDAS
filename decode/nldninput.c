/* ---------------------------------------------------------------------
**
**  Name:     ingestnldn.c
**
**  Purpose:  Read NLDN binary records from LDM feed
**
**  History:  19940302 - Adapted from 'ingesttonc.c' written by Ron
**                       Henderson of SUNY Albany
**
** ---------------------------------------------------------------------
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include "ulog.h"
#include "alarm.h"

#define INGESTORFLASHLEN 28

static unsigned alarm_timeout = 60;	/* seconds; should suffice */

/*
 * SIGALRM handler.
 */
static void
alarm_handler( sig )
int           sig;
{
    assert( SIGALRM == sig );
    uerror( "nldninput(): no data read in %u seconds: simulating EOF", alarm_timeout );
}

/*
 * Read a character from a file.
 *
 * Returns:
 *	1	Success
 *	0	Failure
 */
static int
read_char( file, value )
FILE         *file;
char         *value;
{
    return fread(value, 1, 1, file) == 1;
}


/*
 * Read a short from a file.
 *
 * Returns:
 *	 1	Success
 *	 0	Failure
 */
static int
read_short( file, value )
FILE         *file;
short        *value;
{
    unsigned char x[2];			/* NB: 16-bits assumed! */
    int		retcode;
    int		num = 1;

    if (fread(x, 1, sizeof(x), file) == sizeof(x))
    {
	*value = (x[0] << 8) | x[1];
	retcode = 1;
    }
    else
    {
	retcode = 0;
    }

    return retcode;
}


/*
 * Read an integer from a file.
 *
 * Returns:
 *	 1	Success
 *	 0	Failure
 */
static int
read_int( file, value )
FILE         *file;
int          *value;
{
    unsigned char x[4];			/* NB: 32-bits assumed! */
    int		retcode;
    int		num = 1;

    if (fread(x, 1, sizeof(x), file) == sizeof(x))
    {
	*value = (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3];
	retcode = 1;
    }
    else
    {
	retcode = 0;
    }

    return retcode;
}

/*
 * Read an undetermined 4-byte sequence from a file; determine whether
 * or not it is a beginning of a header record; eat all header records
 * if they exist; and return the integer value at the beginning of a 
 * flash record.
 *
 * Returns:
 *	 1	Success
 *	 0	Failure
 */
static int
read_begin( file, value )
FILE         *file;
int          *value;
{
    unsigned char x[4];			/* NB: 32-bits assumed! */
    int		  retcode = 0;		/* failure default */
    int		  num = 1;

    if (fread(x, 1, sizeof(x), file) == sizeof(x)) 
    {
	if (strncmp((const char *)x, "NLDN", 4) != 0)
	{
	    /*
	     * Not a header record.  Just return the data value.
	     */
	    retcode = 1;
	}
	else
	{
	    /*
	     * Start of header records.  Read them and the first data
	     * value.
	     */
	    int           nhead;

	    uinfo("nldninput(): Product header record found");
	    if (read_int(file, &nhead))
	    {
		int           nflash;

		if (read_int(file, &nflash))
		{
		    unsigned char headr[280];	/* NB: 32-bits assumed! */
		    int           bufsiz = nhead * INGESTORFLASHLEN - 12;

		    if (fread(&headr[12], 1, bufsiz, file) == bufsiz)
		    {
			if (nflash != 0)
			{
			    if (fread(x, 1, sizeof(x), file) == sizeof(x))
				retcode = 1;
			}
		    }
		}
	    }
	}

	if (retcode == 1)
	    *value = (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3];
    }

    return retcode;
}

/*
 * Read an NLDN record from a file stream.
 *
 * Returns:
 *	INGESTORFLASHLEN -> Success
 *	0                -> Failure
 */
static int
nldn_read( file, time_sec, time_nsec, lat, lon, sgnl, mult, semimaj, eccent,
           angle, chisqr )
FILE         *file;
int          *time_sec;
int          *time_nsec;
float        *lat;
float        *lon;
float        *sgnl;
int          *mult;
float        *semimaj;
float        *eccent;
float        *angle;
float        *chisqr;
{
    int   iret;
    int   nbytes;
    short fill_short;
    char  fill_char;
    int   rc = 0;						/* failure */
    struct {
      int   lat;
      int   lon;
      short sgnl;
      char  mult;
      char  semimaj;
      char  eccent;
      char  angle;
      char  chisqr;
    } obs;

    if ( read_begin(file, time_sec)    &&  /* 32-bits assumed */
         read_int(file, time_nsec)     &&  /* 32-bits assumed */
         read_int(file, &obs.lat)      &&
         read_int(file, &obs.lon)      &&
         read_short(file, &fill_short) &&
         read_short(file, &obs.sgnl)   &&
         read_short(file, &fill_short) &&
         read_char(file, &obs.mult)    &&
         read_char(file, &fill_char)   &&
         read_char(file, &obs.semimaj) &&
         read_char(file, &obs.eccent)  &&
         read_char(file, &obs.angle)   &&
         read_char(file, &obs.chisqr)     ) {

      *lat     = obs.lat / 1000.0;
      *lon     = obs.lon / 1000.0;
      *sgnl    = obs.sgnl / 10.0;
      *mult    = obs.mult;
      *semimaj = obs.semimaj;
      *eccent  = obs.eccent;
      *angle   = obs.angle;
      *chisqr  = obs.chisqr;

      rc = INGESTORFLASHLEN;

    }

    return( rc );

}


/*
 * Return parameters from the input.  In order to handle the situation in
 * which we're being fed by an LDM and the end of the data product is never
 * seen, we set an alarm prior to the read.
 *
 * Returns:
 *	-1			Error: read error or EOF
 *	 0			No data
 *	INGESTORFLASHLEN	Number of bytes read
 */
int
nldninput(time_sec, time_nsec, lat, lon, sgnl, mult, semimaj, eccent,
          angle, chisqr)
int          *time_sec;
int          *time_nsec;
float        *lat;
float        *lon;
float        *sgnl;
int          *mult;
float        *semimaj;
float        *eccent;
float        *angle;
float        *chisqr;
{
    int           rc = -1;				/* failure default */
    static int	  alarm_initialized = 0;
    static Alarm  timeout_alarm;

    if ( !alarm_initialized ) {
	  alarm_init( &timeout_alarm, alarm_timeout, alarm_handler );
	  alarm_initialized = 1;
    }

    alarm_on(&timeout_alarm);				/* set alarm */

    rc = nldn_read( stdin, time_sec, time_nsec, lat, lon, sgnl, mult,
                    semimaj, eccent, angle, chisqr );

    alarm_off( &timeout_alarm );			/* disable alarm */

    if ( !rc ) {
      rc = -1;
	  if ( feof(stdin) ) {
	    uinfo( "nldninput(): EOF on stdin" );
	  } else if ( ferror(stdin) ) {
	    serror( "nldninput(): NLDN read error" );
	  } else {
	    uinfo( "nldninput(): No data to read" );
	  }
    }

    return( rc );
}
