#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include "netcdf.h"
#include "ulog.h"
#include "alarm.h"

#include "lightning.h"

#define DEBUG 0

/*
** Function prototypes
*/
int    decode_uspln( lgt_struct * );
int    decode_nldn ( lgt_struct * );
int    ymd_to_jday( int, int, int, char * );
int    sectodaytime( int , int *, int *, char * );
int    nldninput( int *, int *, float *, float *, float *, int *, float *, float *, float *, float * );

int    make_md_file( char * , int , char *, char *, char * );
int    make_md_file_name( int , char * );
int    get_row_header( char *, int, int *, int );
int    update_row_header( char *, int, int *, int );
int    file_observation( char * , int, int , int *, int );
int    write_mcidas( char *, int, char *, char *, lgt_struct );

void   fshrte_(  int *, int * );
int    getrte_(  int *, int *, char * );
void   swbyt4_( void *, int * );
char  *Mcpathname( char * );

/*
** Global defines
*/
static MDHEADER mdhead;
static int      header[4096];
static unsigned alarm_timeout = 60;             /* seconds; should suffice */
int             rtecom[64];
char            mcpathdir[MAX_FILE_NAME];       /* name of data directory */
char            pathname[MAX_FILE_NAME];        /* fully qualified pathname */
FILE           *mdfp = (FILE *)NULL;            /* descriptor for MD file */

/*
 * SIGALRM handler.
 */
static void
alarm_handler( int sig )
{
    assert( SIGALRM == sig );
    unotice( "lgt2md(): no data for %u seconds: simulating EOF", alarm_timeout );
}

int decode_lgt( char *datadir, int mdbase, char *schema, char *pcode, int day, int time )
{
    char        type[6];

    int         ch1, ch2;
    int         isNldn=0;
    int         nflash;
    int         rc;

    lgt_struct  data;

    udebug( "In decode_lgt... " );

    /*
    ** Save the data directory name in a globally accessible variable
    */
    (void) strcpy( mcpathdir, datadir );

    /*
    ** Read one character and determine which kind of data the input
    ** file contains.  As of 20060927 the choices are NLDN or USPLN.
    */
    ch1 = fgetc( stdin );
    ch2 = fgetc( stdin );
    rc = ungetc( ch2, stdin );
    rc = ungetc( ch1, stdin );
    
    if ( strchr("12N",ch1) != (char *)NULL ) {
      if ( strchr("12L",ch2) != (char *)NULL ) {
        isNldn = 1;
      }
    }

    do {

      if ( isNldn ) {
        (void) strcpy( type, "NLDN"  );
        rc = decode_nldn( &data );
      } else {
        (void) strcpy( type, "USPLN" );
        rc = decode_uspln( &data );
      }

      if ( rc > 0 ) {
#if DEBUG
        udebug( "decode_lgt:: %7d %6d %6d %9.4f %9.4f %8.2f %2d",
                data.day, data.time, data.hms, data.lat, data.lon,
                data.sgnl, data.mult );
#endif
        nflash = write_mcidas( type, mdbase, schema, pcode, data );
      }

    } while ( rc > 0 );

    /*
    ** Done
    */
    nflash = write_mcidas( "EXIT", mdbase, schema, pcode, data );
    uinfo( "%d %s flash records in MD file", nflash, type );

    (void) fclose( stdin );
    return( rc );

}

int decode_uspln( lgt_struct *data )

/*
**  decode_uspln - read and convert USPLN lightning data to an internal form
**
**  USPLN record:
**              datetime - CCYY-mm-ddThh:min:sec.msec
**              lat      - latitude  [deg]
**              lon      - longitude [deg, west negative convention]
**              sgnl     - signal strength, sign indicates polarity
**              elpsmaj  - error ellipse major axis [read but not used]
**              elpsmin  - error ellipse major axis [read but not used]
**              angle    - orientation of major axis [read but not used]
**
**  Note:
**        Original format
**              2008-06-24T00:58:41,38.8955652,-101.7522742,-14.9,1
**              2008-06-24T00:58:41,46.5759943,-106.6322711,-37.6,1
**        WSI-broadcast format
**              2008-06-29T23:51:36.472,29.3957235,-86.5991748,-12.9,0.25,0.25,13
**              2008-06-29T23:51:36.504,44.8307982,-119.3533128,-19.3,0.25,0.25,-3
*/

