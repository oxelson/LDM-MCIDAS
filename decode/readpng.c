#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include "libpng/png.h"
#include "mkdirs_open.h"
#include "ulog.h"

png_size_t pngcount=0;
FILE *fp;

png_structp png_ptr;
png_infop info_ptr,end_info;

void
read_row_callback(png_structp pngp,png_uint_32 rownum,int pass)
{
    /*printf("test callback %d %d\n",rownum,pass);*/
}

void
unidata_read_data_fn(png_p,data,length)
png_structp png_p;
png_bytep data;
png_uint_32 length;
{

    png_size_t check,tryagain,read_total;

    check = 0;
    while(check < length) {
      check += (png_size_t)read(STDIN_FILENO,data + check,length - check);
    }
    pngcount += check;

}

void
readpng_row(rowp)
char *rowp;
{
    png_read_row(png_ptr,(png_bytep)rowp, NULL);
}

void usage(progname)
char *progname;
{
    printf("%s: [options]\n",progname);
    exit(0);
}

static char *logfname = "";

int
main(int argc, char *argv)
{
    char filename[80],*dirp,stripped[PATH_MAX+1];
    int bit_depth=8;
    int color_type=PNG_COLOR_TYPE_GRAY;
    int interlace_type = PNG_INTERLACE_NONE;
    int compression_type = PNG_COMPRESSION_TYPE_DEFAULT;
    int filter_type = PNG_FILTER_TYPE_DEFAULT;
    png_uint_32 height,width;
    char *rowp,header[533];
    int i,j;
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
    int ofd,dirlen;
    long ntot=0;

    extern int optind;
    extern int opterr;
    extern char *optarg;
    int ch;

    int logmask = LOG_MASK(LOG_ERR) ;
    int logfd;

    opterr = 1;
    while ((ch = getopt(argc, argv, "nvxl:")) != EOF)
      switch (ch) {
        case 'v':
                 logmask |= LOG_MASK(LOG_INFO);
                 break;
        case 'x':
                 logmask |= LOG_MASK(LOG_DEBUG);
                 break;
        case 'n':
                 logmask |= LOG_MASK(LOG_NOTICE);
                 break;
        case 'l':
                 if (optarg[0] == '-' && optarg[1] != 0) {
                   fprintf(stderr, "logfile \"%s\" ??\n", optarg);
                   usage(argv[0]);
                 }
                 /* else */
                 logfname = optarg;
                 break;
        case '?':
                 usage(argv[0]);
                 break;
    }

    (void) setulogmask(logmask);
    if (argc - optind < 0) usage(argv[0]);


    if (logfname == NULL || !(*logfname == '-' && logfname[1] == 0))
     (void) fclose(stderr);

    logfd = openulog(ubasename(argv[0]), (LOG_CONS|LOG_PID), LOG_LDM, logfname);
    uerror("Starting Up");

    if (logfname == NULL || !(*logfname == '-' && logfname[1] == 0)) {
      setbuf(fdopen(logfd, "a"), NULL);
    }


    /*fp = fopen(argv[1],"rb");*/
    if ((argc - optind) == 0)
      ofd = fileno(stdout);

    else {
      strcpy(filename,argv[optind]);
      ofd = mkdirs_open(filename,O_WRONLY|O_CREAT,mode);

      if (ofd == -1) {                 /* make sure directories to file exist */
        if ((dirp = strrchr(filename, '/'))!= NULL) { /* directory path used */
           dirlen = dirp - filename;
           if (dirlen >= PATH_MAX) {
             uerror("name too long %s",filename);
             exit(1);
           }
           memcpy(stripped, filename, dirlen);
           stripped[dirlen] = 0;
           mkdirs(stripped, (mode | 0111));
        }

        ofd = open(filename,O_WRONLY|O_CREAT,mode);
        if (ofd == -1) {
           uerror("could not open file %s\0",filename);
           exit(2);
        }
      }
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
          /*
          (png_voidp)user_error_ptr,user_error_fn,
          user_warning_fn);
          */

    if (!png_ptr) exit(3);

    info_ptr = png_create_info_struct(png_ptr);
    end_info = png_create_info_struct(png_ptr);

    if (!info_ptr) {
      png_destroy_write_struct(&png_ptr,(png_infopp)NULL);
      exit(4);
    }

    if (setjmp(png_ptr->jmpbuf)) {
      png_destroy_read_struct(&png_ptr,&info_ptr,&end_info);
      exit(5);
    }

    png_set_read_fn(png_ptr,info_ptr,(png_rw_ptr)unidata_read_data_fn);
    /*png_init_io(png_ptr,fp);*/
    png_set_read_status_fn(png_ptr,read_row_callback);

    /* read first 533 bytes for header */
    unidata_read_data_fn(png_ptr,header,533);
    write(ofd,header,533);
    png_read_info(png_ptr,info_ptr);
    png_get_IHDR(png_ptr,info_ptr,&width,&height,&bit_depth,&color_type,
             &interlace_type,&compression_type,&filter_type);

    rowp = (char *)malloc(width);
    for(i=0;i<height;i++) {
      readpng_row(rowp);
      write(ofd,rowp,width);
      ntot += width;
    }

    free(rowp);
    close(ofd);

    unotice("wrote %ld bytes from %ld compressed bytes [%d %d]\0",ntot,pngcount,height,width);
    exit(0);
}

void
pngout_end()
{
    png_write_end(png_ptr,NULL);
    png_destroy_write_struct(&png_ptr,&info_ptr);
    fclose(fp);
}

