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

#include "png.h"
#include "mkdirs_open.h"
#include "ulog.h"
#include "alarm.h"

#define WMO_HED_LEN 21
#define GINI_HED_LEN 533

/*
** Some needed function prototypes <<<<< UPC add 20080918 >>>>>
*/
void eat_stream_and_exit( int );
void readpng_row( char * );

/*
** The following are needed since this links against pngsubs.o which needs
** to have these defined.
*/

char            *annotfile=NULL;
char            *bandfile=NULL;

char             mcpathdir[PATH_MAX];                /* McIDAS MCPATH string */
char             pathname[PATH_MAX];        /* fully qualified file pathname */

int              rtecom[64];

/*
** On to declarations that are used
*/

char             pathname[PATH_MAX];        /* fully qualified file pathname */

int              READ_ERROR=0;

unsigned         alarm_timeout = 60; /* seconds; should suffice */

png_size_t       pngcount=0;
png_structp      png_ptr;
png_infop        info_ptr;
png_infop        end_info;

/*
** Function prototypes
*/

char *GiniFileName( unsigned char *imghed, char *format );
void  swbyt4_( void *, int * );

void  unidata_read_data_fn( png_structp png_p, png_bytep data, png_uint_32 length );
void  read_row_callback( png_structp pngp, png_uint_32 rownum, int pass );

/*
 * SIGALRM handler.
 */
static void
alarm_handler( int sig )
{
    assert( SIGALRM == sig );
    uerror( "pngg2gini: no data read in %u seconds, exiting", alarm_timeout );
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

static void usage(argv0)
char *argv0;
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
    IMGfile         Output image pathname mask (no default)\n\n\
                    'IMGfile' may contain 'replaceables':\n\
                      \\AREA - image coverage info\n\
                      \\BAND - image band info\n\
                      \\RES  - image resolution info\n\
                      \\SAT  - satellite platform info\n\n\
                    and 'strftime' style day/time specifiers:\n\
                      %%Y    - image year       [CCYY]\n\
                      %%m    - image month      [MM]\n\
                      %%d    - image day        [DD]\n\
                      %%j    - image Julian day [JJJ]\n\
                      %%H    - image hour       [HH]\n\
                      %%M    - image minute     [MM]\n\
                      etc.\n\n\
Example Invocation:\n\
    ./pngg2gini -l - -f pngfile data/images/\\AREA/\\RES/\\BAND/\\BAND_%%Y%%m%%d.%%H%%M\n\n\
might expand to the equivalent:\n\
    ./pngg2gini -l - -f pngfile data/images/NHEM-COMP/8km/6.8um/20000511.2000\n\n"

    (void) fprintf (stderr, USAGE_FMT, argv0, argv0, argv0, argv0);
}

static char *logfname = "";

int
main( int argc, char *argv[] )
{
 
    char         *infname  = NULL;
    char         *outfname = NULL;
    char         *p        = NULL;

    char         *rowp;

    char          wmoprd[6];

    char          wmohed[WMO_HED_LEN+1];
    unsigned char imghed[GINI_HED_LEN];

    int           ch;
    int           i;
    int           nbytes;
    int           ok;

    int           bit_depth        = 8;
    int           color_type       = PNG_COLOR_TYPE_GRAY;
    int           interlace_type   = PNG_INTERLACE_NONE;
    int           compression_type = PNG_COMPRESSION_TYPE_DEFAULT;
    int           filter_type      = PNG_FILTER_TYPE_DEFAULT;

    int           logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE)) ;
    int           logfd;

    int           fdo;

    png_uint_32   height,width;

    mode_t        mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    long          ntot = 0;

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
    ** Setup PNG pointers, etc.
    */

    png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if ( !png_ptr ) exit( 2 );

    info_ptr = png_create_info_struct( png_ptr );
    end_info = png_create_info_struct( png_ptr );

    if ( !info_ptr ) {
      png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
      exit( 3 );
    }

    if ( setjmp(png_ptr->jmpbuf) ) {
      png_destroy_read_struct( &png_ptr, &info_ptr, &end_info );
      exit( 4 );
    }

    png_set_read_fn( png_ptr, info_ptr, (png_rw_ptr)unidata_read_data_fn );
    png_set_read_status_fn( png_ptr, read_row_callback );


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
    ** Determine if the input file header looks like a GINI image
    */

    nbytes = GINI_HED_LEN;
    unidata_read_data_fn( png_ptr, (png_bytep) imghed, (png_uint_32) nbytes );

    if ( READ_ERROR ) {
      uerror( "GINI header read error" );
      eat_stream_and_exit( 5 );
    }

    memcpy( wmohed, imghed, WMO_HED_LEN );
    wmohed[WMO_HED_LEN] = '\0';

    if ( strncmp( (char *)imghed, "TI", 2 ) ) {
      (void) memcpy( wmoprd, imghed, 6 );
      wmoprd[5] = '\0';
      uerror( "WMO Header not supported by pngg2gini: %s", wmoprd );
      eat_stream_and_exit( 6 );
    }

    if ( imghed[21] != 1 ) {
      uerror( "Input fails Source test: 1 != %c", imghed[21] );
      eat_stream_and_exit( 7 );
    }

    if ( imghed[63] == 128 ) {              /* zero out the compression flag */
      imghed[63] = 0;
    }


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
      eat_stream_and_exit( 8 );
    }

    uinfo( "output file pathname: %s", outfname );

    /* <<<<< UPC mod 20040914 - add file truncation >>>>> */
    fdo = mkdirs_open( outfname, O_CREAT|O_WRONLY|O_TRUNC, mode );
    if ( fdo == -1 ) {                /* make sure directories to file exist */
      uerror( "Can't open file %s", outfname );
      eat_stream_and_exit( 9 );
    }

    write( fdo, imghed, nbytes );          /* write the header w/o byte flip */


    /*
    ** Read the PNG informational header
    */

    png_read_info( png_ptr, info_ptr );
    png_get_IHDR( png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                  &interlace_type, &compression_type, &filter_type );


    /*
    ** Allocate a buffer into which image lines are unPNGed.
    */

    rowp = (char *) malloc( width );
    if ( rowp == (char *) NULL ) {
      uerror( "Unable to malloc space for AREA row" );
      eat_stream_and_exit( 10 );
    }


    /*
    ** UnPNG an image line at a time and write it to the output file.
    */

    for ( i=0; i<height; i++ ) {
      readpng_row( rowp );
      write( fdo, rowp, width );
      ntot += width;
    }


    /*
    ** Free malloc/realloc arrays.
    */

    alarm_off( &timeout_alarm );

    free( rowp );


    /*
    ** Close the output file and exit.
    */

    close( fdo );
    fclose( stdin );

    ntot     += nbytes;                       /* all bytes written to output */
    pngcount += 16;                         /* compressed bytes + PNG header */

    unotice( "unPNG:: %8ld  %8ld  %6.4f", pngcount, ntot,
              ((float) ntot) / pngcount );

    unotice( "Exiting" );

    exit( 0 );
}
