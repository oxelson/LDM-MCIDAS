/*
** Lightning data structure for NLDN and USPLN data
*/

#ifndef __lightning__
#define __lightning__

#define MISS                  -2139062144
#define RMISS                 -2139062144.
#define BLANK_WORD            0x20202020
#define MAX_FILE_NAME         1024

/*
** Lightning data records
**
*/

typedef struct lgt_struct {
   int                day;			/* data day [ccyyjjj] */
   int                time;			/* time of last flash in row [HHMM00] */
   int                ncol;			/* last column with data in row */
   int                hms;			/* flash time */
   char               stime[12];	/* formatted date string */
   float              lat;			/* flash latitude (north positive) */
   float              lon;			/* flash longitude (west positive) */
   float              sgnl;			/* flash strength [Kamp] */
   int                mult;			/* flash multiplicity */
} lgt_struct;

typedef struct {
    int schema;          /* SCHEMA name */
    int schemver;        /* SCHEMA version number */
    int schemreg;        /* SCHEMA registration date */
    int nrows;           /* default number of rows */
    int ncols;           /* default number of columns */
    int nkeys;           /* total number of keys in the record */
    int nrkeys;          /* number of keys in the row header */
    int nckeys;          /* number of keys in the column header */
    int ndkeys;          /* number of keys in the data portion */
    int colhdpos;        /* 1-based position of the column header */
    int datapos;         /* 1-based position of the data portion */
    int numrep;          /* number of repeat groups */
    int repsiz;          /* size of the repeat group */
    int reploc;          /* starting position of the repeat group */
    int missing;         /* missing data code */
    int mdid;            /* integer ID of the file */
    int mdinfo[8];       /* text ID of the file */
    int projnum;         /* creator's project number */
    int createdate;      /* creation date */
    int createid;        /* creator's ID */
    int rowhdloc;        /* zero-based offset to the row header */
    int colhdloc;        /* zero-based offset to teh column header */
    int dataloc;         /* zero-based offset to the data portion */
    int unused;          /* first unused word in the file */
    int userrecloc;      /* start of the user record */
    int keynamesloc;     /* start of the key names */
    int scaleloc;        /* start of the scale factors */
    int unitsloc;        /* start of the units */
    int reserved[4];     /* reserved */
    int begday;          /* beginning Julian day of the data */
    int begtime;         /* beginning time of the data, hhmmss */
    int endday;          /* ending Julian day of the data */
    int endtime;         /* ending time of the data, hhmmss */
    int reserved2[18];   /* reserved */
    int filename[2];     /* MD file name */
    int filenum;         /* MD file number */
} MDHEADER;

#endif /* __lightning__ */
