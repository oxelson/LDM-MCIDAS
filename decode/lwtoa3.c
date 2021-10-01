/*
 * $Id: lwtoa3.c,v 1.7 1997/01/23 21:21:18 steve Exp $
 *
 * Perform the McIDAS LWTOA3 action on standard input.
 */

#include "udposix.h"

#include <cfortran.h>

#define LWTOA3()	CCALLSFSUB0(LWTOA3,lwtoa3)


/*
 * Perform the McIDAS action.
 */
    void
main0_()
{
    LWTOA3();
}