{
    int         nscan;
    char        stime[12];
    int         rc;

    int         ccyy;				/* year */
    int         m;					/* month */
    int         d;					/* day */
    int         H;					/* hour */
    int         M;					/* minute */
    int         S;					/* second */
    float       sec;                /* seconds ss.msec */
    float       lat;				/* latitude, north positive */
    float       lon;				/* longitude, east positive */
    float       sgnl;				/* signal strength Kamps. 0.0-> cloud */
    float       emaj;               /* error ellipse major axis [km] */
    float       emin;               /* error ellipse minor axis [km] */
    int         angle;				/* orientation of ellipse major axis */
    int         mult=1;             /* multiplicity */
    int         ntok;

    char        line[81];
    char        temp[81];
    char       *c=NULL;

    static int      alarm_initialized = 0;
    static Alarm    timeout_alarm;

    udebug( "In decode_uspln..." );

    if ( !alarm_initialized ) {
      alarm_init( &timeout_alarm, alarm_timeout, alarm_handler );
      alarm_initialized = 1;
    }

    do { 
      alarm_on( &timeout_alarm );             /* set alarm */
      c = fgets( line, (int)sizeof(line), stdin );
      alarm_off( &timeout_alarm );            /* disable alarm */

      if ( c == (char *)NULL ) {

        if ( feof( stdin ) ) {
          unotice( "decode_uspln(): EOF on stdin" );
        } else if ( ferror( stdin ) ) {
          uerror( "decode_uspln(): USPLN read error" );
        } else {
          unotice( "decode_uspln(): No data to read" );
        }
        rc = 0;
        return( rc );

      } else {

        rc = 1;
        udebug( "decode_uspln: line = %s", line );
        c = strstr( (const char *)line, "LIGHTNING" );
        if ( c == (char *) NULL ) {

          /* Data record found */
          /* <<<<< UPC mod 20080917 - support both USPLN record formats >>>>> */
          (void *) strcpy( temp, line );
          ntok = 0;
          c    = strtok( temp, "," );
          while ( c != (char *) NULL ) {
            ntok += 1;
            c = strtok( NULL, "," );
          }

          switch ( ntok ) {
          case 5:
            /* 2008-06-24T00:58:41,38.8955652,-101.7522742,-14.9,1 */
            nscan = sscanf( line, "%4d-%2d-%2dT%2d:%2d:%f,%f,%f,%f,%d",
                            &ccyy, &m, &d, &H, &M, &sec, &lat, &lon, &sgnl,
                            &mult );
            break;

          case 7:
            /* 2008-06-29T23:51:36.472,29.3957235,-86.5991748,-12.9,0.25,0.25,13*/
            nscan = sscanf( line, "%4d-%2d-%2dT%2d:%2d:%f,%f,%f,%f,%f,%f,%d",
                            &ccyy, &m, &d, &H, &M, &sec, &lat, &lon, &sgnl,
                            &emaj, &emin, &angle );
            mult = 1;
            break;

          default:
            udebug("decode_uspln(): Product header record = %s", line);
            nscan = 0;
            rc    = 0;

          }

          if ( nscan == (ntok+5) ) {
            data->day  = ymd_to_jday( ccyy, m, d, stime );
            S          = (int) sec;
            data->time = 10000*H + 100*M + S;                       /* HHMMSS */
            data->hms  = 10000*H + 100*M + S;                       /* HHMMSS */
            (void) strcpy( data->stime, stime );
            data->lat  = lat;
            data->lon  = -lon;		       /* McIDAS west positive convention */
            data->sgnl = sgnl * 5.0;             /* sgnl[kAmps]
                                                    data->sgnl[measures]
                                                    150 measures ~= 30 kAmps  */
            data->mult = mult;                   /* always '1' for USPLN data */
          } else {
            uerror( "Error decoding USPLN flash record: %s", line );
            rc = 0;
          }

        } else {
          udebug("decode_uspln(): Product header record = %s", line);
          rc = 0;
        }

      }

    } while ( rc == 0 );
    
    return( rc );

}

