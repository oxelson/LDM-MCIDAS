/*
 * $Id: csubs.c,v 1.12 1998/10/29 22:46:14 yoksas Exp $
 *
 * Low-level McIDAS-compatible support functions.
 */

#include <udposix.h>

#include <stdlib.h>
#include <unistd.h>	/* for unlink() */
#include <sys/types.h>	/* for stat() */
#include <sys/stat.h>	/* for stat() */
#include <stdio.h>
#include <assert.h>
#include <unistd.h>	/* for read() and write() */
#include <fcntl.h>	/* for open() */
#include <string.h>

#include <cfortran.h>
#include <ulog.h>

#define NFILES 8
#define HEX80 0x80808080

/*
 * FORTRAN routines used by this module:
 */
#define MAIN0()	CCALLSFSUB0(MAIN0,main0)


int		ld	= 0;
int		nlw	= 0;
int		fd[NFILES];
char	f[NFILES][256];

int		ttyfd[10];
char	*displayid;
long	*uc;
long	neguc[200];

static int	initialized;


/*
 * Initialize the user common area.
 */
    static void
init_uc()
{
    size_t	nbytes	= 65536/sizeof(long);

    assert(uc == NULL);
    if ((uc = (long*)malloc(nbytes)) == NULL) {
	  uerror("can't allocate user common area");
	  abort();
    }
    (void) memset(uc, 0, nbytes);
}


/*
 * Initialize the negative user common area.
 */
    static void
init_neguc()
{
    (void) memset(neguc, 0, sizeof(neguc));
}


/*
 * Perform the McIDAS function.  Replaces the McIDAS main() function.
 */
    void
mcmain(header)
    char	*header;
{
    int i, istat;
    char *p;

    assert(!initialized);

    init_uc();
    init_neguc();

    /*
    ** Check the "command" line (header) for DEV=; if it is not there
    ** add DEV=CCN.  Set neguc entries corresponding to 'C' accordingly.
    */

    p = strstr(header,"DEV=");

    if (p == (char *)NULL) {
      strcat(header," DEV=CCN");
    }

    p = strstr(header,"DEV=") + 4;
    for ( i=31 ; i<=33; i++) {
      if ( *p == 'C' ) neguc[i] = 1;
      p++;
    }

    istat = Mcargfree(0);
    istat = M0cmdput(M0cmdparse(header, 0));
    istat = m0cmdglo_();

    initialized	= 1;

    MAIN0();
}


#if 0
    int
/*FORTRAN*/
lwmop()
{
    return (0);
}
#endif


#if 0
    int
/*FORTRAN*/
lwc(char *cf)
{
    return (0);
}
#endif


#if 0
/*
 * Return a routing table entry.
 */
    int
/*FORTRAN*/
getrte(int *inum, int *onum, char *ctext)
{
    assert(initialized);
    *onum	= *inum;		/* set default */
    return 0;
}
#endif


    static int
c_lwopen(xfile, rdflag)
    char           *xfile;
    int             rdflag;
{
    int             id, j;
    int		    len	= strlen(xfile);

    assert(initialized);

    /* printf("\nfile: %s length: %d\n",xfile,len); */
    for (id = 0; id < nlw; id++) {
	for (j = 0; j < len; j++) {
	    if (xfile[j] != f[id][j])
		goto notf;
	}
	return (fd[id]);
      notf:;
    }
    if (ld < nlw)
	close(fd[ld]);
    for (j = 0; j <= len; j++)
	f[ld][j] = xfile[j];
    umask(0);
    if (rdflag)
	fd[ld] = open(xfile, O_RDWR, 0666);
    else
	fd[ld] = open(xfile, O_RDWR | O_CREAT, 0666);
    if (fd[ld] < 0) {
	if (!rdflag)
	    (void) serror("cannot open file \"%s\" for writing", xfile);
	return (-1);
    }
    id = ld;
    ++ld;
    if (ld > nlw)
	nlw = ld;
    if (ld > NFILES - 1)
	ld = 0;
    return (fd[id]);
}


