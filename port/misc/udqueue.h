/*
 * $Id: udqueue.h,v 1.1.1.1 1995/06/15 22:32:01 steve Exp $
 */

#ifndef	UD_QUEUE_H
#define	UD_QUEUE_H


#include "udposix.h"
#include <stddef.h>
#include <uthread.h>
#include "udfifo.h"


/*
 * Access modes:
 */
#define UDQUEUE_WAIT	0	/* wait until action possible */
#define UDQUEUE_NOWAIT	1	/* error return if action not immediately */
				/* possible */
#define UDQUEUE_FORCE	2	/* discard oldest if necessary */


/*
 * Queue data structure.  WARNING: Private definition: don't depend on the
 * details of this structure!
 */
struct Udqueue {
    const char*		cookie;		/* valid-structure cookie */
    Udfifo		fifo;		/* single-threaded, non-suspending
					 * FIFO */
    uthread_mutex_t	mutex;		/* to manage FIFO access */
    uthread_cond_t	cond;		/* to manage waits */
    int			mutex_created;	/* mutex created? */
    int			cond_created;	/* condition variable created? */
    volatile int	getmode;	/* wait, nowait */
    volatile int	putmode;	/* wait, nowait, force */
    int			status;		/* status of queue */
};
typedef struct Udqueue	Udqueue;


/*
 * Return values:
 */
#define UDQUEUE_EFAILURE UDFIFO_EFAILURE	/* -1 */
#define UDQUEUE_ESUCCESS UDFIFO_ESUCCESS	/*  0 */
#define	UDQUEUE_EFORCED	 UDFIFO_EFORCED		/*  1 (success, but oldest */
						/* element discarded) */
#define	UDQUEUE_EEND	2			/* end of queue */

/*
 * Initialize a queue.
 */
UD_EXTERN_FUNC(
    void udqueue_init, (
	Udqueue	*queue,		/* the queue */
	size_t	eltsize,	/* size of an element in bytes */
	int	numelts		/* maximum number of elements */
    ));

/*
 * Set the mode putting into a queue.
 */
UD_EXTERN_FUNC(
    int udqueue_setputmode, (
	Udqueue	*queue,		/* the queue */
	int	mode		/* UDQUEUE_WAIT, UDQUEUE_NOWAIT, 
				 * UDQUEUE_FORCE */
    ));

/*
 * Set the mode for getting from a queue.
 */
UD_EXTERN_FUNC(
    int udqueue_setgetmode, (
	Udqueue	*queue,		/* the queue */
	int	mode		/* UDQUEUE_WAIT, UDQUEUE_NOWAIT */
    ));

/*
 * Add to a queue.
 */
UD_EXTERN_FUNC(
    int udqueue_put, (
	Udqueue		*queue,	/* the queue */
	const void	*new,	/* the element to add */
	void		*old	/* (putmode == UDQUEUE_FORCE && old != NULL) => 
				 * discarded, oldest element */
    ));

/*
 * Remove from a queue.
 */
UD_EXTERN_FUNC(
    int udqueue_get, (		/* UDQUEUE_SUCCESS or UDQUEUE_FAILURE */
	Udqueue	*queue,		/* the queue */
	void	*old		/* old != NULL => the oldest element */
    ));

/*
 * Return the number of elements in a queue.
 */
UD_EXTERN_FUNC(
    int udqueue_count, (
	Udqueue *queue		/* the queue */
    ));

/*
 * Return the number of empty spaces in a queue in terms of elements.
 */
UD_EXTERN_FUNC(
    int udqueue_space, (
	Udqueue *queue		/* the queue */
    ));

/*
 * Destroy a queue.
 */
UD_EXTERN_FUNC(
    void udqueue_destroy, (
	Udqueue *queue		/* the queue */
    ));


#endif	/* UD_QUEUE_H not defined above */
