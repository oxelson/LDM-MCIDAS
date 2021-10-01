#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#include "netcdf.h"
#include "ulog.h"
#include "profiler.h"

#define MISS                  -2139062144
#define RMISS                 -2139062144.
#define BLANK_WORD            0x20202020
#define MAX_FILE_NAME         1024

typedef struct {
    nclong schema;			/* SCHEMA name */
    nclong schemver;		/* SCHEMA version number */
    nclong schemreg;		/* SCHEMA registration date */
    nclong nrows;			/* default number of rows */
    nclong ncols;			/* default number of columns */
    nclong nkeys;			/* total number of keys in the record */
    nclong nrkeys;			/* number of keys in the row header */
    nclong nckeys;			/* number of keys in the column header */
    nclong ndkeys;			/* number of keys in the data portion */
    nclong colhdpos;		/* 1-based position of the column header */
    nclong datapos;			/* 1-based position of the data portion */
    nclong numrep;			/* number of repeat groups */
    nclong repsiz;			/* size of the repeat group */
    nclong reploc;			/* starting position of the repeat group */
    nclong missing;			/* missing data code */
    nclong mdid;			/* integer ID of the file */
    nclong mdinfo[8];		/* text ID of the file */
    nclong projnum;			/* creator's project number */
    nclong createdate;		/* creation date */
    nclong createid;		/* creator's ID */
    nclong rowhdloc;		/* zero-based offset to the row header */
    nclong colhdloc;		/* zero-based offset to teh column header */
    nclong dataloc;			/* zero-based offset to the data portion */
    nclong unused;			/* first unused word in the file */
    nclong userrecloc;		/* start of the user record */
    nclong keynamesloc;		/* start of the key names */
    nclong scaleloc;		/* start of the scale factors */
    nclong unitsloc;		/* start of the units */
    nclong reserved[4];		/* reserved */
    nclong begday;			/* beginning Julian day of the data */
    nclong begtime;			/* beginning time of the data, hhmmss */
    nclong endday;			/* ending Julian day of the data */
    nclong endtime;			/* ending time of the data, hhmmss */
    nclong reserved2[18];	/* reserved */
    nclong filename[2];		/* MD file name */
    nclong filenum;			/* MD file number */
} MDHEADER;
    
MDHEADER mdhead;

nclong rtecom[64];

char mcpathdir[MAX_FILE_NAME];       /* name of data directory */
char pathname[MAX_FILE_NAME];        /* fully qualified file pathname */

/*
** Function prototypes
*/

/* Instantiated in this file */
void write_mcidas( char *, char *, char *, int, prof_struct * );
int  make_md_file( char * , nclong , char *, char * );
int  make_md_file_name( nclong , char * );
int  update_row_header( char *, int , nclong * , int );
int  file_observation( char * , int , int , nclong * );
int  ymd_to_julian_day( int , int , int , char * );

/* Instantiated in rtesubs.c */
void   fshrte_ (  int *, int * );
int    getrte_ (  int *, int *, char * );

/* Instantiated in commonsubs.c */
void   swbyt4_ ( void *, int * );
char  *Mcpathname( char * );

