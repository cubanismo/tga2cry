/* This program converts 24 bit TGA picture files to 16 bit CRY or RGB, or
 * 24 bit RGB.
 *
 * This is generic ANSI C, and should compile with any ANSI compliant
 * compiler (e.g. gcc, Borland, Microsoft, or Lattice).
 *
 * History:
 * 1.16		Added -varmod option
 * 1.15		Added -relative option; made blitter width errors into warnings.
 * 1.14		Added -nodata option
 * 1.13		Added -crop option
 * 1.12		Added 8 bit and 4 bit output formats.
 * 1.11		Added -stripbits flag.
 * 1.10		Added -nozero flag.
 * 1.9		Added -rotate flag. Made -aspect center the output.
 * 1.8		Added glass output format. Fixed dithering bug.
 * 1.7		Added gray output format, and options for manipulating
 *		it.
 * 1.6		Fixed bug in -vflip. Added new filtering options.
 *		Made cry data unsigned char instead of unsigned short.
 * 1.5		Added -rescale option
 * 1.4		Added check for illegal image widths
 * 1.3		Added "-f" option for different formats
 * 1.2		Binary file output
 * 1.1		First command line version
 */

#define VERSION "1.16"

#if __MSDOS__
#define my_malloc(x) farmalloc((long)(x))
#define my_free(x) farfree(x)
#include <conio.h>
#include <alloc.h>
#else
#define my_malloc(x) malloc(x)
#define my_free(x) free(x)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "tgadefs.h"
#include "tgaproto.h"

#ifndef PATHMAX
#define PATHMAX 256
#endif


#define NO		0
#define YES		1

char *infilename;				/* input file name */
char *outfilename;				/* output file name */
char *picname;					/* name of image to be printed in file */
FILE *outhandle;				/* output file pointer */
Pixel *srcfile;				/* buffer holding loaded file */
Pixel *newdata;				/* address of beginning of TGA data */

unsigned int image_w;			/* width of image in pixels from TGA header */
unsigned int image_h;			/* height of image in pixels from TGA header */
int bits_per_pixel;			/* bits per pixel from TGA file header */
int bytes_in_name;			/* bytes in filename at end of TGA header */
int tga_flags;				/* TGA file flags */
int cmap_type;				/* color map type */
int sub_type;				/* TGA file sub type */
int cmap_len;				/* length of color map */

int quiet_flag;				/* Should we print lots of messages to screen? */
int items_per_line;			/* count words per line in new file */
long binary_file_size;			/* size of output binary file (we round to a phrase boundary) */
int binary_bit_size;			/* for counting bits of a fractional byte */

int nodata_flag;			/* if output might not be in .data segment */
int nozero_flag;			/* if only true 0 should result in a zero output */
int hflip_flag;				/* if image should be flipped horizontally */
int vflip_flag;				/* if image should be flipped vertically */
int rotate_flag;			/* if image should be turned 90 degrees clockwise */
int data_type;				/* if new data should be CRY or RGB format */
int dither_flag;			/* if CRY conversion should use dithering */
int header_flag;			/* if new style header should be used */
int binary_flag;			/* if output file should be binary */
int aspect_flag;			/* if aspect ratio should be preserved when scaling */
int varmod_flag;			/* if low bit of data should indicate RGB or CRY output */
int bit_buffer;				/* bit buffer for 1 bit at a time MSK output */
int filter_type;			/* flag for which kind of filter to use */
int gray_threshold;			/* limit for converting gray maps */
int gray_color;				/* color to be or'd in with CRY intensity */
int contrast_min;			/* minimum value for contrast enhancement */
int contrast_max;			/* maximum value for contrast enhancement */
double contrast;			/* scaling for contrast */
int rescale_w, rescale_h;		/* new size for image */
int stripbits_mask;			/* for CRY: controls how many bits of intensity to strip off */
int base_intensity;			/* for CRY: make intensities relative to this */
int max_colors;				/* for palettes: controls max. number of colors to allocate from palette */
int bit_colors;				/* for palettes: gives the limit for max_colors */
int base_color;				/* for palettes: added to all pixel values output */
int num_colors;				/* for palettes: gives number of colors actually in the palette */
int crop_x, crop_y, crop_w, crop_h;	/* crop region, or 0,0,0,0 for no cropping */
Palette_Entry palette[256];		/* here is the palette */

extern unsigned char cry[];		/* cry lookup table */
extern unsigned short cryred[],crygreen[],cryblue[];	/* lookup tables for cry->rgb conversion */

/* constants for data_type */
#define CRY16		0
#define RGB16		1
#define RGB24		2
#define MSK		3
#define GRAY		4
#define GLASS		5
#define CRY8		6
#define RGB8		7
#define CRY4		8
#define RGB4		9
#define CRY1		10
#define RGB1		11

char *progname;				/* name the program was invoked with (should be "tga2cry") */

char wkstr[300];