int decode_nldn( lgt_struct *data )

/*
**  decode_nldn - read and convert NLDN lightning data to an internal form
**
**  NLDN record:
**              tsec    - time in seconds since 1970
**              nsec    - nanoseconds since tsec
**              lat     - latitude  [deg]
**              lon     - longitude [deg, west negative convention]
**              sgnl    - signal strength [150 NLDN measures ~= 30 kAmps]
**              mult    - multiplicity [#strokes per flash]
**              semimaj -              [read but not used]
**              eccent  - eccentricity [read but not used]
**              angle   -              [read but not used]
**              chisqr  -              [read but not used]
*/

{

    int         cyd;
    int         hms;
    int         ok;
    int         rc;
    char        stime[12];

    int         time_sec;
    int         time_nsec;
    float       lat;
    float       lon;
    float       sgnl;
    int         mult;
    float       semimaj;
    float       eccent;
    float       angle;
    float       chisqr;

    udebug( "In decode_nldn..." );

    rc = nldninput( &time_sec, &time_nsec, &lat, &lon, &sgnl, &mult,
                    &semimaj, &eccent, &angle, &chisqr );

    if ( rc > 0 ) {
      ok = sectodaytime( time_sec, &cyd, &hms, stime );
      data->day  = cyd;                                        /* [CCYYDDD] */
      data->time = hms;                                         /* [HHMMSS] */
      data->hms  = hms;                                         /* [HHMMSS] */
      (void) strcpy( data->stime, stime );                 /* [DD MMM CCYY] */
      data->lat  = lat;
      data->lon  = -lon;		         /* McIDAS west positive convention */
      data->sgnl = sgnl;
      data->mult = mult;
    }

    return( rc );

}

int ymd_to_jday( int year, int month, int day, char *stime )

/*
**  ymd_to_jday - converts year, month, and day to julian day
**
**          input:
**              year  - year [ccyy]
**              month - month [1-12]
**              day   - day of month
*/

{
    time_t      datetime;
    struct tm   settime;
    int         value;

    udebug( "In ymd_to_jday..." );

    settime.tm_sec   = 0;
    settime.tm_min   = 0;
    settime.tm_hour  = 0;
    settime.tm_mday  = day;
    settime.tm_mon   = month - 1;
    settime.tm_year  = year - 1900;            /* <<<<< UPC mod 981204 >>>>> */
    settime.tm_wday  = 0;
    settime.tm_yday  = 0;
    settime.tm_isdst = -1;

    datetime = mktime ( &settime );
    value = 1000 * (1900 + settime.tm_year) + settime.tm_yday + 1;

    (void) strftime( stime, (size_t) 12, "%d %b %Y", &settime );

    return( value );
}

int sectodaytime( int secs, int *cyd, int *hms, char *stime )
{
    time_t      t_secs;
    struct tm  *settime;           /* temporary time structure */

    udebug( "In sectodaytime..." );

    /*
    ** convert the input number of seconds to the tm structure
    */
    t_secs  = (time_t) secs;
    settime = gmtime (&t_secs);
    if ( settime == (struct tm *)NULL ) {
      return( -1 );
    }

    /*
    ** convert the tm structure to cyd and hms
    */
    *cyd = (1900 + settime->tm_year) * 1000 + (settime->tm_yday + 1);
    *hms = (settime->tm_hour * 10000) + (settime->tm_min  * 100) + settime->tm_sec;

    (void) strftime( stime, (size_t) 12, "%d %b %Y", settime );

#if DEBUG
    udebug( "sectodaytime:: secs since 1970 = %d, cyd = %d, hms = %d, stime = %s",
            secs, *cyd, *hms, stime );
#endif

    return (0);
}

