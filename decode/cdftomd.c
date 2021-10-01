#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include "netcdf.h"
#include "ulog.h"

#define HEX80                 0x80808080
#define BLANK_WORD            0x40404040
#define NUM_DATA_KEYS         40
#define NUM_STATIONS          32L    /* <<<<< UPC mod 960905 25->32 >>>>> */
#define NUM_HI_LEVEL          37
#define NUM_LO_LEVEL          37L
#define NUM_STRING_LEN        11
#define MAX_FILE_NAME         13
#define NUM_WPRO_KEYS         159
#define NUM_BPRO_KEYS         354
#define NUM_MD_ROWS           48
#define NUM_MD_COLS           35     /* <<<<< UPC mod 971221 50->35 >>>>> */

FILE *stream;

/*
** Function prototypes
*/

 int main( int, char ** );
 int make_md_files( char * , nclong , nclong , int , int );
 int make_md_file_name( nclong , char * );
 int update_row_header(int , char * , nclong * );
 int lgrab_scalar( int , char * , nclong * );
 int fgrab_scalar( int , char * , float * );
 int file_observation( char * , int , int , int , int , int , nclong * );
 int getrte( char *, int *, int *, int * );
 int sysin( int , int , int , int , int , int );
void fbyte4( void * );
void swbyt4( void * );

char data_key_name[NUM_DATA_KEYS][20] = {
     "id"                  , "region"              , "lat"                 ,
     "lon"                 , "elev"                , "azim_n"              ,
     "azim_e"              , "azim_v"              , "elev_n"              ,
     "elev_e"              , "elev_v"              , "sfc_p"               ,
     "sfc_t"               , "sfc_td"              , "sfc_spd"             ,
     "sfc_dir"             , "sfc_rain"            , "ave_power_low"       ,
     "low_u_wind"          , "low_v_wind"          , "low_w_wind"          ,
     "low_moment0_n"       , "low_moment0_e"       , "low_moment0_v"       ,
     "low_moment2_n"       , "low_moment2_e"       , "low_moment2_v"       ,
     "low_qual_indicator"  , "low_qual_summary"    , "high_u_wind"         ,
     "high_v_wind"         , "high_w_wind"         , "high_moment0_n"      ,
     "high_moment0_e"      , "high_moment0_v"      , "high_moment2_n"      ,
     "high_moment2_e"      , "high_moment2_v"      , "high_qual_indicator" ,
     "high_qual_summary"};
     
long start[]  = {0L} , count[] = {NUM_STATIONS};
long start2[] = {0L,0L};
long count2[] = {NUM_STATIONS , NUM_LO_LEVEL};

