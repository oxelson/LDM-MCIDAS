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
#include "png.h"

/*
** Externally declared variables
*/

extern int READ_ERROR;

/*
** Function prototypes
*/

void       unidata_read_data_fn( png_structp, png_bytep, png_uint_32 );
void       read_row_callback( png_structp, png_uint_32, int );


/*
** McIDAS AREA to PNG compressed file converter
*/

char        *prodmap;

int          prodoff;
int          png_deflen;
int          PNG_write_calls=0;

png_structp  png_ptr;
png_infop    info_ptr;

extern png_size_t   pngcount;
#if 0
extern png_structp  png_ptr;
extern png_infop    info_ptr;
#endif
extern png_infop    end_info;

int
png_get_prodlen()
{
    return( prodoff );
}

void
png_header( char *memheap, int length )
{
    (void) memcpy( prodmap+prodoff, memheap, length );

    prodoff    += length;
    png_deflen += length;
}

void
unidata_output_flush_fn( png_structp png_p )
{
    printf("flush called %d\n",png_deflen);
}

void
write_row_callback( png_structp pngp, png_uint_32 rownum, int pass )
{
    /*printf("test callback %d %d [deflen %d]\n",rownum,pass,png_deflen);*/
}

void
unidata_write_data_fn( png_structp png_p, png_bytep data, png_uint_32 length )
{
    png_header( (char *) data, length );
}

void
png_set_memheap( char *memheap )
{
    prodmap    = memheap;
    prodoff    = 0;
    png_deflen = 0;
}

void
pngout_init( int width, int height )
{
    int  bit_depth=8;
    int  color_type=PNG_COLOR_TYPE_GRAY;
    int  interlace_type = PNG_INTERLACE_NONE;
    int  compression_type = PNG_COMPRESSION_TYPE_DEFAULT;
    int  filter_type = PNG_FILTER_TYPE_DEFAULT;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
          NULL, NULL, NULL);/*
          (png_voidp)user_error_ptr,user_error_fn,
          user_warning_fn);*/

    if(!png_ptr) return;

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
      png_destroy_write_struct(&png_ptr,(png_infopp)NULL);
      return;
    }

    if(setjmp(png_ptr->jmpbuf)) {
      png_destroy_write_struct(&png_ptr,&info_ptr);
      return;
    }

    png_set_write_fn(png_ptr,info_ptr,(png_rw_ptr)unidata_write_data_fn,
                     (png_flush_ptr)unidata_output_flush_fn);
    png_set_write_status_fn(png_ptr,write_row_callback);

    /*png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);*/

    png_set_IHDR(png_ptr,info_ptr,width,height,bit_depth,color_type,
             interlace_type,compression_type,filter_type);
    png_write_info(png_ptr,info_ptr);

}

void
pngwrite( char *memheap )
{
    png_write_row(png_ptr,(png_bytep)memheap);
    PNG_write_calls++;
}

void
pngout_end()
{
    /*
    ** Try this, we get core dumps if try to shut down before any writes
    ** take place
    */

    if(PNG_write_calls > 0) png_write_end(png_ptr,NULL);

    png_destroy_write_struct(&png_ptr,&info_ptr);
    PNG_write_calls = 0;
}

void
read_row_callback( png_structp pngp, png_uint_32 rownum, int pass )
{
    /*printf("test callback %d %d\n",rownum,pass);*/
}

void
unidata_read_data_fn( png_structp png_p, png_bytep data, png_uint_32 length )
{

    png_size_t check,read_total;

    read_total = 0;
    while(read_total < length) {
       check = (png_size_t)read(STDIN_FILENO, data+read_total,length-read_total );
       if ( check <= 0 ) {
         READ_ERROR = 1;
         uerror( "Encountered read error after %d bytes\0", read_total );
         return;
        }
       read_total += check;
    }
    pngcount += read_total;

}

void
readpng_row( char *rowp )
{
    png_read_row( png_ptr, (png_bytep)rowp, NULL );
}

