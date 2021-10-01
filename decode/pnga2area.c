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


#define IMG_DIR_LEN 64

char            *annotfile=NULL;
char            *bandfile=NULL;

char             mcpathdir[PATH_MAX];                /* McIDAS MCPATH string */
char             pathname[PATH_MAX];        /* fully qualified file pathname */

int              READ_ERROR=0;

int              rtecom[64];

unsigned         alarm_timeout = 60; /* seconds; should suffice */

png_size_t       pngcount=0;
png_structp      png_ptr;
png_infop        info_ptr;
png_infop        end_info;

/*
** Function prototypes
*/

char *Mcpathname ( char * );
char *GetFileName( int *imgdir, char *format );
void  unidata_read_data_fn( png_structp png_p, png_bytep data, png_uint_32 length );
void  read_row_callback( png_structp pngp, png_uint_32 rownum, int pass );
void  fbyte4_ ( void *, int * );
void  fliprte_( );
void  fshrte_ (  int *, int * );
int   getrte_ (  int *, int *, char * );
int   ischar  ( const void *value );
void  swbyt4_ ( void *, int * );
void  sysin_  (  int *, int * );

/*
 * SIGALRM handler.
 */
static void
alarm_handler( int sig )
{
    assert( SIGALRM == sig );
    uerror( "pnga2area: no data read in %u seconds, exiting", alarm_timeout );
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
       %s <-a SATANNOT> <-b SATBAND> <-vxl-> IMGfile < infile \n\
       %s <-a SATANNOT> <-b SATBAND> <-vxl-> -d datadir -r pcode < infile \n\
       %s <-a SATANNOT> <-b SATBAND> <-vxl-> -f infile IMGfile\n\
       %s <-a SATANNOT> <-b SATBAND> <-vxl-> -f infile -d datadir -r pcode\n\
Flags:\n\
    Help:\n\
    -h              Print the help, then exit the program\n\n\
    Satellite definitions:\n\
    -a SATANNOT     McIDAS SATANNOT pathname (def: /home/mcidas/data/SATANNOT)\n\
    -b SATBAND      McIDAS SATBAND pathname (def: /home/mcidas/data/SATBAND)\n\
    -f infile       Pathname for input PNG compressed file (default is stdin)\n\n\
    Logging:\n\
    -l logname      Log file name (default uses local0; use '-' for stdout)\n\
    -v              Verbose logging\n\
    -x              Debug logging\n\n\
    McIDAS Routing:\n\
    -d directory    Directory to CD to decoding (used with '-r'; no default)\n\
    -r pcode<,anum> Use McIDAS ROUTE.SYS entry for <pcode> for AREA file naming\n\
                    (used with '-d'; no default)\n\
                    pcode - McIDAS routing table product code\n\
                    ,     - if anum is specified, must use comman separator\n\
                    anum  - default output AREA number\n\
Positionals:\n\
    IMGfile         Output image pathname mask if not using McIDAS routing (no default)\n\n\
                    'IMGfile' may contain 'replaceables':\n\
                      \\BAND - band information from SATBAND\n\
                      \\RES  - resolution information from SATBAND\n\
                      \\SAT  - satellite platform info from SATANNOT\n\n\
                    and 'strftime' style day/time specifiers:\n\
                      %%Y    - image year       [CCYY]\n\
                      %%m    - image month      [MM]\n\
                      %%d    - image day        [DD]\n\
                      %%j    - image Julian day [JJJ]\n\
                      %%H    - image hour       [HH]\n\
                      %%M    - image minute     [MM]\n\
                      etc.\n\n\
Example Invocation:\n\
    ./pnga2area -l - -f pngfile \\SAT_\\RES_\\BAND_%%Y%%m%%d.%%H%%M\n\n\
might expand to the equivalent:\n\
    ./pnga2area -l - -f pngfile GOES-8_IMG_8km_6.8um_20000511.2000\n\n"

    (void) fprintf (stderr, USAGE_FMT, argv0, argv0, argv0, argv0);
}

static char *logfname = "";

