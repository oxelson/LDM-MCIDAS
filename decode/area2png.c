#define UD_FORTRAN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#include "ulog.h"
#include "png.h"

#define IMG_DIR_LEN 64

char            *annotfile=NULL;
char            *bandfile=NULL;

char             mcpathdir[]="./";                /* McIDAS MCPATH string */

int              READ_ERROR=0;

int              rtecom[64];     /* supply definition for getrte_ and fshrte_ */

png_size_t       pngcount=0;

/*
** Function prototypes
*/

char *GetFileName( int *imgdir, char *format );
void  fbyte4_( void *buffer, int *num );
void  fshrte_(  int *, int * );
int   getrte_(  int *, int *, char * );
int   ischar ( const void *value );

static void
usage( const char *av0 )
{
#define USAGE_FMT "\
Usage: %s -f AREAfile <-a SATANNOT> <-b SATBAND> <-vxl-> PNGfile\n\
Where:\n\
    -a SATANNOT  McIDAS SATANNOT pathname (def: /home/mcidas/data/SATANNOT)\n\
    -b SATBAND   McIDAS SATBAND pathname (def: /home/mcidas/data/SATBAND)\n\
    -f AREAfile  Input AREA pathname (no default)\n\
    -h           Print the help file, then exit the program\n\
    -n           Echo the output file name to stdout (def=no)\n\
    -l logname   Log file name (default uses local0; use '-' for stdout)\n\
    -v           Verbose logging\n\
    -x           Debug logging\n\
Positionals:\n\
    PNGfile      Output PNG pathname mask (no default)\n\n\
                 'PNGfile' may contain 'replaceables':\n\
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
    ./area2png -l - -f /data/ldm/mcidas/AREA0170 \\SAT_\\RES_\\BAND_%%Y%%m%%d.%%H%%M\n\n\
might expand to the equivalent:\n\
    ./area2png -l - -f /data/ldm/mcidas/AREA0170 GOES-8_IMG_8km_6.8um_20000511.2000\n\n"

    (void) fprintf( stderr, USAGE_FMT, av0 );
}

static char     *logfname = "";

