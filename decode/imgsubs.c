#define UD_FORTRAN_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#ifndef PATH_MAX
#  define PATH_MAX 255
#endif
#include <string.h>
#include <time.h>
#include <assert.h>

#include "ulog.h"

#define SUCCESS 1
#define FAILURE 0

/*
** Externally declared variables
*/

extern char  *annotfile;
extern char  *bandfile;

/*
** Function prototypes
*/

char      *GiniFileName( unsigned char *, char * );
char      *GetFileName ( int *, char * );

int        GetSatAnnotInfo( int, char * );
int        GetSatBandInfo ( int, int *, char *, float *, float * );
int        ReplaceToken   ( char *, char *, char * );


/******************************** GiniFileName *******************************/

/*
** Name:       GiniFileName
**
** Purpose:    Create a string by expanding 'tokens' with replacement values
**
** Parameters:
**             imghed - GINI image header
**             fmt    - token-laden representation of string
**
** Notes:      Tokens can take the form of:
**
**             'strftime'     - format specifiers ('man strftime' for a list)
**             'replaceables' - the list of replaceables and their meanings:
**                              \RES  - AREA spacing in image lines between
**                                        consecutive AREA lines
**                              \BAND - image band number (satellite dependent)
**                              \SS   - sensor source (satellite number)
** Returns:
**             SUCCESS == 1
**             FAILURE == 0
**
*/

