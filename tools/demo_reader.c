
/* demo_reader_3d.c
 *
 * Read a PNG produced by the demo writer that may include a pvs3 chunk and
 * 3D PAETH3/XOR filtering. Apply 3D unfiltering and validate the recovered
 * bytes against the synthetic pattern.
 *
 * Build with libpng:
 *   cc -O2 -o demo_reader_3d demo_reader_3d.c -lpng
 *
 * The program expects the png3d filter functions to be available (png3d_filter.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "png.h"
#include "png3d.h"
#include "png3d_filter.h" /* for png3d_unfilter_row_paeth3 / xor */

#define BLOCK 8  /* must match the writer block size used to generate the image */

/* Synthetic value function used by writer: must match writer's block_value() */
static inline unsigned char block_value(int bx, int by, int bz)
{
    int v = (3*bx + 5*by + 7*bz) & 0xFF;
    return (unsigned char)v;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.png>\n", argv[0]);
        return 1;
    }

    const char *infile = argv[1];
    FILE *fp = fopen(infile, "rb");
    if (!fp) { perror("fopen"); return 1; }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); fprintf(stderr, "png_create_read_struct failed\n"); return 1; }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_read_struct(&png_ptr, NULL, NULL); fclose(fp); fprintf(stderr, "png_create_info_struct failed\n"); return 1; }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        fprintf(stderr, "libpng fatal error\n");
        return 1;
    }

    png_init_io(png_ptr, fp);

    /* Keep unknown chunks so the pvs3 chunk is stored in info_ptr */
