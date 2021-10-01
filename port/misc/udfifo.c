/*
 * $Id: udfifo.c,v 1.1.1.1 1995/06/15 22:32:01 steve Exp $
 */

/*LINTLIBRARY*/


#include "udposix.h"	/* for voidp */

#include <stddef.h>	/* for size_t */
#include <assert.h>	/* for usage, internal, and subsystem checking */
#include <stdio.h>	/* needed by some non-conforming <assert.h>s */
#include <stdlib.h>	/* for malloc() & memcpy() */
#include <string.h>	/* for memcpy() */

#include "udfifo.h"	/* for my public API */
#include "ulog.h"

/*
 * Magic cookie indicating valid data structure:
 */
static const char	cookie[]	= "udfifo";


/*
 * Destroy a FIFO.
 */
    void
udfifo_destroy(fifo)
    Udfifo	*fifo;
{
    if (fifo && cookie == fifo->cookie) {
	if (fifo->buf) {
	    (void) free((voidp)fifo->buf);
	    fifo->buf	= NULL;
	}
    }
}


/*
 * Initiaize a FIFO.
 */
    void
udfifo_init(fifo, eltsize, numelts)
    Udfifo	*fifo;
    size_t	eltsize;
    int		numelts;
{
    assert(fifo);
    assert(eltsize > 0);
    assert(numelts > 0);

    fifo->buf	= (char*)malloc((size_t)(eltsize*numelts));
    assert(fifo->buf);

    fifo->eltsize	= eltsize;
    fifo->maxelts	= numelts;
    fifo->count		= 0;
    fifo->mode		= 0;
    fifo->head		= fifo->buf;
    fifo->tail		= fifo->buf;
    fifo->end		= fifo->buf + (numelts-1)*eltsize;
    fifo->cookie	= cookie;
}


/*
 * Set the mode of a FIFO.
 */
    int
udfifo_setmode(fifo, mode)
    Udfifo	*fifo;
    int		mode;
{
    int		oldmode;

    assert(fifo);
    assert(cookie == fifo->cookie);
    assert(mode == 0 || mode == UDFIFO_FORCE);

    oldmode	= fifo->mode;
    fifo->mode	= mode;

    return oldmode;
}


/*
 * Add to a FIFO.
 */
    int
udfifo_put(fifo, new, old)
    Udfifo	*fifo;
    const void	*new;
    void	*old;
{
    int		status	= 0;

    assert(fifo);
    assert(cookie == fifo->cookie);
    assert(new != NULL);

    if (fifo->count == fifo->maxelts) {
	if (UDFIFO_FORCE == fifo->mode) {
	    (void) udfifo_get(fifo, old);
	    status	= UDFIFO_EFORCED;
	} else {
	    status	= UDFIFO_EFAILURE;
	}
    }

    if (fifo->count < fifo->maxelts) {
	(void) memcpy((voidp)fifo->tail, new, fifo->eltsize);

	fifo->tail	= fifo->tail < fifo->end
			    ? fifo->tail + fifo->eltsize
			    : fifo->buf;

	++fifo->count;
    }

    return status;
}


/*
 * Remove the oldest element from a FIFO.
 */
    int
udfifo_get(fifo, old)
    Udfifo	*fifo;
    void	*old;
{
    int		status;

    assert(fifo);
    assert(cookie == fifo->cookie);

    if (0 == fifo->count) {
	status	= UDFIFO_EFAILURE;
    } else {
	assert(0 < fifo->count);

	if (NULL != old)
	    (void) memcpy(old, (voidp)fifo->head, fifo->eltsize);

	fifo->head	= fifo->head < fifo->end
			    ? fifo->head + fifo->eltsize
			    : fifo->buf;

	--fifo->count;
	status	= 0;
    }

    return status;
}


/*
 * Return the number of elements in a FIFO.
 *
 * Returns:
 *	 -1	Error.
 *	>=0	Number of elements in FIFO.
 */
    int
udfifo_count(fifo)
    const Udfifo	*fifo;
{
    int			count;

    assert(fifo);

    if (cookie == fifo->cookie) {
	assert(fifo->count >= 0);
	count	= fifo->count;
    } else {
	count	= -1;
    }

    return count;
}


/*
 * Return the number of empty spaces in a FIFO in terms of elements.
 */
    int
udfifo_space(fifo)
    const Udfifo	*fifo;
{
    int			nspace;

    assert(fifo);
    assert(cookie == fifo->cookie);
    assert(fifo->maxelts > 0);
    assert(fifo->count >= 0 && fifo->count <= fifo->maxelts);

    nspace	= fifo->maxelts - fifo->count;
    assert(nspace >= 0);

    udebug("udfifo_space(): maxelts=%d, count=%d, nspace=%d",
	   fifo->maxelts,
	   fifo->count,
	   nspace);

    return nspace;
}