void
write_mcidas( char *datadir, char *pcode, char *schema, int mdbase, prof_struct *head )
{
    prof_struct *p;
    prof_data   *plev;

    int
      i, iret, irte, itime,                /* <<<<< UPC mod 981207 >>>>> */
      mode, station_count, time_interval,
      row, col;

    float
      dir, spd,
      pi=3.1415297;

    double
      u_2, v_2;

    nclong
      mdf, mdfo, mod,
      julian_day,
      row_header[4],
      *ptr,
      ob_lo_array[400] ,
      ob_hi_array[400] ;

    char
      ctext[81],
      md_filename[MAX_FILE_NAME],
      stime[12];

    typedef union {
      char string[4];
      nclong longval;
    } U;

    U low_mode, high_mode;

    FILE *fp;

/*
** Announce that we are here
*/

    udebug( "In write_mcidas..." );
    udebug( "write_mcidas:: datadir = %s, pcode = %s, schema = %s, mdbase = %d",
              datadir, pcode, schema, mdbase );

/*
** Save the data directory name in a globally accessible variable
*/

    (void) strcpy (mcpathdir, datadir);

/*
** Set pointer to first element in linked list
*/

    p = head;
    time_interval = p->time_interval;

/*
** Get the observation date and time
*/

    julian_day = ymd_to_julian_day( p->year, p->month, p->day, stime );

/*
** Calculate the MD file number and make the output file name
*/

    mdbase = 10 * (mdbase / 10);
    mdfo = mdbase + (julian_day % 10);
    if ( mdfo == mdbase ) mdfo += 10;

/*
** Check routing table to see if the user wants this product decoded
*/

    (void) strncpy( (char *) &rtecom[1], "    ", 4);
    (void) strncpy( (char *) &rtecom[1], pcode,  2);

    irte = getrte_( &mdfo, &mdf, ctext );
    if ( irte < 0 ) {
      unotice( "Routing table entry for product %s SUSpended, exiting", pcode );
      return;
    }
    udebug( "write_mcidas:: getrte returns %d, mdf = %d", irte, mdf );

    iret = make_md_file_name( mdf, md_filename );

/*
** Set some MD file row header values and other variables
*/

    row_header[0] = julian_day;
    rtecom[6]     = row_header[0];
    row_header[1] = p->hour*10000 + p->minute*100;
    rtecom[7]     = row_header[1];
    row_header[3] = 0;
    row = 2*(p->hour*(60/time_interval) + (p->minute/6)) + 1;

    (void) strncpy (low_mode.string, "LOW ", 4);
    (void) strncpy (high_mode.string,"HIGH", 4);

/*
** Do a dummy open to see if the output MD file exists; if it doesn't
** create it
*/

    if ( (fp = fopen(md_filename, "r")) == NULL ) {
       unotice( "Making %s; may take some time...", md_filename );
       if ( make_md_file( md_filename, julian_day, stime, schema ) < 0 ) {
         serror("Unable to create MD file %s.  Aborting!", md_filename);
         exit(-1);
       }
    } else {
       udebug("Reading existing MD file header");
       iret = fread( (char *)&mdhead, sizeof(mdhead), 1, fp );
       (void) fclose( fp );
    }

/*
** Announce the day and time that is being decoded
*/

    uinfo("Decoding %ld.%02ld%02ld data into %s",julian_day,p->hour,p->minute,md_filename);

/*
** Check to make sure that the MD output row does not exceed the number of
** rows allowed in the MD file.
*/

    if ( row >= mdhead.nrows ) {
      serror("Output row %ld exceeds #rows in MD file %ld.  Aborting!", row, mdhead.nrows);
      exit(-1);
    }

/*
** This loop converts all of the netCDF station-dependent information
** into arrays for filing in the MD file
*/

    col = 1;
    station_count = 0;

    while ( p != (prof_struct *)NULL ) {    /* Station loop */

      udebug("Station data for %d : %s\0",p->wmoStaNum,p->staName);
      udebug("   Lat: %7.2f Lon: %7.2f Elev: %7.2f\0",
         p->staLat,p->staLon,p->staElev);
      udebug("   number of levels: %d\0",p->numlevs);
      udebug("   time interval %d\0",time_interval);
      udebug("   pres %6.2f temp %6.2f dewp %6.2f rain %6.2f\0",
         p->sfc_pres,p->sfc_temp,p->sfc_dwpc,p->sfc_rain_amt);

      /*
      ** Unpack and write data to output file IF don't exceed # columns
      */
      if ( col <= mdhead.ncols ) {

        /*
        ** Set array values to MISSing or BLANKs
        */
        for ( i = 0; i < mdhead.ndkeys; i++ ) {
          ob_lo_array[i] = mdhead.missing;
          ob_hi_array[i] = mdhead.missing;
        }
        for ( i = 1; i <= 2; i++ ) {
          ob_lo_array[i] = BLANK_WORD;
          ob_hi_array[i] = BLANK_WORD;
        }

        /*
        ** Set modification flag to be missing initially.  This will be
        ** reset if there is valid data.
        */
        mod = -1;

        /*
        ** 5-character site ID
        */
        
        (void) memcpy( &ob_lo_array[1], p->staName, strlen(p->staName) );
        (void) memcpy( &ob_hi_array[1], p->staName, strlen(p->staName) );

        /*
        ** Station IDn, latitude, longitude, and elevation
        */

        if ( p->wmoStaNum != 99999 ) {
          ob_lo_array[3] = (nclong) p->wmoStaNum;
          ob_hi_array[3] = (nclong) p->wmoStaNum;
        }

        ob_lo_array[4] = (nclong) ((p->staLat + 0.00005) * 10000.);
        ob_hi_array[4] = (nclong) ((p->staLat + 0.00005) * 10000.);

        ob_lo_array[5] = (nclong) ((p->staLon + 0.00005) * -10000.);
        ob_hi_array[5] = (nclong) ((p->staLon + 0.00005) * -10000.);

        ob_lo_array[6] = (nclong) p->staElev;
        ob_hi_array[6] = (nclong) p->staElev;

        /*
        ** Surface pressure, temperature, dewpoint, precipitation, wind speed,
        ** and wind direction
        */

        if ( p->sfc_pres != RMISS ) {
          mod = 0;
          ob_lo_array[7] = (nclong) ((p->sfc_pres + 0.005) * 100.);
          ob_hi_array[7] = (nclong) ((p->sfc_pres + 0.005) * 100.);
        }

        if ( p->sfc_temp != RMISS) {
          mod = 0;
          ob_lo_array[8] = (nclong) ( (273.16 + p->sfc_temp + 0.005) * 100.);
          ob_hi_array[8] = (nclong) ( (273.16 + p->sfc_temp + 0.005) * 100.);
        }

        if ( p->sfc_dwpc != RMISS ) {
          mod = 0;
          ob_lo_array[9] = (nclong) ( (273.16 + p->sfc_dwpc + 0.005) * 100.);
          ob_hi_array[9] = (nclong) ( (273.16 + p->sfc_dwpc + 0.005) * 100.);
        }

        if ( p->sfc_rain_amt != RMISS ) {
          mod = 0;
          ob_lo_array[10] = (nclong) ( (p->sfc_rain_amt/25.24 + 0.005) * 100.);
          ob_hi_array[10] = (nclong) ( (p->sfc_rain_amt/25.24 + 0.005) * 100.);
        }

        ob_lo_array[11] = (nclong) ( 0.);
        ob_hi_array[11] = (nclong) ( 0.);

        if ( p->sfc_sped != RMISS ) {
          mod = 0;
          ob_lo_array[12] = (nclong) ( ( p->sfc_sped + 0.005 ) * 100.);
          ob_hi_array[12] = (nclong) ( ( p->sfc_sped + 0.005 ) * 100.);
        }

        if ( p->sfc_drct != RMISS ) {
          mod = 0;
          ob_lo_array[13] = (nclong) ( p->sfc_drct );
          ob_hi_array[13] = (nclong) ( p->sfc_drct );
        }

        /*
        ** Upper level heights and u, v, and w wind components
        */

        plev = p->pdata;
        ptr  = ob_lo_array + 15;
        mode = 1;

        while ( plev != NULL ) {              /* Level loop */

          if ( plev->levmode == 2 && mode == 1 ) {
            ptr  = ob_hi_array + 15;
            mode = 2;
          }

          udebug("      Z %5.0f u = %7.4f v = %7.4f w = %7.4f mode %d\0",
               plev->level, plev->u, plev->v, plev->w, plev->levmode);

          if ( plev->level != RMISS ) {
            *ptr = (nclong) plev->level + p->staElev;
          }
          ptr++;

          if ( (plev->u != RMISS) && (plev->v != RMISS) ) {
            mod = 0;
            u_2 = (double) ( plev->u * plev->u );
            v_2 = (double) ( plev->v * plev->v );
            spd = (float) sqrt( u_2 + v_2);
            *ptr = (nclong) ( (spd + 0.005) * 100. );

            dir = (float) atan2((double)plev->v,(double)-plev->u) * 180. / pi;
            if (dir < 0) dir = dir + 360;
            dir = dir - 270;
            if (dir < 0) dir = dir + 360;
            *(ptr+1) = (nclong) ( dir );
          } else {
            spd = RMISS;
            dir = RMISS;
          }
          ptr += 2;

          if ( plev->w != RMISS ) {
            *ptr = (nclong) ((((plev->w < 0.) ? -0.005:0.005) + plev->w)*100. );
          }
          ptr++;

          plev = plev->nextlev;
        }                                  /* Level loop */

/*
**  <<<<< UPC mod 20001202 >>>>> do a simple increment to the number of
**                               stations processed since everything is
**                               written to the file at this point
*/

#if 0
        if ( mod == 0 ) station_count++;
#endif
        station_count++;
        ob_lo_array[0] = mod;
        ob_hi_array[0] = mod;

        iret = file_observation( md_filename, row  , col, ob_lo_array );
        iret = file_observation( md_filename, row+1, col, ob_hi_array );

      }

      p = p->next;
      col++;

    }  /* Station loop */

    if ( col > mdhead.ncols )
      serror("WARNING: MD #cols %ld < dataset #stn %d",mdhead.ncols, col-1);

    row_header[2] = low_mode.longval;
    row_header[3] = station_count;
    iret = update_row_header( md_filename, row  , row_header, 4 );

    row_header[2] = high_mode.longval;
    iret = update_row_header( md_filename, row+1, row_header, 4 );

    if ( irte > 0 ) {
      i = ((time_interval == 60) ? 1 : 10000);
      rtecom[7] *= i;
      iret       = 0;
      (void) fshrte_( &irte, &iret );
    }

    return;

}