int
main(int argc, char *argv[])
{
    char        *infile=NULL;

    char        *prodmmap;
    char        *memheap;

    char        *pathname;

    char         defannotfile[] = "/home/mcidas/data/SATANNOT";
    char         defbandfile[]  = "/home/mcidas/data/SATBAND";

    int          ch;
    int          echo=0;
    int          fdi;
    int          fdo;
    int          imgdir[64];
    int          i;
    int          one=1;

    int          nelem;
    int          nline;
    int          elesiz;
    int          nband;
    int          presiz;
    int          linlen;
    int          imgsiz;
    int          datoff;
    int          cmtlen;
    int          cmtoff;

    int          nbytes;
    int          nwords;
    int          outlen;

    mode_t       mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

    struct stat  statb;

    extern int   optind;
    extern int   opterr;
    extern char *optarg;

    int          logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE)) ;
    int          logfd;

    /*
    ** Validate command line arguments
    */

    if ( argc < 4 ) {
      usage( argv[0] );
      exit( 0 );
    }

    /*
    ** Parse command line flags
    **
    ** NOTE: getopt args expecting a value need to be followed by a colon.
    **
    **       -f /data/mcidas/AREA0120
    */

    opterr = 1;
    while ((ch = getopt(argc,argv,"hna:b:f:vxl:")) != EOF) {
      switch(ch) {
        case 'h':
        case '?':
                 usage( argv[0] );
                 exit( 0 );
                 break;
        case 'a':
                 annotfile = optarg;
                 break;
        case 'b':
                 bandfile = optarg;
                 break;
        case 'f':
                 infile = optarg;
                 break;
        case 'n':
                 echo = 1;
                 break;
        case 'v':
                 logmask |= LOG_MASK(LOG_INFO);
                 break;
        case 'x':
                 logmask |= LOG_MASK(LOG_DEBUG);
                 break;
        case 'l':
                 if ( optarg[0] == '-' && optarg[1] != 0 ) {
                   (void) fprintf( stderr, "logfile \"%s\" ??\n", optarg );
                   usage( argv[0] );
                   exit( 0 );
                 }
                 /* else */
                 logfname = optarg;
                 break;

      }
    }

    /*
    ** Sanity checks
    */

    if ( argc - optind < 0 ) {
      usage( argv[0] );
      exit( 0 );
    }

    if ( infile == (char *) NULL ) {
      printf( "No input file specified" );
      exit( 1 );
    }

    if ( argc != (optind + 1) ) {
      printf( "No output file specified" );
      exit( 1 );
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
    ** Open the input data file specified on the command line
    */

    fdi = open( infile, O_RDONLY, 0 );

    if ( fdi == -1 ) {
      uerror( "Can't open input file %s", infile );
      exit( 1 );
    }

    if ( fstat(fdi, &statb) == -1 ) {
      uerror( "Can't fstat input file %s", infile );
     (void) close( fdi );
     exit( 2 );
    }

    udebug( "input AREA size: %d", (int)statb.st_size );

    /*
    ** Memory map the input file
    */

    prodmmap = (char *) mmap( 0, statb.st_size,
                              PROT_READ, MAP_PRIVATE, fdi, 0 );

    if ( (void *) prodmmap == (void *) -1 ) {
      perror("prodmmap: allocation failed");
      (void) close(fdi);
      exit( 3 );
    }

    /*
    ** Determine if the input file is a McIDAS AREA
    */

    nwords = IMG_DIR_LEN;
    nbytes = 4 * nwords;

    (void) memcpy( imgdir, prodmmap, nbytes );

    if ( imgdir[0] != 0 ) {
      uerror( "Input file %s fails word 0 AREA test", infile );
      exit ( 4 );
    }

    if ( imgdir[1] != 4 ) {
      fbyte4_( &imgdir[1], &one );

      if ( imgdir[1] != 4 ) {
        uerror( "Input file %s fails word 1 AREA test", infile );
        exit( 5 );
      }

      for ( i=2; i<64; i++ ) {           /* flip bytes in non-character words */
        if ( !ischar( &imgdir[i] ) ) fbyte4_( &imgdir[i], &one );
      }
    }

    /*
    ** Flesh out the output file name and verify that it can be created.
    */

    pathname = GetFileName( imgdir, argv[optind] );
    if ( pathname == (char *) NULL ) {
      uerror( "Error creating output name from pattern: %s", argv[optind] );
      (void) close( fdi );
      exit( 6 );
    }

    fdo = mkdirs_open( pathname, O_WRONLY|O_CREAT|O_TRUNC, mode );
    if ( fdo < 0 ) {
      uerror( "Error opening/creating output file name %s", pathname );
      exit( 1 );
    }

    if ( echo )
      printf( "%s", pathname );

    uinfo( "output file pathname: %s", pathname );

    /*
    ** Allocate memory for the PNG compressed output file.  We assume that
    ** worst compression possible will result in an output file size the
    ** same as the input file size.
    */

    memheap = (char *) malloc( statb.st_size );
    if ( memheap == (char *) NULL ) {
      uerror( "Error mallocing %d bytes for compressed output file",
               (int)statb.st_size );
      (void) close( fdi );
      exit( -7 );
    }

    /*
    ** Begin transferring image data to the compressed output file
    */

    udebug( "memory mapped input file %s", infile );

    png_set_memheap( memheap );

    /*
    ** Get the shape of the image(s) contained in the AREA.
    */

    nline  = imgdir[ 8];                    /* number of image lines        */
    nelem  = imgdir[ 9];                    /* number of image elements     */
    elesiz = imgdir[10];                    /* # bytes per element          */
    nband  = imgdir[13];                    /* # image bands                */
    presiz = imgdir[14];                    /* length of line prefix        */
    
    linlen = presiz + nband * elesiz * nelem;
    imgsiz = nline * linlen;

    udebug( "image shape: %d x %d", nline, nelem );
    udebug( "image line length: %d", linlen );
   
    datoff = imgdir[33];
    cmtlen = 80 * imgdir[63];
    cmtoff = datoff + imgsiz;

    udebug( "number of bytes to beginning of data: %d", datoff );
    udebug( "number of bytes in comment cards: %d", cmtlen );
    udebug( "number of bytes to beginning of comment cards: %d", cmtoff );

    /*
    ** Copy all of the image headers to the output file
    */

    png_header( prodmmap, datoff );         /* img, nav, cal, aux dirs      */
    png_header( prodmmap+cmtoff, cmtlen );  /* comment cards                */

    /*
    ** Initialize PNG output routines, and write out compressed image lines.
    */

    pngout_init( linlen, nline ); 

    for ( i = 0; i < nline; i++)
       pngwrite((unsigned char *)(prodmmap+datoff+(i*linlen)));

    /*
    ** Close off the PNG compression
    */
    pngout_end(); 
    close( fdi );

    outlen = png_get_prodlen();

    udebug( "deflated length %d", outlen );

    unotice( "doPNG:: %8d  %8d  %6.4f\n", (int)statb.st_size,
           outlen, ((float) outlen) / statb.st_size );

    /*
    ** Write the output file
    */

    write( fdo, memheap, outlen );
    close( fdo );

    exit( 0 );
}
