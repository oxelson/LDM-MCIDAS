/*
 * $Id: gunrv2.c,v 1.4 1997/01/22 17:16:42 steve Exp $
 *
 * Perform the McIDAS `GUNRV2' action.
 */

#include "udposix.h"

#include "cfortran.h"

#define GUNRV2()	CCALLSFSUB0(GUNRV2,gunrv2)


/*
 * Perform the McIDAS action.
 */
    void
main0_()
{
    GUNRV2();
}