#ifdef PNG_HANDLE_CHUNK_ALWAYS
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, (png_byte*)"pvs3", 1);
#endif

    png_read_info(png_ptr, info_ptr);

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_method;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);

    png_uint_32 channels = png_get_channels(png_ptr, info_ptr);
    png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    printf("Input: %s\n", infile);
    printf("  PNG: %u x %u, bit_depth=%d, color_type=%d, channels=%u, rowbytes=%zu\n",
           (unsigned)width, (unsigned)height, bit_depth, color_type, (unsigned)channels, (size_t)rowbytes);

    /* Extract pvs3 chunk */
    png3d_pvs3_t pvs3;
    int has_pvs3 = png_get_pvs3_from_info(png_ptr, info_ptr, &pvs3);
    if (!has_pvs3) {
        fprintf(stderr, "Warning: pvs3 chunk not found. Will assume no 3D filtering.\n");
        memset(&pvs3, 0, sizeof(pvs3));
        pvs3.predictor = 0;
        /* rowsPerCell default sensible value = height (treat whole image as single z-layer) */
        pvs3.rowsPerCell = height;
        pvs3.nx = width;
        pvs3.ny = height;
        pvs3.nz = 1;
    } else {
        printf("  Found pvs3: nx=%u ny=%u nz=%u rowsPerCell=%u cellBytes=%u mapping=%u predictor=%u\n",
               (unsigned)pvs3.nx, (unsigned)pvs3.ny, (unsigned)pvs3.nz,
               (unsigned)pvs3.rowsPerCell, (unsigned)pvs3.cellBytes,
               (unsigned)pvs3.mapping, (unsigned)pvs3.predictor);
    }

    /* Validate that rowbytes equals expected row size; otherwise adapt. */
    if ((png_uint_32)rowbytes != pvs3.nx * channels * ((bit_depth + 7)/8) && pvs3.nx != 0) {
        fprintf(stderr, "Warning: rowbytes (%zu) doesn't match pvs3.nx*channels*(bitdepth/8) (%u). Continuing anyway.\n",
                (size_t)rowbytes, pvs3.nx * channels * ((bit_depth+7)/8));
    }

    /* Prepare z_buffers: allocate rowsPerCell pointers, each rowbytes in size */
    png_uint_32 rowsPerCell = pvs3.rowsPerCell;
    if (rowsPerCell == 0) rowsPerCell = 1;
    png_byte **z_buffers = (png_byte **)malloc(sizeof(png_byte *) * rowsPerCell);
    if (!z_buffers) { fprintf(stderr, "malloc z_buffers failed\n"); png_destroy_read_struct(&png_ptr, &info_ptr, NULL); fclose(fp); return 1; }
    for (png_uint_32 i = 0; i < rowsPerCell; ++i) {
        z_buffers[i] = (png_byte *)malloc(rowbytes);
        if (!z_buffers[i]) {
            fprintf(stderr, "malloc z_buffer[%u] failed\n", (unsigned)i);
            for (png_uint_32 j = 0; j < i; ++j) free(z_buffers[j]);
            free(z_buffers);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return 1;
        }
        /* initialize to zero */
        memset(z_buffers[i], 0, rowbytes);
    }

    png_bytep row = (png_bytep)malloc(rowbytes);
    if (!row) { fprintf(stderr, "malloc row failed\n"); goto cleanup; }

    /* Prepare png_row_info struct as expected by the 3D unfilter functions */
    png_row_info row_info;
    row_info.width = width;
    row_info.rowbytes = (png_size_t)rowbytes;
    row_info.color_type = color_type;
    row_info.bit_depth = bit_depth;
    row_info.channels = (int)channels;
    row_info.pixel_depth = (int)(bit_depth * channels);

    /* iterate rows in the same order as writer: z major (z loops outer, y loops inner)
     * The writer produced height = NY * NZ rows, with row index r = z*NY + y.
     * We'll treat the input height similarly.
     */
    uint64_t total_rows = height;
    png_uint_32 current_z = 0;
    png_uint_32 current_row_in_z = 0;
    uint64_t mismatches = 0;
    uint64_t total_voxels = 0;

    for (uint64_t r = 0; r < total_rows; ++r) {
        png_read_row(png_ptr, row, NULL); /* libpng already removed PNG row filters */

        /* determine prev_row and prev_z_row pointers expected by unfilter functions */
        png_byte *prev_row = NULL;
        const png_byte *prev_z_row = NULL;
        if (current_row_in_z > 0)
            prev_row = z_buffers[current_row_in_z - 1];
        if (current_z > 0)
            prev_z_row = z_buffers[current_row_in_z];

        /* apply 3D unfilter according to predictor */
        if (pvs3.predictor == 1) {
            png3d_unfilter_row_paeth3(png_ptr, &row_info, row, prev_row, prev_z_row);
        } else {
            png3d_unfilter_row_xor(png_ptr, &row_info, row, prev_row, prev_z_row);
        }

        /* After unfilter, row contains recovered original bytes. Save into z_buffers[current_row_in_z]. */
        memcpy(z_buffers[current_row_in_z], row, rowbytes);

        /* Validate against synthetic block pattern (only for grayscale single-channel images).
         * If image has multiple channels, we check the first channel of each pixel.
         */
        /* compute z and y for synthetic mapping:
         * writer used z-major stacking with height = NY * NZ, and rowsPerCell should equal NY in writer usage.
         * So:
         *   z = r / NY
         *   y = r % NY
         * Then for each x compute bx = x/BLOCK, by = y/BLOCK, bz = z/BLOCK and expected byte.
         */
        png_uint_32 NY = pvs3.ny;
        png_uint_32 NX = pvs3.nx;
        png_uint_32 NZ = pvs3.nz;
        png_uint_32 z = 0, y = 0;
        if (NY > 0) {
            z = (png_uint_32)(r / (uint64_t)NY);
            y = (png_uint_32)(r % (uint64_t)NY);
        } else {
            /* fallback: assume single layer */
            z = 0; y = (png_uint_32)r;
        }

        /* Only validate when the image is single-channel 8-bit grayscale, which the demo uses.
         * For multi-channel or other bit depths, validation would be adapted.
         */
        if (bit_depth == 8 && channels == 1) {
            for (png_uint_32 x = 0; x < NX; ++x) {
                unsigned char got = row[x];
                unsigned char expect = block_value(x / BLOCK, y / BLOCK, z / BLOCK);
                if (got != expect) {
                    if (mismatches < 10) {
                        fprintf(stderr, "Mismatch at r=%" PRIu64 " (z=%u,y=%u), x=%u : got=%u expected=%u\n",
                                r, z, y, x, (unsigned)got, (unsigned)expect);
                    }
                    mismatches++;
                }
                total_voxels++;
            }
        } else {
            /* If not matchable, skip validation but still count bytes */
            total_voxels += rowbytes;
        }

        /* advance indices */
        current_row_in_z++;
        if (current_row_in_z >= rowsPerCell) {
            current_row_in_z = 0;
            current_z++;
        }
    }

    printf("Done reading. total voxels checked: %" PRIu64 ", mismatches: %" PRIu64 "\n", total_voxels, mismatches);
    if (mismatches == 0) {
        printf("3D unfiltering OK: recovered data matches synthetic pattern.\n");
    } else {
        printf("Recovered data has mismatches; unfiltering or mapping may be incorrect.\n");
    }

    /* cleanup */
    free(row);
cleanup:
    for (png_uint_32 i = 0; i < rowsPerCell; ++i) {
        if (z_buffers[i]) free(z_buffers[i]);
    }
    free(z_buffers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);
    return 0;
}
