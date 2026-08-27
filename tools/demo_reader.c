/* demo_reader.c
 * Reads a PNG and attempts to extract the pvs3 chunk using png3d helper.
 */
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png3d.h"

int main(int argc, char** argv){
    const char* in = "pvs3_demo.png";
    FILE* fp = fopen(in, "rb");
    if (!fp) { perror("fopen"); return 1; }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); return 1; }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_read_struct(&png_ptr, NULL, NULL); fclose(fp); return 1; }
    if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_read_struct(&png_ptr, &info_ptr, NULL); fclose(fp); return 1; }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    png_3d_filterp p;
    memset(&p,0,sizeof(p));
    png_read_pvs3_chunk(png_ptr, info_ptr, &p);
    printf("Read pvs3: nx=%u ny=%u nz=%u rowsPerCell=%u cellBytes=%u mapping=%u predictor=%u version=%u\n",
           p.nx,p.ny,p.nz,p.rowsPerCell,p.cellBytes,(unsigned)p.mapping,(unsigned)p.predictor,(unsigned)p.version);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);
    return 0;
}
