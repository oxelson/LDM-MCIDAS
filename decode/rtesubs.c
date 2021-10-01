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

/*
** Externally declared variable(s)
*/

extern int    rtecom[];

/*
** Function prototypes
*/

int   Mckeyin  ( const char * );
int   getrte_  (  int *, int *, char * );

void  fliprte_ ( );
void  fshrte_  (  int *, int * );
void  sysin_   (  int *, int * );

char *Mcpathname( char * );

/********************************* Mckeyin ***********************************/

int
Mckeyin( const char *cmd )

/*
** Name:
**      Mckeyin - Executes a command asynchronously.
**
** Interface:
**      int
**      Mckeyin(const char *cmd);
**
** Input:
**      cmd     - Contains the command to be executed.
**
** Input and Output:
**      none
**
** Output:
**      none
**
** Return values:
**       -2 Program is not executable (or not found).
**       -1 Failure.
**      =>0 Indicates that child process start was attempted.
**
** Remarks:
**      The status of the asynchronous child is not returned.
**
** Categories:
**      system
*/

{

    int          rc;

    static pid_t my_pid;

    /*
    ** First make a copy of the executing process
    */

    my_pid = fork();

    if ( my_pid == (pid_t) -1 ) {                             /* fork failed */
      uerror( "fork failure while attempting to run PostProcess BATCH file" );
      return -1;

    } else if ( my_pid == (pid_t) 0 ) {                     /* child process */
      {
        const char sh[] = "/bin/sh";
        execl(sh, sh, "-c", cmd, (char *)0);
      }
      _exit( 0 );
      return 0;

    } else {                                               /* parent process */
      if ( my_pid < (pid_t) 1 ) {                          /* bad process ID */
        return -2;
      } else {                                        /* process ID of child */
        return (int) my_pid;
      }

    }


}

/********************************* getrte ************************************/

int
getrte_( int *inum, int *onum, char *ctitle )

/*
**    getrte - get routing table entry for product
**
**    input:
**            inum  - First guess AREA/MD file number
**            onum  - Returned AREA/MD file number
**          ctitle  - Name of textual product
**
**    output:
**            entry - System Key Table entry for 'prod'
**
**    returns:
**               <0 - Do not file product
**               =0 - No routing table or entry inconsistent (e.g. TEXT
**                      instead of AREA), file product using defaults
**               >0 - Routing table position of entry (one-based)
**
**    'entry' values:
**
**    WORD(S)  NAME    DESCRIPTION
**      0      rstat   Routing status 0=SUSpended , 1=RELeased
**      1      rprod   Product name
**      2      rtype   Product type (AREA,MD,GRID,TEXT)
**      3      rbeg    Cylinder beginning number
**      4      rend    Cylinder ending number
**      5      rnow    Current product number filed in cylinder
**      6      rcday   Create DAY (CCYYDDD)
**      7      rctim   Create TIME (HHMMSS)
**      8      rrday   Receive DAY (CCYYDDD)
**      9      rrtim   Receive TIME (HHMMSS)
**    10-17    rmemo   Memo field (32 characters)
**    18-20    rpost   BATCH file name for post process
**    21-23    rmask   TEXT file name mask for TYPE=TEXT
**    24-26    rtext   Last TEXT file name created
**     27      rcond   Decoder condition code (1-7 , def=1)
**     29      rlast   Last successful product number (defined by rcond)
**    29-31    rltxt   Last successful text file name (defined by rcond)
**     32      rxcut   Execute Post Process batch file based on rcond
**     33      rsysb   if >0 -- SYSKEY index for beginning cylinder number
**     34      rsyse   if >0 -- SYSKEY index for ending cylinder number
**     35      rsysc   if >0 -- SYSKEY index for current cylinder number
**    36-63    rdum    Reserved
*/

