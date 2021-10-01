/*
 * $Id: udfifo.h,v 1.1.1.1 1995/06/15 22:32:01 steve Exp $
 */

#ifndef	UD_FIFO_H
#define	UD_FIFO_H


#include "udposix.h"
#include <stddef.h>


/*
 * FIFO access mode:
 */
#define UDFIFO_FORCE	1	/* discard oldest if necessary */


/*
 * FIFO data structure.  WARNING: Private definition: don't depend on the
 * details of this structure!
 */
struct Udfifo {
    const char	*cookie;	/* valid-structure cookie */
    size_t	eltsize;	/* size of an element */
    int		mode;		/* access mode */
    int		maxelts;	/* size of FIFO in elements */
    int		count;		/* current number of elements */
    char	*tail;		/* destination for next put() */
    char	*head;		/* source for next get() */
    char	*end;		/* end of buffer */
    char	*buf;		/* element buffer */
};
typedef struct Udfifo	Udfifo;


/*
 * Return values:
 */
#define UDFIFO_EFAILURE	-1
#define UDFIFO_ESUCCESS	 0
#define	UDFIFO_EFORCED	 1	/* success, but oldest element discarded */


/*
 * Initialize a FIFO.
 */
UD_EXTERN_FUNC(
    void udfifo_init, (		/* success or failure */
	Udfifo	*fifo,		/* the FIFO */
	size_t	eltsize,	/* size of an element in bytes */
	int	numelts		/* maximum number of elements */
    ));

/*
 * Set the mode of a FIFO.
 */
UD_EXTERN_FUNC(
    int	udfifo_setmode, (
	Udfifo	*fifo,		/* the FIFO */
	int	mode		/* UDFIFO_FORCE => discard oldest */
    ));

/*
 * Add to a FIFO.
 */
UD_EXTERN_FUNC(
    int udfifo_put, (
	Udfifo		*fifo,	/* the FIFO */
	const void	*new,	/* the element to add */
	void		*old	/* (mode == UDFIFO_FORCE && old != NULL) => 
				 * discarded oldest element */
    ));

/*
 * Remove from a FIFO.
 */
UD_EXTERN_FUNC(
    int udfifo_get, (
	Udfifo	*fifo,		/* the FIFO */
	void	*old		/* old != NULL => the oldest element */
    ));

/*
 * Return the number of elements in a FIFO.
 */
UD_EXTERN_FUNC(
    int udfifo_count, (
	const Udfifo *fifo	/* the FIFO */
    ));

/*
 * Return the number of empty spaces in a FIFO in elements.
 */
UD_EXTERN_FUNC(
    int udfifo_space, (
	const Udfifo *fifo	/* the FIFO */
    ));

/*
 * Destroy a FIFO.
 */
UD_EXTERN_FUNC(
    void udfifo_destroy, (
	Udfifo *fifo		/* the FIFO */
    ));


#endif	/* UD_FIFO_H not defined above */
