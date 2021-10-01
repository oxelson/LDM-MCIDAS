/*
** Name:      lgt2md.c
**
** Purpose:   Lightning decoder for NLDN and USPLN data.
**
** Note:      See usage information for options.
**
** Copyright: Copyright 1997, University Corporation for Atmospheric Research
**            Do not redistribute without permission.
**
** History:   20060923 - created
*/

/*
** Includes
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "ulog.h"

#ifdef NO_ATEXIT
#  include <atexit.h>
#endif

#include "lightning.h"

/*
** Function prototypes
*/
int  decode_lgt( char *, int, char *, char *, int, int );

/*
** Usage
*/

static void usage(argv0)
char *argv0;
{
#define USAGE_FMT "\
Usage: %s [flags] pcode mdbase\n\
Flags:\n\
    -h           Print the help file, then exit the program\n\
    -f filename  Input NetCDF file (default is stdin)\n\
    -n filename  Output NetCDF file name (default not saved)\n\
                 [-n is ignored if -f is used]\n\
    -d path      Change to directory <path> before decoding\n\
    -l filename  Log output (default uses local0, '-' to stdout\n\
    -v           Verbose logging\n\
    -x           Debug logging\n\
    -e VAR=value Set an environmental variable in program\n\
Positionals:\n\
    mdbase       McIDAS base MD file number (91 - 6 min, 81 - hourly)\n\
    schema       McIDAS MD file schema (WPR6 - 6 min, WPRO - hourly)\n\
Keywords:\n\
    DIALPROD=pcode [day] [time]\n\
             pcode - McIDAS product code (LD - NLDN, LP - USPLN)\n\
             day   - day [ccyyjjj]\n\
             time  - time [hhmmss]\n\
    DEV=xyz\n\
             Ignored as of ldm-mcidas v2006\n"

    (void) fprintf (stderr, USAGE_FMT, argv0);
}

/*
** This callback routine registered by atexit().
*/
static void cleanup() {
    uinfo( "Exiting" ) ;
    (void) closeulog();
}


typedef struct envlist{
    char *env;
    struct envlist *next;
} envlist;


main(argc,argv)
int   argc;
char *argv[];
{

    char         *datadir   = NULL;
    char         *infilnam  = NULL;
    char         *mcpathbuf = NULL;       /* MCPATH environment variable ptr */
    char         *logfname  = NULL;
    char          defdir[]  = "./";

    /* Command line flag parsing */
    int           ch;
    extern int    optind;
    extern int    opterr;
    extern char  *optarg;
    envlist      *envhead=NULL;
    envlist      *envobj;

    /* Command line parameters */
    int           mdbase;
    char         *schema;
    char          pcode[3];               /* DIALPROD=pcode day time */
    int           rday;                   /* DIALPROD=pcode day time */
    int           rtime;                  /* DIALPROD=pcode day time */

    /* Logging */
    int           logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE));
    
    lgt_struct   *data=NULL;
    int           rc;
    
    /*
     * register exit handler
     */
    if ( atexit(cleanup) != 0 ) {
      serror( "atexit" );
      exit( 1 );
    }
    
    /*
    ** Parse command line flags
    */
    opterr = 1;
    while ((ch = getopt(argc,argv,"hvxl:d:f:n:e:")) != EOF) {
       switch(ch) {
          case 'h':
             usage(argv[0]);
             exit(0);
             break;
          case 'd':
             datadir  = optarg;
             break;
          case 'f':
             infilnam = optarg;
             break;
          case 'l':
             logfname = optarg;
             break;
          case 'v':
             logmask |= LOG_MASK(LOG_INFO);
             break;
          case 'x':
             logmask |= LOG_MASK(LOG_DEBUG);
             break;
          case 'e':
             envobj       = (envlist *) malloc( sizeof(envlist) );
             envobj->env  = malloc( strlen(optarg) );
             strcpy( envobj->env,optarg );
             envobj->next = envhead;
             envhead      = envobj;
             break;
          case '?':
             usage(argv[0]);
             exit(0);
          }
    }
    if ( datadir == (char *) NULL ) {
      datadir = defdir;
    }
    
    /*
    ** Parse command line positional parameters
    */
    switch( argc-optind ) {
       case 2:
       case 3:
       case 4:
       case 5:
       case 6:
          mdbase = atol(argv[optind++]);
          mdbase = 10 * (mdbase / 10);
          schema = argv[optind++];
          break;
       default:
          usage(argv[0]);
          exit(-1);
    }
    
    /*
    ** Parse command line keyword parameters
    */
    switch( argc-optind ) {
       case 0:
          (void) strcpy( pcode, "LD" );
          rday   = -1;
          rtime  = -1;
          break;
       case 1:
          (void) sscanf( argv[optind++], "DIALPROD=%2s", pcode );
          break;
       case 2:
          (void) sscanf( argv[optind++], "DIALPROD=%2s", pcode );
          rday  = atol( argv[optind++] );
          break;
       case 3:
       case 4:
          (void) sscanf( argv[optind++], "DIALPROD=%2s", pcode );
          rday  = atol( argv[optind++] );
          rtime = atol( argv[optind++] );
          break;
       default:
          usage(argv[0]);
          exit(-1);
    }

    /*
    ** Set ulog mask and announce start
    */
    (void) setulogmask(logmask);
    (void) openulog(ubasename(argv[0]),(LOG_CONS|LOG_PID),LOG_LOCAL0,logfname);
    uinfo("Starting up");
    
    udebug( "MD base = %4d\n", mdbase );
    udebug( "SCHEMA = %s\n", schema );
    udebug( "Pcode = %s\n", pcode );
    udebug( "Day = %d\n", rday );
    udebug( "Time = %d\n", rtime );

	/*
	 * Unset MCPATH environment variable if it exists
	 */
	mcpathbuf = getenv( "MCPATH" );
	if ( mcpathbuf != (char *)NULL ) {
	  uinfo( "unsetting MCPATH environment variable" );
	  (void) putenv( "MCPATH=" );
	}

    /*
    ** Set environment variable if '-e' flag set by user
    */
    envobj = envhead;
    while ( envobj != (envlist *)NULL ) {
      udebug( "putenv %s\0", envobj->env );
      if ( putenv(envobj->env) != 0 )
        uerror( "Can't set environmental variable %s\0", envobj->env );
      envobj = envobj->next;
    }
    
    /*
    ** If input is from a file redirect STDIN
    */
    if ( infilnam != (char *)NULL ) {
       if ( freopen(infilnam, "r", stdin) == NULL ) {
         uerror( "Unable to open input file \"%s\", exiting", infilnam );
         exit( 1 );
       }
       udebug( "input data from %s\0", infilnam );
    } else {
       udebug( "input data from STDIN" );
    }
    
    /*
    ** Call lightning decoder passing command line parameters
    */
    rc = decode_lgt( datadir, mdbase, schema, pcode, rday, rtime );
    
    /*
    ** Done
    */
    exit( 0 );

}