{
    char       *p;
    char        pcodes[512];
    const char *route_file;

    int         badentry, i, iret, j, rc;

    long        offset;

    FILE       *fp;

    time_t      datetime;
    struct tm  *settime;

    /*
    ** Set default AREA number and error return.
    */
    *onum = *inum;                           /* <<<<< UPC add 20021219 >>>>> */
    iret  = 0;

    /*
    ** Check for valid file name
    */
    route_file = (const char *) Mcpathname( "ROUTE.SYS" );
    if ( route_file == (char *) NULL ) {

      uerror( "Error creating fully qualified pathname for ROUTE.SYS" );
      return( iret );

    }

    /*
    ** Check to see if routing file exists
    */
    fp = fopen( route_file, "r" );
    if ( fp == (FILE *) NULL ) {

      uerror( "Error opening routing table %s", route_file );
      return( iret );

    }

    /*
    ** Read product code portion of routing table
    */
    if ( fread (pcodes, 2, 256, fp) != 256 ) {

      serror( "Error reading routing codes" );
      (void) fclose ( fp );
      return ( iret );

    }

    /*
    ** Find product code in routing table
    */
    p = pcodes;

    for ( i = 0; i < 256; i++ ) {

      if ( strncmp(p, (char *) &rtecom[1], 2) == 0 ) {

        iret = -1;
        offset = 4 * ( i*64 + 128 );
        (void) fseek ( fp, offset, SEEK_SET );
        (void) fread ( rtecom, 4, 64, fp );
        (void) fliprte_();

        badentry = strncmp((char *) &rtecom[2], "AREA", 4);
        if ( badentry )
          badentry = strncmp((char *) &rtecom[2], "MD", 2);

#if 0
        for ( j=0; j<36; j++ ) {
          udebug( "rtecom[%02d] = %4d", j, rtecom[j] );
        }
#endif

        if ( rtecom[0] != 0 && !badentry ) {
          iret = i + 1;
          if ( rtecom[3] == -1 && rtecom[4] == -1 ) {
            rtecom[5] = *inum;
          } else {
            if ( rtecom[5] == -1 ) {
              rtecom[5] = rtecom[3];
            } else {
              rtecom[5] = rtecom[5] + 1;
              if ( rtecom[5] > rtecom[4] ) rtecom[5] = rtecom[3];
            }
            *onum = rtecom[5];
          }
        } else if ( badentry ) {
          unotice( "Routing entry is of wrong type.  Filing using defaults" );
          iret  = 0;
        } else {
          iret  = 0;
        }

        break;

      }

      p += 2;

    }

    (void) fclose (fp);

    /*
    ** Get the calendar time.   <<<<< UPC move 20021219 >>>>>
    */
    if ( iret >= 0 ) {

      if ( time( &datetime ) != -1 ) {

        settime   = gmtime( &datetime );
        rtecom[8] = 1000*(1900+settime->tm_year) + settime->tm_yday + 1;
        rtecom[9] = 10000*settime->tm_hour + 100*settime->tm_min + settime->tm_sec;

      } else {

        serror( "Error getting calendar time" );
        return ( 0 );

      }

    }

    /*
    ** Return product code position (one-based).
    ** Note:  0 -> product code not found or entry has wrong type
    */

    return ( iret );

}

/********************************* fshrte ************************************/

void
fshrte_( int *irte, int *istat )

/*
**    fshrte - get routing table entry for product
**
**    input:
**            irte  - First guess AREA file number
**           istat  - Returned AREA file number
**
**    istat:
**               -2 - Total decode failure
**               -1 - Partial decode success
**                1 - Total decode success
**
*/