static void
usage(av0)
    const char   *av0;                  /* id string */
{
#define USAGE_FMT "\
Usage: %s [options] pcode pfile wpro <bpro>\n\
Where:\n\
    -d path      Change to directory <path> before decoding.\n\
    -l logfile   Log to <logfile> (\"-\" means standard error). Default uses\n\
                 syslogd(8).\n\
    -v           Verbose mode: log decoding diagnostics.\n\
    -x           Debug mode: log debugging diagnostics.\n\
\n\
    pcode        McIDAS product code (example: U4)\n\
    pfile        Profiler netCDF file name (example: PROFILER.CDF)\n\
    wpro         output meteorological MD file base number (example: 81)\n\
    bpro         output antenna MD file base number <optional> (example: 91)\n"
 
    (void) fprintf(stderr, USAGE_FMT, av0);
}

int main (argc,argv)
int  argc;
char *argv[];
{
    int
        i , j , k , l , ptr , id_ptr , include_bpro ,
        id_data[NUM_DATA_KEYS] , fileid , destination_row,
		rsysb, rsyse, rsysc;

    nclong
        year, month, day, hour, minute , /* low_mode , high_mode , */
        julian_day , elevation , station_count ,
        mdbase , mdf , mdf2 , /* wpro_schema , bpro_schema , */
        row_header[4] , mod_bpro, mod_wpro , /* <<<<< UPC mod 961206 >>>>> */
        wpro_ob_lo_array[NUM_WPRO_KEYS] ,
        bpro_ob_lo_array[NUM_BPRO_KEYS] ,
        wpro_ob_hi_array[NUM_WPRO_KEYS] ,
        bpro_ob_hi_array[NUM_BPRO_KEYS];

    long
        number_of_stations_sent;

    typedef union {
      char string[4];
      nclong longval;
    } U;

    U low_mode, high_mode, wpro_schema, bpro_schema;

    char
        qc[NUM_LO_LEVEL];

    nclong
        ymd_to_julian_day( nclong , nclong , nclong);


    float
        ftemp , pi , low_vgw , high_vgw , frequency ,
/*      utemp, */
        gain , enoise , cnoise , tnoise , m20low , m20high ,
        ave_power_high ,
        float_array[1024] , u[NUM_LO_LEVEL] , v[NUM_LO_LEVEL] ,
        ul[NUM_LO_LEVEL] , vl[NUM_LO_LEVEL] , w[NUM_LO_LEVEL] ,
        z_low[NUM_LO_LEVEL] , z_high[NUM_LO_LEVEL];
    
    char
        *pcode, *filename , *string , *lwfile ,
        byte_array[NUM_LO_LEVEL] , sitename[NUM_STRING_LEN] ,
        wpro_md_file_name[MAX_FILE_NAME] ,
        bpro_md_file_name[MAX_FILE_NAME];

    double
        u_2 , v_2 , wind_speed , wind_angle;

/*
** Type declarations for getopt stuff
*/

    char       *datadir;
    char       *logpath;
    int         logmask;
    char       *input_path;

    int          status = 0;
    int          ch;
    extern int   optind;
    extern int   opterr;
    extern char *optarg;

/*
** Set some values
*/

    include_bpro = -1;
    pi           = (float)3.1415297;

    (void) strncpy (low_mode.string,    "LOW ", 4);
    (void) strncpy (high_mode.string,   "HIGH", 4);
    (void) strncpy (wpro_schema.string, "WPRO", 4);
    (void) strncpy (bpro_schema.string, "BPRO", 4);

#ifdef NODEF
    low_mode     = 0x20574f4c;
    high_mode    = 0x48474948;
    wpro_schema  = 0x4f525057;
    bpro_schema  = 0x4f525042;
#endif

    logpath = NULL;
    logmask = (LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE));
    input_path = NULL;

    opterr = 1;

    /*
    ** Decode options.
    */

    while ((ch = getopt(argc, argv, "d:l:vx")) != EOF )
    {
        switch (ch)
        {
            case 'd':
                datadir = optarg;
                status = 1;
                break;
            case 'l':
                logpath = optarg;
                status = 1;
                break;
            case 'v':
                logmask |= LOG_MASK(LOG_INFO);
                status = 1;
                break;
            case 'x':
                logmask |= LOG_MASK(LOG_DEBUG);
                status = 1;
                break;
        }
    }


    if ( status == 0 && argc < 4 ) {
       usage(argv[0]);
       exit(-1);
    }

    if ( optind < argc ) {
      pcode = argv[optind++];
    } else {
      usage(argv[0]);
      exit(-1);
    }
    
    if ( optind < argc ) {
      filename = argv[optind++];
    } else {
      usage(argv[0]);
      exit(-1);
    }
   
    if ( optind < argc ) {
      lwfile = argv[optind++];
    } else {
      usage(argv[0]);
      exit(-1);
    }

/*
** Initialize logger
*/

    (void) setulogmask(logmask);
    (void) openulog(ubasename(argv[0]),
             (LOG_CONS | LOG_PID), LOG_LOCAL0, logpath);
    unotice("CDFTOMD - Begin");

/*
** Calculate MD base decade from passed parameter
*/

    mdbase = atol(lwfile);
    mdbase = mdbase / 10 * 10;

/*
** Change to data directory
*/

   if (datadir != NULL) {
     if ( chdir(datadir) != 0 ) {
        serror("can't change to directory \"%s\"", datadir);
        exit(-1);
     }
     uinfo("changing to directory %s", datadir);
   }

/*
**  open the input netCDF file
*/

   fileid = ncopen(filename,NC_NOWRITE);
   if ( fileid < 0 ) {
       serror("unable to open netCDF file:\"%s\"",filename);
       exit(-1);
   }

/*
**  get the number of stations sent in this netCDF file
*/

   id_ptr = ncdimid(fileid , "station");
   ptr = ncdiminq(fileid , id_ptr , (char *) 0, &number_of_stations_sent);

/*
**  get the observation date and time
*/

   if ( lgrab_scalar( fileid, "year"   , &year)   < 0) exit(-1);
   if ( lgrab_scalar( fileid, "month"  , &month)  < 0) exit(-1);
   if ( lgrab_scalar( fileid, "day"    , &day)    < 0) exit(-1);
   if ( lgrab_scalar( fileid, "hour"   , &hour)   < 0) exit(-1);
   if ( lgrab_scalar( fileid, "minute" , &minute) < 0) exit(-1);

   julian_day      = ymd_to_julian_day(year,month,day);
   row_header[0]   = julian_day;
   row_header[1]   = hour*10000 + minute*100;
   row_header[3]   = 0;
   destination_row = (int)(hour * 2 + 1);


   mdf = mdbase + (julian_day % 10);
   if (mdf == mdbase)mdf = mdf + 10;
   i = make_md_file_name(mdf,wpro_md_file_name);
   mdf2 = -1;

/*
**  if the BPRO md file base is specified
*/

   if ( optind < argc ) {
      lwfile = argv[optind];
      mdbase = atol(lwfile);
      mdbase = mdbase / 10 * 10;
      include_bpro = 1;
      mdf2 = mdbase + (julian_day % 10);
      if (mdf2 == mdbase)mdf2 = mdf2 + 10;
      i = make_md_file_name(mdf2,bpro_md_file_name);
   }

   uinfo("Data date/time: %5ld: %4d%02d%02d.%4d",
       julian_day, year, month, day, row_header[1]/100);
   uinfo("MD files %5ld %5ld \n", mdf , mdf2);

/*
**  do a dummy open to see if the MD file that is to be written
**  to already exists, if it doesn't create it
*/

   if ( (stream = fopen(wpro_md_file_name,"r")) == NULL){
      udebug("Making MD file %ld ... will take some time\n" , mdf );
      make_md_files(wpro_md_file_name , julian_day , wpro_schema.longval , 0 ,
                    NUM_WPRO_KEYS);
   }
   else{
      fclose(stream);
   }
   if (include_bpro > 0){
      if ( (stream = fopen(bpro_md_file_name,"r")) == NULL){
         uinfo("Making MD file %ld ... will take some time\n" , mdf2 );
         make_md_files(bpro_md_file_name , julian_day , bpro_schema.longval , 0 ,
                       NUM_BPRO_KEYS);
      }
      else{
         fclose(stream);
      }
   }

/*
**  get pertinent scalar information that will be put in the data section 
**  of each observation 
*/

   if (fgrab_scalar( fileid, "low_vgw"        , &low_vgw)        < 0) exit(-1);
   if (fgrab_scalar( fileid, "high_vgw"       , &high_vgw)       < 0) exit(-1);
   if (fgrab_scalar( fileid, "frequency"      , &frequency)      < 0) exit(-1);
   if (fgrab_scalar( fileid, "gain"           , &gain)           < 0) exit(-1);
   if (fgrab_scalar( fileid, "equiv_noise"    , &enoise)         < 0) exit(-1);
   if (fgrab_scalar( fileid, "cosmic_noise"   , &cnoise)         < 0) exit(-1);
   if (fgrab_scalar( fileid, "total_noise"    , &tnoise)         < 0) exit(-1);
   if (fgrab_scalar( fileid, "minus20_BW_low" , &m20low)         < 0) exit(-1);
   if (fgrab_scalar( fileid, "minus20_BW_high", &m20high)        < 0) exit(-1);
   if (fgrab_scalar( fileid, "ave_power_high" , &ave_power_high) < 0) exit(-1);

/*
**  load the low and high level heights to the nclong arrays;
**  low_level and high_level setting [0] to 0 meters for both
*/

   id_ptr = ncvarid (fileid , "low_level");
   count[0] = 36;
   ptr = ncvarget(fileid , id_ptr , start , count , (void *) z_low);

   id_ptr = ncvarid (fileid , "high_level");
   ptr = ncvarget(fileid , id_ptr , start , count , (void *) z_high);


/*
**  initialize the id pointers for the 40 variables in the
**  station-dependent data section
*/

   for (i=0 ; i < NUM_DATA_KEYS ; i++)
      id_data[i] = ncvarid(fileid , data_key_name[i]);


   station_count = 0;

/*
**  this loop converts all of the netCDF station-dependent
**  information into arrays for filing in the MD file
*/

     for (i=0 ; i < (int)number_of_stations_sent ; i++) {

/*
**  Initialize the output arrays to hex 80's
*/

       for (j=0 ; j < NUM_WPRO_KEYS ; j++){
           wpro_ob_lo_array[j] = HEX80;
           wpro_ob_hi_array[j] = HEX80;
       }

       if ( include_bpro > 0){
          for (j=0 ; j < NUM_BPRO_KEYS ; j++){
              bpro_ob_lo_array[j] = HEX80;
              bpro_ob_hi_array[j] = HEX80;
          }

/*
**  Re-load the station-independent data into the output arrays
*/

          bpro_ob_lo_array[7]  = (nclong) (low_vgw * 100.);
          bpro_ob_hi_array[7]  = (nclong) (high_vgw * 100.);
          bpro_ob_lo_array[8]  = (nclong) (frequency);
          bpro_ob_hi_array[8]  = (nclong) (frequency);
          bpro_ob_lo_array[9]  = (nclong) (gain * 100.);
          bpro_ob_hi_array[9]  = (nclong) (gain * 100.);
          bpro_ob_lo_array[10] = (nclong) (enoise * 100.);
          bpro_ob_hi_array[10] = (nclong) (enoise * 100.);
          bpro_ob_lo_array[11] = (nclong) (cnoise * 100.);
          bpro_ob_hi_array[11] = (nclong) (cnoise * 100.);
          bpro_ob_lo_array[12] = (nclong) (tnoise * 100.);
          bpro_ob_hi_array[12] = (nclong) (tnoise * 100.);
          bpro_ob_lo_array[13] = (nclong) (m20low * 100.);
          bpro_ob_hi_array[13] = (nclong) (m20high * 100.);
          bpro_ob_hi_array[20] = (nclong) (ave_power_high * 100.);
       }

#ifdef NODEF
/*
**  re-load the level information into the output arrays
*/

       wpro_ob_lo_array[11] = 0;
       wpro_ob_hi_array[11] = 0;
       if (include_bpro > 0){
          bpro_ob_lo_array[21] = 0;
          bpro_ob_hi_array[21] = 0;
       }

       for (l=0 , j=30 , k=15 ; l <= 35 ; l++ , j = j + 9 , k = k + 4){
          wpro_ob_lo_array[k] = (nclong) z_low[l];
          if (include_bpro > 0)bpro_ob_lo_array[j] = (nclong) z_low[l];
       }

       for (l=0 , j=30 , k = 15 ; l <= 35 ; l++ , j = j + 9 , k = k + 4){
          wpro_ob_hi_array[k] = (nclong) z_high[l];
          if (include_bpro > 0)bpro_ob_hi_array[j] = (nclong) z_high[l];
       }

#endif

/*
**  now get the station-dependent information
*/

       start[0]  = i;
       start2[0] = i;
       start2[1] = 0;
       count2[0] = 1;
       count2[1] = NUM_STRING_LEN;

/*
**  get station name
*/

       ptr = ncvarget(fileid , id_data[0] , start2 , count2 ,
             (void *) sitename);

       sitename[8]=0;

/*
**  if the station id is garbage skip ob and goto the next one
*/

       if (isupper(sitename[0])==0 || isupper(sitename[1])==0) continue;

/*       station_count++;  <<<<< UPC remove 961207 >>>>> */

/*
**  set modification flag default to be missing <<<<< UPC mod 961206 >>>>>
*/

       mod_bpro = -1;
       mod_wpro = -1;

/*
**  station 5-character ID
*/

       string = memcpy( &wpro_ob_lo_array[1] ,sitename,8);
       string = memcpy( &wpro_ob_hi_array[1] ,sitename,8);

/*
**  get station latitude
*/

       ptr = ncvarget1(fileid , id_data[2] , start , (void *) &ftemp);
       wpro_ob_lo_array[4] = ( nclong ) ( ftemp * 10000. );
       wpro_ob_hi_array[4] = ( nclong ) ( ftemp * 10000. );
       if (include_bpro > 0){
          bpro_ob_lo_array[4] = ( nclong ) ( ftemp * 10000. );
          bpro_ob_hi_array[4] = ( nclong ) ( ftemp * 10000. );
       }
/*
**  get station longitude
*/

       ptr = ncvarget1(fileid , id_data[3] , start , (void *) &ftemp);
       wpro_ob_lo_array[5] = ( nclong ) ( ftemp * 10000. );
       wpro_ob_hi_array[5] = ( nclong ) ( ftemp * 10000. );
       if (include_bpro > 0){
          bpro_ob_lo_array[5] = ( nclong ) ( ftemp * 10000. );
          bpro_ob_hi_array[5] = ( nclong ) ( ftemp * 10000. );
       }

/*
**  get station elevation 
*/

       ptr = ncvarget1(fileid , id_data[4] , start , (void *) &elevation);
       wpro_ob_lo_array[6] = elevation;
       wpro_ob_hi_array[6] = elevation;
       if (include_bpro > 0){
          bpro_ob_lo_array[6] = elevation;
          bpro_ob_hi_array[6] = elevation;
       }

/*
**  re-load the level information into the output arrays
*/

       wpro_ob_lo_array[11] = 0;
       wpro_ob_hi_array[11] = 0;
       if (include_bpro > 0){
          bpro_ob_lo_array[21] = 0;
          bpro_ob_hi_array[21] = 0;
       }

       for (l=0 , j=30 , k=15 ; l <= 35 ; l++ , j = j + 9 , k = k + 4){
          wpro_ob_lo_array[k] = (nclong) z_low[l]+elevation;
          if (include_bpro > 0)bpro_ob_lo_array[j] = (nclong) z_low[l]+elevation;
       }

       for (l=0 , j=30 , k = 15 ; l <= 35 ; l++ , j = j + 9 , k = k + 4){
          wpro_ob_hi_array[k] = (nclong) z_high[l]+elevation;
          if (include_bpro > 0)bpro_ob_hi_array[j] = (nclong) z_high[l]+elevation;
       }


       if (include_bpro > 0){

/*
**  get azimuth information 
*/

/*
**  north beam 
*/

          ptr = ncvarget1(fileid , id_data[5] , start , (void *) &ftemp);

          if (fabs(ftemp) <= 361.){
              mod_bpro = 0;
              bpro_ob_lo_array[14] = (nclong) (ftemp*10000.);
              bpro_ob_hi_array[14] = (nclong) (ftemp*10000.);
          }

/*
**  east beam 
*/

          ptr = ncvarget1(fileid , id_data[6] , start , (void *) &ftemp);

          if (fabs(ftemp) <= 361.){
              mod_bpro = 0;
              bpro_ob_lo_array[15] = (nclong) (ftemp*10000.);
              bpro_ob_hi_array[15] = (nclong) (ftemp*10000.);
          }

/*
**  vertical beam 
*/

          ptr = ncvarget1(fileid , id_data[7] , start , (void *) &ftemp);

          if (fabs(ftemp) <= 361.){
              mod_bpro = 0;
              bpro_ob_lo_array[16] = (nclong) (ftemp*10000.);
              bpro_ob_hi_array[16] = (nclong) (ftemp*10000.);
          }

/*
**  get beam elevation information 
*/

/*
**  north beam 
*/

          ptr = ncvarget1(fileid , id_data[8] , start , (void *) &ftemp);
          if (fabs(ftemp) <= 91.){
              mod_bpro = 0;
              bpro_ob_lo_array[17] = (nclong) (ftemp*10000.);
              bpro_ob_hi_array[17] = (nclong) (ftemp*10000.);
          }

/*
**  east beam 
*/

          ptr = ncvarget1(fileid , id_data[9] , start , (void *) &ftemp);

          if (fabs(ftemp) <= 91.){
              mod_bpro = 0;
              bpro_ob_lo_array[18] = (nclong) (ftemp*10000.);
              bpro_ob_hi_array[18] = (nclong) (ftemp*10000.);
          }

/*
**  vertical beam 
*/

          ptr = ncvarget1(fileid , id_data[10] , start , (void *) &ftemp);

          if (fabs(ftemp) <= 91.){
              mod_bpro = 0;
              bpro_ob_lo_array[19] = (nclong) (ftemp*10000.);
              bpro_ob_hi_array[19] = (nclong) (ftemp*10000.);
          }

/*
**  get average power transmission: low mode 
*/

          ptr = ncvarget1(fileid , id_data[17] , start , (void *) &ftemp);
          if (fabs(ftemp) <= 1000.)
              mod_bpro = 0;
              bpro_ob_lo_array[20] = (nclong) ( ftemp * 100.);
       }

/*
**  get surface pressure 
*/

       ptr = ncvarget1(fileid , id_data[11] , start , (void *) &ftemp);
       if (fabs(ftemp) <= 1500.){
           mod_wpro = 0;
           wpro_ob_lo_array[7] = (nclong) (ftemp*100.);
           wpro_ob_hi_array[7] = (nclong) (ftemp*100.);
       }

/*
**  get surface temperature 
*/

       ptr = ncvarget1(fileid , id_data[12] , start , (void *) &ftemp);
       if (fabs(ftemp) <= 101.){
           mod_wpro = 0;
           wpro_ob_lo_array[8] = (nclong) ( (273.16 + ftemp) * 100.);
           wpro_ob_hi_array[8] = (nclong) ( (273.16 + ftemp) * 100.);
       }

/*
**  get surface dewpoint 
*/

       ptr = ncvarget1(fileid , id_data[13] , start , (void *) &ftemp);
       if (fabs(ftemp) <= 101.){
           mod_wpro = 0;
           wpro_ob_lo_array[9] = (nclong) ( (273.16 + ftemp) * 100.);
           wpro_ob_hi_array[9] = (nclong) ( (273.16 + ftemp) * 100.);
       }

/*
**  get surface wind speed 
*/

       ptr = ncvarget1(fileid , id_data[14] , start , (void *) &ftemp);
       if (fabs(ftemp) <= 151.){
           mod_wpro = 0;
           wpro_ob_lo_array[12] = (nclong) ( ( ftemp + 0.05 ) * 100.);
           wpro_ob_hi_array[12] = (nclong) ( ( ftemp + 0.05 ) * 100.);
       }

/*
**  get surface wind direction 
*/

       ptr = ncvarget1(fileid , id_data[15] , start , (void *) &ftemp);
       if (fabs(ftemp) <= 361.){
           mod_wpro = 0;
           wpro_ob_lo_array[13] = (nclong) ( ftemp );
           wpro_ob_hi_array[13] = (nclong) ( ftemp );
       }

/*
**  get precipitation  
*/

       ptr = ncvarget1(fileid , id_data[16] , start , (void *) &ftemp);
       if (fabs(ftemp) <= 1001.){
           mod_wpro = 0;
           wpro_ob_lo_array[10] = (nclong) ( ftemp / 25.24 * 100.);
           wpro_ob_hi_array[10] = (nclong) ( ftemp / 25.24 * 100.);
       }

/*
**  this loop goes through twice first getting low mode info 
**  then high mode info 
*/

       for ( j=0 ; j<=1 ; j++) {
         start2[0] = i;
         start2[1] = 0;
         count2[0] = 1;
         count2[1] = NUM_LO_LEVEL - 1;

/*
**  get u component wind for a station 
*/

         ptr = 18 + 11 * j;
         l = ncvarget(fileid , id_data[ptr] , start2 , count2 ,
	   (void *) u);

/*
**  get v component wind for a station 
*/

         ptr = 19 + 11 * j;
         l = ncvarget(fileid , id_data[ptr] , start2 , count2 ,
	   (void *) v);

/*  
**  get w component wind for a station 
*/

         ptr = 20 + 11 * j;
         l = ncvarget(fileid , id_data[ptr] , start2 , count2 ,
	   (void *) w);

/*
**  get the wind quality control flag for a station
**
**  FSL Quality Control flags as of 960812
**  --------------------------------------------------
**  Passed all tests                                10
**  Missing data (no wind produced)                 25
**
**  Continuity results inconclusive:
**
**          Upass_Vinc                              19
**          Uinc_Vpass                              20
**          Uinc_Vinc                               21
**
**  Continuity results bad:
**
**          Ufail_Vpass                             22
**          Upass_Vfail                             23
**          UV_fail                                 26
**          Ufail_Vinc                              27
**          Uinc_Vfail                              28
**          failed_bird_check                       29
**  --------------------------------------------------
*/

         ptr = 27 + 11 * j;
         l = ncvarget(fileid , id_data[ptr] , start2 , count2 ,
	   (void *) qc);

/*
**  if the u and v wind are valid convert to spd and dir 
*/

         for ( k=0 ; k < (int)count2[1] ; k++){
	   wind_speed = -1.;
	   if (j == 0){
		   ul[k] = u[k];
		   vl[k] = v[k];
	   }

/*
	   if ( (fabs(u[k]) <= 150.) && (fabs(v[k]) <= 150.) ){
	   udebug("Quality control flag for level %d = %d\n" , k, qc[k] );
       }
*/
	   if ( qc[k] == 10 ){
		   u_2 = (double) ( u[k] * u[k] );
		   v_2 = (double) ( v[k] * v[k] );
		   wind_speed = sqrt( u_2 + v_2 );

		   if ( (u[k] != (float)0.) && (v[k] != (float)0.))
			   wind_angle
				   = atan2 ( (double) v[k] , (double) u[k] );

		   wind_angle = wind_angle * ( 180. / pi );

		   if ( (u[k] >= (float)0.) && (v[k] >= (float)0.))
			   wind_angle = 270. - wind_angle;

		   if ( (u[k] > (float)0.) && (v[k] <= (float)0.))
			   wind_angle = 270. - wind_angle;

		   if ( (u[k] < (float)0.) && (v[k] < (float)0.))
			   wind_angle = -1.* wind_angle -90.;

		   if ( (u[k] < (float)0.) && (v[k] > (float)0.))
			   wind_angle = 270. - wind_angle;

		   if ( (u[k] == (float)0.) && (v[k] == (float)0.))wind_angle = 0.;

	   }   /* if the wind data is valid */

/*
**  put the resultant wind speed and direction into the
**  nclong integer arrays for output 
*/

	   if (wind_speed >= 0.){
		   mod_wpro = 0;
		   ptr = 16 + k * 4;
		   if ( j == 0 ){
			  wpro_ob_lo_array[ptr] = (nclong) ( wind_speed * 100.);
			  ptr++;
			  wpro_ob_lo_array[ptr] = (nclong) ( wind_angle );
		   }
		   else{
			  wpro_ob_hi_array[ptr] = (nclong) ( wind_speed * 100.);
			  ptr++;
			  wpro_ob_hi_array[ptr] = (nclong) ( wind_angle );
		   }
	   }

	   if ( fabs(w[k]) <= 150. ){
		   mod_wpro = 0;
		   ptr = 18 + k * 4;
		   if ( j == 0 )
			  wpro_ob_lo_array[ptr] = (nclong) ( w[k] * 100. );
		   else
			  wpro_ob_hi_array[ptr] = (nclong) ( w[k] * 100. );

	   }

   } /* k loop */

   if (include_bpro > 0){

/*
**  get the 0th moment, north beam info 
*/

	  ptr = 21 + j * 11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) float_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 31 + k*9;
                  if ( fabs(float_array[k]) <= 10000.)
                  if (j == 0)
                     bpro_ob_lo_array[ptr] = (nclong) (float_array[k] * 100.);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (float_array[k] * 100.);

              }


/*
**  get the 0th moment, east beam info 
*/

              ptr = 22+j*11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) float_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 32 + k*9;
                  if ( fabs(float_array[k]) <= 10000.)
                  if (j == 0)
                     bpro_ob_lo_array[ptr] = (nclong) (float_array[k] * 100.);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (float_array[k] * 100.);
              }