int
main( int argc, char *argv[] )
{
 
    char        *datadir  = NULL;
    char        *infname  = NULL;
    char        *outfname = NULL;
    char        *mcpath   = NULL;
    char        *p        = NULL;

    char        *header;

    char        *rowp;

    char         defannotfile[]="/home/mcidas/data/SATANNOT";
    char         defbandfile[]="/home/mcidas/data/SATBAND";

    char         areaname[12];
    char         ctext[12];
    char         pcode[12]="            ";

    int          ch;
    int          cmtlen;
    int          datlen;
    int          datoff;
    int          i;
    int          iarea=0;
    int          narea=0;
    int          iflip=0;
    int          imgdir[IMG_DIR_LEN];
    int          imghed[IMG_DIR_LEN];
    int          irte=0;                     /* <<<<< UPC mod 20021219 >>>>> */
    int          nbytes;
    int          one=1;

    int          bit_depth        = 8;
    int          color_type       = PNG_COLOR_TYPE_GRAY;
    int          interlace_type   = PNG_INTERLACE_NONE;
    int          compression_type = PNG_COMPRESSION_TYPE_DEFAULT;
    int          filter_type      = PNG_FILTER_TYPE_DEFAULT;

    int          logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE)) ;
    int          logfd;

    int          fdo;

    png_uint_32  height,width;

    mode_t       mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    long         ntot = 0;

    extern int   optind;
    extern int   opterr;
    extern char *optarg;

    int          alarm_initialized = 0;
    Alarm        timeout_alarm;

    /*
    ** If nothing was specified on the command line, print usage and exit.
    */
    if ( argc == 1 ) {
      usage( argv[0] );
      exit( 0 );
    }

    opterr = 1;

    while ( (ch = getopt(argc, argv, "a:b:hf:d:r:vxl:")) != EOF )
      switch (ch) {
        case 'a':
                 annotfile = optarg;
                 break;
        case 'b':
                 bandfile = optarg;
                 break;
        case 'h':
        case '?':
                 usage( argv[0] );
                 exit( 0 );
                 break;
        case 'f':
                 infname = optarg;
                 break;
        case 'd':
                 datadir = optarg;
                 break;
        case 'r':
                 p = strstr( optarg, "," );
                 if ( p != (char *) NULL ) {
                   i = p - optarg;
                   (void) strncpy( pcode, optarg, i );
                   *(pcode+i) = 0;
                   iarea = atoi( p+1 );
                 } else {
                   (void) strcpy( pcode, optarg );
                   iarea = 10;
                 }
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
      if ( pcode != (char *) NULL ) {                 /* '-r' flag specified */
        if ( datadir == (char *) NULL ) {         /* '-d' flag not specified */
          (void) strcpy( mcpathdir, "./" );
        } else {
          (void) strcpy( mcpathdir, datadir );
        }
      } else {
        printf( "No output file name specified, exiting\n" );
        exit( 1 );
      }
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
    ** Fill in default SATANNOT and SATBAND pathnames if needed
    */

    if ( annotfile == (char *) NULL ) annotfile = defannotfile;
    if ( bandfile  == (char *) NULL ) bandfile = defbandfile;

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
    ** Determine if the input file header looks like a McIDAS AREA
    */

    nbytes = 4 * IMG_DIR_LEN;
    unidata_read_data_fn( png_ptr, (png_bytep) imghed, (png_uint_32) nbytes );

    if ( READ_ERROR ) {
      uerror( "AREA header read error" );
      eat_stream_and_exit( 5 );
    }

    if ( imghed[0] != 0 ) {
      uerror( "Input fails word 0 AREA test" );
      eat_stream_and_exit( 6 );
    }

    if ( imghed[1] != 4 ) {
      fbyte4_( &imghed[1], &one );

      if ( imghed[1] != 4 ) {
        uerror( "Input fails word 1 AREA test" );
        eat_stream_and_exit( 7 );
      }

      fbyte4_( &imghed[1], &one );
      iflip = 1;
    }

    (void) memcpy( imgdir, imghed, nbytes );

    if ( iflip ) {                      /* flip bytes in non-character words */
      for ( i=0; i<IMG_DIR_LEN; i++ ) {
        if ( !ischar( &imgdir[i] ) ) fbyte4_( &imgdir[i], &one );
      }
    }

    /*
    ** Create the output file name.  There are two cases:
    **
    **   AREAnnnn - '-r pcode' -> file name comes from McIDAS routing table
    **   IMGfile  - file name or mask specified.  In this case, we need to
    **              expand the "replaceables" (\BAND, \RES, and \SAT) and
    **              strftime format specifications (e.g., %Y, %m, %d, etc.)
    **              in user specified filename using information from the
    **              AREA file header.
    */

    if ( *pcode != ' ' ) {                  /* use McIDAS routing table info */
#if 0
      mcpath = getenv( "MCPATH" );
      if ( mcpath != (char *) NULL ) {
        (void) putenv( "MCPATH=" );
      }
      uinfo( "setting MCPATH to %s", mcpathdir );
      (void) putenv( mcpathdir );
#endif
      (void) strncpy( (char *) &rtecom[1], "    ", 4);
      (void) strncpy( (char *) &rtecom[1], pcode,  2);

      udebug( "calling getrte with iarea = %d, narea = %d", iarea, narea );
      irte = getrte_( &iarea, &narea, ctext );

      if ( irte < 0 ) {

        unotice( "Product %s routing table entry SUSpended, exiting", pcode );
        eat_stream_and_exit( 8 );

      }

      udebug( "getrte returns %d, iarea = %d, narea = %d", irte, iarea, narea );
      (void) sprintf( areaname, "AREA%04d", narea );
      outfname = (char *) Mcpathname( areaname );

    } else {                                        /* use IMGfile name/mask */
      outfname = GetFileName( imgdir, argv[optind] );
      if ( outfname == (char *) NULL ) {
        uerror( "Error creating output name from pattern: %s", argv[optind] );
        eat_stream_and_exit( 9 );
      }
    }

    uinfo( "output file pathname: %s", outfname );

    /* <<<<< UPC mod 20040914 - add file truncation to open >>>>> */
    fdo = mkdirs_open( outfname, O_CREAT|O_WRONLY|O_TRUNC, mode );
    if ( fdo == -1 ) {                /* make sure directories to file exist */
      uerror( "Can't open file %s", outfname );
      eat_stream_and_exit( 10 );
    }

    write( fdo, imghed, nbytes );          /* write the header w/o byte flip */

    /*
    ** Now figure out where the beginning of the PNG compressed image is.
    */

    datoff = imgdir[33];
    datlen = datoff - nbytes;
    cmtlen = 80 * imgdir[63];

    udebug( "datoff: %d", datoff );
    udebug( "datlen: %d", datlen );
    udebug( "cmtlen: %d", cmtlen );

    /*
    ** Allocate space for the AREA header portion that lies between the
    ** image directory and the data block.
    */

    header = (char *) malloc( datlen );
    if ( header == (char * ) NULL ) {
      uerror( "Unable to malloc space for AREA file header" );
      eat_stream_and_exit( 11 );
    }

    /*
    ** Read the AREA header portion between image directory and comment
    ** cards and write the block to the output file.
    */

    unidata_read_data_fn( png_ptr, (png_bytep) header, (png_uint_32) datlen );
    write( fdo, header, datlen );

    /*
    ** Check to see if the length of the comment cards is greater than
    ** the AREA header portion between the image directory and the
    ** data block.  If it is, realloc a new buffer, otherwise use
    ** the header buffer just allocated and read the comment cards into
    ** it.  This block will be written to the output file after the data.
    */

    if ( cmtlen > datlen ) {
      header = realloc( header, cmtlen );
      if ( header == (char * ) NULL ) {
        uerror( "Unable to realloc space for AREA comment cards" );
        eat_stream_and_exit( 12 );
      }
    }

    unidata_read_data_fn( png_ptr, (png_bytep) header, (png_uint_32) cmtlen );

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
      eat_stream_and_exit( 13 );
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
    ** Write out the comment card section.
    */

    write( fdo, header, cmtlen );

    /*
    ** Free malloc/realloc arrays.
    */

    alarm_off( &timeout_alarm );

    free( header );
    free( rowp );

    /*
    ** Update routing table if using McIDAS routing to determine outfname
    */

    if ( irte != 0 ) {                       /* <<<<< UPC mod 20021219 >>>>> */
      rtecom[6] = imgdir[3];
      rtecom[7] = imgdir[4];
      udebug( "calling fshrte with irte = %d, one = %d", irte, one );
      fshrte_( &irte, &one );
    }

    /*
    ** Close the output file and exit.
    */

    close( fdo );
    fclose( stdin );

    ntot     += nbytes + datlen + cmtlen;     /* all bytes written to output */
    pngcount += 16;                         /* compressed bytes + PNG header */

    unotice( "unPNG:: %8ld  %8ld  %6.4f", pngcount, ntot,
              ((float) ntot) / pngcount );

    unotice( "Exiting" );

    exit( 0 );
}
