/*
 * $Id: lwtmd2.c,v 1.3 1996/04/29 18:18:02 steve Exp $
 *
 * Perform the McIDAS `LWTMD2' action.
 */

#include "udposix.h"

#include <cfortran.h>

/*
 * FORTRAN routines used by this module:
 */
#define MDINIT()	CCALLSFSUB0(MDINIT,mdinit)
#define LWTMD2()	CCALLSFSUB0(LWTMD2,lwtmd2)


/*
 * Perform the McIDAS action.
 */
    void
main0_()
{
    MDINIT();
    LWTMD2();
}
