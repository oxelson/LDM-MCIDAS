#define UD_FORTRAN_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#ifndef PATH_MAX
#  define PATH_MAX 255
#endif
#include <string.h>
#include <time.h>
#include <assert.h>

#define SUCCESS  1
#define FAILURE  0


/*
** Externally declared variables
*/

extern char   mcpathdir[];

/*
** Function prototypes
*/

char *Mcpathname ( char * );

int   ischar     ( const void * );

void  fbyte4_    ( void *, int * );
void  fbyte2_    ( void *, int * );
void  swbyt4_    ( void *, int * );
void  swbyt2_    ( void *, int * );


/***************************** Mcpathname ************************************/

char *
Mcpathname( char *filename )
{

    /*
    ** Mcpathname is added so that the real McIDAS Mcpathname routine
    ** will not be linked into the executable
    */

    char   *pathname=NULL;

    pathname = (char *) malloc( PATH_MAX );

    if ( pathname != (char *) NULL ) {
      (void) strcpy(pathname, mcpathdir);
      if ( pathname[strlen(pathname)-1] != '/' ) strcat (pathname,"/");
      (void) strcat(pathname, filename);
    }

    return pathname;
}

/******************************** ischar *************************************/

int
ischar( const void *value )

/*
**    ischar - check to see if all characters are printable
**
** Interface:
**      int
**      ischar( const void *value )
**
** Input:
**      value   - Field containing four characters to be tested.
**
** Input and Output:
**      none
**
** Output:
**      none
**
** Return values:
**      1       - The values can be printed.
**      0       - The values can not be printed.
**
** Remarks:
**      Important use info, algorithm, etc.
**
** Categories:
**      utility
**
*/

{
    const char *val;
    int i;

    val=(const char *)value;
    for (i = 0 ; i < 4; i++) {
      if (isprint(val[i]) == 0) {
        return(0);
      }
    }

    return(1);

}

/******************************** fbyte4_ ************************************/

void
fbyte4_( void *buffer, int *num )

/*
**    fbyte4 - flip bytes in 4-byte words
**
**    input:
**          buffer  - word to flip
**             num  - number of 4-byte words to flip
**
**    return:
**             none
**
*/
{
    char *cbuf = (char *)buffer;
    char  b;
    int   i, n;

    n = *num;
    for (i = 0; i < n; i++) {
      b       = cbuf[0];
      cbuf[0] = cbuf[3];
      cbuf[3] = b;
      b       = cbuf[1];
      cbuf[1] = cbuf[2];
      cbuf[2] = b;
      cbuf   += 4;
    }
}

/******************************** fbyte2_ ************************************/

void
fbyte2_( void *buffer, int *num )

/*
**    fbyte2 - flip bytes in 2-byte words
**
**    input:
**          buffer  - word to flip
**             num  - number of 4-byte words to flip
**
**    return:
**             none
**
*/
{
    char *cbuf = (char *)buffer;
    char b;
    int i, n;

    n = *num;
    for (i = 0; i < n; i++) {
      b       = cbuf[0];
      cbuf[0] = cbuf[1];
      cbuf[1] = b;
      cbuf   += 2;
    }
}

/******************************** swbyt4_ ************************************/

void
swbyt4_( void *buf, int *n )

/*
**    swbyt4 - convert to big-endian byte order
**
**    input:
**             buf  - word(s) to flip
**               n  - number of words to flip
**
**    return:
**             none
**
*/
{
    /* determine byte order from first principles */
    union
    {
        char    bytes[sizeof(int)];
        int   	word;
    } q;

    q.word = 1;
    if (q.bytes[3] == 1)
        return;

    /* swap bytes */
    fbyte4_( buf, n );
}

/******************************** swbyt2_ ************************************/

void
swbyt2_( void *buf, int *n )
/*
**    swbyt2 - convert to big-endian byte order
**
**    input:
**             buf  - word(s) to flip
**               n  - number of words to flip
**
**    return:
**             none
**
*/
{
	/* determine byte order from first principles */
    union
    {
        char    bytes[sizeof(int)];
        int     word;
    } q;

	q.word = 1;
	if (q.bytes[1] == 1)
		return;

	/* swap bytes */
	fbyte2_(buf, n);
}

/************************** eat_steam_and_exit *******************************/

void
eat_stream_and_exit( int errcode )
/*
**    eat_stream_and_exit - read STDIN until stream is exhausted, then exit
**
**    input:
**         errcode  - word(s) to flip
**
**    return:
**             none
**
*/
{
    char     buf[8192];

    while( read(STDIN_FILENO, buf, 8192) > 0 );

    exit( errcode );
}

