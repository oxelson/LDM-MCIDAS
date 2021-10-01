/*
 * $Id: alarm.c,v 1.1 1995/07/12 20:19:27 steve Exp $
 */

#include "udposix.h"
#include <stddef.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "ulog.h"
#include "alarm.h"

/*
 * Initialize an alarm.
 */
void
alarm_init(alrm, timeout, handler)
    Alarm	*alrm;
    unsigned 	timeout;
    void	(*handler)UD_PROTO((int sig));
{
    int				status;

    alrm->newact.sa_handler = handler;

#ifdef SA_INTERRUPT
    alrm->newact.sa_flags = SA_INTERRUPT;	/* for POSIX.1 semantics */
#else
    alrm->newact.sa_flags = 0;
#endif

    (void) sigemptyset(&alrm->newact.sa_mask);

    alrm->timeout = timeout;

    alrm->newact_set = 1;
}


/*
 * Enable an alarm.
 */
void
alarm_on(alrm)
    Alarm	*alrm;
{
    int				status;

    (void) alarm(0);

    status = sigaction(SIGALRM, &alrm->newact, &alrm->oldact);
    assert(status == 0);

    alrm->oldact_set = 1;

    assert(alrm->newact.sa_handler != alrm->oldact.sa_handler);

    if (SIG_DFL != alrm->oldact.sa_handler && 
	SIG_IGN != alrm->oldact.sa_handler)
    {
	udebug("alarm_on(): other SIGALRM handler installed");
    }

    (void) alarm(alrm->timeout);
}


/*
 * Disable an alarm.
 */
void
alarm_off(alrm)
    Alarm	*alrm;
{
    int		status;

    (void) alarm(0);

    assert(alrm->oldact_set);
    status = sigaction(SIGALRM, &alrm->oldact, (struct sigaction*)NULL);
    assert(status == 0);
}


/*
 * Destroy an alarm.
 */
void
alarm_destroy(alrm)
    Alarm	*alrm;
{
    (void) memset(alrm, 0, sizeof(*alrm));

    alrm->oldact.sa_handler = NULL;
    alrm->newact.sa_handler = NULL;
}

