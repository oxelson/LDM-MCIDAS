#include "udposix.h"

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

    int
usleep(usec)
    unsigned		usec;
{
    struct timeval	timeout;

    timeout.tv_sec	= 0;
    timeout.tv_usec	= usec;

    return select(0, (fd_set*)0, (fd_set*)0, (fd_set*)0, &timeout);
}