int make_md_file_name( int mdf , char *output )

/*
**  make_md_file_name - builds the LW file name for a given MD file number
**      input:
**          mdf     - md file number
**      output:
**          output  - LW file name
**      returns:
**          -1      - if invalid MD file number specified
**           1      - if successful
*/

{
    char *tempname;

    udebug( "In md_file_name..." );

    if ( mdf < 10000 ){
        (void) sprintf(output, "MDXX%04ld", mdf);
    }
    else if (mdf > 9999 && mdf < 100000){
        (void) sprintf(output, "MDX%05ld",  mdf);
    }
    else if (mdf > 99999 && mdf < 1000000){
        (void) sprintf(output, "MD%06ld",   mdf);
    }
    else{
        return( -1 );
    }

    tempname = Mcpathname(output);
    (void) strcpy(output,tempname);
    return( 1 );
}

int make_md_file( char *file, int cyd, char *stime, char *schema, char *type )

/*
**      make_md_file    - builds the skeletons for the MD files needed
**                        for the data.  It also pads the entire
**                        MD file with hex80s so subsequent writes will go
**                        faster.
**
**          input:
**              file    - LW file name of the MD file being created
**              cyd     - Julian day that the data is valid for
**              schema  - Schema to use in create
**              type    - Data type [NLDN|USPLN]
**
**          return codes:
**              -3      - unable to locate SCHEMA LW file
**              -2      - unable to find schema in SCHEMA file
**              -1      - unable to open destination MD file
**               0      - if successful
*/

