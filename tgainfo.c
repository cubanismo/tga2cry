/* This program prints information about 24 bit TARGA files.
 *
 * This is generic ANSI C, and should compile with any ANSI compliant
 * compiler (e.g. gcc, Borland, Microsoft, or Lattice).
 *
 * History:
 * 1.1		Added code for counting number of colors.
 * 1.0		First version.
 */

#define VERSION "1.1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tgadefs.h"

#if __MSDOS__
#define my_malloc(x) farmalloc((long)(x))
#define my_free(x) farfree(x)
#include <conio.h>
#include <alloc.h>
#else
#define my_malloc(x) malloc(x)
#define my_free(x) free(x)
#endif

#ifndef PATHMAX
#define PATHMAX 256
#endif

#define NO		0
#define YES		1

char *infilename;				/* input file name */
int count_colors;			/* flag to tell whether or not to count colors */
Pixel *srcfile;				/* buffer holding loaded file */

unsigned int image_w;			/* width of image in pixels from TGA header */
unsigned int image_h;			/* height of image in pixels from TGA header */
int bits_per_pixel;			/* bits per pixel from TGA file header */
int bytes_in_name;			/* bytes in filename at end of TGA header */
int tga_flags;				/* TGA file flags */
int cmap_type;				/* color map type */
int sub_type;				/* TGA file sub type */
int cmap_len;				/* length of color map */

char *progname;				/* name the program was invoked with (should be "tgainfo") */

void read_file( char * );

void
usage(void)
{
	printf("%s Version %s\n\n", progname, VERSION);
	printf("Usage: %s [-colors] file.tga\n", progname);
	exit(2);
}

int hflip_flag, vflip_flag;	/* here for historical reasons (I snarfed the code from elsewhere) */

int
main(int argc, char **argv)
{
	hflip_flag = NO;							/* default option is no hflip */
	vflip_flag = NO;							/* default option is no vflip */
	count_colors = NO;

	progname = *argv++;
	if (!*progname) {			/* if for some reason the runtime library didn't get our name... */
		progname = "tgainfo";		/* assume this is our name */
	}
	argc--;
	while (*argv) {
		if (**argv != '-') break;
		if (!strcmp(*argv, "-colors")) {
			count_colors = YES;
		} else {
			usage();		/* illegal option */
		}
		argv++; argc--;
	}
	if (argc != 1) {		/* should be exactly one argument left, the input file name */
		usage();
	}

	read_file(argv[0]);
	return 0;
}

void
err_eof(void)
{
	fprintf(stderr, "Error: unexpected EOF\n");
	exit(1);
}

/* State info for reading RLE-coded pixels; both counts must be init to 0 */
static int block_count;		/* # of pixels remaining in RLE block */
static int dup_pixel_count;	/* # of times to duplicate previous pixel */
static void (*read_pixel)(FILE *fhandle, Pixel *place);
static char tga_pixel[4];

/*
 * read_rle_pixel: read a pixel from an RLE encoded .TGA file
 */
static void
read_rle_pixel(FILE *fhandle, Pixel *place)
{
	int i;

	/* if we're in the middle of reading a duplicate pixel */
	if (dup_pixel_count > 0) {
		dup_pixel_count--;
		place->blue = tga_pixel[0];
		place->green = tga_pixel[1];
		place->red = tga_pixel[2];
		return;
	}
	/* should we read an RLE block header? */
	if (--block_count < 0) {
		i = fgetc(fhandle);
		if (i < 0) err_eof();
		if (i & 0x80) {
			dup_pixel_count = i & 0x7f;	/* number of duplications after this one */
			block_count = 0;		/* then a new block header */
		} else {
			block_count = i & 0x7f;		/* this many unduplicated pixels */
		}
	}
	place->blue = tga_pixel[0] = fgetc(fhandle);
	place->green = tga_pixel[1] = fgetc(fhandle);
	place->red = tga_pixel[2] = fgetc(fhandle);
}

/*
 * read a pixel from an uncompressed .TGA file
 */
static void
read_norm_pixel(FILE *fhandle, Pixel *place)
{
	int i;

	place->blue = fgetc(fhandle);
	place->green = fgetc(fhandle);
	i = fgetc(fhandle);
	if (i < 0) err_eof();
	place->red = i;
}

void
read_row(FILE *fhandle, Pixel *place)
{
	int i;
	void (*rpixel)(FILE *, Pixel *);

	rpixel = read_pixel;
	if (hflip_flag) {
		place += image_w;
		for (i = 0; i < image_w; i++) {
			place--;
			(*rpixel)(fhandle, place);
		}
	} else {
		for (i = 0; i < image_w; i++) {
			(*rpixel)(fhandle, place);
			place++;
		}
	}
}