/*
**  get the 0th moment, vertical beam info 
*/

              ptr = 23 + j * 11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) float_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 33 + k * 9;
                  if ( fabs(float_array[k]) <= 10000.)
                  if (j == 0)
                     bpro_ob_lo_array[ptr] = (nclong) (float_array[k] * 100.);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (float_array[k] * 100.);
              }

/*
**  get the 2th moment, north beam info 
*/

              ptr = 24 + j * 11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) float_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 34 + k * 9;
                  if ( fabs(float_array[k]) <= 10000.)
                  if (j == 0)
                     bpro_ob_lo_array[ptr] = (nclong) (float_array[k] * 100.);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (float_array[k] * 100.);
              }

/*
**  get the 2th moment, east beam info 
*/

              ptr = 25 + j * 11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) float_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 35 + k * 9;
                  if ( fabs(float_array[k]) <= 10000.)
                  if (j==0)
                     bpro_ob_lo_array[ptr] = (nclong) (float_array[k] * 100.);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (float_array[k] * 100.);

              }

/*
**  get the 2th moment, vertical beam info 
*/

              ptr = 26 + j * 11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) float_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 36 + k * 9;
                  if ( fabs(float_array[k]) <= 10000.)
                  if (j==0)
                     bpro_ob_lo_array[ptr] = (nclong) (float_array[k] * 100.);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (float_array[k] * 100.);
              }