{
    const char *schema_file;
    MDHEADER    mhead;

    int         i, loc, pages;
    int         num;
    long        pointer, beginning=0;

    char        info[33];
    char       *p;

    int         buffer[1024];

    size_t      nrc;

    FILE       *fp;

    udebug( "In make_md_file, file = \"%s\"", file );

    /*
    ** Open SCHEMA file aborting on error
    */
    schema_file = Mcpathname( "SCHEMA" );
    if ( schema_file == (char *)NULL )  return(-1);

    fp = fopen( schema_file, "r" );
    if ( fp == NULL ) {
       uerror( "unable to open SCHEMA file \"%s\"", schema_file );
       return( -3 );
    }

    /*
    ** Find the highest version number of schema in SCHEMA.  Exit if not found.
    */
    loc = 0;
    mdhead.schemver = -1;
    nrc = fseek( fp, beginning, SEEK_SET );

    for ( i = 1; i <= 500; i++ ) {
      (void) fread( (void *)&mhead, sizeof(mhead), (size_t)1, fp );
      num = 1;
      (void) swbyt4_( &mhead.schemver, &num );
      if ( !strncmp((char *)&mhead.schema, schema, 4) &&
           (mhead.schemver > mdhead.schemver) )          {
        mdhead = mhead;
        loc = i;
      }
    }

    if ( loc == 0 ) {
       uerror( "unable to locate schema \"%s\" in SCHEMA file", schema );
       (void) fclose( fp );
       return( -2 );
    }

    /*
    ** Position the file pointer at the starting location of the MD
    ** skeleton in the SCHEMA file and read all necessary information.
    */
    pointer = ((loc-1) * 4096 + 32768) * 4;
    nrc     = fseek( fp, pointer, SEEK_SET );
    nrc     = fread( (void *)header, sizeof(int), (size_t)4096, fp );
    (void) fclose( fp );

    /*
    ** swap the bytes if necessary  <<<<< UPC add 970107 >>>>
    **
    **   Swap all words in the 64 word schema header (0 based) except:
    **         0 = Schema name
    **     16-23 = Text description
    **        26 = Creator's initials
    **     61-62 = file name
    */
    num = 15;
    (void) swbyt4_( &header[ 1], &num );
    num = 2;
    (void) swbyt4_( &header[24], &num );
    num = 34;
    (void) swbyt4_( &header[27], &num );
    num = 1;
    (void) swbyt4_( &header[63], &num );

    /*
    ** Populate the header structure
    */
    memcpy( &mdhead, header, sizeof(int)*64 );

    /*
    ** swap scale factor words
    */
    num = mdhead.nkeys;
    (void) swbyt4_( &header[864], &num );

    /*
    ** Rewrite the Text description block (words 16-23)
    */
    if ( strncmp(type, "NLDN", (size_t) 4) == 0 )
      (void) sprintf( info, "NLDN Lightning for   %s", stime );
    else
      (void) sprintf( info, "USPLN Lightning for  %s", stime );

    (void) strncpy( (char *) &header[16], info, 32 );

    /*
    ** Open the MD file
    */
    mdfp = fopen( file, "wb+" );
    if ( mdfp == NULL ) {
      uerror( "unable to open output file \"%s\"", file );
      return( -1 );
    }

    /*
    ** Set output data buffer values to MISS.
    */
    for ( i = 0; i <= 1023; i++ ) buffer[i] = mdhead.missing;

    /*
    ** Position file to beginning of the file and write out the skeleton
    */
    nrc   = fseek( mdfp, beginning, SEEK_SET );
    /*
    ** <<<<< UPC mod 20070421 - change from writing entire file for speed >>>>>
    ** pages = (int) (header[30] / 1024) + 3;
    */
    pages = (int) (header[29] / 1024) + 3;
    for ( i = 0; i < pages; i++ ) {
      nrc = fwrite( (char *)buffer, sizeof(int), (size_t)1024, mdfp );
    }
    (void) fflush( mdfp );

    /*
    ** Set the integer ID of the file to the date of the data and the creation
    ** date to the current day.  Set the creator's ID to USER.
    ** MD file created date must be in YYYDDD form.
    */
    header[15] = cyd;
    header[25] = 1000 * ((rtecom[8] / 1000) - 1900) + (rtecom[8] % 1000);
    (void) strncpy( (char *) &header[26], "USER", 4 );

    /*
    ** Fill header entries 39-42 (0-based) with date and time represented
    ** in the file for faster ADDE searches.
    */
    header[38] = 0;
    header[39] = header[15];
    header[40] = 0;
    header[41] = header[39];
    header[42] = 235959;

    /*
    ** Write MD file name and number into file header.
    ** NB: here it is assumed that the file name is MDXXnnnn!
    **
    */
    p = strstr( file, "MDXX" );
    if ( p != (char *) NULL ) {
       (void) strncpy( (char *) &header[61], p, 8 );
       header[63] = atoi( p+4 );
    }

    /*
    ** Write out MD file header
    */
    nrc = fseek( mdfp, beginning, SEEK_SET );
    nrc = fwrite( (char *)header, sizeof(int), (size_t)4096, mdfp );
    (void) fflush( mdfp );

    return( (int)nrc );

}

int get_row_header( char *mdf, int row, int *record, int nread )
/*
**        get_row_header - reads 'nread' elements from mdf into record
**          input:
**              mdf      - MD file name for the data being filed
**              row      - row header number to read
**              record   - data repository
**              nread    - number of elements to read
*/

{
    int     i;
    long    fptr;
    size_t  nrc;

    udebug( "In get_row_header, MD file = %s", mdf );

    fptr = 4 * (mdhead.rowhdloc + (row-1)*mdhead.nrkeys);

    nrc  = fseek( mdfp, fptr, SEEK_SET );
    nrc  = fread( (void *)record, (size_t)4, (size_t)nread, mdfp );

    return( (int)nrc );
}

int update_row_header( char *mdf, int row, int *record, int nwrite )
/*
**      update_row_header - writes 'nwrite' elements from 'record'
**                          to mdf 'row'
**          input:
**              mdf       - MD file name for the data being filed
**              row       - row header number to write to
**              record    - data to write
**              nwrite    - number of elements to write
*/

