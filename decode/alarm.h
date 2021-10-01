/*
 * $Id: alarm.h,v 1.1 1995/07/12 20:19:28 steve Exp $
 */


#ifndef UD_ALARM_H
#define UD_ALARM_H


#include "udposix.h"
#include <signal.h>


/*
 * An alarm structure.
 *
 * WARNING: Don't rely on its details.
 */
typedef struct
{
    int			oldact_set;
    int			newact_set;
    unsigned		timeout;
    struct sigaction	oldact;
    struct sigaction	newact;
} Alarm;

UD_EXTERN_FUNC(void alarm_init,	(
    Alarm	*alrm,				/* alarm structure (out) */
    unsigned 	timeout,			/* timeout in seconds (in) */
    void	(*handler)(int sig)		/* SIGALRM handler (in) */
));

UD_EXTERN_FUNC(void alarm_on, (
    Alarm	*alrm				/* alarm structure (in/out) */
));

UD_EXTERN_FUNC(void alarm_off, (
    Alarm	*alrm				/* alarm structure (in) */
));

UD_EXTERN_FUNC(void alarm_destroy, (
    Alarm	*alrm				/* alarm structure (out) */
));


#endif	/* header-file lockout */