char *
GiniFileName( unsigned char *imghed, char *fmt )
{

    char        *format;
    char        *pathname;

    char         areastr[32]="";
    char         bandstr[32]="";
    char         resstr[32]="";
    char         satstr[32]="";

    char         satname[][20]  = { "           ",       "           ",
                                    "COMPOSITE",         "DMSP", 
                                    "ERS",               "           ",
                                    "GOES",              "           ",
                                    "           ",       "JERS",
                                    "GMS",               "           ",
                                    "METEOSAT",          "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "TIROS",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           " };

    char         goesarea[][20] = { "AK-REGIONAL",       "AK-NATIONAL",
                                    "ALTIMETER",         "TROP-DISCUSS", 
                                    "EAST-CONUS",        "NHEM-COMP",
                                    "GAC",               "HI-NATIONAL",
                                    "HI-REGIONAL",       "PRECIP-ESTIMATES",
                                    "SCATTEROMETER",     "LAC",
                                    "SOLAR-ENV-MONITOR", "SUPER-NATIONAL",
                                    "OCEAN-COLOR",       "PR-REGIONAL",
                                    "PR-NATIONAL",       "HRPT",
                                    "SOUNDINGS",         "SST",
                                    "WINDS",             "SSM-I",
                                    "WEST-CONUS",        "WEFAX",
                                    "ASOS-CLOUD",        "AVAILABLE" };

    char         comparea[][20] = { "AK-REGIONAL",       "AK-NATIONAL",
                                    "ALTIMETER",         "TROP-DISCUSS", 
                                    "EAST-CONUS",        "NHEM-MULTICOMP",
                                    "GAC",               "HI-NATIONAL",
                                    "HI-REGIONAL",       "PRECIP-ESTIMATES",
                                    "SCATTEROMETER",     "LAC",
                                    "SOLAR-ENV-MONITOR", "SUPER-NATIONAL",
                                    "OCEAN-COLOR",       "PR-REGIONAL",
                                    "PR-NATIONAL",       "HRPT",
                                    "SOUNDINGS",         "SST",
                                    "WINDS",             "SSM-I",
                                    "WEST-CONUS",        "WEFAX",
                                    "ASOS-CLOUD",        "NEXRCOMP" };

    char         elsearea[][20] = { "           ",       "           ",
                                    "           ",       "           ", 
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           " };

    char         goesband[][20] = { "0.65",              "10.7",
                                    "12.0",              "3.9",
                                    "6.7",               "DER1",
                                    "DER2",              "DER3",
                                    "DER4",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "N0R",
                                    "    ",              "    ",
                                    "    ",              "    "};

    char         compband[][20] = { "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "NET",               "N0R",
                                    "NCR",               "NVL",
                                    "N1P",               "NTP"};

    char         elseband[][20] = { "0.65",              "3.9",
                                    "6.7",               "10.7",
                                    "12.0",              "DER1",
                                    "DER2",              "DER3",
                                    "DER4",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "N0R",
                                    "    ",              "    ",
                                    "    ",              "    "};

    int          band=0;
    int          ires=0;

    int          aindx;
    int          sindx;
    int          rc;

    struct tm    t={0};
    time_t       tsec=0;

    udebug( "in GiniFileName" );

    /*
    ** Create a string long enough to hold expanded file name
    */

    pathname = (char *) malloc( 256 );
    if ( pathname == (char *) NULL ) return( pathname );

    format = (char *) malloc( 256 );
    if ( format == (char *) NULL ) return( format );

    (void) strcpy( format, fmt );

    /*
    ** Figure out the Satellite, Coverage, Band, and Resolution
    **
    ** Supported headers look like:
    **
    **      TIG[A-Z][01-05]
    **      TICZ[00-99]
    */

    sindx = imghed[2] - 'A';                /* index into satname array      */
    aindx = imghed[3] - 'A';                /* index into xxxxarea array     */

    band  = imghed[24];                     /* Physical element/Channel ID   */
    ires  = imghed[62];                     /* Image Resolution [km]         */

    switch ( imghed[2] ) {

      case 'G':
               (void) strcpy( areastr, goesarea[aindx] );
               (void) strcpy( bandstr, goesband[band]  );
               break;

      case 'C':
               (void) strcpy( areastr, comparea[aindx] );
               (void) strcpy( bandstr, compband[band]  );
               break;

      case 'D':
      case 'E':
      case 'J':
      case 'K':
      case 'M':
      case 'T':
               (void) strcpy( areastr, elsearea[aindx] );
               (void) strcpy( bandstr, elseband[band]  );
               break;

      default:
               uerror( "Unsupported satellite: %c", imghed[2] );
               return( (char *) NULL );
    }

    (void) sprintf( resstr, "%dkm", ires );
    (void) strcpy( satstr,  satname[sindx]  );

    udebug( "sat:  %s", satstr  );
    udebug( "area: %s", areastr );
    udebug( "band: %s", bandstr );
    udebug( "res:  %s", resstr  );

    /*
    ** Replace tokens
    */

    rc = ReplaceToken( format, "\\AREA", areastr );;
    rc = ReplaceToken( format, "\\BAND", bandstr );
    rc = ReplaceToken( format, "\\RES",  resstr  );
    rc = ReplaceToken( format, "\\SAT",  satstr  );;

    /*
    ** Fill in the time structure with values from the image
    */

    if ( imghed[29] >= 70 )
      t.tm_year  = imghed[29];
    else
      t.tm_year  = 100 + imghed[29];
    t.tm_mon   = imghed[30]-1;
    t.tm_mday  = imghed[31];
    t.tm_hour  = imghed[32];
    t.tm_min   = imghed[33];
    t.tm_sec   = imghed[34];
    t.tm_isdst = -1;

    tsec = mktime( &t );

    t = *localtime( &tsec );

    rc = (int) strftime( pathname, 256, format, &t );

    if ( rc == 0 )
      return( (char *) NULL );
    else
      return( pathname );
}


/******************************** GetFileName ********************************/

/*
** Name:       GetFileName
**
** Purpose:    Create a string by expanding 'tokens' with replacement values
**
** Parameters:
**             imgdir - McIDAS AREA directory (64 4-byte words)
**             fmt    - token-laden representation of string
**
** Notes:      Tokens can take the form of:
**
**             'strftime'     - format specifiers ('man strftime' for a list)
**             'replaceables' - the list of replaceables and their meanings:
**                              \RES  - AREA spacing in image lines between
**                                        consecutive AREA lines
**                              \BAND - image band number (satellite dependent)
**                              \SS   - sensor source (satellite number)
** Returns:
**             SUCCESS == 1
**             FAILURE == 0
**
*/

char *
GetFileName( int *imgdir, char *fmt )
{
    char        *format;
    char        *pathname;
    char        *s;

    char         cband[32]="";
    char         cfreq[32]="";
    char         cres[32]="";
    char         bandstr[81]="";
    char         satstr[81]="UNKSAT";

    int          band=0;
    int          mask=0;
    int          nband=0;
    int          lres;
    int          ss=0;

    int          i;
    int          n;
    int          rc;

    float        blres=1.0;
    float        beres=1.0;

    struct tm    t={0};
    time_t       tsec=0;

    udebug( "in GetFileName" );

    /*
    ** Create a string long enough to hold expanded file name
    */

    pathname = (char *) malloc( 256 );
    if ( pathname == (char *) NULL ) return( pathname );

    format = (char *) malloc( 256 );
    if ( format == (char *) NULL ) return( format );

    (void) strcpy( format, fmt );

    ss   = imgdir[ 2];                      /* satellite identification num. */
    lres = imgdir[11];                      /* line resolution: # image lines
                                               between AREA lines            */
    mask = imgdir[18];                      /* image filter mask             */
    
    i = 1;
    for ( n=0; n< 32; n++ ) {
      if ( i & mask ) {
        band = n+1;
        nband++;
      }
      i = i << 1;
    }

    udebug( "mask: %d", mask );
    udebug( "band: %d", band );

    /*
    ** Get satellite name information from SATANNOT file
    ** <<<<< UPC mod 20060212 - only if replaceables specified >>>>>
    */

    if ( strstr( (const char *)fmt, "\\" ) != (char *)NULL ) {

      rc = GetSatAnnotInfo( ss, satstr );

      /*
      ** Get satellite band information from SATBAND file
      */

      if ( nband == 1 ) {
        rc = GetSatBandInfo( band, imgdir, bandstr, &blres, &beres );
        udebug( "bandstr: %s", bandstr );
        if ( rc == 0 ) {
          blres *= lres;
          s = strstr( (const char *)bandstr, "DESC=" );
          if ( s != (char *)NULL ) {
            (void) sscanf( bandstr, "%d", &band );
            (void) sscanf( s+6, "%s %s", cband, cfreq );
          } else {
            (void) sscanf( bandstr, "%d %s %s", &band, cband, cfreq );
          }
          (void) strcat( cband, cfreq );
        } else {
          blres = lres;
          (void) strcpy( cband, "UNKBAND" );
        }

      } else {
       (void) strcpy( cband, "MULTIBAND" );
      }

      (void) sprintf( cres, "%.0fkm", blres );
      udebug( "cres: %s", cres);

      /*
      ** Replace tokens
      */

      rc = ReplaceToken( format, "\\BAND", cband   );
      rc = ReplaceToken( format, "\\RES",  cres    );
      rc = ReplaceToken( format, "\\SAT",  satstr  );;

    }

    /*
    ** Fill in the time structure with values from the image
    */

    t.tm_sec   = imgdir[4] % 100;
    t.tm_min   = (imgdir[4] / 100) % 100;
    t.tm_hour  = imgdir[4] / 10000;
    t.tm_mday  = imgdir[3] % 1000;
    t.tm_mon   = 0;
    t.tm_year  = imgdir[3] / 1000;
    t.tm_isdst = -1;

    tsec = mktime( &t );

    t = *localtime( &tsec );

    rc = (int) strftime( pathname, 256, format, &t );

    if ( rc == 0 )
      return( (char *) NULL );
    else
      return( pathname );
}

/******************************* ReplaceToken ********************************/

int
ReplaceToken( char *mask, char *token, char *replace )

/*
** Name:       ReplaceToken
**
** Purpose:    Substitute string for replacable tokens in a string
**
** Parameters:
**             mask    - string in which to replace token
**             token   - token to replace
**             replace - replacement for token
**
** Note:       Valid tokens begin with the '\' character.
**
** Return:     >=0 first location of replacable in string
**             -1  FAILURE
**
** History: 19990520 - Created for McIDAS-X 7.50
**          19991201 - Changed logic for replacement
**
*/

{

    char *m, *p;                            /* generic character pointers    */
    char *string;                           /* working string                */

    int   loc=-1;                           /* replacable location in string */
    int   rlen;                             /* replacement string length     */
    int   tlen;                             /* token length                  */

    udebug( "in ReplaceToken" );

    string = (char *) malloc( strlen(mask) );

    if ( string == (char *)NULL ) {
      (void) serror( "string token replacement memory allocation error" );
      return loc;
    }

    m    = mask;
    rlen = strlen( replace );
    tlen = strlen( token );

    while ( (p = strstr(m,token)) != (char *)NULL ) {

      if ( loc == -1 ) loc = p - mask;

      (void) strcpy( string, p+tlen );
      *p = 0;
      (void) strcat( mask, replace );
      (void) strcat( mask, string );
      udebug( "mask: %s", mask );
      m = p + rlen;

    }

    (void) free( string );
    return loc;
}

/****************************** GetSatBandInfo *******************************/

int
GetSatBandInfo(int band, int *imgdir, char *cdesc, float *blres, float *beres)

/*
** Name:       GetSatBandInfo
**
** Purpose:    Get satellite band information from a McIDAS SATBAND file
**
** Parameters:
**             band    - band for which satellite information is desired
**             imgdir  - McIDAS AREA directory
**             cdesc   - character string description of band
**             blres   - base line resolution of satellite
**             beres   - base element resolution of satellite
**
** Return:     0 - success
**            -1 - unable to open SATBAND file
**            -2 - SATBAND has misformed 'BRes' line
**            -3 - unable to extract SATBAND band number for current Cal
**            -4 - no band information found for calibration for this sat/band
**            -5 - sat or band information not found in SATBAND
**
** History: 20000504 - Created for ldm-mcidas PNG compression/decompression
**
*/

{

    char  *s;

    char   cal[5]="    ";
    char   css[4];
    char   curcal[5];
    char   ctext[81];

    int    rc;
    int    sband;
    int    ss;

    FILE  *fp;

    udebug( "in GetSatBandInfo" );

    /*
    ** Open the SATBAND file
    */

    fp = fopen( bandfile, "r" );
    if ( fp == (FILE *) NULL ) {
      serror( "Error: unable to find SATBAND" );
      return( -1 );
    }
      
    /*
    ** Extract some parameters from the AREA directory:
    **
    **       imgdir[ 2] -> satellite identification number
    **       imgdir[51] -> calibration type
    **       imgdir[56] -> calibration type (overrides imgdir[51])
    */

    ss = imgdir[2];
    (void) sprintf( css, "%d", ss );
    udebug( "css: %s", css );

    if ( ischar( &imgdir[56] ) )
      (void) memcpy( cal, &imgdir[56], 4 );
    else
      (void) memcpy( cal, &imgdir[51], 4 );
    udebug( "cal: %s", cal );

    if ( (s = strstr(cal, " ")) != (char *) NULL ) *s=0;

    /*
    ** Read SATBAND until a 'Sat' line is found
    */

    while ( fgets(ctext, sizeof(ctext)-1, fp) != (char *) NULL ) {

      if ( (strstr(ctext, "Sat") != (char *) NULL)  &&
           (strstr(ctext, css  ) != (char *) NULL)     ) {

        while ( fgets(ctext, sizeof(ctext), fp) != (char *) NULL ) {

          if ( strstr(ctext, "EndSat") != (char *) NULL ) {
            (void) fclose( fp );
            return( -4 );
          }

          if ( (s = strstr(ctext, "Cal")) != (char *) NULL ) {
            if ( strncmp(s+4, cal, 4) == 0 ) {
              (void) strcpy( curcal, cal );
            } else {
              (void) strcpy( curcal, "    " );
            }

          } else if ( (s = strstr(ctext, "BRes")) != (char *) NULL ) {
            rc = sscanf( s+5, "%f %f", blres, beres );
            if ( rc != 2 ) {
              serror( "Error reading satellite base resolutions" );
              (void) fclose( fp );
              return( -2 );
            }

          } else {
            if ( *curcal != ' ' ) {
              rc = sscanf( ctext, "%d", &sband );
              if ( rc != 1 ) {
                (void) fclose( fp );
                return( -3 );
              }
              if ( band == sband ) {
                (void) strcpy( cdesc, ctext );
                (void) fclose( fp );
                return( 0 );
              }
            }
          }

        } /* while unpacking information for appropriate satellite/band */

      }  /* if we found a Sat line that has the desired satellite number */

    } /* while looking for a 'Sat' line that has desired satellite number */

    /*
    ** Done.  Close SATBAND file and return with non error status
    */

    uerror( "GetSatBandInfo: Sat or Band info not found in SATBAND\n" );

    (void) fclose( fp );
    return( -5 );
}

/***************************** GetSatAnnotInfo *******************************/

int
GetSatAnnotInfo( int ss, char *satstr )

/*
** Name:       GetSatAnnotInfo
**
** Purpose:    Get descriptive string for satellite
**
** Parameters:
**             ss      - satellite identification number
**             satstr  - satellite description
**
** Return:     0 - success
**            -1 - unable to open SATANNOT file
**            -2 - error reading SATANNOT file
**            -3 - satellite identification number not found in file
**
** History: 20000504 - Created for ldm-mcidas PNG compression/decompression
**
*/

{

    char  *s;

    char   cline[81];

    int    rc;
    int    sat;

    FILE  *fp;

    udebug( "in GetSatAnnotInfo" );

    /*
    ** Open the SATANNOT file
    */

    fp = fopen( annotfile, "r" );
    if ( fp == (FILE *) NULL ) {
      serror( "Error: unable to find SATANNOT" );
      return( -1 );
    }
      
    /*
    ** Read SATANNOT until the line containing the desired satellite is found
    */

    while ( fgets(cline, 81, fp) != (char *) NULL ) {

      rc = sscanf( cline+20, "%d", &sat );
      if ( rc == 1 ) {
        if ( sat == ss ) {
          (void) fclose( fp );
          s = cline + 18;
          while ( *s == ' ' ) {
            *s = 0;
            s--;
          }

          /*
          ** Special code to replace occurrances of G- with GOES-
          */

          if ( strncmp(cline, "G-", 2) == 0 ) {
            (void) strcpy( satstr, "GOES-" );
            (void) strcat( satstr, cline+2 );
          } else {
            (void) strcpy( satstr, cline );
          }

          /*
          ** Replace blanks with underscores
          */

          s = strstr( satstr, " " );
          if ( s != (char *) NULL ) *s = '_';

          /*
          ** Done fiddling
          */

          return( 0 );
        }

      } else {
        (void) fclose( fp );
        uerror( "Error reading SATANNOT file" );
        return( -2 );
        
      }

    }

    (void) fclose( fp );
    return( -3 );

}

