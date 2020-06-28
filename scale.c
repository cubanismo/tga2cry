/*
 * rescaling code: from Dale Schumacher's code in
 * Graphics Gems III
 */
/*
 *		Filtered Image Rescaling
 *
 *		  by Dale Schumacher
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "tgadefs.h"
#include "tgaproto.h"

#if __MSDOS__
#include <alloc.h>
#define my_calloc(x, y) farcalloc((long)(x), (long)(y))
#define my_malloc(x) farmalloc((long)(x))
#define my_free(x) farfree(x)
#else
#define my_calloc(x, y) calloc(x, y)
#define my_malloc(x) malloc(x)
#define my_free(x) free(x)
#endif

#ifndef M_PI
#define M_PI PI
#endif


#if 0
static char	_Program[] = "fzoom";
static char	_Version[] = "0.20";
static char	_Copyright[] = "Public Domain 1991 by Dale Schumacher";
#endif

#define	WHITE_PIXEL	(255)
#define	BLACK_PIXEL	(0)

Image *
new_image(xsize, ysize)	/* create a blank image */
int xsize, ysize;
{
	Image *image;

	if((image = (Image *)my_malloc(sizeof(Image)))
	&& (image->data = (Pixel *)my_calloc(ysize*(size_t)xsize, sizeof(Pixel)))) {
		image->xsize = xsize;
		image->ysize = ysize;
		image->span = xsize;
		return(image);
	}
	return 0;
}

void
free_image(image)
Image *image;
{
	my_free(image->data);
	my_free(image);
}

Pixel
get_pixel(image, x, y)
Image *image;
int x, y;
{
	static Image *im = NULL;
	static int yy = -1;
	static Pixel *p = NULL;
	static Pixel dummypix = { 0, 0, 0 };

	if((x < 0) || (x >= image->xsize) || (y < 0) || (y >= image->ysize)) {
		return dummypix;
	}
	if((im != image) || (yy != y)) {
		im = image;
		yy = y;
		p = image->data + (y * image->span);
	}
	return(p[x]);
}

void
get_row(row, image, y)
Pixel *row;
Image *image;
int y;
{
	if((y < 0) || (y >= image->ysize)) {
		return;
	}
	memcpy(row,
		image->data + (y * image->span),
		(sizeof(Pixel) * image->xsize));
}

void
get_column(column, image, x)
Pixel *column;
Image *image;
int x;
{
	int i, d;
	Pixel *p;

	if((x < 0) || (x >= image->xsize)) {
		return;
	}
	d = (int)image->span;
	for(i = image->ysize, p = image->data + x; i-- > 0; p += d) {
		*column++ = *p;
	}
}

void
put_pixel(image, x, y, data)
Image *image;
int x, y;
Pixel data;
{
	static Image *im = NULL;
	static int yy = -1;
	static Pixel *p = NULL;

	if((x < 0) || (x >= image->xsize) || (y < 0) || (y >= image->ysize)) {
		return;
	}
	if((im != image) || (yy != y)) {
		im = image;
		yy = y;
		p = image->data + (y * image->span);
	}
	p[x] = data;
}


static INLINE int
CLAMP(double value, int min, int max)
{
	int v = (int)value;

	if (v < min) return min;
	if (v > max) return max;
	return v;
}

/*
 *	filter function definitions
 */

#define	box_support		(0.5)

double
box_filter(t)
double t;
{
	if((t > -0.5) && (t <= 0.5)) return(1.0);
	return(0.0);
}

#define	triangle_support	(1.0)

double
triangle_filter(t)
double t;
{
	if(t < 0.0) t = -t;
	if(t < 1.0) return(1.0 - t);
	return(0.0);
}

#define	bell_support		(1.5)

double
bell_filter(t)		/* box (*) box (*) box */
double t;
{
	if(t < 0) t = -t;
	if(t < .5) return(.75 - (t * t));
	if(t < 1.5) {
		t = (t - 1.5);
		return(.5 * (t * t));
	}
	return(0.0);
}

#define	B_spline_support	(2.0)

