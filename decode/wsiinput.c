/*LINTLIBRARY*/

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "cfortran.h"
#include "ulog.h"
#include "alarm.h"
#include "wsi.h"

#undef	PI
#define	PI	3.14159265358979323846264338327950288419716939937510

#define TIMEOUT		(unsigned)600
#define DegToRad(theta)	((theta)*PI/180.0)

/*
 * FORTRAN COMMON blocks:
 */
#define MSGHEAD		COMMON_BLOCK(MSGHEAD, msghead)
#define PRODDESC	COMMON_BLOCK(PRODDESC, proddesc)
#define PRODINFO	COMMON_BLOCK(PRODINFO, prodinfo)

COMMON_BLOCK_DEF(Msghead, MSGHEAD);
COMMON_BLOCK_DEF(Proddesc, PRODDESC);
COMMON_BLOCK_DEF(Prodinfo, PRODINFO);


/*
 * Virtual divider, id, length, structure:
 */
typedef struct DivIdLen {
    int2	divider;	/* Always -1 */
    int2	id;
    int4	length;		/* Length of block */
} DivIdLen;

/*
 * Product symbology header:
 */
typedef struct product_symbology_header {
    int2	divider;	/* Always -1 */
    int2	id;		/* Always 1 */
    int4	length;		/* Length of block */
    int2	layers;		/* Number of layers */
} product_symbology_header;

/*
 * Graphic alphanumeric header;
 */
typedef struct graphic_alphanumeric_header {
    int2	divider;	/* Always -1 */
    int2	id;		/* Always 2 */
    int4	length;		/* Length of block */
    int2	pages;		/* Number of pages */
} graphic_alphanumeric_header;

/*
 * Nexrad Tabular Alphanumeric Block:
 */
typedef struct tabular_alphanumeric_header {
    int2	divider;	/* Always -1 */
    int2	id;		/* Always 3 */
    int4	length;		/* Length of block */
} tabular_alphanumeric_header;


/*
 * WSI Nexrad trailer:
 */
typedef struct trailer {
    char		id[3];	/* Id */
    unsigned char	status;	/* Status */
} trailer;


static void
alarm_handler(sig)
    int		sig;
{
    assert(sig == SIGALRM);
    uerror("wsiinput(): unable to read data for %u seconds: terminating", TIMEOUT);
	exit (0);
}


/*
 * Get N bytes from a file descriptor.
 *
 * Returns:
 *	-1	Error
 *	 0	End-of-file
 *	 1	Success
 */
static int
getbuf(fd, buf, nelem, eltsize)
    int		fd;
    char	*buf;
    int		nelem;
    int		eltsize;
{
    int		retcode;
    int         nbyte;

    nbyte = nelem * eltsize;

    if (nbyte < 0)
	retcode = -1;	/* error: invalid arguments */
    else
    if (nbyte == 0)
	retcode = 0;	/* end-of-file */
    else
    {
	/*
	** Read in nbytes from 'fd'
	*/
	char	*p = buf;

	retcode = 1;

	do
	{
	    int	i = read(fd, p, nbyte);

	    switch (i)
	    {
		case -1:
		    retcode = -1;	/* error: I/O */
		    break;
		case 0:
		    retcode = 0;	/* end-of-file */
		    break;
		default:
		    p += i;
		    nbyte -= i;
	    }
	}
	while (retcode == 1 && nbyte > 0);

	if (retcode == 1)
	{
	    /*
	    ** Swap bytes on little-endian systems
	    */
	    switch (eltsize) {
		case 2:
		    (void) swbyt2_(buf,&nelem);
		    break;
		case 4:
		    (void) swbyt4_(buf,&nelem);
		    break;
	    }
	}
    }

    return retcode;
}


#define SHORT(x)	memcpy(p, (char *) &x, 2); p += 2
#define INT(x)		memcpy(p, (char *) &x, 4); p += 4

/*
 * Derive some derivable metadata.
 */
