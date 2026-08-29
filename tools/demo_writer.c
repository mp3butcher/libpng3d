/* demo_writer.c
 * Minimal example to write a tiny PNG and embed a pvs3 chunk using the
 * prototype API.  Build and run from the repo root after building libpng.
 */

#include <stdio.h>
#include <stdlib.h>
#include "png.h"
#include "png3d.h"
#include "png3d_filter.h"

/* Parameters for the synthetic volume */
#define NX 512
#define NY 512
#define NZ 512
#define BLOCK 8   /* block size in each axis (8x8x8) */

/* Value function: gives per-block value; ensures adjacent blocks have nearby values.
 * We choose a simple ramp-like function across block coordinates.
 */
static inline unsigned char block_value(int bx, int by, int bz)
{
    /* scale coordinates to produce variations but keep nearby values */
    int v = (3*bx + 5*by + 7*bz) & 0xFF;
    return (unsigned char)v;
}

/* Helper to write one file either with or without 3D filtering.
 * filename: output filename
 * use_3d:   non-zero -> enable 3D PAETH3 filtering (pvs3.predictor = 1)
 */
int write_volume_png(const char *filename, int use_3d)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) { perror("fopen"); return 1; }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); fprintf(stderr, "png_create_write_struct failed\n"); return 1; }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_write_struct(&png_ptr, NULL); fclose(fp); fprintf(stderr, "png_create_info_struct failed\n"); return 1; }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        fprintf(stderr, "libpng error\n");
        return 1;
    }

    png_init_io(png_ptr, fp);

    /* PNG dimensions:
     * width  = NX
     * height = NY * NZ (we emit rows for each (z,y))
     */
    png_uint_32 img_width = NX;
    png_uint_32 img_height = (png_uint_32)NY * (png_uint_32)NZ;

    /* Grayscale 8-bit */
    png_set_IHDR(png_ptr, info_ptr, img_width, img_height, 8,
                 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    /* Prepare pvs3 metadata */
    png3d_pvs3_t pvs3;
    memset(&pvs3, 0, sizeof(pvs3));
    pvs3.nx = NX;
    pvs3.ny = NY;
    pvs3.nz = NZ;
    /* rowsPerCell: how many PNG rows form one logical grid row.
     * For our mapping, each z layer contains NY rows, so use rowsPerCell = NY.
     */
    pvs3.rowsPerCell = NY;
    /* cellBytes not used by filtering logic below, set reasonably:
     * bytes per row (width * channels) = NX * 1
     */
    pvs3.cellBytes = NX;
    pvs3.mapping = 0; /* slice-major, row-major within slice (as used elsewhere) */
    pvs3.predictor = use_3d ? 1 : 0; /* 1 = PAETH3, 0 = XOR/no 3D */
    pvs3.version = 1;
    pvs3.reserved = 0;

    /* Write pvs3 ancillary chunk immediately (caller should do this before IDATs) */
    png_write_pvs3_chunk(png_ptr, info_ptr, &pvs3);

    /* Prepare row buffer and row_info */
    size_t rowbytes = (size_t)NX; /* 1 channel * 1 byte per sample */
    png_bytep row = (png_bytep)malloc(rowbytes);
    if (!row) { fprintf(stderr, "malloc row failed\n"); png_destroy_write_struct(&png_ptr, &info_ptr); fclose(fp); return 1; }

    png_row_info row_info;
    row_info.width = NX;
    row_info.rowbytes = (png_size_t)rowbytes;
    row_info.color_type = PNG_COLOR_TYPE_GRAY;
    row_info.bit_depth = 8;
    row_info.channels = 1;
    row_info.pixel_depth = 8; /* channels * bit_depth */

    /* If using 3D filtering, create a write-state */
    png3d_write_state_t *ws = NULL;
    if (use_3d) {
        ws = png3d_write_state_new_public(png_ptr, &pvs3, rowbytes);
        if (!ws) {
            fprintf(stderr, "Failed to create 3D write state\n");
            free(row);
            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
            return 1;
        }
    }

    /* Generate and write rows. Loop order matches README: for each z, for each y */
    /* We write image rows in order: z=0..NZ-1, y=0..NY-1  (height = NZ*NY) */
    for (int z = 0; z < NZ; ++z) {
        for (int y = 0; y < NY; ++y) {
            /* Fill row bytes by block logic */
            int by = y / BLOCK;
            int bz = z / BLOCK;
            for (int x = 0; x < NX; ++x) {
                int bx = x / BLOCK;
                unsigned char v = block_value(bx, by, bz);
                row[x] = v;
            }

            /* Apply 3D filter if requested (this mutates row inplace) */
            if (use_3d) {
                png3d_filter_write_row_public(png_ptr, &row_info, row, ws);
            }

            png_write_row(png_ptr, row);
        }

        /* Optional: progress output per z layer */
        if ((z & 0x3F) == 0) { /* every 64 layers */
            fprintf(stderr, "%s: z=%d/%d\n", filename, z, NZ);
        }
    }

    /* Cleanup */
    if (ws)
        png3d_write_state_free_public(png_ptr, ws);

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(row);
    fclose(fp);

    return 0;
}

int main(void)
{
    printf("Writing volume (this may take a while) ...\n");

    /* Write without 3D filtering */
    if (write_volume_png("pvs3_no3d.png", 0) != 0) {
        fprintf(stderr, "Failed to write pvs3_no3d.png\n");
        return 1;
    }

    /* Write with 3D PAETH3 filtering */
    if (write_volume_png("pvs3_paeth3.png", 1) != 0) {
        fprintf(stderr, "Failed to write pvs3_paeth3.png\n");
        return 1;
    }

    printf("Done. Outputs: pvs3_no3d.png, pvs3_paeth3.png\n");
    return 0;
}