double
B_spline_filter(t)	/* box (*) box (*) box (*) box */
double t;
{
	double tt;

	if(t < 0) t = -t;
	if(t < 1) {
		tt = t * t;
		return((.5 * tt * t) - tt + (2.0 / 3.0));
	} else if(t < 2) {
		t = 2 - t;
		return((1.0 / 6.0) * (t * t * t));
	}
	return(0.0);
}

double
sinc(x)
double x;
{
	x *= M_PI;
	if(x != 0) return(sin(x) / x);
	return(1.0);
}

#define	Lanczos3_support	(3.0)

double
Lanczos3_filter(t)
double t;
{
	if(t < 0) t = -t;
	if(t < 3.0) return(sinc(t) * sinc(t/3.0));
	return(0.0);
}

#define Sinc_support		(4.0)

double
Sinc_filter(t)
double t;
{
	if (t < 0) t = -t;
	if (t >= Sinc_support) return 0.0;
	return sinc(t);
}

#define	Mitchell_support	(2.0)

#define	B	(1.0 / 3.0)
#define	C	(1.0 / 3.0)

double
Mitchell_filter(t)
double t;
{
	double tt;

	tt = t * t;
	if(t < 0) t = -t;
	if(t < 1.0) {
		t = (((12.0 - 9.0 * B - 6.0 * C) * (t * tt))
		   + ((-18.0 + 12.0 * B + 6.0 * C) * tt)
		   + (6.0 - 2 * B));
		return(t / 6.0);
	} else if(t < 2.0) {
		t = (((-1.0 * B - 6.0 * C) * (t * tt))
		   + ((6.0 * B + 30.0 * C) * tt)
		   + ((-12.0 * B - 48.0 * C) * t)
		   + (8.0 * B + 24 * C));
		return(t / 6.0);
	}
	return(0.0);
}

/*
 *	image rescaling routine
 */

typedef struct {
	int	pixel;
	double	weight;
} CONTRIB;

typedef struct {
	int	n;		/* number of contributors */
	CONTRIB	*p;		/* pointer to list of contributions */
} CLIST;

CLIST	*contrib;		/* array of contribution lists */

