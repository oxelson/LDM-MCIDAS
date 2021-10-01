#define UD_FORTRAN_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#ifndef PATH_MAX
#  define PATH_MAX 255
#endif
#include "zlib.h"
#include "zutil.h"
#include "mkdirs_open.h"
#include "ulog.h"
#include "alarm.h"

#define GINI_HED_LEN 533
#define NUMREAD      10000
#define WMO_HED_LEN  21
#define ZLIB_BUF_LEN 10000

/*
** Some needed function prototypes <<<<< UPC add 20080918 >>>>>
*/
int getbuf( int, char *, int, int );
int GetInt( unsigned char *, int );

/*
** The following are needed since this links against imgsubs.o which needs
** to have these defined.
*/

char            *annotfile=NULL;
char            *bandfile=NULL;

char             mcpathdir[PATH_MAX];                /* McIDAS MCPATH string */
char             pathname[PATH_MAX];        /* fully qualified file pathname */


/*
** On to declarations that are used
*/

unsigned         alarm_timeout = 60;              /* seconds; should suffice */


/*
** Function prototypes
*/

char      *GiniFileName( unsigned char *, char * );
void       eat_stream_and_exit( int );

/*
 * SIGALRM handler.
 */
static void
alarm_handler( int sig )
{
    assert( SIGALRM == sig );
    uerror( "zlibggini: no data read in %u seconds, exiting", alarm_timeout );
    exit( 1 );
}

/*
** Called at exit.
** This callback routine registered by atexit().
*/

static void
cleanup()
{
    (void) closeulog();
}

/*
** Called upon receipt of signals.
** This callback routine registered in set_sigactions() .
*/

static void
signal_handler( int sig )
{
    switch( sig ) {
      case SIGHUP:
                   udebug( "SIGHUP" ) ;
                   return ;
      case SIGINT:
                   unotice( "Interrupt" ) ;
                   exit( 0 );
      case SIGTERM:
                   udebug( "SIGTERM" ) ;
                   exit( 0 );
      case SIGUSR1:
                   udebug( "SIGUSR1" ) ;
                   return;
      case SIGUSR2:
                   if ( toggleulogpri(LOG_INFO) )
                     unotice( "Going verbose" ) ;
                   else
                     unotice( "Going silent" ) ;
                   return;
      case SIGPIPE:
                   unotice( "SIGPIPE" ) ;
                   exit( 0 );
    }

    udebug( "signal_handler: unhandled signal: %d", sig );
}

static void
set_sigactions()
{
    struct sigaction sigact ;

    sigact.sa_handler   = signal_handler;
    sigemptyset(&sigact.sa_mask) ;
    sigact.sa_flags     = 0 ;

    (void) sigaction( SIGHUP,  &sigact, (struct sigaction *) NULL );
    (void) sigaction( SIGINT,  &sigact, (struct sigaction *) NULL );
    (void) sigaction( SIGTERM, &sigact, (struct sigaction *) NULL );
    (void) sigaction( SIGUSR1, &sigact, (struct sigaction *) NULL );
    (void) sigaction( SIGUSR2, &sigact, (struct sigaction *) NULL );
    (void) sigaction( SIGPIPE, &sigact, (struct sigaction *) NULL );
}


/*
** Usage
*/

static void usage( char *argv0 )
{
#define USAGE_FMT "\
Example Usage:\n\
       %s <-vxl-> IMGfile < infile \n\
       %s <-vxl-> -f infile IMGfile\n\
Flags:\n\
    Help:\n\
    -h              Print the help, then exit the program\n\n\
    Satellite definitions:\n\
    -f infile       Pathname for input PNG compressed file (default is stdin)\n\n\
    Logging:\n\
    -l logname      Log file name (default uses local0; use '-' for stdout)\n\
    -v              Verbose logging\n\
    -x              Debug logging\n\n\
Positionals:\n\
    IMGfile         Output image pathname (no default)\n\n\
Example Invocation:\n\
    %s -f zlibfile outfile\n\n\
    -- or --\n\n\
    %s outfile < zlibfile\n\n"

    (void) fprintf (stderr, USAGE_FMT, argv0, argv0, argv0, argv0);
}

static char *logfname = "";