/*
**  get the quality indicator 
*/

              ptr = 27 + j * 11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) byte_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 37 + k * 9;
                  if (j==0)
                     bpro_ob_lo_array[ptr] = (nclong) (byte_array[k]);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (byte_array[k]);

              }

/*
**  get the quality summary 
*/

              ptr = 28 + j * 11;
              l = ncvarget(fileid , id_data[ptr] , start2 , count2
                       ,(void *) byte_array);
              for ( k=0 ; k < (int)count2[1] ; k++){
                  mod_bpro = 0;
                  ptr = 38 + k * 9;
                  if (j==0)
                     bpro_ob_lo_array[ptr] = (nclong) (byte_array[k]);
                  else
                     bpro_ob_hi_array[ptr] = (nclong) (byte_array[k]);
              }
           }
       }   /* j loop */

/*
**  <<<<< UPC mod 19961207 >>>>> set modification flag based on existance
**                               of data <<<<< UPC mod 961207 >>>>>
**
**  <<<<< UPC mod 20001202 >>>>> do a simple increment to the number of
**                               stations processed since everything is
**                               written to the file at this point
*/

#if 0
       if (mod_wpro == 0) station_count++;
#endif
       station_count++;
       wpro_ob_lo_array[0] = mod_wpro;
       wpro_ob_hi_array[0] = mod_wpro;
       if (include_bpro > 0){
          bpro_ob_lo_array[0] = mod_bpro;
          bpro_ob_hi_array[0] = mod_bpro;
          string = memcpy( &bpro_ob_lo_array[1] ,sitename,8);
          string = memcpy( &bpro_ob_hi_array[1] ,sitename,8);
       }