void
zoom(dst, src, filterf, fwidth)
Image *dst;				/* destination image structure */
Image *src;				/* source image structure */
double (*filterf)();			/* filter function */
double fwidth;				/* filter width (support) */
{
	Image *tmp;			/* intermediate image */
	double xscale, yscale;		/* zoom scale factors */
	int i, j, k;			/* loop variables */
	int n;				/* pixel number */
	double center, left, right;	/* filter calculation variables */
	double width, fscale, weight;	/* filter calculation variables */
	double red, green, blue;
	Pixel *raster;			/* a row or column of pixels */
	Pixel tmppixel;

	/* create intermediate image to hold horizontal zoom */
	tmp = new_image(dst->xsize, src->ysize);
	if (!tmp) {
		fprintf(stderr, "Unable to allocate memory for intermediate image\n");
		exit(1);
	}
	xscale = (double) dst->xsize / (double) src->xsize;
	yscale = (double) dst->ysize / (double) src->ysize;

	/* pre-calculate filter contributions for a row */
	contrib = (CLIST *)my_calloc(dst->xsize, sizeof(CLIST));
	if(xscale < 1.0) {
		width = fwidth / xscale;
		fscale = 1.0 / xscale;
		for(i = 0; i < dst->xsize; ++i) {
			contrib[i].n = 0;
			contrib[i].p = (CONTRIB *)my_calloc((int) (width * 2 + 1),
					sizeof(CONTRIB));
			center = (double) i / xscale;
			left = ceil(center - width);
			right = floor(center + width);
			for(j = left; j <= right; ++j) {
				weight = center - (double) j;
				weight = (*filterf)(weight / fscale) / fscale;
				if(j < 0) {
					n = -j;
				} else if(j >= src->xsize) {
					n = (src->xsize - j) + src->xsize - 1;
				} else {
					n = j;
				}
				k = contrib[i].n++;
				contrib[i].p[k].pixel = n;
				contrib[i].p[k].weight = weight;
			}
		}
	} else {
		for(i = 0; i < dst->xsize; ++i) {
			contrib[i].n = 0;
			contrib[i].p = (CONTRIB *)my_calloc((int) (fwidth * 2 + 1),
					sizeof(CONTRIB));
			center = (double) i / xscale;
			left = ceil(center - fwidth);
			right = floor(center + fwidth);
			for(j = left; j <= right; ++j) {
				weight = center - (double) j;
				weight = (*filterf)(weight);
				if(j < 0) {
					n = -j;
				} else if(j >= src->xsize) {
					n = (src->xsize - j) + src->xsize - 1;
				} else {
					n = j;
				}
				k = contrib[i].n++;
				contrib[i].p[k].pixel = n;
				contrib[i].p[k].weight = weight;
			}
		}
	}

	/* apply filter to zoom horizontally from src to tmp */
	raster = (Pixel *)my_calloc(src->xsize, sizeof(Pixel));
	for(k = 0; k < tmp->ysize; ++k) {
		get_row(raster, src, k);
		for(i = 0; i < tmp->xsize; ++i) {
			red = green = blue = 0.0;
			for(j = 0; j < contrib[i].n; ++j) {
				red += raster[contrib[i].p[j].pixel].red
					* contrib[i].p[j].weight;
				green += raster[contrib[i].p[j].pixel].green
					* contrib[i].p[j].weight;
				blue += raster[contrib[i].p[j].pixel].blue
					* contrib[i].p[j].weight;
			}
			tmppixel.red = CLAMP(red, BLACK_PIXEL, WHITE_PIXEL);
			tmppixel.green = CLAMP(green, BLACK_PIXEL, WHITE_PIXEL);
			tmppixel.blue = CLAMP(blue, BLACK_PIXEL, WHITE_PIXEL);
			put_pixel(tmp, i, k, tmppixel);
		}
	}
	my_free(raster);

	/* free the memory allocated for horizontal filter weights */
	for(i = 0; i < tmp->xsize; ++i) {
		my_free(contrib[i].p);
	}
	my_free(contrib);

	/* pre-calculate filter contributions for a column */
	contrib = (CLIST *)my_calloc(dst->ysize, sizeof(CLIST));
	if(yscale < 1.0) {
		width = fwidth / yscale;
		fscale = 1.0 / yscale;
		for(i = 0; i < dst->ysize; ++i) {
			contrib[i].n = 0;
			contrib[i].p = (CONTRIB *)my_calloc((int) (width * 2 + 1),
					sizeof(CONTRIB));
			center = (double) i / yscale;
			left = ceil(center - width);
			right = floor(center + width);
			for(j = left; j <= right; ++j) {
				weight = center - (double) j;
				weight = (*filterf)(weight / fscale) / fscale;
				if(j < 0) {
					n = -j;
				} else if(j >= tmp->ysize) {
					n = (tmp->ysize - j) + tmp->ysize - 1;
				} else {
					n = j;
				}
				k = contrib[i].n++;
				contrib[i].p[k].pixel = n;
				contrib[i].p[k].weight = weight;
			}
		}
	} else {
		for(i = 0; i < dst->ysize; ++i) {
			contrib[i].n = 0;
			contrib[i].p = (CONTRIB *)my_calloc((int) (fwidth * 2 + 1),
					sizeof(CONTRIB));
			center = (double) i / yscale;
			left = ceil(center - fwidth);
			right = floor(center + fwidth);
			for(j = left; j <= right; ++j) {
				weight = center - (double) j;
				weight = (*filterf)(weight);
				if(j < 0) {
					n = -j;
				} else if(j >= tmp->ysize) {
					n = (tmp->ysize - j) + tmp->ysize - 1;
				} else {
					n = j;
				}
				k = contrib[i].n++;
				contrib[i].p[k].pixel = n;
				contrib[i].p[k].weight = weight;
			}
		}
	}

	/* apply filter to zoom vertically from tmp to dst */
	raster = (Pixel *)my_calloc(tmp->ysize, sizeof(Pixel));
	for(k = 0; k < dst->xsize; ++k) {
		get_column(raster, tmp, k);
		for(i = 0; i < dst->ysize; ++i) {
			red = green = blue = 0.0;
			for(j = 0; j < contrib[i].n; ++j) {
				red += raster[contrib[i].p[j].pixel].red
					* contrib[i].p[j].weight;
				green += raster[contrib[i].p[j].pixel].green
					* contrib[i].p[j].weight;
				blue += raster[contrib[i].p[j].pixel].blue
					* contrib[i].p[j].weight;
			}
			tmppixel.red = CLAMP(red, BLACK_PIXEL, WHITE_PIXEL);
			tmppixel.green = CLAMP(green, BLACK_PIXEL, WHITE_PIXEL);
			tmppixel.blue = CLAMP(blue, BLACK_PIXEL, WHITE_PIXEL);
			put_pixel(dst, k, i, tmppixel);
		}
	}
	my_free(raster);

	/* free the memory allocated for vertical filter weights */
	for(i = 0; i < dst->ysize; ++i) {
		my_free(contrib[i].p);
	}
	my_free(contrib);
	free_image(tmp);
}