int
main( int argc, char *argv[] )
{

    Bytef          uncompr[ZLIB_BUF_LEN];
    uLong          uncomprLen=ZLIB_BUF_LEN;
    
    unsigned long  intot;
    unsigned long  outtot=0;

    char          *infname  = NULL;
    char          *outfname = NULL;

    char           buf[101]={0};

    const char    *c;

    unsigned char *b;
    unsigned char *p;

    unsigned char  imghed[GINI_HED_LEN+1];
    unsigned char  data[NUMREAD];

    int            ch;
    int            hoff=0;
    int            numin;
    int            rc;

    int            elems;
    int            lines;
    int            nline;
    int            totlin = 0;

    int            logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE)) ;
    int            logfd;

    size_t         nbyte;
    size_t         nmove;

    z_stream       d_stream;                    /* Zlib decompression stream */

    mode_t         mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int            fd_in = 0;                              /* standard input */
    int            fd_out;

    extern int    optind;
    extern int    opterr;
    extern char  *optarg;

    int           alarm_initialized = 0;
    Alarm         timeout_alarm;


    /*
    ** If nothing was specified on the command line, print usage and exit.
    */
    if ( argc == 1 ) {
      usage( argv[0] );
      exit( 0 );
    }

    opterr = 1;

    while ( (ch = getopt(argc, argv, "hf:vxl:")) != EOF )
      switch (ch) {
        case 'h':
        case '?':
                 usage( argv[0] );
                 exit( 0 );
                 break;
        case 'f':
                 infname = optarg;
                 break;
        case 'v':
                 logmask |= LOG_MASK(LOG_INFO);
                 break;
        case 'x':
                 logmask |= LOG_MASK(LOG_DEBUG);
                 break;
        case 'l':
                 if ( optarg[0] == '-' && optarg[1] != 0 ) {
                   fprintf( stderr, "logfile \"%s\" ??\n", optarg );
                   usage( argv[0] );
                   exit( 0 );
                 }
                 /* else */
                 logfname = optarg;
                 break;
      }

    /*
    ** Sanity checks
    */

    if ( argc - optind < 0 ) {
      usage( argv[0] );
      exit( 0 );
    }

    if ( argc != (optind + 1) ) {          /* output file name not specified */
      printf( "No output file name specified, exiting\n" );
      exit( 1 );
    }


    /*
    ** If user specified an input file with '-f' flag, redirect
    ** stdin to the file aborting on error.
    */

    if ( infname != (char *) NULL ) {
      if ( freopen( infname, "r", stdin ) == (FILE *) NULL ) {
        printf( "Can't open input file: %s\n", infname );
        exit( 1 );
      }
    }


    /*
    ** Setup syslog logging.
    */

    (void) setulogmask( logmask );

    if ( logfname == NULL || !(*logfname == '-' && logfname[1] == 0) )
      (void) fclose( stderr );

    logfd = openulog( ubasename(argv[0]), (LOG_CONS|LOG_PID), LOG_LDM,
                      logfname );

    if ( logfname == NULL || !(*logfname == '-' && logfname[1] == 0) ) {
      setbuf( fdopen(logfd, "a" ), NULL);
    }


    /*
    ** Start
    */

    unotice( "Starting Up" );

    /*
    ** Register exit handler, establish signal handlers, and set alarm handler.
    */

    (void) atexit( cleanup );

    set_sigactions();

    if ( !alarm_initialized ) {
      alarm_init( &timeout_alarm, alarm_timeout, alarm_handler );
      alarm_initialized = 1;
    }

    alarm_on( &timeout_alarm );

    /*
    ** Actual processing
    */

    rc = getbuf( fd_in, (char *)data, 1, (size_t) NUMREAD );
    (void) memcpy( buf, data, (size_t)100 );
    buf[100] = 0;

    p = (unsigned char *) strstr( (const char *)buf, "\r\r\n" );
    if ( p == (unsigned char *) NULL ) {
      uerror( "header not found in compressed image" );
      eat_stream_and_exit( 1 );
    }

    intot = p + 3 - (unsigned char *) buf;
    b     = data + intot;
    numin = NUMREAD - intot;

    /*
    ** Get the Zlib compressed image header
    */

    d_stream.zalloc    = (alloc_func) 0;
    d_stream.zfree     = (free_func) 0;
    d_stream.opaque    = (voidpf) 0;
    d_stream.next_in   = (Bytef *) b;
    d_stream.avail_in  = (uInt) numin;
    d_stream.next_out  = uncompr;
    d_stream.avail_out = (uInt) uncomprLen;

    rc = inflateInit( &d_stream );
    if ( rc != Z_OK ) {
      uerror( "inflateInit error, exiting!" );
      eat_stream_and_exit( 2 );
    }

    rc = inflate( &d_stream, Z_NO_FLUSH );
    if ( rc != Z_OK && rc != Z_STREAM_END ) {
      fprintf( stderr, "inflate error, exiting!\n" );
      eat_stream_and_exit( 3 );
    }

    if ( d_stream.total_out != GINI_HED_LEN ) {
      fprintf( stderr, "inflated image header size error, exiting!\n" );
      eat_stream_and_exit( 4 );
    }

    (void) memcpy( imghed, uncompr, (size_t)GINI_HED_LEN );
    imghed[GINI_HED_LEN] = 0;

    if ( imghed[21] != 1 ) {
      uerror( "Input fails source test: 1 != %c", imghed[21] );
      eat_stream_and_exit( 5 );
    }

    if ( imghed[63] != 0 ) {                /* zero out the compression flag */
      imghed[63] = 0;
    }

    c = strstr( (const char *)imghed, "KNES" );
    if ( c != (const char *) NULL ) {                       /* 'KNES' found  */
      c = strstr( c, "\n" );
      if ( c != (const char *) NULL ) {                     /* NL found      */
        hoff = c - (const char *) imghed + 1;
      }
    }

    if ( hoff != WMO_HED_LEN ) {
      uerror( "image header length mismatch" );
      eat_stream_and_exit( 6 );
    }

    elems = GetInt( (unsigned char *)imghed + hoff + 16, 2 );
    lines = GetInt( (unsigned char *)imghed + hoff + 18, 2 );

    udebug( "Input image:: lines = %d, elements = %d", lines, elems );

    /*
    ** Create the output file name:
    **
    **   IMGfile  - file name or mask specified.  In this case, we need to
    **              expand the "replaceables" (\BAND, \RES, and \SAT) and
    **              strftime format specifications (e.g., %Y, %m, %d, etc.)
    **              in user specified filename using information from the
    **              GINI file header.
    */

    outfname = GiniFileName( imghed, argv[optind] );
    if ( outfname == (char *) NULL ) {
      uerror( "Error creating output name from pattern: %s", argv[optind] );
      eat_stream_and_exit( 7 );
    }

    uinfo( "output file pathname: %s", outfname );

    /* <<<<< UPC mod 20040914 - add file truncation >>>>> */
    fd_out = mkdirs_open( outfname, O_CREAT|O_WRONLY|O_TRUNC, mode );
    if ( fd_out == -1 ) {             /* make sure directories to file exist */
      uerror( "Can't open file %s", outfname );
      eat_stream_and_exit( 8 );
    }


    do {

      intot  += d_stream.total_in;
      outtot += d_stream.total_out;

      nline   = d_stream.total_out / elems;
      totlin += nline;

      if ( totlin > lines ) break;

      write( fd_out, uncompr, d_stream.total_out );

      udebug( "bytes in: %4d, bytes out: %4d, nlines = %2d, total lines = %4d",
               d_stream.total_in, d_stream.total_out, nline, totlin );

      nmove = numin - d_stream.total_in;
      if ( nmove > 0 )
        (void) memmove( data, b+d_stream.total_in, nmove );
      else
        break;

      nbyte = NUMREAD - nmove;
      numin = nmove + getbuf( fd_in, (char *)(data+nmove), 1, nbyte );
      b     = data;

      d_stream.zalloc    = (alloc_func) 0;
      d_stream.zfree     = (free_func) 0;
      d_stream.opaque    = (voidpf) 0;
      d_stream.next_in   = (Bytef *) b;
      d_stream.avail_in  = (uInt) numin;
      d_stream.next_out  = uncompr;
      d_stream.avail_out = (uInt) uncomprLen;

      rc = inflateInit( &d_stream );
      if ( rc != Z_OK ) {
        uerror( "inflateInit error, exiting!" );
        break;
      }

      rc = inflate( &d_stream, Z_NO_FLUSH );

    } while( (rc == Z_STREAM_END) || (rc == Z_OK) );


    alarm_off( &timeout_alarm );

    /*
    ** Close the output file and exit.
    */

    close( fd_out );
    fclose( stdin );

    unotice( "unZlib:: %8ld  %8ld  %6.4f",
             intot, outtot, ((float)outtot / intot) );

    unotice( "Exiting" );

    exit( 0 );

}
