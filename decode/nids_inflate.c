#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "zlib/zlib.h"

#define CHECK_ERR(err, msg) { \
  if (err != Z_OK) { \
    uerror("%s error: %d\0", msg, err); \
  } \
}

int
main(int argc, char *argv[])
{
    int           fd,ofd,bufsiz;
    char          buf[80],filnam[513];
    unsigned char b1,b2;
    Byte          uncompr[10000];
    uLong         uncomprLen = 10000;
    int           i,j,err;
    z_stream      d_stream; /* decompression stream */
    uLong         lentot,lenout;
    int           CCB=0,ccblen;

    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

    lentot = 0;
    lenout = 0;

    fd = fileno(stdin);

    if ( argc == 1 )
      ofd = fileno(stdout);
    else {
      strcpy(filnam,argv[1]);
      ofd = open(filnam,O_WRONLY|O_CREAT,mode);
    }

    /* see if we can get first 4 lines
       ^A \r \r \n
       seqno \r \r \n
       WMO \r \r \n
       PIL \r \r \n
    */

    for ( i=0; i<4; i++ ) {
      while((bufsiz = read(fd,buf,1) > 0)&&
         (buf[0] != '\n'))
         /*printf("look char %d\n",buf[0])*/;
    }


    while ( (bufsiz = read(fd,buf,1)) > 0 ) {

      d_stream.zalloc   = (alloc_func)0;
      d_stream.zfree    = (free_func)0;
      d_stream.opaque   = (voidpf)0;

      d_stream.next_in  = (Byte *)buf;
      d_stream.avail_in = 1;

      err = inflateInit( &d_stream );
      CHECK_ERR( err, "inflateInit" );

      d_stream.next_out  = uncompr;        
      d_stream.avail_out = (uInt)uncomprLen;

      for ( ;; ) {

        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;

        CHECK_ERR(err, "large inflate");

        if((bufsiz = read(fd,buf,1)) < 1) exit(0);
        d_stream.next_in  = (Byte *)buf;
        d_stream.avail_in = 1;

      }

      lentot += d_stream.total_in;
      lenout += d_stream.total_out;

      /*
      printf("Inflated1 %d/%d %d\n",d_stream.total_out,lentot,d_stream.total_in);
      */

      if ( CCB == 0 ) {    /* strip CCB, WMO and PIL from compressed product */
        b1 = uncompr[0];
        b2 = uncompr[1];
        ccblen = 2 * (((b1 & 63) << 8) + b2);

        i = ccblen;
        for ( j=0; j<2; j++ ) {
          while ( (i < d_stream.total_out)&& (uncompr[i] != '\n') ) {
             /*printf("check %d %d\n",i,uncompr[i]);*/
            i++;
          }
          i++;
        }
        write(ofd,uncompr+i,d_stream.total_out-i);
        CCB++; 
      } else
         write(ofd,uncompr,d_stream.total_out);

      err = inflateEnd(&d_stream);
      CHECK_ERR(err, "inflateEnd");
    }

    printf("Done Inflated %d/%d\n",lentot,lenout);
    close(ofd);
    close(fd);

    exit(0);
}