/*
**  write the data arrays to the output MD file
*/

       l = file_observation(wpro_md_file_name , NUM_WPRO_KEYS ,
                            NUM_MD_ROWS , NUM_MD_COLS , destination_row , 
                            i+1 , wpro_ob_lo_array);
       l = file_observation(wpro_md_file_name , NUM_WPRO_KEYS ,
                            NUM_MD_ROWS , NUM_MD_COLS , destination_row + 1 ,
                            i+1 , wpro_ob_hi_array);

       if (include_bpro > 0){
          l = file_observation(bpro_md_file_name , NUM_BPRO_KEYS ,
                               NUM_MD_ROWS , NUM_MD_COLS , destination_row ,
                               i+1 , bpro_ob_lo_array);
          l = file_observation(bpro_md_file_name , NUM_BPRO_KEYS ,
                               NUM_MD_ROWS , NUM_MD_COLS , destination_row + 1 ,
                               i+1 , bpro_ob_hi_array);
       }

    } /* station loop */

   row_header[2] = low_mode.longval;
   row_header[3] = station_count;
   l = update_row_header(destination_row,wpro_md_file_name,row_header);
   if (include_bpro > 0)
	l = update_row_header(destination_row,bpro_md_file_name,row_header);

   row_header[2] = high_mode.longval;
   l = update_row_header(destination_row+1,wpro_md_file_name,row_header);
   if (include_bpro > 0)
	l = update_row_header(destination_row+1,bpro_md_file_name,row_header);