static void
code_lookup(code, prod_info)
    int		code;
    Prodinfo	*prod_info;
{
    static product_type types[110] = {
	Other, Other, Other, Other, Other,	/* 0-9 */
	Other, Other, Other, Other, Other,
	Other, Other, Other, Other, Other,	/* 10-19 */
	Other, Base_Reflect, Base_Reflect,
	Base_Reflect, Base_Reflect,
	BaseReflect248, Base_Reflect, Velocity,	/* 20-29 */
	Velocity, Velocity, Velocity, Velocity,
	Velocity, Other, Other,
	Other, Other, Other, Other, Other,	/* 30-39 */
	Comp_Reflect, Comp_Reflect,
	Comp_Reflect, Comp_Reflect, Other,
	Other, Echo_Tops, Other, Other, Other,	/* 40-49 */
	Other, Other, Other, Other, Other,
	Other, Other, Other, Other, Other,	/* 50-59 */
	StrmRelMeanVel, StrmRelMeanVel, Vert_Liquid, Other, Other,
	Other, Other, Other, Layer_Reflect_Avg,	/* 60-69 */
	Layer_Reflect_Avg, Layer_Reflect_Max,
	Layer_Reflect_Max, Other, Other, Other,
	Other, Other, Other, Other, Other,	/* 70-79 */
	Other, Other, Other, Precip_1, Precip_3,
	Precip_Accum, Precip_Array, Other,	/* 80-89 */
	Other, Other, Other, Other, Other,
	Other, Layer_Reflect_Avg,
	Layer_Reflect_Max, Other, Other, Other,	/* 90-99 */
	Other, Other, Other, Other, Other, Other,
	Other, Other, Other, Other, Other,	/* 100-109 */
	Other, Other, Other, Other, Other,
    };

    static float  res[110] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0-9 */
	0, 0, 0, 0, 0, 0, 1, 2, 4, 1,	/* 10-19 */
	2, 4, 0.25, 0.5, 1, 0.25, 0.5, 1, 0, 0,	/* 20-29 */
	0, 0, 0, 0, 0, 1, 4, 1, 4, 0,	/* 30-39 */
	0, 4, 0, 0, 0, 0, 0, 0, 0, 0,	/* 40-49 */
	0, 0, 0, 0, 0, 0.5, 1, 4, 0, 0,	/* 50-59 */
	0, 0, 0, 4, 4, 4, 4, 0, 0, 0,	/* 60-69 */
	0, 0, 0, 0, 0, 0, 0, 0, 2, 2,	/* 70-79 */
	2, 0, 0, 0, 0, 0, 0, 0, 0, 4,	/* 80-89 */
	4, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 90-99 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 100-109 */
    };

    static int    levels[110] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0-9 */
	0, 0, 0, 0, 0, 0, 8, 8, 8, 16,	/* 10-19 */
	16, 16, 8, 8, 8, 16, 16, 16, 0, 0,	/* 20-29 */
	0, 0, 0, 0, 0, 8, 8, 16, 16, 0,	/* 30-39 */
	0, 16, 0, 0, 0, 0, 0, 0, 0, 0,	/* 40-49 */
	0, 0, 0, 0, 0, 16, 16, 16, 0, 0,	/* 50-59 */
	0, 0, 0, 8, 8, 8, 8, 0, 0, 0,	/* 60-69 */
	0, 0, 0, 0, 0, 0, 0, 0, 16, 16,	/* 70-79 */
	16, 0, 0, 0, 0, 0, 0, 0, 0, 8,	/* 80-89 */
	8, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 90-99 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 100-109 */
    };

    if (code < 0 || code > 109)
    {
	prod_info->type = Other;
	prod_info->data_res = 0;
	prod_info->levels = 0;
    }
    else
    {
	prod_info->type = types[code];
	prod_info->data_res = res[code];
	prod_info->levels = levels[code];
    }
    return;
}


/*
 * Read a message header from a file descriptor.
 *
 * Returns:
 *	0	Failure
 *	1	Success
 */
static int
read_msghead(unit, buf)
    int		unit;
    Msghead	*buf;
{
    return getbuf(unit, (char*)&buf->code, 1, 2) == 1 &&
	   getbuf(unit, (char*)&buf->date, 1, 2) == 1 &&
	   getbuf(unit, (char*)&buf->time, 1, 4) == 1 &&
	   getbuf(unit, (char*)&buf->length, 1, 4) == 1 &&
	   getbuf(unit, (char*)&buf->source, 3, 2) == 1;
}

/*
 * Read a message header
 *
 * Returns:
 *      0       Failure
 *      1       Success
 */
static int
read_dividlen(unit, buf)
    int		unit;
    DivIdLen	*buf;
{
    return getbuf(unit, (char*)&buf->divider, 1, 2) == 1 &&
	   getbuf(unit, (char*)&buf->id, 1, 2) == 1 &&
	   getbuf(unit, (char*)&buf->length, 1, 4) == 1;
}


/*
 * Read a product description from a file descriptor.
 *
 * Returns:
 *	0	Failure
 *	1	Success
 */