/*
 * FORTRAN interface to the above function.
 */
FCALLSCFUN2(INT,c_lwopen,LWOPEN,lwopen,STRING,INT)


    int
c_lwfile(cfile)
    char	*cfile;
{
    int		exists;		/* file exists? */

    assert(initialized);

    exists	= cfile != NULL && c_lwopen(cfile, 1) >= 0;

    return exists;
}


/*
 * FORTRAN interface to the above function.
 */
FCALLSCFUN1(INT,c_lwfile,LWFILE,lwfile,STRING)

/*
 * Write a text string to the "screen".
 */
    void
c_spout(line)
    char        *line;
{
    assert(initialized);

    if (line != NULL) {
	unotice(line);

	/*
	 * *** KLUDGE ***
	 * Remove MD file if appropriate.  It's appropriate if the file has 
	 * been created with invalid header information (possibly from a 
	 * noisy data transmission).  Unfortunately, there's no definitive
	 * way to check for this.
	 */
	{
	    int mdno;           /* MD cylinder number */
	    int code;           /* FSHRTE() status code */

	    /*
	     * Example line: `BATCH MA 4 93014 212733 -1 "SFC.BAT'
	     */
	    if (2 == sscanf(line, "BATCH MA %d %*ld %*ld %d \"%*s", &mdno, 
			    &code) &&
		0 > code) {

		char    path[12];
		struct stat     statbuf;

		assert(0 <= mdno && 9999 >= mdno);

		(void) sprintf(path, "MDXX%04d", mdno);

		if (stat(path, &statbuf) == 0 &&/* MD file exists */
		    S_ISREG(statbuf.st_mode) && /* is regular file */
		    250000 > statbuf.st_size) { /* has at most 1 hour of data */

		    if (unlink(path) != 0) {
			serror("couldn't remove corrupted file \"%s\"", path);
		    } else {
			uerror("removed corrupted file \"%s\"", path);
		    }
		}                               /* corrupted MD file */
	    }                                   /* post-process "bad" MD file */
	}                                       /* MD removal kludge */
    }                                           /* non-NULL line to print */
}

/*
 * FORTRAN interface to the above routine.
 */
FCALLSCSUB1(c_spout,SPOUT,spout,STRING)


/*
 * SDEST
 */
    void
c_sdest(ctext,lval)
    const char	*ctext;
    const int   lval;
{
    if ( neguc[31] == 0 ) return;

    if ( lval == 0 ) {
      (void) c_spout(ctext);
    } else {
      char newtext[160];
      (void) sprintf(newtext,"%s %d",ctext,lval);
      (void) c_spout(newtext);
    }
}


/*
 * FORTRAN interface to the above routine.
 */
FCALLSCSUB2(c_sdest,SDEST,sdest,STRING,INT)

/*
 * DDEST
 */
    void
c_ddest(ctext,lval)
    const char	*ctext;
    const int   lval;
{
    if ( neguc[33] == 0 ) return;

    if ( lval == 0 ) {
      (void) c_spout(ctext);
    } else {
      char newtext[160];
      (void) sprintf(newtext,"%s %d",ctext,lval);
      (void) c_spout(newtext);
    }
}


/*
 * FORTRAN interface to the above routine.
 */
FCALLSCSUB2(c_ddest,DDEST,ddest,STRING,INT)

/*
 * EDEST
 */
    void
c_edest(ctext,lval)
    const char	*ctext;
    const int   lval;
{
    if ( neguc[32] == 0 ) return;

    if ( lval == 0 ) {
      (void) c_spout(ctext);
    } else {
      char newtext[160];
      (void) sprintf(newtext,"%s %d",ctext, lval);
      (void) c_spout(newtext);
    }
}


/*
 * FORTRAN interface to the above routine.
 */
FCALLSCSUB2(c_edest,EDEST,edest,STRING,INT)