/* <<<<< UPC removed 981026 >>>>> close of stream that is already closed
   fclose(stream);
*/
   ncclose(fileid);

   l = getrte(pcode, &rsysb, &rsyse, &rsysc);

   if (l != -1 ) {
      l = sysin (rsysb, (int)mdf, rsyse, (int)row_header[1]/10000, rsysc, (int)julian_day);
   }

   unotice("CDFTOMD - End\n");
   exit(0);
}

int file_observation( char file[] , int num_data_keys , int nr , int nc ,
                      int row , int col , nclong data[])
/*
**      file_observation - files the entire data section for a given
**                         MD file , row , and column 
**          input:
**              file          - lw file name for the data being filed
**              num_data_keys - total number of data keys in MD file
**              nr            - total number of rows in MD file
**              nc            - total number of columns in MD file            
**              row           - row number to file to
**              col           - column number to file to
**              data          - nclong array containing data
*/

{
    int
       i;
    long
       file_pointer , base , ltemp;

    base = 4096 + nr * 4;
    ltemp = (long) (nc * ( row - 1 ) + ( col - 1 )) * num_data_keys;
    file_pointer = (base + ltemp) * 4;
    stream = fopen(file,"r+b");
    i = fseek(stream,file_pointer,SEEK_SET);
    fwrite((char*)data , sizeof(nclong) , num_data_keys , stream);
    fclose(stream);
    return (0);
}

