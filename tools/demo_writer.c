/* demo_writer.c
 * Minimal example to write a tiny PNG and embed a pvs3 chunk using the
 * prototype API.  Build and run from the repo root after building libpng.
 */

#include <stdio.h>
#include <stdlib.h>
#include "png.h"
#include "png3d.h"

int main(void)
{
    const char *outname = "pvs3_demo.png";
    FILE *fp = fopen(outname, "wb");
    if (!fp) { perror("fopen"); return 1; }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); return 1; }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_write_struct(&png_ptr, NULL); fclose(fp); return 1; }

    if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_write_struct(&png_ptr, &info_ptr); fclose(fp); return 1; }

    png_init_io(png_ptr, fp);

    /* Tiny 2x2 RGB image */
    png_set_IHDR(png_ptr, info_ptr, 2, 2, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    /* Prepare a tiny pvs3 description */
    png3d_pvs3_t pvs3;
    pvs3.nx = 16; pvs3.ny = 16; pvs3.nz = 8;
    pvs3.rowsPerCell = 2; pvs3.cellBytes = 32;
    pvs3.mapping = 0; pvs3.predictor = 1; pvs3.version = 1; pvs3.reserved = 0;

    png_write_pvs3_chunk(png_ptr, info_ptr, &pvs3);

    /* Write four RGB rows (each row is width * 3 bytes) */
    png_bytep row = (png_bytep)malloc(2 * 3);
    if (!row) { png_destroy_write_struct(&png_ptr, &info_ptr); fclose(fp); return 1; }

    /* Row 0: red, green */
    row[0]=255; row[1]=0; row[2]=0; row[3]=0; row[4]=255; row[5]=0;
    png_write_row(png_ptr, row);
    /* Row 1: blue, white */
    row[0]=0; row[1]=0; row[2]=255; row[3]=255; row[4]=255; row[5]=255;
    png_write_row(png_ptr, row);

    free(row);

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    printf("Wrote %s with embedded pvs3 chunk\n", outname);
    return 0;
}
