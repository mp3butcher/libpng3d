/* demo_writer.c
 * Create a synthetic PVS-like dataset (few cells) and write a PNG embedding pvs3 chunk using png3d API.
 * This demo does not implement the full 3D filter native integration, but demonstrates chunk writing and usage.
 */

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png3d.h"

int main(int argc, char** argv) {
    const char* out = "pvs3_demo.png";
    int width = 16;
    int height = 4;

    FILE* fp = fopen(out, "wb");
    if (!fp) { perror("fopen"); return 1; }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); return 1; }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_write_struct(&png_ptr, (png_infopp)NULL); fclose(fp); return 1; }
    if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_write_struct(&png_ptr, &info_ptr); fclose(fp); return 1; }

    png_init_io(png_ptr, fp);

    /* Write simple 8-bit grayscale image */
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_GRAY,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    /* Prepare example 3D parameters */
    png_3d_filterp p;
    p.nx = 2; p.ny = 1; p.nz = 2; p.rowsPerCell = 1; p.cellBytes = 2; p.mapping = PNG_3D_MAPPING_ROW_MAJOR; p.predictor = PNG_3D_PREDICTOR_PAETH3; p.version = 1; p.reserved = 0;

    /* embed pvs3 chunk */
    png_write_info(png_ptr, info_ptr);
    png_write_pvs3_chunk(png_ptr, &p);

    /* image data: synthetic bytes */
    png_bytep row = (png_bytep) malloc(width);
    for (int y=0;y<height;y++){
        for (int x=0;x<width;x++) row[x] = (unsigned char)((x + y*3) & 0xFF);
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(row);
    fclose(fp);
    printf("Wrote %s (demo)\n", out);
    return 0;
}