long color_count[32768];

/*
 * routines to convert to/from color indexes
 */
static INLINE int
HASH(Pixel pix)
{
	int index;

	index = ((int)pix.red>>3) << 5;
	index |= (pix.green>>3);
	index = (index << 5)| (pix.blue>>3);

	return index;
}

static INLINE Pixel
UNHASH(int i)
{
	Pixel p;

	p.blue = (i & 0x1f) << 3;
	i = i >> 5;
	p.green = (i & 0x1f) << 3;
	i = i >> 5;
	p.red = (i & 0x1f) << 3;

	return p;
}

/*
 * count how often each color occurs
 */

static long
do_count(Pixel *pix, size_t numpixels)
{
	int index;
	long numcolors;

	numcolors = 0;

	while (numpixels) {
		index = HASH(*pix);
		if (color_count[index] == 0) {
			color_count[index]++; 
			numcolors++;
		}
		pix++;
		numpixels--;
	}

	return numcolors;
}

void
read_file(char *infile)
{
	FILE *fhandle;
	int c;
	Pixel *row_pixels;
	long i;

	block_count = dup_pixel_count = 0;
	fhandle = fopen(infile, "rb");
	if (!fhandle) {
		perror(infile);
		exit(1);
	}
	bytes_in_name = fgetc(fhandle);
	cmap_type = fgetc(fhandle);
	sub_type = fgetc(fhandle);
	c = fgetc(fhandle);			/* skip bytes 3 and 4 */
	c = fgetc(fhandle);
	if (c < 0) err_eof();

	cmap_len = fgetc(fhandle) + ((unsigned)fgetc(fhandle) << 8);
	c = fgetc(fhandle);			/* skip bytes 7 through 11 */
	c = fgetc(fhandle);
	c = fgetc(fhandle);
	c = fgetc(fhandle);
	c = fgetc(fhandle);
	if (c < 0) err_eof();

	image_w = fgetc(fhandle) + ((unsigned)fgetc(fhandle) << 8);
	image_h = fgetc(fhandle) + ((unsigned)fgetc(fhandle) << 8);
	bits_per_pixel = fgetc(fhandle);
	if (bits_per_pixel < 0) err_eof();
	tga_flags = fgetc(fhandle);

	printf("%s is a %d by %d Targa file with %d bits per pixel\n", infile, image_w, image_h, bits_per_pixel);
	if (!count_colors)
		return;

	/* if we are to count colors, read the file in */
	if (cmap_type != 0) {
		fprintf(stderr, "ERROR: Targa files with color maps not supported\n");
		exit(1);
	}
	if (bits_per_pixel != 24) {
		fprintf(stderr, "ERROR: Only 24 bit Targa files are supported\n");
		exit(1);
	}
	if ((tga_flags & 0x20) == 0) {		/* this picture is bottom-up */
		vflip_flag = !vflip_flag;
	}
	if (tga_flags & 0xc0) {
		fprintf(stderr, "ERROR: Interlaced Targa files are not supported\n");
		exit(1);
	}
/* figure out how to read source pixels */
	if (sub_type > 8) {
	/* an RLE-coded file */
		read_pixel = read_rle_pixel;
		sub_type -= 8;
	} else {
		read_pixel = read_norm_pixel;
	}

	if (sub_type == 1) {
		fprintf(stderr, "ERROR: Colormapped Targa files not supported\n");
		exit(1);
	} else if (sub_type == 2) {
		/* everything is OK */ ;
	} else {
		fprintf(stderr, "ERROR: Invalid or unsupported Targa file\n");
		exit(1);
	}

/* skip the image name */
	for (i = 0; i < bytes_in_name; i++) {
		c = fgetc(fhandle);
		if (c < 0) err_eof();
	}

	srcfile = my_malloc(sizeof(Pixel) * (size_t)image_w*(size_t)image_h);
	if (!srcfile) {
		fprintf(stderr, "ERROR: insufficient memory for image\n");
		exit(1);
	}

	if (vflip_flag) {
		row_pixels = srcfile + image_h*(long)image_w;
		for (i = 0; i < image_h; i++) {
			row_pixels -= image_w;
			read_row(fhandle, row_pixels);
		}
	} else {
		row_pixels = srcfile;
		for (i = 0; i < image_h; i++) {
			read_row(fhandle, row_pixels);
			row_pixels += image_w;
		}
	}

	fclose(fhandle);

	/* now that the file has been read, count the number of different colors in it */
	printf("It has %ld distinct (15 bit) colors\n", do_count(srcfile, (size_t)image_w*(size_t)image_h));
}

