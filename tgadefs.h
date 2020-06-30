#include <stdint.h>

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} Pixel;

typedef struct {
	Pixel	color;			/* the color of this palette entry */
	uint16_t outval;		/* the converted (RGB or CRY) value */
} Palette_Entry;

typedef struct {
	int	xsize;		/* horizontal size of the image in Pixels */
	int	ysize;		/* vertical size of the image in Pixels */
	Pixel *	data;		/* pointer to first scanline of image */
	long	span;		/* Pixel offset between two scanlines */
} Image;

/* constants for filter_type */
#define FILTER_BOX	0
#define FILTER_BELL	1
#define FILTER_LANC	2
#define FILTER_MITCH	3
#define FILTER_SINC	4
#define FILTER_TRI	5

#ifdef __GNUC__
#define INLINE __inline__
#else
#define INLINE
#endif
