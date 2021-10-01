/* 
 * $Id: main.c,v 1.11 1998/11/11 00:35:27 yoksas Exp $
 *
 * LDM decoder for the McIDAS data stream.
 * Replaces McIDAS main routine
 */

#include <udposix.h>

#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>		/* access() modes */
#include <limits.h>		/* _POSIX_PATH_MAX */
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <cfortran.h>
#include "ulog.h"
#include "mc_proto.h"

UD_EXTERN_FUNC(void mcmain, (char*));


static void
usage(av0)
    const char   *av0;			/* id string */
{
#define USAGE_FMT "\
Usage: %s [options]\n\
Where:\n\
    -d path      Change to directory <path> before decoding.\n\
    -f path      Read from <path> rather than standard input.\n\
    -F path      Read from <path> rather than standard input; delete file\n\
		 at exit.\n\
    -l logfile   Log to <logfile> (\"-\" means standard error). Default uses\n\
                 syslogd(8).\n\
    -v           Verbose mode: log decoding diagnostics.\n\
    -x           Debug mode: log debugging diagnostics.\n"

    (void) fprintf(stderr, USAGE_FMT, av0);
}


/*
 * Decode the invocation line.
 *
 * Returns:
 *	0:	failure
 *	1:	success
 */
static int
deccmd(argc, argv, datadir, logpath, logmask, input_path, remove_input)
    int           argc;
    char        **argv;			/* should be `const' qualified but
					 * SunOS 5 getopt() complains */
    char        **datadir;
    char        **logpath;		/* log file pathname */
    int          *logmask;		/* logging mask */
    char        **input_path;		/* input file pathname */
    int          *remove_input;		/* remove input file? */
{
    int           status = 1;		/* status (success) */
    int           ch;
    extern int    optind;
    extern int    opterr;
    extern char  *optarg;

    /*
     * Set defaults.
     */
    *logpath = NULL;
    *logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE));
    *input_path = NULL;
    *remove_input = 0;

    opterr = 1;

    /*
     * Decode options.
     */
    while ((ch = getopt(argc, argv, "d:F:f:l:vx")) != EOF)
    {
	switch (ch)
	{
	case 'd':
	    *datadir = optarg;
	    break;
	case 'F':
	    *input_path = optarg;
	    *remove_input = 1;
	    break;
	case 'f':
	    *input_path = optarg;
	    *remove_input = 0;
	    break;
	case 'l':
	    *logpath = optarg;
	    break;
	case 'v':
	    *logmask |= LOG_MASK(LOG_INFO);
	    break;
	case 'x':
	    *logmask |= LOG_MASK(LOG_DEBUG);
	    break;
	default:
	    status = 0;
	}
    }

    if (optind < argc)
	status = 0;

    if (!status)
	/*
	 * Print usage message.
	 */
	usage(argv[0]);

    return status;
}


/*
 * Called at exit.
 * This callback routine registered by atexit().
 */
    static void
cleanup()
{
    (void) closeulog();
}


/*
 * Called upon receipt of signals.
 * This callback routine registered in set_sigactions() .
 */
    static void
signal_handler(sig)
     int sig ;
{
    switch(sig) {
      case SIGHUP :
	udebug("SIGHUP") ;
	return ;
      case SIGINT :
	unotice("Interrupt") ;
	exit(0) ;
      case SIGTERM :
	udebug("SIGTERM") ;
	exit(0) ;
      case SIGUSR1 :
	udebug("SIGUSR1") ;
	return ;
      case SIGUSR2 :
	if (toggleulogpri(LOG_INFO))
	  unotice("Going verbose") ;
	else
	  unotice("Going silent") ;
	return ;
      case SIGPIPE :
	unotice("SIGPIPE") ;
	exit(0) ;
    }
    udebug("signal_handler: unhandled signal: %d", sig) ;
}


    static void