int make_md_files( char file[] , nclong yyddd , nclong schema , int version ,
                   int num_keys )
/*
**      make_md_files - builds the skeletons for the 2 MD files needed
**                      for the profiler data.  It also pads the entire
**                      MD file with hex80s so subsequent writes will go
**                      faster.
**
**          input:
**              wpro    - LW file name of the MD file being created
**              yyddd   - Julian day that the data is valid for
**              schema  - Schema to create
**              version - version to create
**              num_keys- number of data keys in the MD file
**          return codes:
**              -4      - unable to locate SCHEMA LW file
**              -3      - unable to find WPRO schema in SCHEMA file
**              -2      - unable to find BPRO schema in SCHEMA file
**              -1      - unable to open destination LW file
**               1      - if successful
*/

{
   char *schema_file = "SCHEMA";
   nclong long_buffer[1024];
   int i , numread , pages;
   nclong header[4096] ;
   long pointer, beginning ;

   time_t datetime;
   struct tm *settime;

   for (i=0 ; i <= 1023 ; i++) long_buffer[i] = HEX80;

   stream = fopen(schema_file,"rb");
   if (stream == NULL){
      serror("unable to open SCHEMA file");
      return(-4);
   }

   i = 0;
   do {
      i++;
      numread = fread((char*)header , sizeof (nclong) , 64, stream);
   }
   while ( (header[0] != HEX80) && (header[0] != schema) );

/*
**  if you have made it to here either you have found the
**  schema you are looking for or you have come to the end of
**  the schema list. 
*/

   if (header[0] == HEX80){
      serror("unable to locate schema in SCHEMA file");
      return(-3);
   }

/*
**  the correct schema has been found.  position the file
**  pointer at the starting location of the md skeleton
**  in the SCHEMA lw file. 
*/

   pointer = ((i-1) * 4096 + 32768) * 4;
   i = fseek(stream,pointer,SEEK_SET);
   numread = fread((char*)header , sizeof(nclong),4096,stream);
   fclose(stream);

/*
** swap the bytes if necessary  <<<<< UPC add 970107 >>>>
**
**   Swap all words in the 64 word schema header (0 based) except:
**         0 = Schema name
**     16-23 = Text description
**        26 = Creator's initials
**     61-62 = file name
*/
   for (i=1  ; i <= 15 ; i++) (void) swbyt4(&header[i]);
   for (i=24 ; i <= 25 ; i++) (void) swbyt4(&header[i]);
   for (i=27 ; i <= 60 ; i++) (void) swbyt4(&header[i]);
   (void) swbyt4(&header[63]);
/*    swap all words in the scale factor section */
   for (i=864 ; i <= 1263 ; i++) (void) swbyt4(&header[i]);

/*
**  open your md file 
*/

   stream = fopen(file,"w+b");
   if (stream == NULL){
     serror("unable to open file \"%s\"",file);
     return(-1);
   }

/*
**  position file pointer at the beginning of the file
**  and write out the skeleton 
*/

   beginning = 0;
   i = fseek(stream,beginning,SEEK_SET);
   pages = (int) (header[30] / 1024) + 3;
   for (i = 0 ; i < pages; i++)
       fwrite((char*)long_buffer , sizeof(nclong),1024,stream);

   i = fseek(stream,beginning,SEEK_SET);
   header[15] = yyddd;

/*
** Grab the current GMT time
*/
   i = time( &datetime );
   if ( i != -1 ) {
     settime = gmtime( &datetime );
     header[25] = settime->tm_year*1000 + settime->tm_yday + 1;
   } else {
     header[25] = yyddd;
   }

/*
** <<<<< UPC add 970909 >>>>> Fill header entries 39-42 (0-based)
** with date and time represented in the file for faster ADDE searches.
*/
   if ( header[15] < 70000 )
     header[39] = header[15] + 2000000;
   else
     header[39] = header[15] + 1900000;
   header[40] = 0;
   header[41] = header[39];
   header[42] = 235959;

   fwrite((char*)header , sizeof(nclong),4096,stream);
   fclose(stream);
   return(1);

}