static int
read_proddesc(unit, buf)
    int		unit;
    Proddesc	*buf;
{
    return getbuf(unit, (char*)&buf->divider, 1, 2) == 1     &&
	   getbuf(unit, (char*)&buf->latitude, 1, 4) == 1    &&
	   getbuf(unit, (char*)&buf->longitude, 1, 4) == 1   &&
	   getbuf(unit, (char*)&buf->height, 1, 2) == 1      &&
	   getbuf(unit, (char*)&buf->code, 1, 2) == 1        &&
	   getbuf(unit, (char*)&buf->mode, 1, 2) == 1        &&
	   getbuf(unit, (char*)&buf->coverage, 1, 2) == 1    &&
	   getbuf(unit, (char*)&buf->seqnum, 1, 2) == 1      &&
	   getbuf(unit, (char*)&buf->vs_num, 1, 2) == 1      &&
	   getbuf(unit, (char*)&buf->vs_date, 1, 2) == 1     &&
	   getbuf(unit, (char*)&buf->vs_time, 1, 4) == 1     &&
	   getbuf(unit, (char*)&buf->prod_date, 1, 2) == 1   &&
	   getbuf(unit, (char*)&buf->prod_time, 1, 4) == 1   &&
	   getbuf(unit, (char*)&buf->p1, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->p2, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->elev_num, 1, 2) == 1    &&
	   getbuf(unit, (char*)&buf->p3, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->threshold, 16, 2) == 1  &&
	   getbuf(unit, (char*)&buf->p4, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->p5, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->p6, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->p7, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->p8, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->p9, 1, 2) == 1          &&
	   getbuf(unit, (char*)&buf->p10, 1, 2) == 1         &&
	   getbuf(unit, (char*)&buf->num_maps, 1, 2) == 1    &&
	   getbuf(unit, (char*)&buf->off_symbol, 1, 4) == 1  &&
	   getbuf(unit, (char*)&buf->off_graphic, 1, 4) == 1 &&
	   getbuf(unit, (char*)&buf->off_tabular, 1, 4) == 1;
}


/*
 * Convert a WSI raster product into a raster image.
 *
 * Returns:
 *	 0	Success
 *	-1	Error
 *	-2	Insufficient room in passed-in array
 */
static int
raster_image(input, raster, size, prod_info, rcode)
    int			input;
    unsigned char	*raster;
    size_t		size;
    Prodinfo		*prod_info;
    uint2		rcode;
{
    /*
     * Packet header for raster data:
     */
    typedef struct packet_header {
	uint2                code[3];	/* 0xba0f/7 8000 00c0 */
	int2		     i;		/* i coordinate start (km/4) */
	int2                 j;		/* j coordinate start (km/4)  */
	int2                 xscale;	/* x scaling factor */
	int2                 xscalefract;	/* reserved */
	int2                 yscale;	/* x scaling factor */
	int2                 yscalefract;	/* reserved */
	int2                 num_rows;	/* number of rows */
	int2                 packing;	/* packing format (always 2) */
    } packet_header;

    packet_header        p_head;	/* Packet header */
    unsigned char       *data = NULL, *p;
    int                  run;
    int                  i, nrow, ncol, num_cols, top, bottom;
    short                length;
    float                scale;

    scale = prod_info->data_res / prod_info->raster_res;
/* <<<<< UPC removed 961007; replaced with code immediately following >>>>>
    if (getbuf(input, (char*)&p_head, 11, 2) != 1)
	return -1;
*/
    p_head.code[0] = rcode;
    if (getbuf(input, (char*)&p_head.code[1], 2, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.i, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.j, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.xscale, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.xscalefract, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.yscale, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.yscalefract, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.num_rows, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.packing, 1, 2) != 1)
	return -1;

    if ((p_head.code[0] & 0xfff7) != 0xba07 ||
	p_head.code[1] != 0x8000 || p_head.code[2] != 0x00c0)
    {
	unotice("incorrect packet code for raster image");
	return (-1);
    }
    prod_info->size = p_head.num_rows * scale;
    for (nrow = 0; nrow < p_head.num_rows; ++nrow)
    {

	if (getbuf(input, (char*)&length, 1, 2) != 1)
	    return -1;
	if (data == NULL)
	    data = (unsigned char *) malloc(length);
	else
	    data = (unsigned char *) realloc(data, length);
	if (getbuf(input, (char*)data, length, 1) != 1)
	    return -1;
	if (nrow == 0)
	{
	    num_cols = 0;
	    for (run = 0; run < length && data[run]; ++run)
	    {
		unsigned int         drun = data[run] >> 4;
		num_cols += drun;
	    }
	    uinfo("  first %d/%d, scale %d/%d, nrows %d, ncols %d",
		  p_head.i, p_head.j, p_head.xscale, p_head.yscale,
		  p_head.num_rows, num_cols);
	    if (p_head.num_rows != num_cols)
		uerror(" Rows != Columns: %d/%d",
		       p_head.num_rows, num_cols);
	    prod_info->size = p_head.num_rows * scale;
	    if (prod_info->size * prod_info->size > size)
	    {
		uerror("Raster array isn't large enough: have=%lu, need=%lu",
		       (unsigned long) size, 
		       (unsigned long) (prod_info->size * prod_info->size));
		return (-2);
	    }
	}
	bottom = scale * (p_head.num_rows - nrow - 1);
	top = scale * (p_head.num_rows - nrow);
	p = raster + prod_info->size * bottom;
	ncol = 0;
	uinfo("   Row %d, length %d",
	       nrow, length);
	for (run = 0; run < length && data[run]; ++run)
	{
	    unsigned int         drun = data[run] >> 4;
	    unsigned int         dcode = data[run] & 0xf;
	    int                  iend = (int) (scale * (ncol + drun)) -
					(int) (scale * ncol);

	    ncol += drun;
	    for (i = 0; i < iend; ++i)
	    {
		*p++ = dcode;
	    }

	}
	if (p - raster != prod_info->size * (bottom + 1)
	    || ncol != num_cols)
	{
	    unotice("***Bad size raster %d, run %d",
		    nrow, ncol);
	    return (-1);
	}
	for (i = bottom + 1; i < top; ++i)
	    memcpy(raster + prod_info->size * i,
		   raster + prod_info->size * (i - 1), prod_info->size);
    }

    return (0);
}


