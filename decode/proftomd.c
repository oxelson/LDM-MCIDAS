/*
**   Copyright 1997, University Corporation for Atmospheric Research
**   Do not redistribute without permission.
**
**   proftomd.c
**      Generic netCDF format profiler decoder. See usage information
**   for options.
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
#include <atexit.h>
#endif

#include "mkdirs_open.h"
#include "profiler.h"

extern void  write_mcidas( char *, char *, char *, int, prof_struct * );
extern void *decode_ncprof( char *, long, int * );

#define HEX80	0x80808080
#define MISS	HEX80

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
    pcode        McIDAS product code (U2 - 6 min, U4 - hourly)\n\
    schema       McIDAS MD file schema (WPR6 - 6 min, WPRO - hourly)\n\
    mdbase       McIDAS base MD file number (91 - 6 min, 81 - hourly)\n"

    (void) fprintf (stderr, USAGE_FMT, argv0);
}

/*
 * Called at exit.
 * This callback routine registered by atexit().
 */
static void cleanup() {
    uinfo("Exiting") ;
    (void) closeulog();
}


typedef struct envlist{
    char *env;
    struct envlist *next;
} envlist;


main(argc,argv)
int argc;
char *argv[];
{

    char          line[255];
    char         *buf;
    char         *datadir = NULL;
    char         *infilnam = NULL;
    char         *mcpathbuf = NULL;       /* MCPATH environment variable ptr */
    char         *outfilnam = NULL;
    char         *logfname = NULL;
    char         *ofil = NULL;
    char         *odir,*tempnam,*dpos;
    char         *pcode, *schema;

    int           in_file;
    int           out_file;
    int           len,mdbase;
    
    envlist *envhead=NULL,*envobj;
    
    extern int    optind;
    extern int    opterr;
    extern char  *optarg;

    int           ch;
    int           logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE));
    
    int           rlen;
    int           DO_REMOVE = 0;
    
    mode_t        omode=S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
    
    prof_struct  *head=NULL;
    int           iret;
    
    /*
     * register exit handler
     */
    if ( atexit(cleanup) != 0 ) {
      serror("atexit");
      exit(1);
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
             datadir = optarg;
             break;
          case 'f':
             infilnam = optarg;
             break;
          case 'n':
             outfilnam = optarg;
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
             envobj = (envlist *)malloc(sizeof(envlist));
             envobj->env = malloc(strlen(optarg));
             strcpy(envobj->env,optarg);
             envobj->next = envhead;
             envhead = envobj;
             break;
          case '?':
             usage(argv[0]);
             exit(0);
          }
       }
    
    /*
    ** Parse command line positional parameters
    */
    switch(argc - optind) {
       case 3:
          pcode  = argv[optind++];
          schema = argv[optind++];
          mdbase = atol(argv[optind++]);
          break;
       default:
          usage(argv[0]);
          exit(-1);
    }
    
    /*
    ** Update directory for temporary file if 'datadir' specified by user
    */
    if ( datadir != (char *)NULL ) {
      odir = (char *) malloc(strlen(datadir)+20);
      odir[0] = '\0';
      strcat(odir,datadir);
      strcat(odir,"/");
      ofil = odir;
    }
    
    /*
    ** Set ulog mask and announce start
    */
    (void) setulogmask(logmask);
    (void) openulog(ubasename(argv[0]), (LOG_CONS|LOG_PID), LOG_LOCAL0, logfname) ;
    uinfo("Starting up");
    
	/*
	 * Unset MCPATH environment variable if it exists
	 */
	mcpathbuf = getenv("MCPATH");
	if (mcpathbuf != NULL) {
	  uinfo("unsetting MCPATH environment variable");
	  (void) putenv("MCPATH=");
	}

    /*
    ** Set environment variable if '-e' flag set by user
    */
    envobj = envhead;
    while (envobj != NULL) {
      udebug("putenv %s\0",envobj->env);
      if (putenv(envobj->env) != 0)
        uerror("Can't set environmental variable %s\0",envobj->env);
      envobj = envobj->next;
    }
    
    /*
    ** if input from stdin, redirect to netcdf file "outfilnam"
    */
    if (infilnam == 0) {
       if (outfilnam == 0) { /* create a temporary file name */
          tempnam = (char *)malloc(19);
          tempnam[0] = '\0';
          sprintf(tempnam,".tmp_netcdf.XXXXXX\0");
          mktemp(tempnam);
          udebug("creating temporary netcdf file %s\0",tempnam);
          /* create temp file in directory of output  (get path to ofil)*/
          dpos = strrchr(ofil,'/');
          if (dpos == NULL)
             outfilnam = tempnam;
          else {
             outfilnam = (char *)malloc(strlen(ofil)+20);
             outfilnam[0] = '\0';
             strncat(outfilnam,ofil,dpos-ofil);
             strncat(outfilnam,"/",1);
             strcat(outfilnam,tempnam);
          }
          DO_REMOVE = 1;
          udebug("look %s\0",outfilnam);
       }
    
       in_file = STDIN_FILENO;
       out_file = mkdirs_open(outfilnam,O_WRONLY|O_CREAT|O_TRUNC,omode);
       if(out_file < 0) {
          uerror("could not open %s\n",outfilnam);
          exit(-1);
       }

       buf = (char *)malloc(8196);
       while((rlen = read(in_file,buf,8196)) > 0) {
          udebug("writing %d bytes\0",rlen);
          write(out_file,buf,rlen);
       }
       free(buf);
       close(out_file);
       infilnam = outfilnam;
       udebug("now will input netcdf from %s\0",infilnam);
    } else {
       udebug("will get input from %s\0",infilnam);
    }
    
    /*
    ** call netcdf decoder with infilnam
    */
    head = (prof_struct *) decode_ncprof(infilnam,MISS,&iret);
    
    if ( (head != NULL) && (iret == 0) )
       write_mcidas(datadir, pcode, schema, mdbase, head);
    else
       uerror("could not decode data %d\0",iret);
    
    /*
    ** <<<<< UPC move 20021221 >>>>>
    ** we are done with netcdf file now, remove if necessary
    */
    if ( DO_REMOVE == 1 ) { 
      udebug( "deleting temporary netcdf file %s\0", outfilnam );
      if ( unlink(outfilnam) != 0 )
        uerror( "Could not delete temp file %s\0", outfilnam );
    }
    
    /*
    ** Done
    */
    exit(0);
}
