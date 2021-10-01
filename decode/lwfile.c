/*
 * $Id: lwfile.c,v 1.5 1996/04/29 18:18:01 steve Exp $
 *
 * Perform the McIDAS `LWFILE' action on standard input.
 */

#include <udposix.h>

#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "cfortran.h"
#include "ulog.h"
#include "mcidas.h"

#ifndef PATH_MAX
#   define PATH_MAX	_POSIX_PATH_MAX
#endif

#include "mc_proto.h"

/*
 * McIDAS FORTRAN routines used by this module:
 */
PROTOCCALLSFFUN3(INT,GETRTE,getrte,INT,PINT,PSTRING)
#define GETRTE(in,out,str) \
    CCALLSFFUN3(GETRTE,getrte,INT,PINT,PSTRING,in,out,str)
#define FSHRTE(irte,istat) \
    CCALLSFSUB2(FSHRTE,fshrte,INT,INT,irte,istat)
PROTOCCALLSFFUN1(INT,NKWP,nkwp,STRING)
#define NKWP(str) \
    CCALLSFFUN1(NKWP,nkwp,STRING,str)


/*
 * Perform the McIDAS action.
 */
    void
main0_()
{
    const char	*outpath;		/* output file pathname */

    /*
     * Establish output file pathname.
     */
    if (Mccmdstr(NULL, 1, "LWFILE.DAT", &outpath) < 0)
	uerror(
	    "Mccmdstr() failure: Couldn't get pathname of LWFILE output file");
    else
    {
	int		    irte	= 0;
	int		    in_no	= 0;
	int		    out_no;

	if (NKWP("DIALPROD") != 0)
	    irte	= GETRTE(in_no, out_no, outpath);

	if (irte >= 0) {
	    int		status	= -2;	/* return status (total failure) */
	    FILE	*outfp;

	    /*
	     * Open the output file.
	     */
	    if ((outfp = fopen(outpath, "w")) == NULL) {
		serror("Can't open output file \"%s\"", outpath);

	    } else {
		static struct mcpacket pkt;

		status	= -1;
		if (mc_datacopy(&pkt, outfp, INT_MAX) >= 0)
		    status	= 1;

		if (fclose(outfp) == EOF) {
		    serror("Can't close output file \"%s\"", outpath);
		    status	= -1;
		}
	    }				/* output file opened */

	    if (irte > 0)
		FSHRTE(irte, status);
	}					/* GETRTE => decode product */
    }						/* outpath obtained */
}