int make_md_file_name( nclong mdf , char *output )

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
    char char_array[8] , *string;
    int length;
    char *tempname;

    udebug( "In make_md_file_name" );

    if ( mdf < 10000){
        length = 4;
        sprintf(char_array, "%04ld\n" , mdf);
        char_array[4] = '\0';
        strcpy(output,"MDXX");
    }
    else if (mdf > 9999 && mdf < 100000){
        length = 5;
        sprintf(char_array, "%05ld\n", mdf);
        char_array[5] = '\0';
        strcpy(output,"MDX");
    }
    else if (mdf > 99999 && mdf < 1000000){
        length = 6;
        sprintf(char_array, "%06ld\n", mdf);
        char_array[6] = '\0';
        strcpy(output,"MD");
    }
    else{
        return(-1);
    }

    string = strcat(output,char_array);
    tempname = Mcpathname(output);
    (void) strcpy(output,tempname);
    return(1);
}

int make_md_file( char file[], nclong ccyyddd, char *stime, char *schema )

/*
**      make_md_file    - builds the skeletons for the 2 MD files needed
**                        for the profiler data.  It also pads the entire
**                        MD file with hex80s so subsequent writes will go
**                        faster.
**
**          input:
**              file    - LW file name of the MD file being created
**              ccyyddd - Julian day that the data is valid for
**              schema  - Schema to use in create
**
**          return codes:
**              -3      - unable to locate SCHEMA LW file
**              -2      - unable to find schema in SCHEMA file
**              -1      - unable to open destination MD file
**               0      - if successful
*/