{
    int     i;
    long    fptr;
    size_t  nrc;

    udebug( "In update_row_header, row = %d", row );

#if DEBUG
    for (i=0; i<nwrite; i++ )
      udebug( "update_row_header:: record[%02d] = %8d", i, record[i] );
#endif

    fptr = 4 * (mdhead.rowhdloc + (row-1)*mdhead.nrkeys);

    nrc = fseek( mdfp, fptr, SEEK_SET );
    nrc = fwrite( (void *)record, sizeof(record[0]), (size_t)nwrite, mdfp);
    (void) fflush( mdfp );

    return( (int)nrc );
}

int file_observation( char *mdf, int row, int col, int *record, int nkeys )
/*
**      file_observation - files the entire data section for a given
**                         MD file, row, and column
**          input:
**              mdf      - MD file name for the data being filed
**              row      - row number to file to
**              col      - column number to file to
**              record   - int array containing data
**              nkeys    - number of keys to write to file
*/

{
    int     i;
    long    fptr;
    size_t  nrc;

    udebug( "In file_observation, row = %d, col = %d", row, col );

    fptr  = mdhead.dataloc + (mdhead.ncols*(row-1) + (col-1))*nkeys;
    fptr *= sizeof(int);

    nrc = fseek( mdfp, fptr, SEEK_SET );
    nrc = fwrite( (void *)record, sizeof(int), (size_t)mdhead.ndkeys, mdfp );
    (void) fflush( mdfp );

    return( (int)nrc );
}

