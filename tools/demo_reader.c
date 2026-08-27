/* demo_reader.c
 * Read the pvs3 chunk from a PNG written with demo_writer.  Use
 * png_set_keep_unknown_chunks before png_read_info so that the unknown chunk
 * is preserved in the info structure for inspection.
 */

#include <stdio.h>
#include <stdlib.h>
#include "png.h"
#include "png3d.h"

int main(void)
{
    const char *inname = "pvs3_demo.png";
    FILE *fp = fopen(inname, "rb");
    if (!fp) { perror("fopen"); return 1; }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); return 1; }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_read_struct(&png_ptr, NULL, NULL); fclose(fp); return 1; }

    if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_read_struct(&png_ptr, &info_ptr, NULL); fclose(fp); return 1; }

    /* Ask libpng to keep our custom chunk when reading */
#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, (png_byte*)"pvs3", 1);
#endif

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    png3d_pvs3_t pvs3;
    if (png_get_pvs3_from_info(png_ptr, info_ptr, &pvs3))
    {
        printf("Found pvs3: nx=%u ny=%u nz=%u rowsPerCell=%u cellBytes=%u mapping=%u predictor=%u version=%u\n",
               (unsigned)pvs3.nx, (unsigned)pvs3.ny, (unsigned)pvs3.nz,
               (unsigned)pvs3.rowsPerCell, (unsigned)pvs3.cellBytes,
               (unsigned)pvs3.mapping, (unsigned)pvs3.predictor, (unsigned)pvs3.version);
    }
    else
    {
        printf("No pvs3 chunk found\n");
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);
    return 0;
}