set_sigactions()
{
    struct sigaction sigact ;
    
    sigact.sa_handler	= signal_handler;
    sigemptyset(&sigact.sa_mask) ;
    sigact.sa_flags	= 0 ;
    
    (void) sigaction(SIGHUP,  &sigact, (struct sigaction *)NULL);
    (void) sigaction(SIGINT,  &sigact, (struct sigaction *)NULL);
    (void) sigaction(SIGTERM, &sigact, (struct sigaction *)NULL);
    (void) sigaction(SIGUSR1, &sigact, (struct sigaction *)NULL);
    (void) sigaction(SIGUSR2, &sigact, (struct sigaction *)NULL);
    (void) sigaction(SIGPIPE, &sigact, (struct sigaction *)NULL);
}


/*
 * Read McIDAS F0 header packet from standard input.
 *
 * Returns:
 *	 1	Success
 *	 0	Error
 */
    static int
get_header(data)
    char		*data;
{
    unsigned char	packet[768];
    int			status	= 0;

    if (getpkt(packet) < 0) {
	if (feof(stdin)) {
	    uerror("EOF trying to read header packet from standard input");
	} else {
	    serror("Can't read header packet from standard input");
	}
    } else if (packet[1] != HEADER_PKT) {
	uerror("Expected header packet not found: routing_code=0x%x",
	       packet[1]);
    } else {
	size_t	datalen	= packet[2]*256 + packet[3] - 5;

	assert(sizeof(packet) >= datalen+5);
	assert(0 <= datalen);

	(void) memcpy((void*)data, packet+4, datalen);
	data[datalen]	= 0;

	status	= 1;
    }

    return status;
}


    int
main(ac, av)
    int             ac;
    char          **av;
{
    int		    logmask	= (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE));
    int		    status	= 0;	/* exit status */
    int		    remove_input;	/* remove input file? */
    int		    rlen;

    char	   *logpath	= NULL;	/* log file name */
    char	   *datadir	= NULL;	/* data directory */
    char	   *input_path	= NULL;	/* input file pathname */
    char	   *mcpathbuf   = NULL;

    char		buf[8192];

    /*
     * Decode invocation line.
     */
    if (!deccmd(ac, av, &datadir, &logpath, &logmask, &input_path,
		&remove_input)) {
	status	= 1;
    } else {

	/*
	 * Initialize logger.
	 */
	(void) setulogmask(logmask);
	(void) openulog(ubasename(av[0]),
			(LOG_CONS | LOG_PID), LOG_LOCAL0, logpath);
	uinfo("Starting Up");

	/*
	 * Unset MCPATH environment variable if it exists
	 */
	mcpathbuf = getenv("MCPATH");
	if (mcpathbuf != NULL) {
	    uinfo("unsetting MCPATH environment variable");
	    (void) putenv("MCPATH=");
	}

	/*
	 * Change to data directory.
	 */
	if (datadir != NULL) {
	    uinfo("changing to directory %s", datadir);
	    if (chdir(datadir) != 0) {
		serror("can't change to directory \"%s\"", datadir);
		status	= 1;
	    }
	}

	if (status == 0) {

	    /*
	     * Register exit handler.
	     */
	    (void) atexit(cleanup);

	    /*
	     * Establish signal handlers.
	     */
	    set_sigactions();

	    /*
	     * Redirect standard input to a file if necessary.
	     */
	    if (input_path != NULL 
		    && freopen(input_path, "r", stdin) == NULL) {
		serror("Can't open input file \"%s\"", input_path);
		status	= 1;
	    } else {
		char	    header[768];	/* McIDAS header */

		/*
		 * Read F0 header packet.
		 */
		if (!get_header(header)) {
		    status	= 1;
		} else {
		    /*
		     * Perform the McIDAS function.
		     */
		    uinfo("decoding \"%s\"", header);
		    mcmain(header);

            /*
            ** Read any remaining data from the input stream
            */
		    while( (rlen = read(STDIN_FILENO, buf, 8196)) > 0 ) {
		      unotice( "finish reading %d bytes from input", rlen );
		    }

		    status	= 0;

		    (void) fclose(stdin);

		    /*
		     * Delete input file if appropriate.
		     */
		    if (remove_input && remove(input_path))
			serror("Can't remove input file \"%s\"",
			       input_path);
		}			/* read F0 header packet */
	    }				/* input file OK */
	}				/* changed to data directory */
    }					/* command line decoded */

    uinfo("Exiting") ;

    return status;
}