/*
 * This routine adds a radial to the raster array.  The origin of the raster
 * array is in the bottom left.  The technique depends
 * on which quadrant of the circle the radial is in.  If it is in the top
 * (315-45) or bottom (135-225) quadrant, the rasterization is done by rows
 * from the bottom to the top, and from left to right within the rows.
 * Thus the rasterization start from the beginning of the radial in the top
 * quadrant and from the end of the radial in the bottom quadrant.
 * If it is in the right of left quadrant, the rasterization is done by
 * columns from left to right, and from bottom to top witin the rows.
 */
static void
rasterize(raster, radial, prod_info, num_radial, start, delta)
    unsigned char	*raster;	/* Pointer to square raster array */
    unsigned char	*radial;	/* Pointer to array with radial data */
    Prodinfo		*prod_info;	/* Misc product info */
    int			num_radial;	/* Number of point in radial array */
    double		start;		/* Start angle for this radial */
    double		delta;		/* Angle delta for this radial */
{
    float                az,		/* Angle of mid-point of radial */
                         az1,		/* Angle of left/bottom of radial */
                         az2,		/* Angle of right/top of radial */
                         pgs;		/* raster points per radial point */
    double               sin1, cos1,	/* sine and cosine of az1 */
                         sin2, cos2,	/* sine and cosine of az2 */
                         inc1,		/* Tangent/cotangent of az1 */
                         inc2,		/* Tangent/cotangent of az2 */
                         rowinc,	/* Change in radial index per row */
                         colinc,	/* Change in radial index per column */
                         x0,		/* Raster x value of end of radial */
                         y0,		/* Raster y value of end of radial */
                         gate,		/* Radial index of start of
					 * row/column */
                         gate_col,	/* Radial index of current column */
                         gate_row,	/* Radial index of current row */
                         left, right,	/* Raster index limits for column */
                         bottom, top;	/* Raster index limits for row */
    unsigned char       *dp;		/* Pointer into raster array */
    int                  col, row,	/* Current column/row */
                         vertical,	/* Is this a vertical radial
					 * (315-45,135-225) */
                         flip = 0;	/* Was az1 and za2 flipped? */

    /*
     * Set up radial angles az, az1 and az2.
     */

    az1 = start;
    if ((az = start + 0.5 * delta) >= 360.0)
	az -= 360;
    if ((az2 = start + delta) >= 360.0)
	az2 -= 360;
    /*
     * Figure out the orientation of the radial.  Then, if need be, swap the
     * two angles to correspond to the direction in which we will be
     * rasterizing (i.e. az1 is always left of az2 in a vertical radial, and
     * az1 is always below az2 in a horizontal radial).
     */
    vertical = (az <= 45.0 || (az >= 135.0 && az <= 225.0)
		|| az >= 315.0);
    if ((vertical && az > 90.0 && az < 270.0) ||
	(!vertical && az < 180.0))
    {
	float                tmp = az1;
	az1 = az2;
	az2 = tmp;
	++flip;
    }
    sin1 = sin(DegToRad(az1));
    cos1 = cos(DegToRad(az1));
    sin2 = sin(DegToRad(az2));
    cos2 = cos(DegToRad(az2));
    pgs = prod_info->raster_res / prod_info->data_res;
    /*
     * For vertical radials (315-45, 135-225).
     */
    if (vertical)
    {
	inc1 = sin1 / cos1;
	inc2 = sin2 / cos2;
	rowinc = (sin1 < sin2) ? pgs / cos1 : pgs / cos2;
	colinc = pgs * sin(DegToRad(az));
	/*
	 * If in right side of circle, use az2 (right) side of beam in
	 * calculation of y0 (and vice-versa).
	 */
	if (az < 180.)
	    y0 = prod_info->size * 0.5 + prod_info->size * 0.5 * cos2;
	else
	    y0 = prod_info->size * 0.5 + prod_info->size * 0.5 * cos1;
	/*
	 * flip => bottom quadrant of circle, start rasterization from end of
	 * radial (edge).
	 */
	if (flip)
	{
	    gate = num_radial - .00001;
	    left = prod_info->size * 0.5 + (y0 - prod_info->size * 0.5) * inc1;
	    right = prod_info->size * 0.5 + (y0 - prod_info->size * 0.5) * inc2;
	    bottom = y0;
	    top = prod_info->size * 0.5;
	}
	/*
	 * else start rasterization from beginning of radial (center).
	 */
	else
	{
	    left = prod_info->size * 0.5;
	    right = prod_info->size * 0.5;
	    bottom = prod_info->size * 0.5;
	    top = y0;
	    gate = 0;
	}
	/*
	 * Correct the starting index of the radial for the right side of the
	 * circle since we will be rasterizing towards the edge.
	 */
	if (az < 180.)
	    gate -= colinc * (right - left + 1);
	/*
	 * Set up pointer for raster array.
	 */
	dp = raster + ((int) bottom) * prod_info->size;
	/*
	 * Outer loop is by row.
	 */
	if ((int) bottom < 0 || (int) top >= prod_info->size)
	    udebug("  vTB %6.1f %6.3f %6.3f", az, bottom, top);
	for (row = (int) bottom; row < (int) top; row++)
	{
	    gate_col = gate;
	    /*
	     * Inner loop is by column.
	     */
	    if ((int) left < 0 || (int) right >= prod_info->size)
		udebug("  vLR %6.1f %6.3f %6.3f", az, left, right);
	    for (col = (int) left; col <= (int) right; col++)
	    {
		if ((int) gate_col < 0 || (int) gate_col >= num_radial)
		    udebug("    vBAD %6.1f %d %d %6.3f %6.3f",
			    az, row, col, gate, gate_col);
		dp[col] = radial[(int) gate_col];
		gate_col += colinc;
	    }
	    /*
	     * Set up for next row.
	     */
	    gate += rowinc;
	    left += inc1;
	    right += inc2;
	    dp += prod_info->size;
	}
    }
    /*
     * For horizontal radials (45-135, 225-315).
     */
    else
    {
	inc1 = cos1 / sin1;
	inc2 = cos2 / sin2;
	rowinc = pgs * cos(DegToRad(az));
	colinc = (cos1 < cos2) ? pgs / sin1 : pgs / sin2;
	/*
	 * If in bottom of circle, use az1 (bottom) side of beam in
	 * calculation of x0 (and vice-versa).
	 */
	if (az > 90. && az < 270.)
	    x0 = prod_info->size * 0.5 + prod_info->size * 0.5 * sin1;
	else
	    x0 = prod_info->size * 0.5 + prod_info->size * 0.5 * sin2;
	/*
	 * not flip => left quadrant of circle, start rasterization from end
	 * of radial (edge).
	 */
	if (!flip)
	{
	    gate = num_radial - .00001;
	    bottom = prod_info->size * 0.5 + (x0 - prod_info->size * 0.5) *
					     inc1;
	    top = prod_info->size * 0.5 + (x0 - prod_info->size * 0.5) * inc2;
	    left = x0;
	    right = prod_info->size * 0.5;
	}
	/*
	 * else start rasterization from beginning of radial (center).
	 */
	else
	{
	    bottom = prod_info->size * 0.5;
	    top = prod_info->size * 0.5;
	    left = prod_info->size * 0.5;
	    right = x0;
	    gate = 0;
	}
	/*
	 * Correct the starting index of the radial for the top of the circle
	 * since we will be rasterizing towards the edge.
	 */
	if (az < 90. || az > 270.)
	    gate -= rowinc * (top - bottom + 1);
	/*
	 * Outer loop is by column.
	 */
	if ((int) left < 0 || (int) right >= prod_info->size)
	    udebug("  hLR %6.1f %6.3f %6.3f", az, left, right);
	for (col = (int) left; col < (int) right; col++)
	{
	    dp = raster + ((int) bottom) * prod_info->size + col;
	    gate_row = gate;
	    /*
	     * Inner loop is by row.
	     */
	    if ((int) bottom < 0 || (int) top >= prod_info->size)
		udebug("  hTB %6.1f %6.3f %6.3f", az, bottom, top);
	    for (row = (int) bottom; row <= (int) top; row++)
	    {
		if ((int) gate_row < 0 || (int) gate_row >= num_radial)
		    udebug("    hBAD %6.1f %d %d %6.3f %6.3f",
			    az, row, col, gate, gate_row);
		*dp = radial[(int) gate_row];
		gate_row += rowinc;
		dp += prod_info->size;
	    }
	    /*
	     * Set up for next column.
	     */
	    gate += colinc;
	    bottom += inc1;
	    top += inc2;
	}
    }
}


