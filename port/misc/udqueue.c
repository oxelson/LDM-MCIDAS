/*
 * $Id: udqueue.c,v 1.1.1.1 1995/06/15 22:32:01 steve Exp $
 */

/*LINTLIBRARY*/


#include "udposix.h"	/* for voidp */

#include <stddef.h>	/* for size_t */
#include <assert.h>	/* for usage, internal, and subsystem checking */
#include <stdio.h>	/* needed by some non-conforming <assert.h>s */
#include <uthread.h>	/* for UNIDATA/POSIX threads */
#include <stdlib.h>	/* for abort() */

#include "ulog.h"
#include "udfifo.h"	/* for FIFO API */
#include "udqueue.h"	/* for my public API */


/*
 * Magic cookie indicating valid data structure:
 */
static const char	cookie[]	= "udqueue";


/*
 * Destroy a queue.
 */
    void
udqueue_destroy(queue)
    Udqueue	*queue;
{
    int		status;

    assert(queue);
    assert(cookie == queue->cookie);

    status	= uthread_cond_broadcast(&queue->cond);
    assert(0 == status);
    status	= uthread_mutex_lock(&queue->mutex);
    assert(0 == status);
    (void) udfifo_destroy(&queue->fifo);

    {
	int		i;

	i	= uthread_cond_destroy(&queue->cond);
	assert(0 == i);
	i	= uthread_mutex_unlock(&queue->mutex);
	assert(0 == i);
	i	= uthread_mutex_destroy(&queue->mutex);
	assert(0 == i);
    }
}


/*
 * Initialize a new queue.
 */
    void
udqueue_init(queue, eltsize, numelts)
    Udqueue	*queue;
    size_t	eltsize;
    int		numelts;
{
    int		status;

    assert(queue);
    assert(eltsize > 0);
    assert(numelts > 0);

    /*
     * Create a FIFO.
     */
    udfifo_init(&queue->fifo, eltsize, numelts);

    queue->putmode	= UDQUEUE_WAIT;
    queue->getmode	= UDQUEUE_WAIT;

    /*
     * Create a mutex to manage asynchronous access to the FIFO.
     */
    status	= uthread_mutex_init(&queue->mutex, NULL);
    assert(0 == status);

    /*
     * Create a condition variable to wait upon.
     */
    status	= uthread_cond_init(&queue->cond, NULL);
    assert(0 == status);

    queue->cookie	= cookie;
}


/*
 * Set the mode for putting into a queue.  Idempotent.
 *
 * Returns old mode.
 */
    int
udqueue_setputmode(queue, mode)
    Udqueue	*queue;
    int		mode;
{
    int		status;
    int		oldmode;

    assert(queue);
    assert(cookie == queue->cookie);
    assert(mode == UDQUEUE_WAIT || mode == UDQUEUE_NOWAIT || mode == UDQUEUE_FORCE);

    oldmode		= queue->putmode;
    queue->putmode	= mode;
    udfifo_setmode(&queue->fifo, mode == UDQUEUE_FORCE
				    ? UDFIFO_FORCE
				    : 0);
    status	= uthread_cond_broadcast(&queue->cond);
    assert(0 == status);

    return oldmode;
}


/*
 * Set the mode for getting from a queue.  Idempotent.
 */
    int
udqueue_setgetmode(queue, mode)
    Udqueue	*queue;
    int		mode;
{
    int		status;
    int		oldmode;

    assert(queue);
    assert(cookie == queue->cookie);
    assert(mode == UDQUEUE_WAIT || mode == UDQUEUE_NOWAIT);

    oldmode		= queue->getmode;
    queue->getmode	= mode;
    status		= uthread_cond_broadcast(&queue->cond);
    assert(0 == status);

    return oldmode;
}


/*
 * Add to a queue.
 *
 * Returns:
 *	
 */
    int
udqueue_put(queue, new, old)
    Udqueue	*queue;
    const void	*new;
    void	*old;
{
    int		status;

    assert(queue);
    assert(cookie == queue->cookie);

    udebug("udqueue_put()");

    status	= uthread_mutex_lock(&queue->mutex);
    assert(status == 0);

    /*
     * If necessary, wait until there's room in the FIFO.
     */
    while (UDQUEUE_WAIT == queue->putmode && 0 == udfifo_space(&queue->fifo)) {
	udebug("udqueue_put(): waiting for room");
	(void) uthread_cond_wait(&queue->cond, &queue->mutex);
    }

    udebug("udqueue_put(): putting into fifo");
    status	= udfifo_put(&queue->fifo, new, old);

    (void) uthread_mutex_unlock(&queue->mutex);

    {
	int	i;

	udebug("udqueue_put(): signaling condition variable");
	i	= uthread_cond_signal(&queue->cond);
	assert(0 == i);
    }

    udebug("udqueue_put(): returning %d", status);

    return status;
}


/*
 * Remove the oldest element from a queue.
 *
 * Returns:
 *	UDQUEUE_EFAILURE	Error
 *	UDQUEUE_ESUCCESS	Success
 */
    int
udqueue_get(queue, old)
    Udqueue	*queue;
    void	*old;
{
    int		status;

    udebug("udqueue_get(): entered");

    assert(queue);
    assert(cookie == queue->cookie);

    status	= uthread_mutex_lock(&queue->mutex);
    assert(status == 0);

    /*
     * If necessary, wait until there's something in the FIFO.
     */
    while (UDQUEUE_WAIT == queue->getmode && 0 == udfifo_count(&queue->fifo)) {
	udebug("udqueue_get(): waiting for something");
	(void) uthread_cond_wait(&queue->cond, &queue->mutex);
    }

    udebug("udqueue_get(): getting from fifo");
    status	= udfifo_get(&queue->fifo, old);

    uthread_mutex_unlock(&queue->mutex);

    {
	int	i;

	udebug("udqueue_get(): signaling condition variable");
	i	= uthread_cond_signal(&queue->cond);
	assert(0 == i);
    }

    udebug("udqueue_get(): returning %d", status);

    return status;
}


/*
 * Return the number of elements in a queue.
 *
 * Returns:
 *	 -1	Error.
 *	>=0	Number of elements in FIFO.
 */
    int
udqueue_count(queue)
    Udqueue	*queue;
{
    int		status;

    assert(queue);
    assert(cookie == queue->cookie);
    
    return udfifo_count(&queue->fifo);
}


/*
 * Return the number of empty spaces in a queue in terms of elements.
 */
    int
udqueue_space(queue)
    Udqueue	*queue;
{
    int		status;

    assert(queue);
    assert(cookie == queue->cookie);

    status	= uthread_mutex_lock(&queue->mutex);
    assert(status == 0);

    status	= udfifo_space(&queue->fifo);
    (void) uthread_mutex_unlock(&queue->mutex);

    return status;
}