/*
 *	interface to tga2cry program
 */

Pixel *
rescale(Pixel *oldpix, unsigned old_w, unsigned old_h, unsigned new_w, unsigned new_h, int filter_type, int aspect)
{
	Image oldimage, newimage;
	Pixel *newpix;
	double delta, fwidth;
	unsigned vert_border, horiz_border;
	double (*filterf)();

	newpix = my_calloc(new_w*(size_t)new_h, sizeof(Pixel));
	if (!newpix) return 0;

	oldimage.span = (long)old_w;
	oldimage.xsize = (int)oldimage.span;
	oldimage.ysize = old_h;
	oldimage.data = oldpix;

	newimage.span = new_w;
	newimage.data = newpix;
/*
 * figure out the proper new width and height to preserve aspect ratios
 * first, we'll try scaling by width (leaving a border at the bottom)
 */

	delta = (double)new_w/(double)old_w;
	vert_border = delta*old_h;

	if (vert_border > new_h) {
		/* this is no good! we'll have to scale by height and leave borders
		 * at the sides
		 */
		delta = (double)new_h/(double)old_h;
		horiz_border = delta*old_w;
		vert_border = new_h;
	} else {
		horiz_border = new_w;
	}

	if (aspect) {
		newimage.xsize = horiz_border;
		newimage.ysize = vert_border;
	/* center the output */
		newimage.data = newpix + ((new_w*(new_h - vert_border)/2) + (new_w - horiz_border)/2);
	} else {
		newimage.xsize = new_w;
		newimage.ysize = new_h;
	}

/*
 * pick a filter type
 */
	switch(filter_type) {
	case FILTER_BOX:
		filterf = box_filter;
		fwidth = box_support;
		break;
	case FILTER_BELL:
		filterf = bell_filter;
		fwidth = bell_support;
		break;
	case FILTER_LANC:
		filterf = Lanczos3_filter;
		fwidth = Lanczos3_support;
		break;
	case FILTER_MITCH:
	default:
		filterf = Mitchell_filter;
		fwidth = Mitchell_support;
		break;
	case FILTER_SINC:
		filterf = Sinc_filter;
		fwidth = Sinc_support;
		break;
	case FILTER_TRI:
		filterf = triangle_filter;
		fwidth = triangle_support;
		break;
	}
	zoom(&newimage, &oldimage, filterf, fwidth);
	return newpix;
}

/*
 * crop an input image to a specified window
 */
Pixel *
crop(Pixel *oldpix, unsigned image_w, unsigned image_h, unsigned crop_x, unsigned crop_y, unsigned crop_w, unsigned crop_h)
{
	unsigned x, y;
	Pixel *inptr, *outptr;
	Pixel *newpix;

	newpix = (Pixel *)my_calloc(crop_w * (size_t)crop_h, sizeof(Pixel));
	if (!newpix) {
		fprintf(stderr, "ERROR: insufficient memory for cropping picture\n");
		exit(1);
	}
	crop_w += crop_x;		/* move to lower left hand corner */
	crop_h += crop_y;

	inptr = oldpix + crop_y*((long)image_w);
	outptr = newpix;
	for (y = crop_y; y < crop_h; y++) {
		for (x = 0; x < image_w; x++) {
			if (x >= crop_x && x < crop_w)
				*outptr++ = *inptr;
			inptr++;
		}
	}
	return newpix;
}