void
usage( char *msg )
{
	if (msg != (char *)0)
		printf("%s", msg);

	printf("%s Version %s\n\n", progname, VERSION);
	printf("Usage: %s {options} [-resize w,h][-crop x,y,w,h][-f outformat][-filter outfilter][-o outfile] file.tga\n", progname);
	printf("Valid options are:\n");
	printf("\t-aspect       Preserve aspect ratio when resizing, by adding a black border\n");
	printf("\t-binary       Output raw binary instead of assembly language\n");
	printf("\t-dither       Dither CRY output for better conversion from RGB\n");
	printf("\t-header       Add texture map header\n");
	printf("\t-hflip        Flip picture horizontally\n");
	printf("\t-nodata       Don't output a .data directive\n");
	printf("\t-nozero       Only output a 0x0000 color if input red=green=blue=0\n");
	printf("\t-quiet        Quiet mode, print only FATAL ERROR messages to screen.\n");
	printf("\t-rotate       Rotate picture 90 degrees clockwise\n");
	printf("\t-varmod       Set or clear low bit of data to indicate RGB or CRY mode.\n");
	printf("\t-vflip        Flip picture veritcally\n");
	printf("\t-crop x,y,w,h Use a subset of the input: (x,y) is the upper left corner, (w,h) the width & height\n");
	printf("\t-resize w,h   Resize output to w pixels wide and h hide\n");
	printf("Valid output formats are:\n");
	printf("\tcry           16 bit CRY (default)\n");
	printf("\tcry8          8 bits/pixel with CRY palette appended\n");
	printf("\tcry4		4 bits/pixel with CRY palette appended\n");
	printf("\tcry1		1 bit/pixel with CRY palette appended\n");
	printf("\tgray          16 bit CRY intensities only\n");
	printf("\tglass         16 bit CRY intensities relative to 0x80\n");
	printf("\tmsk           1 bit mask for black/non-black\n");
	printf("\trgb           16 bit RGB\n");
	printf("\trgb8          8 bits/pixel with RGB palette appended\n");
	printf("\trgb4		4 bits/pixel with RGB palette appended\n");
	printf("\trgb1		1 bit/pixel with RGB palette appended\n");
	printf("\trgb24         24 bit (Jaguar) RGB\n");
	printf("Valid filters for resizing are:\n");
	printf("\tbell          Bell filter\n");
	printf("\tbox           Box filter\n");
	printf("\tlanc          Lanczos filter\n");
	printf("\tmitch         Mitchell filter (default)\n");
	printf("\tsinc          Sin(x)/x (support 4)\n");
	printf("\ttri           Triangle filter\n");
	printf("Options for cry format:\n");
	printf("\t-stripbits n  Strip the lower n bits of a CRY picture\n");
	printf("\t-relative  n  Make all intensities signed offsets from n\n");
	printf("Options for cry8, rgb8, cry4, and rgb4 formats:\n");
	printf("\t-maxcolors n  Use at most n colors in the palette\n");
	printf("\t-basecolor n  Add n to every pixel value\n");
	printf("Options for gray and glass formats:\n");
	printf("\t-glimit n     Make any intensity < n black (n is from 0 to 254)\n");
	printf("\t-gcolor n     Set the CRY color byte to n, rather than 0\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	data_type = CRY16;							/* default is to write 16 bit CRY data */
	hflip_flag = NO;							/* default option is no hflip */
	vflip_flag = NO;							/* default option is no vflip */
	rotate_flag = NO;
	dither_flag = NO;
	header_flag = NO;
	filter_type = FILTER_MITCH;
	aspect_flag = NO;
	quiet_flag = NO;
	nodata_flag = NO;
	varmod_flag = NO;
	rescale_w = rescale_h = 0;
	crop_x = crop_y = crop_w = crop_h = 0;
	gray_threshold = gray_color = 0;
	contrast_min = 0;
	contrast_max = 255;
	stripbits_mask = 0xff;
	base_intensity = 0;			/* indicates no base, i.e. output raw intensities */
	max_colors = bit_colors = 0;		/* indicates unlimited colors */
	base_color = 0;
	progname = *argv++;
	if (!*progname) {			/* if for some reason the runtime library didn't get our name... */
		progname = "tga2cry";		/* assume this is our name */
	}
	argc--;
	if (!*argv) {
		usage( (char *)0 );		/* program invoked with no arguments */
	}
	while (*argv) {
		if (**argv != '-') break;
		if (!strcmp(*argv, "-binary")) {
			binary_flag = YES;
		} else if (!strcmp(*argv, "-quiet")) {
			quiet_flag = YES;
		} else if (!strcmp(*argv, "-dither")) {
			dither_flag = YES;
		} else if (!strcmp(*argv, "-header")) {
			header_flag = YES;
		} else if (!strcmp(*argv, "-nodata")) {
			nodata_flag = YES;
		} else if (!strcmp(*argv, "-hflip")) {
			hflip_flag = YES;
		} else if (!strcmp(*argv, "-vflip")) {
			vflip_flag = YES;
		} else if (!strcmp(*argv, "-rotate")) {
			rotate_flag = YES;
		} else if (!strcmp(*argv, "-nozero")) {
			nozero_flag = YES;
		} else if (!strcmp(*argv, "-aspect")) {
			aspect_flag = YES;
		} else if (!strcmp(*argv, "-glimit")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No argument given for '-glimit' flag\n" );
			}
			if (sscanf(*argv, "%i", &gray_threshold) != 1)
				usage( "Invalid argument given for '-glimit' flag\n" );
		} else if (!strcmp(*argv, "-stripbits")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No argument given for '-stripbits' flag\n" );
			}
			if (sscanf(*argv, "%i", &stripbits_mask) != 1)
				usage( "Invalid argument given for '-stripbits' flag\n" );
			stripbits_mask = ~((1 << stripbits_mask) - 1);
		} else if (!strcmp(*argv, "-relative")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No argument given for '-relative' flag\n" );
			}
			if (sscanf(*argv, "%i", &base_intensity) != 1)
				usage( "Invalid argument given for '-relative' flag\n" );
		} else if (!strcmp(*argv, "-maxcolors")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No argument given for '-maxcolors' flag\n" );
			}
			if (sscanf(*argv, "%i", &max_colors) != 1)
				usage( "Invalid argument given for '-maxcolors' flag\n" );
		} else if (!strcmp(*argv, "-basecolor")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No argument given for '-basecolor' flag\n" );
			}
			if (sscanf(*argv, "%i", &base_color) != 1)
				usage( "Invalid argument given for '-basecolor' flag\n" );
		} else if (!strcmp(*argv, "-gcolor")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No argument given for '-gcolor' flag\n" );
			}
			if (sscanf(*argv, "%i", &gray_color) != 1)
				usage( "Invalid argument given for '-gcolor' flag\n" );
			gray_color = gray_color << 8;
		} else if (!strcmp(*argv, "-resize")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No arguments given for '-resize' flag\n" );
			}
			if (sscanf(*argv, "%i,%i", &rescale_w, &rescale_h) != 2)
				usage( "Invalid argument(s) given for '-resize' flag\n" );
		} else if (!strcmp(*argv, "-crop")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No arguments given for '-crop' flag\n" );
			}
			if (sscanf(*argv, "%i,%i,%i,%i", &crop_x, &crop_y, &crop_w, &crop_h) != 4)
				usage( "Invalid argument(s) given for '-crop' flag\n" );
		} else if (!strcmp(*argv, "-gcontrast")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No arguments given for '-gcontrast' flag\n" );
			}
			if (sscanf(*argv, "%i,%i", &contrast_min, &contrast_max) != 2)
				usage( "Invalid argument(s) given for '-gcontrast' flag\n" );
		} else if (!strcmp(*argv, "-f")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No format type specified\n" );
			}
			if (!strcmp(*argv, "cry") || !strcmp(*argv, "cry16")) {
				data_type = CRY16;
			} else if (!strcmp(*argv, "cry8")) {
				data_type = CRY8;
				bit_colors = 256;
			} else if (!strcmp(*argv, "cry4")) {
				data_type = CRY4;
				bit_colors = 16;
			} else if (!strcmp(*argv, "cry1")) {
				data_type = CRY1;
				bit_colors = 2;
			} else if (!strcmp(*argv, "rgb") || !strcmp(*argv, "rgb16")) {
				data_type = RGB16;
			} else if (!strcmp(*argv, "rgb8")) {
				data_type = RGB8;
				bit_colors = 256;
			} else if (!strcmp(*argv, "rgb4")) {
				data_type = RGB4;
				bit_colors = 16;
			} else if (!strcmp(*argv, "rgb1")) {
				data_type = RGB1;
				bit_colors = 2;
			} else if (!strcmp(*argv, "rgb24")) {
				data_type = RGB24;
			} else if (!strcmp(*argv, "msk")) {
				data_type = MSK;
			} else if (!strcmp(*argv, "gray")) {
				data_type = GRAY;
			} else if (!strcmp(*argv, "glass")) {
				data_type = GLASS;
			} else {
				usage( "Invalid format given with '-f' flag\n" );
			}
		} else if (!strcmp(*argv, "-filter")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No filter type given\n" );
			}
			if (!strcmp(*argv, "box")) {
				filter_type = FILTER_BOX;
			} else if (!strcmp(*argv, "bell")) {
				filter_type = FILTER_BELL;
			} else if (!strcmp(*argv, "lanc")) {
				filter_type = FILTER_LANC;
			} else if (!strcmp(*argv, "tri")) {
				filter_type = FILTER_TRI;
			} else if (!strcmp(*argv, "sinc")) {
				filter_type = FILTER_SINC;
			} else if (!strcmp(*argv, "mitch")) {
				filter_type = FILTER_MITCH;
			} else {
				usage( "Invalid filter type specified\n" );
			}
		} else if (!strcmp(*argv, "-o")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No output file name given with '-o'\n" );
			}
			outfilename = *argv;
		} else {
			sprintf( wkstr, "Illegal option given: '%s'\n", *argv );
			usage(wkstr);		/* illegal option */
		}
		argv++; argc--;
	}
	if (argc != 1) {		/* should be exactly one argument left, the input file name */
		usage( "Exactly one input file must be specified\n" );
	}

	/* sanity checking on arguments */
	if (bit_colors && !max_colors)
		max_colors = bit_colors;

	if (max_colors != 0 && bit_colors == 0) {		/* palette requested but not palette output format */
		fprintf(stderr, "-maxcolors option only valid with palette output formats\n");
		usage( (char *)0 );
	}
	if (max_colors + base_color > bit_colors) {
		fprintf(stderr, "-basecolor set too large for this output format\n");
		usage( (char *)0 );
	}

	infilename = *argv;
	contrast = (double)(255-gray_threshold)/(double)(contrast_max-contrast_min);
	if (!outfilename) {
		if (data_type == CRY16 || data_type == GRAY || data_type == GLASS)
			outfilename = change_extension(infilename, ".cry");
		else if (data_type == MSK)
			outfilename = change_extension(infilename, ".msk");
		else if (data_type == CRY8)
			outfilename = change_extension(infilename, ".cr8");
		else if (data_type == CRY4)
			outfilename = change_extension(infilename, ".cr4");
		else if (data_type == CRY1)
			outfilename = change_extension(infilename, ".cr1");
		else if (data_type == RGB8)
			outfilename = change_extension(infilename, ".rg8");
		else if (data_type == RGB4)
			outfilename = change_extension(infilename, ".rg4");
		else if (data_type == RGB1)
			outfilename = change_extension(infilename, ".rg1");
		else
			outfilename = change_extension(infilename, ".rgb");
	}
	picname = strip_extension(outfilename);
	return do_file(infilename, outfilename);
}