/*
 * Convert a WSI radial product into a raster image.
 *
 * Returns:
 *	 0	Success
 *	-1	Error
 *	-2	Insufficient room in passed-in array
 */
static int
radial_image(input, raster, size, prod_info, rcode)
    int			input;
    unsigned char	*raster;
    size_t		size;
    Prodinfo		*prod_info;
    uint2		rcode;
{
    /*
     * Packet header for radial data:
     */
    typedef struct packet_header {
	uint2	code;		/* 0xaf1f */
	int2	first_bin;	/* Index of first range bin */
	int2	num_bin;	/* Number of range bins */
	int2	i;		/* i center of sweep (km/4) */
	int2	j;		/* j center of sweep (km/4)  */
	int2	scale;		/* scale factor(230/# of bins)in .001 pixels */
	int2	num_radials;	/* number of radials */
    } packet_header;

    /*
     * Radial data header:
     */
    typedef struct radial_header {
	int2	length;		/* Number of RLE halfwords in radial */
	int2	start;		/* Starting angle (.1) */
	int2	delta;		/* Angle delta (.1) */
    } radial_header;

    int                  radial;
    int                  run;
    int                  i;
    int                  nbeam;
    packet_header        p_head;	/* Packet header */
    radial_header        r_head;	/* Radial header */
    static unsigned char *data = NULL;
    static unsigned char *beam = NULL;

/* <<<<< UPC removed 961007; replaced with code immediately following >>>>>
    if (getbuf(input, (char*)&p_head, 7, 2) != 1)
	return -1;
*/
    p_head.code = rcode;
    if (getbuf(input, (char*)&p_head.first_bin, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.num_bin, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.i, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.j, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.scale, 1, 2) != 1)
	return -1;
    if (getbuf(input, (char*)&p_head.num_radials, 1, 2) != 1)
	return -1;

    if (p_head.code != 0xaf1f)
    {
	unotice("incorrect packet code for radial image");
	return (-1);
    }
    udebug("  1st %d, nbins %d, i %d, j %d, scale %d, nrad %d",
	  p_head.first_bin, p_head.num_bin, p_head.i, p_head.j, p_head.scale,
	  p_head.num_radials);
    if (beam == NULL)
	beam = (unsigned char *) malloc(p_head.num_bin + 2);
    else
	beam = (unsigned char *) realloc(beam, p_head.num_bin + 2);

    /*
     * For rasterization overflows.
     */
    beam[p_head.num_bin] = FILL;
    beam[p_head.num_bin + 1] = FILL;
    prod_info->size = ((int) (p_head.num_bin * prod_info->data_res /
			      prod_info->raster_res + 0.5)) * 2;
    if (prod_info->size * prod_info->size > size)
    {
	uerror("Raster array isn't large enough: have=%lu, need=%lu",
	       (unsigned long) size, 
	       (unsigned long) (prod_info->size * prod_info->size));
	return (-2);
    }
    else
    {
	memset(raster, FILL, prod_info->size * prod_info->size);
	for (radial = 0; radial < p_head.num_radials; ++radial)
	{
	    if (getbuf(input, (char*)&r_head, 3, 2) != 1)
		return -1;
	    if (data == NULL)
		data = (unsigned char *) malloc(2 * r_head.length);
	    else
		data = (unsigned char *) realloc(data, 2 * r_head.length);
	    if (getbuf(input, (char*)data, 2 * r_head.length, 1) != 1)
		return -1;
	    udebug("   Radial %d, length %d, start %d, delta %d",
		   radial, r_head.length, r_head.start,
		   r_head.delta);
	    nbeam = 0;
	    for (run = 0; run < 2 * r_head.length && data[run]; ++run)
	    {
		unsigned int         drun = data[run] >> 4;
		unsigned int         dcode = data[run] & 0xf;

		for (i = 0; i < drun; ++i)
		{
		    beam[nbeam++] = dcode;
		}
	    }
	    if (nbeam != p_head.num_bin)
	    {
		unotice("***Bad size radial %d, run %d, p_head.num_bin %d",
			radial, nbeam, p_head.num_bin);
		return (-1);
	    }
	    rasterize(raster, beam, prod_info, p_head.num_bin,
		      r_head.start / 10., r_head.delta / 10.);
	}
    }
    return (0);
}


