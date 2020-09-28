#include <stdio.h>
#include <stdlib.h>

typedef unsigned short UWORD;
typedef unsigned char UBYTE;
typedef short WORD;
typedef char BYTE;

UWORD do_cry(int, int, int);
extern UBYTE cry[];

int
main(argc, argv)
WORD	argc;
BYTE	*argv[];
{
	if (argc == 4)
	{
		int red, green, blue;

		red = atoi(argv[1]);
		green = atoi(argv[2]);
		blue = atoi(argv[3]);

		printf("%d %d %d RGB is:\t 0x%04x CRY\n", red, green, blue, do_cry(red, green, blue));
		return(0);
	}
	else
	{
		printf("rgb2cry <red value> <green value> <blue value>\n");
		printf("Transform a RGB value to a CRY value.\n");
		return 1;
	}
}



UWORD do_cry(int red, int green, int blue)
{
	UWORD	intensity;
	UWORD	color_offset;
	UWORD	q;

	intensity = red;				/* start with red */
	if(green > intensity)
		intensity = green;
	if(blue > intensity)
		intensity = blue;			/* get highest RGB value */
	if(intensity != 0)
	{
		red = (unsigned int)red * 255 / intensity;
		green = (unsigned int)green * 255 / intensity;
		blue = (unsigned int)blue * 255 / intensity;
	}
	else
		red = green = blue = 0;		/* R, G, B, were all 0 (black) */

	color_offset = (red & 0xF8) << 7;
	color_offset += (green & 0xF8) << 2;
	color_offset += (blue & 0xF8) >> 3;

	q = (UWORD)(((UWORD)cry[color_offset] << 8) | (UBYTE) intensity);
	return q;
}