{
    const char *route_file;

    char       *p;
    char        rtype[5];
    char        ckyn[161];
    char        cpost[13];
    char        cprod[5];
    char        cval[13];

    int         ipos;
    int         pass;
    int         rcond;
    int         rval;
    int         rc;

    long        offset;

    size_t      nwrite;

    FILE        *fp;

/*
** Check for valid file name
*/
    route_file = (const char *) Mcpathname( "ROUTE.SYS" );
    if ( route_file == (char *) NULL ) {
      uerror( "Error creating fully qualified pathname" );
      return;
    }

/*
** Check to see if routing file exists
*/
    fp = fopen( route_file, "r+" );
    if ( fp == (FILE *) NULL ) {
      serror( "Error opening routing table" );
      return;
    }

/*
** Initialize strings
*/
    (void) memset( ckyn,  ' ', 160 );
    (void) memset( cpost, ' ',  12 );
    (void) memset( cprod, ' ',   4 );
    (void) memset( cval,  ' ',  12 );

/*
** Set file write position
*/
    ipos = *irte - 1;
    offset = 4 * ( 64 * ipos + 128 );
    (void) fseek ( fp, offset, SEEK_SET );

/*
** Check condition code
*/
    pass = 0;
    rcond = rtecom[27];

    if ( *istat >= 0 ) {
       if ( rcond == 0 || rcond == 1 || rcond == 3 ||
            rcond == 5 || rcond == 7) pass = 1;
    } else if( *istat == -1 ) {
       if ( rcond == 2 || rcond == 3 || rcond == 6 ||
            rcond == 7) pass = 1;
    } else {
       if ( rcond == 4 || rcond == 6 || rcond == 7) pass = 1;
    }

/*
** Write product information back to routing table
*/
    if ( pass == 0 ) {

      uerror( "Product Routing: Product failed condition test: %d", rcond);
      rtecom[5] = rtecom[29];
      (void) fliprte_();
      (void) fwrite ( rtecom, 4, 64, fp );
      (void) fliprte_();
      if ( rtecom[32] == 0 ) {
        (void) fclose( fp );
        return;
      }

    } else {

       rtecom[29] = rtecom[5];
       (void) fliprte_();
#if 0
       (void) fwrite ( rtecom, 4, 64, fp );
#endif
       nwrite = fwrite ( rtecom, 4, 64, fp );
       udebug( "fshrte:: nwrite items written by fwrite = %ld", nwrite);

       (void) fliprte_();
       (void) fclose( fp );

       (void) strncpy( rtype, (char *) &rtecom[2], 4 );
       p = strstr( rtype, " " );
       if ( p != (char *) NULL ) *p = 0;

       if ( rtecom[33] != 0 ) {
         if        ( !strncmp( rtype, "TEXT", 4 ) )  {
           (void) sysin_( &rtecom[33], &rtecom[ 3] );
           (void) sysin_( &rtecom[34], &rtecom[ 4] );
           (void) sysin_( &rtecom[35], &rtecom[29] );
         } else if ( !strncmp( rtype, "AREA", 4 ) )  {
           (void) sysin_( &rtecom[33], &rtecom[3] );
           (void) sysin_( &rtecom[34], &rtecom[4] );
           (void) sysin_( &rtecom[35], &rtecom[ 5] );
         } else if ( !strncmp( rtype, "GRID", 4 ) )  {
           (void) sysin_( &rtecom[33], &rtecom[29] );
           rval = rtecom[ 7]/10000;
           (void) sysin_( &rtecom[34], &rval );
           (void) sysin_( &rtecom[35], &rtecom[ 6] );
         } else if ( !strncmp( rtype, "MD", 2 ) )  {
           (void) sysin_( &rtecom[33], &rtecom[29] );
           rval = rtecom[ 7]/10000;
           (void) sysin_( &rtecom[34], &rval );
           (void) sysin_( &rtecom[35], &rtecom[ 6] );
         } else {
           uerror( "Unrecognized product type %s", rtype );
         }
       }
    }

    if ( strncmp( (char *) &rtecom[18], " ", 1 ) != 0 ) {

      (void) memcpy( cprod, &rtecom[1], 4);
      p = strstr( cprod, " " );
      if ( p != (char *) NULL ) *p = 0;

      (void) memcpy( cpost, &rtecom[18], 12 );
      p = strstr( cpost, " " );
      if ( p != (char *) NULL ) *p = 0;

      if ( strcmp( rtype, "TEXT" ) == 0 ) {
        (void) strncpy( cval, (char * ) &rtecom[24], 12 );
        p = strstr( cval, " " );
        if ( p != (char *) NULL ) *p = 0;
      } else {
        (void) sprintf( cval, "%d", rtecom[5] );
      }

      (void) sprintf( ckyn, "batch.k BATCH %s %s %d %d %d \\\"%s",
                      cprod, cval, rtecom[6], rtecom[7], *istat, cpost );
      udebug( ckyn );

      rc = Mckeyin( ckyn );
      if ( rc < 0 ) {
        uerror( "Error %d while trying to execute ROUTE PP BATCH file", rc );
      }
    }

}

/********************************* sysin_ *************************************/

void
sysin_( int *rsys, int *rval )

/*
**    sysin_ - get routing table entry for product
**
**    input:
**            rsys  - System Key Table entry to update
**            rval  - value to put in 'rsys'
**
**    return:
**            none
**
*/

{
    FILE       *fp;
    int         val;
    int         one=1;
    int         sval;
    long        sloc;
    const char *syskey_file;

    syskey_file = (const char *) Mcpathname( "SYSKEY.TAB" );
    if ( syskey_file == (char *) NULL )  {
      serror( "Error creating SYSKEY.TAB pathname" );
      return;
    }

    fp = fopen( syskey_file, "r+" );
    if ( fp == (FILE *) NULL ) {
      serror( "SYSKEY.TAB not found or is writable" );
      return;
    }

    sloc = (long) *rsys * 4;
    sval = *rval;

    (void) fread( &val, 1, 4, fp );
    if ( val < 0L || val > 999999L ) {
      fbyte4_( &sval, &one );
    }

    (void) fseek ( fp, sloc, SEEK_SET );
    (void) fwrite( &sval, 1, 4, fp );

    fclose (fp);
    return;

}

/******************************** fliprte ************************************/

void
fliprte_( )

/*
**    fliprte - flip appropriate entries in routing table common
**
**    input:
**            none
**    return:
**            none
**
*/

{
    int num;

/*
** Flip the non-ASCII bytes in the global RTECOM array
*/

    num = 1;
    (void) swbyt4_( &rtecom[ 0], &num );       /* <<<<< UPC mod 990505 >>>>> */
    num = 7;
    (void) swbyt4_( &rtecom[ 3], &num );       /* <<<<< UPC mod 990505 >>>>> */
    num = 2;
    (void) swbyt4_( &rtecom[27], &num );       /* <<<<< UPC mod 990505 >>>>> */
    num = 32;
    (void) swbyt4_( &rtecom[32], &num );       /* <<<<< UPC mod 990505 >>>>> */

}