/*
 * Get the next WSI data product from standard input.
 *
 * Returns:
 *	 0	Success
 *	-1	Error
 *	-2	Insufficient room in passed-in array.
 *	-3	Mismatched station ID's
 */
static int
wsi_read(input, header, product, prod_info, raster, size, station_id)
    int			 input;		/* input FD (in) */
    Msghead		*header;	/* message header (out) */
    Proddesc		*product;	/* product description (out) */
    Prodinfo		*prod_info;	/* derived product information (out) */
    unsigned char       *raster;	/* image buffer (out) */
    size_t		 size;		/* size of image buffer in bytes (in) */
    char          	 station_id[4];	/* station ID (in/out) */
{
    int		n;
    int		radial = 0;
    uint2	rcode = 0;
    int		nbytes;
    trailer	trail;
    char       *data = NULL;

    if (!read_msghead(input, header))
    {
	uerror("Couldn't read %lu-byte product header",
	       (unsigned long)sizeof(*header));
	return -1;
    }

    nbytes = header->length;

    uinfo("code %d, date %d, time %d, length %d, blocks %d",
	  header->code, header->date, header->time, header->length,
	  header->blocks);

    if (!read_proddesc(input, product))
    {
	uerror("product descriptor read error; aborting");
	return -1;
    }

    if (product->divider != -1)
    {
	uerror("product->divider != -1");
	return -1;
    }
    code_lookup(product->code, prod_info);

    uinfo("Product code %d, mode %d, coverage %d, seqnum %d, vs %d\n, "
	  "elev_num %d, elevation %d, raster_res %d",
	  product->code, product->mode, product->coverage,
	  product->seqnum, product->vs_num, product->elev_num,
	  prod_info->elevation, prod_info->raster_res);

    prod_info->max = product->p4;
    if (prod_info->type == Base_Reflect)
    {
	radial = 1;
	prod_info->elevation = product->p3;
    }
    else if (prod_info->type == BaseReflect248)
    {
	radial = 1;
	prod_info->elevation = product->p3;
    }
    else if (prod_info->type == Velocity)
    {
	radial = 1;
	prod_info->elevation = product->p3;
	prod_info->min = product->p4;
	prod_info->max = product->p5;
    }
    else if (prod_info->type == Comp_Reflect)
    {
	radial = 0;
	prod_info->elevation = -1;
    }
    else if (prod_info->type == Layer_Reflect_Avg ||
	     prod_info->type == Layer_Reflect_Max)
    {
	radial = 0;
	prod_info->elevation = product->p5;
	prod_info->top = product->p6;
    }
    else if (prod_info->type == Echo_Tops)
    {
	radial = 0;
	prod_info->elevation = -1;
    }
    else if (prod_info->type == Vert_Liquid)
    {
	radial = 0;
	prod_info->elevation = -1;
    }
    else if (prod_info->type == Precip_1
	     || prod_info->type == Precip_3)
    {
	radial = 0;
	prod_info->elevation = -1;
	prod_info->max /= 10;
	prod_info->end_time = (product->p7 - 1) * 86400 +
			      product->p8 * 60;
    }
    else if (prod_info->type == Precip_Accum)
    {
	radial = 0;
	prod_info->elevation = -1;
	prod_info->start_time = (product->p5 - 1) * 86400 +
				product->p6 * 60;
	prod_info->end_time = (product->p7 - 1) * 86400 +
			      product->p8 * 60;
	/* Other info available */
    }
    else if (prod_info->type == StrmRelMeanVel)
    {
	radial = 1;
	prod_info->elevation = product->p3;
	prod_info->min = product->p4;
	prod_info->max = product->p5;
    }
    else
    {
	unotice("Unknown product code %d", product->code);
	return -1;
    }

    if (prod_info->raster_res == 0)
	prod_info->raster_res = prod_info->data_res;

    uinfo("vs_date %d, vs_time %d, prod_date %d, prod_time %d",
	  (int)product->vs_date, product->vs_time, (int)product->prod_date,
	  product->prod_time);

    if (product->off_symbol != 0)
    {
	int				layer;
	product_symbology_header	ps_head;

	if (!read_dividlen(input, (DivIdLen*)&ps_head) ||
	    getbuf(input, (char*)&ps_head.layers, 1, 2) != 1)
	{
	    unotice("getbuf error reading product symbology header");
	    return -1;
	}
	if (ps_head.divider != -1)
	{
	    unotice("ps_head.divider != -1");
	    return -1;
	}
	uinfo(" PS id %d, length %d, layers %d",
	      ps_head.id, ps_head.length, ps_head.layers);
	for (layer = 0; layer < ps_head.layers; ++layer)
	{
	    int2	divider;
	    int4	layer_length;

	    if (getbuf(input, (char*)&divider, 1, 2) != 1)
	    {
	       unotice("getbuf error reading product divider");
	       return -1;
	    }
	    if (divider != -1)
	    {
		unotice("layer divider %d != -1", layer);
		return -1;
	    }

	    if (getbuf(input, (char*)&layer_length, 1, 4) != 1)
	    {
	       unotice("getbuf error reading layer length");
	       return -1;
	    }

	    uinfo(" Layer %d, length %d", layer, layer_length);

	    /* <<<<< UPC add 961007 >>>>> */
	    if (getbuf(input, (char*)&rcode, 1, 2) != 1)
	    {
	       unotice("getbuf error reading radial/raster flag");
	       return -1;
	    }

	    /* <<<<< UPC mod 961007 >>>>> */
	    if (rcode == 0xaf1f)
	    {
		int	retcode = radial_image(input, raster, size,
					    prod_info, rcode);
		if (retcode != 0)
		    return retcode;
	    }
	    else
	    {
		int	retcode = raster_image(input, raster, size,
					    prod_info, rcode);
		if (retcode != 0)
		    return retcode;
	    }
	}
    }

    if (product->off_graphic != 0)
    {
	graphic_alphanumeric_header	ga_head;

	n = 10;
	if (!read_dividlen(input, (DivIdLen*)&ga_head) ||
	    getbuf(input, (char*)&ga_head.pages, 1, 2) != 1)
	{
	    unotice("getbuf error reading graphic alphanumeric header");
	    return -1;
	}
	if (ga_head.divider != -1)
	{
	    unotice("ga_head.divider != -1");
	    return -1;
	}
	uinfo(" GA id %d, length %d, pages %d",
	      ga_head.id, ga_head.length, ga_head.pages);
	if (ga_head.length > n)
	{
	    /* realloc() doesn't handle NULL pointers on some platforms. */
	    data = data == NULL
			? (char*) malloc(ga_head.length - n)
			: (char*) realloc(data, ga_head.length - n);
	    if (data == NULL)
	    {
		serror("Couldn't allocate %lu bytes for graphic header",
		       (unsigned long)(ga_head.length - n));
		return -1;
	    }
	    if (getbuf(input, data, ga_head.length - n, 1) != 1)
	    {
		uerror("Couldn't read %lu-byte graphic header",
		       (unsigned long)(ga_head.length - n));
		return -1;
	    }
	}
    }

    if (product->off_tabular != 0)
    {
	tabular_alphanumeric_header	ta_head;

	n = 8;
	if (!read_dividlen(input, (DivIdLen*)&ta_head))
	{
	    unotice("getbuf error reading tabular alphanumeric header");
	    return -1;
	}
	if (ta_head.divider != -1)
	{
	    unotice("ta_head.divider != -1");
	    return -1;
	}
	uinfo(" TA id %d, length %d",
	      ta_head.id, ta_head.length);
	if (ta_head.length > n)
	{
	    /* realloc() doesn't handle NULL pointers on some platforms. */
	    data = data == NULL
			? (char*) malloc(ta_head.length - n)
			: (char*) realloc(data, ta_head.length - n);
	    if (data == NULL)
	    {
		serror("Couldn't allocate %lu bytes for tabular header",
		       (unsigned long)(ta_head.length - n));
		return -1;
	    }
	    if (getbuf(input, data, ta_head.length - n, 1) != 1)
	    {
		uerror("Couldn't read %lu-byte tabular header",
		       (unsigned long)(ta_head.length - n));
		return -1;
	    }
	}
    }

    if (getbuf(input, (char*)&trail, 4, 1) != 1)
    {
       unotice("getbuf error reading trailer");
       return -1;
    }
    nbytes += 4;
    uinfo(" Trailer %3.3s %d %d %d %d", trail.id,
	  trail.id[0], trail.id[1], trail.id[2], trail.status);
    if (station_id[0] == 0)
    {
	(void) memcpy(station_id, trail.id, 3);
	station_id[3] = 0;
    }
    else
    {
	if (strncmp(trail.id, station_id, 3))
	{
	    unotice("***** Mismatched station ID's or missing trailer");
	    unotice("ID expected=\"%3.3s\", ID found=\"%3.3s\"", 
		    station_id, trail.id);
	    return -3;
	}
    }

    return 0;					/* success */
}