#if __MSDOS__
void draw_percentage( int pct )
{
	if( ! quiet_flag )
	{
		printf( "Image Conversion %d%% Complete\n", pct );
		gotoxy( wherex(), wherey() - 1 );
	}
}
#else
void draw_percentage( int pct )
{
	if( ! quiet_flag )
	{
		if (pct > 100)
			printf( "Image Conversion %d%% Complete\n", 100 );
		else
			printf( "Image Conversion %d%% Complete\r", pct );

		fflush( stdout );
	}
}
#endif /* __MSDOS__ */

/*************************************************************************
change_extension(name, ext): creates a duplicate string, containing the
given file name but with its extension changed to ext; if the file had
no extension, one is added.
ext must contain the appropriate '.' character
**************************************************************************/
char *
change_extension(char *name, char *ext)
{
	char *s;		/* temporary string pointer */
	size_t len;		/* length of the string */
	size_t extpos;		/* position where the extension is to be added */
	char *newname;

	len = extpos = 0;

	for (s = name; *s; s++) {
		if (*s == '\\' || *s == '/') {		/* account for both UNIX and DOS path separators */
			extpos = 0;			/* no extension yet, the name isn't finished */
		} else if (*s == '.') {
			extpos = len;
		}
		len++;
	}
	if (extpos == 0)
		extpos = len;

	newname = my_malloc(len+strlen(ext)+1);		/* the "+1" is for the trailing 0 */
	if (!newname) {
		fprintf(stderr, "ERROR: insufficient memory\n");
		exit(1);
	}
	strcpy(newname, name);
	strcpy(newname+extpos, ext);
	return newname;
}

