#ifndef UD_WSI_H
#define	UD_WSI_H


#include <udposix.h>
#include <stddef.h>
#include <limits.h>

#if USHRT_MAX == 0xffff
    typedef short		int2;
    typedef unsigned short	uint2;
#endif
#if UINT_MAX == 4294967295U
    typedef int			int4;
#elif ULONG_MAX == 4294967295U 
    typedef long		int4;
#else
#   include "Can't figure out type for `int4'"
#endif

#define FILL		0

typedef enum product_type
{
    Other,
    Base_Reflect,
    Velocity,
    Comp_Reflect,
    Layer_Reflect_Avg,
    Layer_Reflect_Max,
    Echo_Tops,
    Vert_Liquid,
    Precip_1,
    Precip_3,
    Precip_Accum,
    Precip_Array,
    BaseReflect248,
    StrmRelMeanVel
} product_type;


/*
 * Data product header:
 */
typedef struct Msghead
{
    int4	time;	/* Seconds after midnight GMT */
    int4	length;	/* Length of block */
    int2	code;	/* Message code */
    int2	date;	/* Date - days since 700101 */
    int2	source;
    int2	dest;
    int2	blocks;	/* Number of blocks including this one */
} Msghead;


/*
 * Description of data product:
 */
typedef struct Proddesc
{
    int4	latitude;	/* Latitude in .001 degrees */
    int4	longitude;	/* Longitude in .001 degrees */
    int4	vs_time;	/* Volume scan time in seconds */
    int4	prod_time;	/* Product generation time in seconds */
    int4	off_symbol;	/* Number of halfwords to Product Symbology */
    int4	off_graphic;	/* Number of halfwords to Graphic
				 * Alphanumeric */
    int4	off_tabular;	/* Number of halfwords to Tabular
				 * Alphanumeric */
    int2	code;		/* Product code */
    int2	divider;	/* Always -1 */
    int2	height;		/* Height of radar in feet */
    int2	mode;		/* Operational mode 0=maint, 1=clear,
				 * 2=precip/storm */
    int2	coverage;	/* RDA volume coverage pattern */
    int2	seqnum;		/* Sequence number of request */
    int2	vs_num;		/* Volume scan number (1-80) */
    int2	vs_date;	/* Volume scan date in days from 1/1/70 */
    int2	p1;		/* Product dependent value */
    int2	prod_date;	/* Product generation date in days from
				 * 1/1/70 */
    int2	p2;		/* Product dependent value */
    int2	elev_num;	/* Elevation number within volume scan */
    int2	p3;		/* Product dependent value */
    int2	threshold[16];	/* Data level thresholds */
    int2	p4;		/* Product dependent value */
    int2	p5;		/* Product dependent value */
    int2	p6;		/* Product dependent value */
    int2	p7;		/* Product dependent value */
    int2	p8;		/* Product dependent value */
    int2	p9;		/* Product dependent value */
    int2	p10;		/* Product dependent value */
    int2	num_maps;	/* Number of map pieces if map, otherwise 0 */
} Proddesc;


/*
 * Derivable, additional information on data product:
 */
typedef struct Prodinfo
{
    float	min;		/* Minimum value */
    float	max;		/* Maximum value */
    float	data_res;	/* Data resolution in km */
    float	raster_res;	/* Raster resolution in km */
    int4	type;		/* Type of product */
    int4	levels;		/* Number of data levels */
    int4	start_time;	/* Start time for Precip */
    int4	end_time;	/* End time for Precip */
    int4	size;		/* Size of raster image */
    int2	elevation;	/* Elevation in 0.1 degrees (or bottom of
				 * layer ft) */
    int2	top;		/* Top of layer in feet */
} Prodinfo;		/* Derived product information */


UD_EXTERN_FUNC(int wsiinput, (
    signed char         *image,		/* image buffer (out) */
    size_t		 size,		/* size of buffer in bytes (in) */
    char                *id		/* station ID (in/out) */
));


#endif	/* header-file lockout */