int make_md_file_name( nclong mdf , char *output )

/*
**  make_md_file_name - builds the LW file name for a given MD file number
**      input:
**          mdf     - md file number
**      output:
**          output  - LW file name
**      returns:
**          -1      - if invalid MD file number specified
**           1      - if successful
*/

{
    char char_array[8] , *string;
    int length;

    if ( mdf < 10000){
        length = 4;
        sprintf(char_array, "%04ld\n" , mdf);
        char_array[4] = 0;
        strcpy(output,"MDXX");
    }
    else if (mdf > 9999 && mdf < 100000){
        length = 5;
        sprintf(char_array, "%05ld\n", mdf);
        char_array[5] = 0;
        strcpy(output,"MDX");
    }
    else if (mdf > 99999 && mdf < 1000000){
        length = 6;
        sprintf(char_array, "%06ld\n", mdf);
        char_array[6] = 0;
        strcpy(output,"MD");
    }
    else{
        return(-1);
    }

    string = strncat(output,char_array,length);
    return(1);
}

nclong ymd_to_julian_day( nclong year , nclong month , nclong day)

/*
**  ymd_to_julian_day - converts year , month , day to julian day
*/

{
    static int
        first_of_month[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    nclong value , tyear , lyear;

    value = -1;
    if (day < 1 || day > 31 || month < 1 || month > 31)return(value);
    value = day + first_of_month[month-1];
    tyear = year % 100;
    lyear = year % 4;
    if ( lyear == 0 && tyear != 0 && month > 2)value++;
    value = tyear * 1000 + value;
    return(value);
}


int update_row_header( int row , char wpro[] , nclong row_header[])

{
    int
       i;
    long
       file_pointer;

    file_pointer = (4096 + (row-1)*4)*4;

    stream = fopen(wpro,"r+b");
    i = fseek(stream,file_pointer,SEEK_SET);
    fwrite((char*)row_header , sizeof(nclong) , 4 , stream);
    fclose(stream);
    return(1);
}

int fgrab_scalar( int fileid , char var_name[] , float *value )

{
   int 
      id_ptr , status;
   float
      ftemp;
   
   id_ptr = ncvarid (fileid , var_name);
   status = ncvarget1(fileid , id_ptr , start , (void *) &ftemp);
   *value = ftemp;
   if (status < 0)
      serror("error in reading \"%s\" field", var_name);

   return(status);
}

int lgrab_scalar( int fileid , char var_name[] , nclong *value)

{
   int 
      id_ptr , status;
   nclong
      ltemp;
   
   id_ptr = ncvarid (fileid , var_name);
   status = ncvarget1(fileid , id_ptr , start , (void *) &ltemp);
   *value = ltemp;
   if (status < 0)
      serror("error in reading \"%s\" field", var_name);

   return(status);
}

int getrte( char *prod, int *rsysb, int *rsyse, int *rsysc )
{
	char		pcodes[512];
	char		*p;
	FILE		*fp;
	int			i;
	nclong		entry[64];
	long		offset;

	fp = fopen("ROUTE.SYS","r");
	if (fp != NULL) {
		if (fread (pcodes, 2, 256, fp) == 256) {
			p = pcodes;
			for (i=0; i<256; i++) {
				if (strncmp(p, prod, 2) == 0) {
					offset = 4*(i*64 + 128);
					(void) fseek (fp, offset, SEEK_SET);
					(void) fread (entry, 4, 64, fp);
					(void) swbyt4(&entry[33]);
					(void) swbyt4(&entry[34]);
					(void) swbyt4(&entry[35]);
					*rsysb = (int) entry[33]; 
					*rsyse = (int) entry[34]; 
					*rsysc = (int) entry[35]; 
					return (i);
				}
				p += 2;
			}
		}
		fclose (fp);
	}
	return (-1);
}

int 
sysin( int rsysb, int sbval, int rsyse, int seval, int rsysc, int scval )
{
	FILE		*fp;
	nclong		val;
	int		bval, eval, cval;
	long 		bloc, eloc, cloc;

	fp = fopen("SYSKEY.TAB","r+");
	if (fp != NULL) {
		bloc = (long) 4*rsysb;
		eloc = (long) 4*rsyse;
		cloc = (long) 4*rsysc;

		bval = sbval;
		eval = seval;
		cval = scval;

		(void) fread(&val, 1, 4, fp);
		if (val<0L || val>999999L) {
			fbyte4(&bval);
			fbyte4(&eval);
			fbyte4(&cval);
		}

		(void) fseek (fp, bloc, SEEK_SET);
		(void) fwrite(&bval, 1, 4, fp);
		(void) fseek (fp, eloc, SEEK_SET);
		(void) fwrite(&eval, 1, 4, fp);
		(void) fseek (fp, cloc, SEEK_SET);
		(void) fwrite(&cval, 1, 4, fp);

		fclose (fp);
		return (0);
	}

	serror("SYSKEY.TAB not found");
	return (-1);
}

void fbyte4( void *buffer )
{
	char *buf;
	char a, b, c, d;

	buf = buffer;

	a = *buf++;
	b = *buf++;
	c = *buf++;
	d = *buf;

	buf = buffer;

	*buf++ = d;
	*buf++ = c;
	*buf++ = b;
	*buf   = a;
}

void
swbyt4(void *buf)
{
    /* determine byte order from first principles */
    union
    {
        char	bytes[sizeof(int)];
        int	word;
    } q;
 
    q.word = 1;
    if (q.bytes[3] == 1)
        return;
 
    /* swap bytes */
    fbyte4(buf);
}