/*************************************************************************
strip_extension(name): creates a duplicate string, containing the
given file name but with no extension or path name, i.e. the string
"c:\foo\bar.tga" gets turned into just plain "bar"
**************************************************************************/
char *
strip_extension(char *name)
{
	char *s;		/* temporary string pointer */
	size_t len;		/* length of the string */
	size_t extpos;		/* position where the extension is to be added */
	char *newname;

	len = extpos = 0;

	for (s = name; *s; s++) {
		if (*s == '\\' || *s == '/') {		/* account for both UNIX and DOS path separators */
			extpos = len = 0;		/* no extension yet, the name isn't finished */
			name = s+1;			/* throw away everything before this */
		} else if (*s == '.') {
			extpos = len++;
		} else {
			len++;
		}
	}
	if (extpos == 0)
		extpos = len;

	newname = my_malloc(len+1);		/* the "+1" is for the trailing 0 */
	if (!newname) {
		fprintf(stderr, "ERROR: insufficient memory\n");
		exit(1);
	}
	strcpy(newname, name);
	strcpy(newname+extpos, "");
	return newname;
}

/****************************************************************************/
/* do_file() writes the data in the srcfile buffer into a new file	    */
/****************************************************************************/
int
do_file(char *infile, char *outfile)
{
	read_file(infile);

	if (binary_flag) {
		outhandle = fopen(outfilename, "wb");
	} else {
		outhandle = fopen(outfilename, "w");
	}
	if (!outhandle) {
		perror(outfilename);
		return 1;
	}
	make_newdata();
	fclose(outhandle);
	my_free(srcfile);

	return(0);
}