int write_mcidas( char *type, int mdb, char *schema, char *pcode, lgt_struct data )
{
    char        ctext[81];

    double      drecord[400];

    int         i, indx, jday, loc, rc;
    int         mdbase, mdfo;

    size_t      nrc;

    static char md_filename[MAX_FILE_NAME];

    static int  irte    = 0,
                mdfile  = 0,
                nflash  = 0,
                row     = 0,
                col     = 0,
                record[400];

    FILE       *fp;

    udebug( "In write_mcidas, lightning type = %s", type );

    /*
    ** Check to see if we are done writing to the file.  The necessary
    ** conditions are:
    **
    ** type   == EXIT  <- indication from calling routine to terminate
    ** irte   == 1     <- routing table has entry for pcode
    ** nflash  > 0     <- data has been decoded
    */

    if ( !strncmp(type,"EXIT",4) ) {
      if ( nflash > 0 ) {
        rc = update_row_header( md_filename,row,record,mdhead.nrkeys );
        if ( irte )
          (void) fshrte_( &irte, &rc );
      }
      return( nflash );
    }

    /*
    ** Calculate the MD file number and make the output file name
    */

    jday = (data.day % 10);
    if ( jday == 0 ) jday = 10;
    mdfo = mdb + jday;

    if ( mdfo != mdfile ) {

      if ( mdfile != 0 ) {
        rc = update_row_header( md_filename,row,record,mdhead.nrkeys );
        if ( irte )
          (void) fshrte_( &irte, &rc );
      }

      mdfile = mdfo;
      nflash = 0;

      /*
      ** Check routing table to see if the user wants this product decoded
      */
      (void) strncpy( (char *) &rtecom[1], "    ", 4);
      (void) strncpy( (char *) &rtecom[1], pcode,  2);

      irte = getrte_( &mdb, &mdbase, ctext );
      if ( irte < 0 ) {
        unotice( "Routing entry for product %s SUSpended, exiting", pcode );
        return( -1 );
      }
      rtecom[6] = data.day;
      rtecom[7] = data.hms;

      mdbase = 10 * (mdbase / 10);
      mdfo   = mdbase + jday;

#if 1
      unotice( "day = %5d, mdfo = %4d, mdfile = %4d", data.day, mdfo, mdfile );
#endif

      /*
      ** Do a dummy open to see if the output MD file exists; if it doesn't
      ** create it
      */

      rc = make_md_file_name( mdfo, md_filename );
      
      mdfp = fopen( md_filename, "rb" );
      if ( mdfp == NULL ) {
         unotice( "Making %s; may take some time...", md_filename );
         if ( make_md_file(md_filename,data.day,data.stime,schema,type) < 0 ) {
           serror("Unable to create MD file %s.  Aborting!", md_filename);
           return( -1 );
         }
         row = 1;
         col = 0;
      } else {
         mdfp = freopen( md_filename, "rb+", mdfp );
         unotice( "Reading %s header...", md_filename );
         nrc = fseek( mdfp, 0, SEEK_SET );
         nrc = fread( (void *)header, sizeof(int), 4096, mdfp );
         memcpy( &mdhead, header, sizeof(int)*64 );
         row = 0;
         do {
           row++;
           rc  = get_row_header( md_filename, row, record, mdhead.nrkeys );
           if ( record[2] != mdhead.missing ) {        /* normal processing */
             col = record[2];
           } else {                   /* special: last write was end of row */
             col = 0;
           }
           nflash += col;
         } while ( col == 1000 );
      }

    }

    /*
    ** Announce the day and time that is being decoded
    */

#if DEBUG
    udebug( "Decoding %d.%06d data into %s",data.day,data.hms,md_filename );
#endif

    col++;
    if ( col > mdhead.ncols ) {
      row++;
      /* Make sure MD output row does not exceed the number of rows allowed */
      if ( row > mdhead.nrows ) {
        uerror( "Output row %ld exceeds #rows allowed in MD file %d.  Aborting!",
                row, mdhead.nrows );
        return( -1 );
      }
      col = 1;
    }

    /*
    ** Write the observation to the MD file
    **
    ** Values:
    **        (d)record[0] = DAY,  date [CCYYJJJ] of last flash in row
    **        (d)record[1] = TIME, time [HHMMSS] of last flash in row
    **        (d)record[2] = CMAX, column of last flash record in row
    **        (d)record[3] = HMS,  flash time [HHMMSS]
    **        (d)record[4] = LAT,  latitude [DEG]
    **        (d)record[5] = LON,  longitude [DEG]
    **        (d)record[6] = SGNL, signal strength [KAMPS]
    **        (d)record[7] = MULT, multiplicity
    */
    loc = header[32];                          /* MD file start of key names */
    for ( i=0; i<mdhead.nkeys; i++ ) {
      if (        !strncmp((char *)&header[loc+i], "DAY",  3) ) {
        drecord[i] = data.day;
      } else if ( !strncmp((char *)&header[loc+i], "TIME", 4) ) {
        drecord[i] = data.time;
      } else if ( !strncmp((char *)&header[loc+i], "CMAX", 4) ) {
        drecord[i] = col;
      } else if ( !strncmp((char *)&header[loc+i], "HMS",  3) ) {
        drecord[i] = data.hms;
      } else if ( !strncmp((char *)&header[loc+i], "LAT",  3) ) {
        drecord[i] = data.lat;
      } else if ( !strncmp((char *)&header[loc+i], "LON",  3) ) {
        drecord[i] = data.lon;
      } else if ( !strncmp((char *)&header[loc+i], "SGNL", 3) ) {
        drecord[i] = data.sgnl;
        indx = i;
      } else if ( !strncmp((char *)&header[loc+i], "MULT", 3) ) {
        drecord[i] = data.mult;
      }
    }

    /*
    ** Check 'sgnl's units.  If 'KAMP', scale 'sgnl' which is in
    ** NLDN measures to kAmps using:  150 NLDN measures ~= 30 kAmps
    */
    loc = header[34];                              /* MD file start of units */
    if ( !strncmp((char *)&header[loc+indx],"KAMP",4) ) {
       drecord[indx] /= 5.0;
    }

    /*
    ** Scale all keys
    */
    loc = header[33];                      /* MD file start of scale factors */
    for ( i=0; i<mdhead.nkeys; i++ ) {
      record[i] = drecord[i] * pow( 10.0, (double)header[loc+i] );
    }

    /*
    ** File the observation and update the row header if successful
    */
    i = mdhead.nrkeys + mdhead.nckeys;
    if ( file_observation(md_filename,row,col,&record[i],mdhead.ndkeys) ) {
      nflash++;
    }

    if ( col == mdhead.ncols )
      rc = update_row_header( md_filename,row,record,mdhead.nrkeys );

    return( nflash );
}
