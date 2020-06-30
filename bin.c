#include <stdio.h>

extern unsigned char cry[];

static unsigned int
do_cry(unsigned char red, unsigned char green, unsigned char blue)
{
	unsigned int intensity;
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

	result = ((color_offset = cry[color_offset]) << 8) | intensity;

	return result;
}

int
main()
{
	uint32_t i;
	unsigned char red, green, blue;
	unsigned int output;
	FILE *f;

	f = fopen("cry.bin", "wb");
	for (i = 0; i < 2*32768; i++) {
		red = ((i>>11) & 0x1f) << 3;
		blue = ((i>>6) & 0x1f) << 3;
		green = (i & 0x3f) << 2;
		output = do_cry(red, green, blue);
		fputc((output >> 8), f);
		fputc((output & 0xff), f);
	}
	fclose(f);
	return 0;
}