{
   const char *schema_file;
   MDHEADER mhead;

   int  i, loc, pages;
   int  num;
   long pointer, beginning;

   char info[33];
   char *p;

   nclong long_buffer[1024];
   nclong header[4096];

   time_t datetime;
   struct tm *settime;

   FILE      *fp;

   udebug( "In make_md_file" );

/*
** Open SCHEMA file aborting on error
*/

   schema_file = Mcpathname("SCHEMA");
   if (schema_file == (char *) NULL)  return(-1);
   fp = fopen( schema_file, "rb" );
   if ( fp == NULL ) {
      serror("unable to open SCHEMA file");
      return(-3);
   }

/*
** Find the highest version number of schema in SCHEMA.  Exit if not found.
*/

   loc = 0;
   mdhead.schemver = -1;

   for ( i = 1; i <= 500; i++ ) {
     (void) fread( (char*)&mhead, sizeof(mhead), 1, fp );
     num = 1;
     (void) swbyt4_( &mhead.schemver, &num );
     if ( !strncmp((char *)&mhead.schema, schema, 4) &&
          (mhead.schemver > mdhead.schemver) )          {
       mdhead = mhead;
       loc = i;
     }
   }

   if ( loc == 0 ) {
      serror("unable to locate schema %s in SCHEMA file", schema);
      (void) fclose( fp );
      return(-2);
   }

   num = 14;
   (void) swbyt4_( &mdhead.schemreg, &num );
   num = 2;
   (void) swbyt4_( &mdhead.projnum , &num );
   num = 34;
   (void) swbyt4_( &mdhead.rowhdloc, &num );
   num = 1;
   (void) swbyt4_( &mdhead.filenum,  &num );

/*
** The correct schema has been found.  Set output data buffer to MISS
** and position the file pointer at the starting location of the MD
** skeleton in the SCHEMA lw file.
*/

   for ( i = 0; i <= 1023; i++ ) long_buffer[i] = mdhead.missing;

   pointer = ((loc-1) * 4096 + 32768) * 4;
   i = fseek( fp, pointer, SEEK_SET );
   i = fread( (char *)header, sizeof(nclong), 4096, fp );
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
   (void) swbyt4_( &header[  1], &num );
   num = 2;
   (void) swbyt4_( &header[ 24], &num );
   num = 34;
   (void) swbyt4_( &header[ 27], &num );
   num = 1;
   (void) swbyt4_( &header[ 63], &num );
   num = 400;
   (void) swbyt4_( &header[864], &num );  /* swap all scale factor words */

/*
** Rewrite the Text description block (words 16-23)
*/
   if ( strncmp(schema, "WPRO", (size_t) 4) == 0 )
     (void) sprintf( info, "Hourly Prof data for %s", stime );
   else
     (void) sprintf( info, "6-min Prof data for  %s", stime );

   (void) strncpy( (char *) &header[16], info, 32 );

/*
** Open the MD file
*/

   fp = fopen( file, "w+b" );
   if ( fp == NULL ) {
     serror( "unable to open file \"%s\"", file );
     return(-1);
   }

/*
** Position file to beginning of the file and write out the skeleton
*/

   beginning = 0;
   i = fseek( fp, beginning, SEEK_SET );
   pages = (int) (header[30] / 1024) + 3;
   for ( i = 0; i < pages; i++ ) {
     fwrite( (char *)long_buffer, sizeof(nclong), 1024, fp );
   }
   i = fseek( fp, beginning, SEEK_SET );

/*
** Set the integer ID of the file to the date of the data and the creation
** date to the current day.  Set the creator's ID to USER.
** <<<<< UPC mod 20000117 - MD file created date must be in YYYDDD form >>>>>
*/
   header[15] = ccyyddd;
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
   fwrite( (char *)header, sizeof(nclong), 4096, fp );
   (void) fclose( fp );

   return(0);

}

