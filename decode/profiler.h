/* profiler data structure for FSL NetCDF files */

#ifndef __profiler__
#define __profiler__


typedef struct prof_data {
   float             level;
   float             u;
   float             v;
   float             w;
   float             sigma_uv;
   float             sigma_w;
   int               levmode;
   struct prof_data *nextlev;
} prof_data;

typedef struct prof_struct {
   int                 wmoStaNum;
   char               *staName;
   float               staLat;
   float               staLon;
   float               staElev;
   double              timeObs;
   int                 year;
   int                 month;
   int                 day;
   int                 hour;
   int                 minute;
   int                 numlevs;
   int                 time_interval;
   struct prof_data   *pdata;
   struct prof_struct *next;
   float               sfc_sped;
   float               sfc_drct;
   float               sfc_temp;
   float               sfc_relh;
   float               sfc_dwpc;
   float               sfc_pres;
   float               sfc_rain_rate;
   float               sfc_rain_amt;
} prof_struct;

#endif /* __profiler__ */
