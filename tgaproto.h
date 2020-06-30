#if defined(__STDC__) || defined(__cplusplus)
# define P_(s) s
#else
# define P_(s) ()
#endif


/* tga2cry.c */
void usage P_((char *));
int main P_((int argc, char **argv));
char *change_extension P_((char *name, char *ext));
char *strip_extension P_((char *name));
int do_file P_((char *infile, char *outfile));
void err_eof P_((void));
void read_row P_((FILE *fhandle, Pixel *place));
void read_file P_((char *infile));
void output_word P_((FILE *f, uint16_t w));
void output_long P_((FILE *f, uint32_t w));
void output_bit P_((FILE *f, int b));
uint32_t wid P_((unsigned int image_w));
void make_newdata P_((void));

/* filter.c */
Image *new_image P_((int xsize, int ysize));
void free_image P_((Image *image));
Pixel get_pixel P_((Image *image, int x, int y));
void get_row P_((Pixel *row, Image *image, int y));
void get_column P_((Pixel *column, Image *image, int x));
void put_pixel P_((Image *image, int x, int y, Pixel data));
double filter P_((double t));
double box_filter P_((double t));
double triangle_filter P_((double t));
double bell_filter P_((double t));
double B_spline_filter P_((double t));
double sinc P_((double x));
double Lanczos3_filter P_((double t));
double Mitchell_filter P_((double t));
void zoom P_((Image *dst, Image *src, double (*filterf )(), double fwidth));
Pixel *rescale P_((Pixel *oldpix, unsigned old_w, unsigned old_h, unsigned new_w, unsigned new_h, int filter_type, int aspect));
Pixel *crop P_((Pixel *oldpix, unsigned old_w, unsigned old_h, unsigned new_x, unsigned new_y, unsigned new_w, unsigned new_h));

/* palette.c */
int build_palette P_((int max_colors, Palette_Entry *palette, Pixel *pix, long numpixels));

#undef P_
