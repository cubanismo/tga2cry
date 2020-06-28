/*
 * code for building a palette from a 24 bit RGB picture
 * Inputs:
 * max_colors	== maximum number of colors allowed in the palette
 * palette	== table of palette entries (at least max_colors long)
 * pix		== pointer to pixel data
 * numpixels	== number of pixels
 *
 * Output:
 * number of colors actually used in the palette
 * *palette is updated to contain the palette netries
 *
 * The algorithm used is the extremely simple "most popular colors"
 * algorithm, which just picks the N most often used colors
 * in the picture. To find those colors, we accumulate a histogram
 * of how often each color is used. In order to keep the table within
 * a reasonable size, only 5 bits each of red, green, and blue are
 * used.
 */

#include <stdio.h>
#include <stdlib.h>
#include "tgadefs.h"
#include "tgaproto.h"

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
static void
count_colors(Pixel *pix, long numpixels)
{
	int index;

	while (numpixels) {
		index = HASH(*pix);
		color_count[index]++; 
		pix++;
		numpixels--;
	}
}

/*
 * find the index of the most popular color
 * returns -1 if no color occurs more than 0 times
 */
static INLINE int
most_popular_color(void)
{
	long popcount;
	long popidx;
	int i;

	popidx = -1;
	popcount = 0;

	for (i = 0; i < 32768; i++) {
		if (color_count[i] > popcount) {
			popcount = color_count[i];
			popidx = i;
		}
	}
	return popidx;
}

int
build_palette(int max_colors, Palette_Entry *palette, Pixel *pix, long numpixels)
{
	int i;
	int colidx;

	/* find how often various colors occur */
	count_colors(pix, numpixels);

	/* now find the "max_colors" most frequently occuring colors */
	for (i = 0; i < max_colors; i++) {
		colidx = most_popular_color();
		if (colidx < 0) {		/* no more colors left in picture */
			return i;
		}
		palette[i].color = UNHASH(colidx);
		color_count[colidx] = 0;	/* remove that color from consideration */
	}
	if (most_popular_color() >= 0)
		fprintf(stderr, "Warning: more than %d colors in image\n", max_colors);
	return max_colors;
}