int ymd_to_julian_day( int year, int month, int day, char *stime )

/*
**  ymd_to_julian_day - converts year, month, and day to julian day
**
**          input:
**              year  - year [ccyy]
**              month - month [1-12]
**              day   - day of month
*/

{
    time_t datetime;
    struct tm settime;

    int    value;

    size_t nchar;

    udebug( "In ymd_to_julian_day" );

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
    
	nchar = strftime( stime, (size_t) 11, "%d %b %Y", &settime );
    if ( nchar == 0 ) stime[0] = '0';

    udebug( "ymd_to_julian_day:: value = %d, stime = %s", value, stime );

    return( value );
}


int update_row_header( char mdf[], int row, nclong row_header[], int nwrite )
/*
**      update_row_header  - writes 'nwrite' elements from 'row_header'
**                          to mdf 'row'
**          input:
**              mdf        - MD file name for the data being filed
**              row        - row header number to write to
**              row_header - data to write
**              nwrite     - number of elements to write
*/

{
    int     i;
    long    fptr;
    size_t  j;

    FILE   *fp;

    udebug( "In update_row_header" );

    fptr = 4 * (mdhead.rowhdloc + (row-1)*4);

    fp = fopen( mdf, "r+b" );
    i  = fseek( fp, fptr, SEEK_SET );
    j  = fwrite( (char *)row_header, sizeof(row_header[0]), nwrite, fp);
    (void) fclose( fp );

    return(0);
}

int file_observation( char mdf[], int row, int col, nclong data[] )
/*
**      file_observation - files the entire data section for a given
**                         MD file, row, and column
**          input:
**              mdf      - MD file name for the data being filed
**              row      - row number to file to
**              col      - column number to file to
**              data     - nclong array containing data
*/

{
    int     i;
    long    fptr;
    size_t  j;

    FILE   *fp;

    udebug( "In file_observation" );

    fptr = mdhead.rowhdloc + mdhead.nrows * 4;
    fptr = 4 * (fptr + (mdhead.ncols * (row-1) + (col-1)) * mdhead.ndkeys);

    fp = fopen( mdf, "r+b" );
    i  = fseek( fp, fptr, SEEK_SET );
    j  = fwrite( (char *)data, sizeof(nclong), mdhead.ndkeys, fp );
    (void) fclose( fp );

    return (0);
}