/*
 * Get the next WSI data product from standard input.  Put the metadata
 * in FORTRAN common blocks.  Error-return if the data product isn't
 * obtained in a reasonable amount of time.
 *
 * Returns:
 *	 0	Success
 *	-1	Error
 *	-2	Insufficient room in passed-in array.
 *	-3	Station ID in product doesn't match non-empty passed-in
 *		station ID
 */
int
wsiinput(image, size, id)
    signed char	*image;		/* image buffer (out) */
    size_t	 size;		/* size of buffer in bytes (in) */
    char         id[4];		/* station ID (in/out) */
{
    int		 input = 0;	/* standard input */
    int		 retcode;
    static int	 alarm_initialized = 0;
    static Alarm timeout_alarm;

    uinfo("wsiinput(): id=\"%3.3s\"", id);

    if (!alarm_initialized)
    {
	alarm_init(&timeout_alarm, TIMEOUT, alarm_handler);
	alarm_initialized = 1;
    }

    alarm_on(&timeout_alarm);

    retcode = wsi_read(input, &MSGHEAD, &PRODDESC, &PRODINFO, 
		       (unsigned char*)image, size, id);

    alarm_off(&timeout_alarm);

    return retcode;
}


/*
 * FORTRAN interface to the above function:
 */
FCALLSCFUN3(INT,wsiinput,WSIIN,wsiin,PBYTE,INT,PSTRING)
