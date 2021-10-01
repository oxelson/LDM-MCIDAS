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

#include "ulog.h"
#include "zlib.h"
#include "zutil.h"

#define SUCCESS 1
#define FAILURE 0


/******************************** IsZlibed ***********************************/

int
IsZlibed( unsigned char *buf )

/*
** Name:       IsZlibed
**
** Purpose:    Check a two-byte sequence to see if it indicates the start of
**             a zlib-compressed buffer
**
** Parameters:
**             buf     - buffer containing at least two bytes
**
** Returns:
**             SUCCESS 1
**             FAILURE 0
**
*/

{

    udebug( "in IsZlibed" );

    /*
    ** These tests were extracted from the 'inflate.c' routine that is
    ** distributed with the zlib distribution.
    */

    if ( (*buf & 0xf) == Z_DEFLATED ) {
      if ( (*buf >> 4) + 8 <= DEF_WBITS ) {
        if ( !(((*buf << 8) + *(buf+1)) % 31) ) {
          return SUCCESS;
        }
      }
    }

    return FAILURE;
}


/********************************** GetInt ***********************************/

int
GetInt( unsigned char *ptr, int num )

/*
** Name:       GetInt
**
** Purpose:    Convert GINI 2 or 3-byte quantities to int
**
** Parameters: 
**             ptr  - pointer to first header element of sequence
**             num  - number of bytes in quantity to convert
**
** Returns:
**             integer represented by byte sequence
**
** History:    ???????? - Create by SSEC for GINI to AREA file converter
**             19991022 - Adapted for GINI ADDE 'ADIR' and 'AGET' servers
**             20001011 - Changed way 'word' is calculated; incorrect
**                        values were being generated under SC5.0 x86 C
**
*/

{

    unsigned char  octet[3];

    int            base=1;
    int            i;
    int            word=0;

    /*
    ** Make a local copy of the byte sequence
    */

    (void) memcpy( octet, ptr, num );

    /*
    ** Check MSBit: if set, the number is negative
    */

    if ( *octet > 127 ) {
      *octet -= 128;
      base = -1;
    }

    /*
    ** Calculate the integer value of the byte sequence
    */

    for ( i = num-1; i >= 0; i-- ) {
      word += base * octet[i];
      base *= 256;
    }

    /*
    ** Done
    */

    return word;

}


/*
 * Get N bytes from a file descriptor.
 *
 * Returns:
 *	-1	Error
 *	 0	End-of-file
 *	>0	Number of bytes read
 */

int
getbuf( fd, buf, nelem, eltsize )
    int		fd;
    char   *buf;
    int		nelem;
    int		eltsize;
{
    int		retcode;
    int     nbyte;

    nbyte   = nelem * eltsize;
    retcode = 0;

    if ( nbyte < 0 )
	  retcode = -1;	/* error: invalid arguments */
    else
      if ( nbyte == 0 )
	    retcode = 0;	/* end-of-file */
    else
    {
	  /*
	  ** Read in nbytes from 'fd'
	  */
	  char	*p = buf;

	  do {
	    int i = read( fd, p, nbyte );

	    switch( i ) {
		  case -1:
		      retcode = -1;	/* error: I/O */
		      break;
		  case 0:
              retcode = ( (retcode > 0) ? retcode : 0 );
              nbyte   = 0;
		      break;
		  default:
		      p       += i;
		      nbyte   -= i;
              retcode += i;
	    }

	  } while ( nbyte > 0 );

#if 0
	  if ( retcode > 0 ) {

	      /*
	      ** Swap bytes on little-endian systems
	      */
	      switch ( eltsize ) {
		  case 2:
		      (void) swbyt2_( buf, &nelem );
		      break;
		  case 4:
		      (void) swbyt4_( buf, &nelem );
		      break;
	      }

	  }
#endif

    }

    return retcode;
}