void
err_eof(void)
{
	fprintf(stderr, "ERROR: unexpected EOF\n");
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
	if (rotate_flag) {
		if (hflip_flag) {
			place += image_h*(long)image_w;
			for (i = 0; i < image_h; i++) {
				place -= image_w;
				(*rpixel)(fhandle, place);
			}
		} else {
			for (i = 0; i < image_h; i++) {
				(*rpixel)(fhandle, place);
				place += image_w;
			}
		}
	} else if (hflip_flag) {
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

/* set input crop window */
	if (crop_w != 0 && crop_h != 0) {
		if ( (crop_x + crop_w > image_w) || (crop_y + crop_h > image_h) ) {
			fprintf(stderr, "WARNING: crop window exceeds size of input image\n");
			exit(1);
		}
	}

	bits_per_pixel = fgetc(fhandle);
	if (bits_per_pixel < 0) err_eof();
	tga_flags = fgetc(fhandle);

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

	if (rotate_flag) {
		unsigned int temp;
		temp = image_w;
		image_w = image_h;
		image_h = temp;

		if (vflip_flag) {
			row_pixels = srcfile;
			for (i = 0; i < image_w; i++) {
				read_row(fhandle, row_pixels);
				row_pixels++;
			}
		} else {
			row_pixels = srcfile + image_w;
			for (i = 0; i < image_w; i++) {
				row_pixels--;
				read_row(fhandle, row_pixels);
			}
		}
	} else if (vflip_flag) {
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

	/* crop input */
	/* LOGICALLY, this should happen *before* -hflip, -vflip, or -rotate, but it's too much
	 * hassle to implement that now
	 */
	if (crop_w != 0 && crop_h != 0) {
		Pixel *newpix;

		newpix = crop(srcfile, image_w, image_h, crop_x, crop_y, crop_w, crop_h);
		free(srcfile);
		srcfile = newpix;
		image_w = crop_w;
		image_h = crop_h;
	}
}

static INLINE void
diffuse_error(Pixel newcolor, Pixel origcolor, Pixel *where, long linelen)
{
	int	err;
	int	x;

/* diffuse error in red */
	err = (int)origcolor.red - (int)newcolor.red;
	x = where[1].red + ((7*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[1].red = x;

	x = where[linelen-1].red + ((3*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen-1].red = x;

	x = where[linelen].red + ((5*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen].red = x;

	x = where[linelen+1].red + (err>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen+1].red = x;

/* diffuse error in green */
	err = (int)origcolor.green - (int)newcolor.green;
	x = where[1].green + ((7*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[1].green = x;

	x = where[linelen-1].green + ((3*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen-1].green = x;

	x = where[linelen].green + ((5*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen].green = x;

	x = where[linelen+1].green + (err>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen+1].green = x;

/* diffuse error in blue */
	err = (int)origcolor.blue - (int)newcolor.blue;
	x = where[1].blue + ((7*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[1].blue = x;

	x = where[linelen-1].blue + ((3*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen-1].blue = x;

	x = where[linelen].blue + ((5*err)>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen].blue = x;

	x = where[linelen+1].blue + (err>>4);
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	where[linelen+1].blue = x;
}

void
output_byte(FILE *f, unsigned char w)
{
	binary_file_size++;
	if (binary_flag) {
		fputc(w, f);
	} else {
		if (items_per_line == 0) {
			fprintf(f, "\tdc.b\t$%02X", w);
		} else {
			fprintf(f, ",$%02X", w);
		}
		if (items_per_line++ == 15) {
			fputc('\n', f);
			items_per_line = 0;
		}
	}
}

void
output_word(FILE *f, uint16_t w)
{
	binary_file_size += 2;
	if (binary_flag) {
		int lo, hi;

		lo = (w & 0x00ff);
		hi = (w >> 8);
		fputc(hi, f);
		fputc(lo, f);
	} else {
		if (items_per_line == 0) {
			fprintf(f, "\tdc.w\t$%04" PRIX16, w);
		} else {
			fprintf(f, ",$%04" PRIX16, w);
		}
		if (items_per_line++ == 15) {
			fputc('\n', f);
			items_per_line = 0;
		}
	}
}

void
output_long(FILE *f, uint32_t w)
{
	binary_file_size += 4;
	if (binary_flag) {
		int lo, hi;

		hi = (w >> 24);
		lo = ((w >> 16) & 0x00ff);
		fputc(hi, f);
		fputc(lo, f);
		hi = (w >> 8) & 0x00ff;
		lo = w & 0x00ff;
		fputc(hi, f);
		fputc(lo, f);
	} else {
		if (items_per_line == 0) {
			fprintf(f, "\tdc.l\t$%08" PRIX32, w);
		} else {
			fprintf(f, ",$%08" PRIX32, w);
		}
		if (items_per_line++ == 7) {
			fputc('\n', f);
			items_per_line = 0;
		}
	}
}

void
output_bit(FILE *f, int b)
{
	bit_buffer = (bit_buffer << 1) | b;
	binary_bit_size++;
	if (binary_bit_size >= 8) {
		binary_file_size++;
		binary_bit_size = 0;
		if (binary_flag) {
			fputc(bit_buffer, f);
		} else {
			fprintf(f, "\tdc.b\t$%02X\n", bit_buffer);
		}
		bit_buffer = 0;
	}
}

/* output 4 bits */
void
output_nybble(FILE *f, int b)
{
	bit_buffer = (bit_buffer << 4) | b;
	binary_bit_size += 4;
	if (binary_bit_size == 8) {
		binary_file_size++;
		binary_bit_size = 0;
		if (binary_flag) {
			fputc(bit_buffer, f);
		} else {
			fprintf(f, "\tdc.b\t$%02X\n", bit_buffer);
		}
		bit_buffer = 0;
	}
}

/*
 * synchronize output to a word boundary
 */
void
output_sync(FILE *f)
{
	if (binary_flag == 0 && items_per_line != 0) {
		fputc('\n', f);
		items_per_line = 0;
	}
	while (binary_bit_size != 0) {
		output_bit(f, 0);
	}
	if (binary_file_size & 1) {
		output_byte(f, 0);
		if (binary_flag == 0) {
			fputc('\n',f);
			items_per_line = 0;
		}
	}
}

/*
 * functions for converting palette entries into 16 bit RGB or CRY, respectively
 */
void
rgbize_palette()
{
	int i;
	unsigned int red, green, blue;
	Pixel p;

	for (i = 0; i < num_colors; i++) {
		p = palette[i].color;
		red = p.red >> 3;
		green = p.green >> 2;
		blue = p.blue >> 3;

		if (varmod_flag)
			palette[i].outval = (red << 11) | (blue << 6) | green | 1;
		else
			palette[i].outval = (red << 11) | (blue << 6) | green | 1;
		palette[i].color.red = red << 3;
		palette[i].color.green = green << 2;
		palette[i].color.blue = blue << 3;
	}
}

void
cryize_palette()
{
	int i;
	int intensity;
	unsigned int color_offset;		/* offset for cry lookup table */
	unsigned int rcomp,gcomp,bcomp;
	unsigned int red, green, blue;

	for (i = 0; i < num_colors; i++) {
		red = palette[i].color.red;
		green = palette[i].color.green;
		blue = palette[i].color.blue;

		intensity = red;				/* start with red */
		if(green > intensity)
			intensity = green;
		if(blue > intensity)
			intensity = blue;			/* get highest RGB value */
		if(intensity != 0)
		{
			rcomp = (unsigned int)red * 255 / intensity;
			gcomp = (unsigned int)green * 255 / intensity;
			bcomp = (unsigned int)blue * 255 / intensity;
		}
		else
			rcomp = gcomp = bcomp = 0;		/* R, G, B, were all 0 (black) */

		color_offset = (rcomp & 0xF8) << 7;
		color_offset |= (gcomp & 0xF8) << 2;
		color_offset |= (bcomp & 0xF8) >> 3;		/* now we have offset for cry table */

		intensity = intensity & stripbits_mask;
		if (base_intensity > 0) {
			intensity = intensity - base_intensity;
			if (intensity > 0x7f) intensity = 0x7f;
			else if (intensity < -0x7f) intensity = -0x7f;
		}
		if (varmod_flag)
			intensity &= 0xfe;

		palette[i].outval = ((color_offset = cry[color_offset]) << 8) | (intensity & 0x00ff);

		/* now convert back to RGB for dithering purposes */
		palette[i].color.red = (intensity*cryred[color_offset]) >> 8;
		palette[i].color.green = (intensity*crygreen[color_offset]) >> 8;
		palette[i].color.blue = (intensity*cryblue[color_offset]) >> 8;
	}
}

static INLINE unsigned int
do_cry(unsigned char red, unsigned char green, unsigned char blue, int line, int column)
{
	int intensity;
	unsigned int color_offset;		/* offset for cry lookup table */
	unsigned int result;
	unsigned int rcomp,gcomp,bcomp;

	intensity = red;				/* start with red */
	if(green > intensity)
		intensity = green;
	if(blue > intensity)
		intensity = blue;			/* get highest RGB value */
	if(intensity != 0)
	{
		rcomp = (unsigned int)red * 255 / intensity;
		gcomp = (unsigned int)green * 255 / intensity;
		bcomp = (unsigned int)blue * 255 / intensity;
	}
	else
		rcomp = gcomp = bcomp = 0;		/* R, G, B, were all 0 (black) */

	color_offset = (rcomp & 0xF8) << 7;
	color_offset |= (gcomp & 0xF8) << 2;
	color_offset |= (bcomp & 0xF8) >> 3;		/* now we have offset for cry table */

	intensity = intensity & stripbits_mask;
	if (base_intensity > 0) {
		intensity = intensity - base_intensity;
		if (intensity > 0x7f) intensity = 0x7f;
		else if (intensity < -0x7f) intensity = -0x7f;
	}
	result = ((color_offset = cry[color_offset]) << 8) | (intensity & 0x00ff);

	if (varmod_flag) {
		result &= 0xfffe;
	}

	output_word(outhandle, result);

/*
 * if we're supposed to dither the final CRY, convert it back to RGB and use it to find
 * the error
 */
	if (dither_flag) {
		long linelen = image_w;
		Pixel oldcolor, newcolor, *where;

		oldcolor.red = red;
		oldcolor.green = green;
		oldcolor.blue = blue;

		newcolor.red = (intensity*cryred[color_offset]) >> 8;
		newcolor.green = (intensity*crygreen[color_offset]) >> 8;
		newcolor.blue = (intensity*cryblue[color_offset]) >> 8;

		if (column >= 3 && column < linelen - 3 && line < image_h - 1) {
			where = &newdata[line*linelen + column];
			diffuse_error(newcolor, oldcolor, where, linelen);
		}
	}
	return result;
}

static INLINE unsigned int
do_gray(unsigned char red, unsigned char green, unsigned char blue)
{
	double intensity;
	unsigned result;

	intensity = (0.59*green + 0.30*red + 0.11*blue);
	if (intensity < gray_threshold) intensity = 0;
	else if (intensity < contrast_min) intensity = gray_threshold;
	else if (intensity > contrast_max) intensity = 255;
	else intensity = gray_threshold + contrast*(intensity-contrast_min);

	if (intensity < 0) intensity = 0;
	else if (intensity > 255.0) intensity = 255.0;

	if (intensity == 0 && nozero_flag && (green != 0 || red != 0 || blue != 0))
		intensity = 2;

	result = gray_color | (unsigned)intensity;

	if (varmod_flag)
		result &= 0xfffe;

	output_word(outhandle, result);
	return result;
}

static INLINE unsigned int
do_glass(unsigned char red, unsigned char green, unsigned char blue)
{
	double intensity;
	int i;

	intensity = (0.59*green + 0.30*red + 0.11*blue);
	if (intensity < gray_threshold) intensity = 0;
	else if (intensity < contrast_min) intensity = gray_threshold;
	else if (intensity > contrast_max) intensity = 255;
	else intensity = gray_threshold + contrast*(intensity-contrast_min);

	if (intensity < 0) intensity = 0;
	else if (intensity > 255.0) intensity = 255.0;

	i = (intensity - 128.0);

	if (varmod_flag)
		i &= 0xfe;

	output_word(outhandle, gray_color|(i & 0x00ff));
	return intensity;
}

static INLINE void
do_rgb16(unsigned char red, unsigned char green, unsigned char blue)
{
	int temp0;
	temp0 = (red >> 3) << 5;					/* reduce red to 5 bits, shift left 5 bits */
	temp0 += blue >> 3;						/* reduce blue to 5 bits */
	temp0 = temp0 << 6;						/* make room for green */
	temp0 += green >> 2;					/* reduce green to 6 bits */

	if (nozero_flag && temp0 == 0 && (red != 0 || green != 0 || blue != 0))
		temp0 = 1;

	if (varmod_flag)
		temp0 |= 1;

	output_word(outhandle, temp0);
}

static INLINE void
do_rgb24(unsigned char red, unsigned char green, unsigned char blue)
{
	uint32_t temp0;

	temp0 = ((uint32_t)green << 24) | ((uint32_t)red << 16) | blue;
	output_long(outhandle, temp0);
}

static INLINE void
do_msk(unsigned char red, unsigned char green, unsigned char blue)
{
	if((red != 0) || (blue != 0) || (green != 0))
		output_bit(outhandle, 0);			/* 0 if color is not RGB 000 */
	else
		output_bit(outhandle, 1);
}

/*
 * look through a palette, looking for the best match for a palette entry,
 * and then output the 8 bit index
 */
static INLINE void
do_palette(unsigned char red, unsigned char green, unsigned char blue, int line, int column)
{
	int32_t dist, bestdist;
	int bestcolor;
	int i, rdist, bdist, gdist;

	bestdist = 0x7fffffff;
	bestcolor = 0;
	for (i = 0; i < num_colors; i++) {
		rdist = (int)red - (int)palette[i].color.red;
		gdist = (int)green - (int)palette[i].color.green;
		bdist = (int)blue - (int)palette[i].color.blue;
		dist = rdist*(int32_t)rdist+gdist*(int32_t)gdist+bdist*(int32_t)bdist;
		if (dist <= bestdist) {
			bestdist = dist;
			bestcolor = i;
		}
	}

	output_byte(outhandle, bestcolor + base_color);

	/* dither the error, if we're supposed to */
	if (dither_flag) {
		long linelen = image_w;
		Pixel oldcolor, *where;

		oldcolor.red = red;
		oldcolor.green = green;
		oldcolor.blue = blue;

		if (column >= 3 && column < linelen - 3 && line < image_h - 1) {
			where = &newdata[line*linelen + column];
			diffuse_error(palette[bestcolor].color, oldcolor, where, linelen);
		}
	}
}

/*
 * look through a palette, looking for the best match for a palette entry,
 * and then output the 4 bit index
 */
static INLINE void
do_4palette(unsigned char red, unsigned char green, unsigned char blue, int line, int column)
{
	int32_t dist, bestdist;
	int bestcolor;
	int i, rdist, bdist, gdist;

	bestdist = 0x7fffffff;
	bestcolor = 0;
	for (i = 0; i < num_colors; i++) {
		rdist = (int)red - (int)palette[i].color.red;
		gdist = (int)green - (int)palette[i].color.green;
		bdist = (int)blue - (int)palette[i].color.blue;
		dist = rdist*(int32_t)rdist+gdist*(int32_t)gdist+bdist*(int32_t)bdist;
		if (dist <= bestdist) {
			bestdist = dist;
			bestcolor = i;
		}
	}

	output_nybble(outhandle, bestcolor + base_color);

	/* dither the error, if we're supposed to */
	if (dither_flag) {
		long linelen = image_w;
		Pixel oldcolor, *where;

		oldcolor.red = red;
		oldcolor.green = green;
		oldcolor.blue = blue;

		if (column >= 3 && column < linelen - 3 && line < image_h - 1) {
			where = &newdata[line*linelen + column];
			diffuse_error(palette[bestcolor].color, oldcolor, where, linelen);
		}
	}
}

/*
 * look through a palette, looking for the best match for a palette entry,
 * and output the 1 bit index
 */
static INLINE void
do_1palette(unsigned char red, unsigned char green, unsigned char blue, int line, int column)
{
	int32_t dist, bestdist;
	int bestcolor;
	int i, rdist, bdist, gdist;

	bestdist = 0x7fffffff;
	bestcolor = 0;
	for (i = 0; i < num_colors; i++) {
		rdist = (int)red - (int)palette[i].color.red;
		gdist = (int)green - (int)palette[i].color.green;
		bdist = (int)blue - (int)palette[i].color.blue;
		dist = rdist*(int32_t)rdist+gdist*(int32_t)gdist+bdist*(int32_t)bdist;
		if (dist <= bestdist) {
			bestdist = dist;
			bestcolor = i;
		}
	}

	output_bit(outhandle, bestcolor);

	/* dither the error, if we're supposed to */
	if (dither_flag) {
		long linelen = image_w;
		Pixel oldcolor, *where;

		oldcolor.red = red;
		oldcolor.green = green;
		oldcolor.blue = blue;

		if (column >= 3 && column < linelen - 3 && line < image_h - 1) {
			where = &newdata[line*linelen + column];
			diffuse_error(palette[bestcolor].color, oldcolor, where, linelen);
		}
	}
}

static INLINE void
convert_rgb_pixel(unsigned char red,unsigned char green,unsigned char blue,int line,int column)
{
	switch(data_type)
	{
		case CRY16:
			do_cry(red,green,blue,line,column);
			break;
		case GRAY:
			do_gray(red,green,blue);
			break;
		case GLASS:
			do_gray(red,green,blue);
			break;
		case RGB16:
			do_rgb16(red,green,blue);
			break;
		case RGB24:
			do_rgb24(red,green,blue);
			break;
		case MSK:
			do_msk(red,green,blue);
			break;
		case CRY8:
		case RGB8:
			do_palette(red,green,blue,line,column);
			break;
		case CRY4:
		case RGB4:
			do_4palette(red,green,blue,line,column);
			break;
		case CRY1:
		case RGB1:
			do_1palette(red,green,blue,line,column);
			break;
	}
}

/*************************************************************************
wid(image_w): return the blitter bits for a given image width
This is done by a table lookup on "widtab"; each entry in widtab consists
of two longs, the first being the width as an integer, the second being
the corresponding blitter bits.
*************************************************************************/
static uint32_t widtab[] = {
2,	0x00000800,
4,	0x00001000,
6,	0x00001400,
8,	0x00001800,
10,	0x00001A00,
12,	0x00001C00,
14,	0x00001E00,
16,	0x00002000,
20,	0x00002200,
24,	0x00002400,
28,	0x00002600,
32,	0x00002800,
40,	0x00002A00,
48,	0x00002C00,
56,	0x00002E00,
64,	0x00003000,
80,	0x00003200,
96,	0x00003400,
112,	0x00003600,
128,	0x00003800,
160,	0x00003A00,
192,	0x00003C00,
224,	0x00003E00,
256,	0x00004000,
320,	0x00004200,
384,	0x00004400,
448,	0x00004600,
512,	0x00004800,
640,	0x00004A00,
768,	0x00004C00,
896,	0x00004E00,
1024,	0x00005000,
1280,	0x00005200,
1536,	0x00005400,
1792,	0x00005600,
2048,	0x00005800,
2560,	0x00005A00,
3072,	0x00005C00,
3584,	0x00005E00,
0,	0x00000000
};

uint32_t
wid(unsigned int image_w)
{
	uint32_t *ptr;

	ptr = widtab;
	while (*ptr != 0) {
		if (*ptr == (uint32_t)image_w) {
			return ptr[1];
		}
		ptr += 2;		/* skip the image width and blitter bits */
	}
/* if header_flag is set, then this is a fatal error (we can't generate the
 * header); otherwise it should just be a warning
 */
	if (header_flag) {
		fprintf(stderr, "ERROR: Unsupported width (%d)\n", (int)image_w);
		exit(1);
	} else {
		fprintf(stderr, "Warning: %d is not a blittable width\n", (int)image_w);
	}
	return 0;
}

/*************************************************************************
make_newdata(): here's where the actual TGA to CRY conversion takes
place
**************************************************************************/
 
void
make_newdata()
{
	unsigned char red,green,blue;		/* RGB colors for each pixel */
	int line,column;
	long completed;
	long linelen;
	uint32_t blitflags;
	int pixsiz;

	items_per_line = 0;				/* count words per line in new file */

	if (rescale_w && rescale_h) {			/* we should resize the picture */
		if ( !quiet_flag )
			printf("Resizing image to %d x %d...\n", rescale_w, rescale_h);
		newdata = rescale(srcfile, image_w, image_h, rescale_w, rescale_h, filter_type, aspect_flag);
		if (!newdata) {
			fprintf(stderr, "ERROR: Unable to allocate memory to resize picture\n");
		}
		image_w = rescale_w;
		image_h = rescale_h;
	} else {
		newdata = srcfile;
	}

/*
 * if max_colors is nonzero, we must palettize the image
 */
	if (max_colors != 0) {
		if (!quiet_flag)
			printf("Constructing palette for image...\n");
		num_colors = build_palette(max_colors, palette, newdata, (long)image_w * (long)image_h);
		if (data_type == CRY8 || data_type == CRY4 || data_type == CRY1) {
			cryize_palette();
		} else {
			rgbize_palette();
		}
	}

/* by always calculating the blitter flags, we always check for legal
 * widths in the "wid" function...
 */
	if (data_type == RGB24) {
		pixsiz = 32;
		blitflags = 0x00030028u|wid(image_w);		/* PITCH1|PIXEL32|XADDINC|WIDxxx */
	} else if (data_type == MSK || data_type == CRY1 || data_type == RGB1 ) {
		pixsiz = 1;
		blitflags = 0x00030000u|wid(image_w);		/* PITCH1|PIXEL1|XADDINC|WIDxxx */
	} else if (data_type == CRY8 || data_type == RGB8) {
		pixsiz = 8;
		blitflags = 0x00030018u|wid(image_w);		/* PITCH1|PIXEL8|XADDINC|WIDxxx */
	} else if (data_type == CRY4 || data_type == RGB4) {
		pixsiz = 4;
		blitflags = 0x00030010u|wid(image_w);		/* PITCH1|PIXEL4|XADDINC|WIDxxx */
	} else {
		pixsiz = 16;
		blitflags = 0x00030020u|wid(image_w);		/* PITCH1|PIXEL16|XADDINC|WIDxxx */
	}

	if (header_flag) {
	/* do a fancy header */
		if (binary_flag) {
			binary_file_size = 0;
			output_word(outhandle, image_w);
			output_word(outhandle, image_h);
			output_long(outhandle, blitflags);
		} else {
			fprintf(outhandle, "\t.globl\t%s\n",picname);
			if (!nodata_flag)
				fprintf(outhandle, "\t.data\n");
			fprintf(outhandle, "\t.phrase\n");
			fprintf(outhandle, "%s:\n", picname);
			fprintf(outhandle, "\tdc.w\t%d,%d\n",image_w,image_h);
			fprintf(outhandle, "\tdc.l\t$%08" PRIX32 "\t;(PITCH1|PIXEL%d|WID%d|XADDINC)\n", blitflags, pixsiz, image_w);
		}
	} else {
	/* do a plain header */
		if (binary_flag) {
			binary_file_size = 0;
		} else {
			fprintf(outhandle, "\t.globl\t%s\n",picname);
			if (!nodata_flag)
				fprintf(outhandle, "\t.data\n");
			fprintf(outhandle, "\t.phrase\n");
			fprintf(outhandle, "%s:\n", picname);
			fprintf(outhandle, ";%d x %d\n",image_w,image_h);
		}
	}

	linelen = image_w;

	for(line = 0; line < image_h; line++)
	{
		for(column = 0; column < image_w; column++)
		{
			blue = newdata[line * linelen + column].blue;
			green = newdata[line * linelen + column].green;
			red = newdata[line * linelen + column].red;
			convert_rgb_pixel(red,green,blue,line,column);
		}
		completed = (image_h - line) * 100L / image_h;
		draw_percentage(100-completed);
	}

	draw_percentage(101);		/* mark the end of the progress report */

/* sync to a word boundary */
	output_sync(outhandle);

/* now output the palette, if there is one */
	if (max_colors != 0) {
		if (!binary_flag) {
			fprintf(outhandle,"\n;palette data: number of colors, then the palette entries\n");
		}
		output_word(outhandle, num_colors);
		for (line = 0; line < num_colors; line++) {
			output_word(outhandle, palette[line].outval);
		}
	}

/* round binary file size off to a phrase boundary */
	if (binary_flag) {
		binary_file_size &= 0x7;
		if (binary_file_size != 0) {
			while (binary_file_size != 8) {
				fputc(0, outhandle);
				++binary_file_size;
			}
		}
	}
}

