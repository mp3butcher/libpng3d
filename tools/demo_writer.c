/* demo_writer.c
 * Minimal example to write a tiny PNG and embed a pvs3 chunk using the
 * prototype API.  Build and run from the repo root after building libpng.
 */

#include <stdio.h>
#include <stdlib.h>
#include "png.h"
#include "png3d.h"
#include "png3d_filter.h"
#include <inttypes.h>
#include <math.h>
#include <sys/stat.h>
/* Parameters */
#define NX 256
#define NY 256
#define NZ 256
#define CELLBYTES 1024/8
#define BLOCK 8

static inline unsigned char block_value(int bx, int by, int bz)
{
    int v = (3*bx + 5*by + 7*bz) & 0xFF;
    return (unsigned char)v;
}

static double compute_entropy_bits_per_byte(const uint64_t hist[256], uint64_t total)
{
    if (total == 0) return 0.0;
    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (hist[i] == 0) continue;
        double p = (double)hist[i] / (double)total;
        entropy -= p * (log(p) / log(2.0));
    }
    return entropy;
}

static int64_t get_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

/* Writes volume and fills histograms.
 * filename: output filename
 * use_3d: non-zero -> apply 3D PAETH3 before png_write_row()
 * comp_level: 0..9 compression level to pass to png_set_compression_level
 * hist_orig and hist_filt must be arrays of 256 uint64_t, will be filled.
 */
int write_volume_png(const char *filename, int use_3d, int comp_level,
                     uint64_t hist_orig[256], uint64_t hist_filt[256])
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
        fprintf(stderr, "libpng fatal error\n");
        return 1;
    }

    png_init_io(png_ptr, fp);

    /* Set compression level before writing IDATs */
    png_set_compression_level(png_ptr, comp_level);
    /* Use builtin PNG filters when not using the custom 3D filter.
     * If use_3d == 0 (pvs3_no3d.png) let libpng try all filters to pick
     * the best per-scanline filter. If use_3d != 0 we disable libpng
     * filtering so the custom 3D filter output is written verbatim. */
    if (!use_3d) {
        png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_ALL_FILTERS);
    } else {
        png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_FILTER_NONE);
    }
    /* Dimensions: width = NX, height = NY * NZ (z-major stacking) */
    png_uint_32 img_width = NX;
    png_uint_32 img_height = (png_uint_32)NY * (png_uint_32)NZ;

    png_set_IHDR(png_ptr, info_ptr, img_width, img_height, 8,
                 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    /* Prepare pvs3 metadata */
    png3d_pvs3_t pvs3;
    memset(&pvs3, 0, sizeof(pvs3));
    pvs3.nx = NX; pvs3.ny = NY; pvs3.nz = NZ;
    pvs3.rowsPerCell = NY;
    pvs3.cellBytes = CELLBYTES;
    pvs3.mapping = 0;
    pvs3.predictor = use_3d ? 1 : 0;
    pvs3.version = 1;
    pvs3.reserved = 0;
    png_write_pvs3_chunk(png_ptr, info_ptr, &pvs3);

    size_t rowbytes = (size_t)NX*pvs3.cellBytes;
    png_bytep row = (png_bytep)malloc(rowbytes);
    if (!row) { fprintf(stderr, "malloc row failed\n"); png_destroy_write_struct(&png_ptr, &info_ptr); fclose(fp); return 1; }

    png_row_info row_info;
    row_info.width = NX;
    row_info.rowbytes = (png_size_t)rowbytes;
    row_info.color_type = PNG_COLOR_TYPE_GRAY;
    row_info.bit_depth = 8;
    row_info.channels = 1;
    row_info.pixel_depth = 8;

    memset(hist_orig, 0, sizeof(uint64_t) * 256);
    memset(hist_filt, 0, sizeof(uint64_t) * 256);

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

    uint64_t total_bytes = 0;
    for (int z = 0; z < NZ; ++z) {
        for (int y = 0; y < NY; ++y) {
            int by = y / BLOCK;
            int bz = z / BLOCK;
            /* Fill the row with NX cells. Each cell is a fixed-size bitset             * occupying pvs3.cellBytes bytes. We generate a deterministic
             * bit pattern per-cell based on the block indices so the demo
             * data is non-trivial and repeatable. Pack little-endian. */
            for (int x = 0; x < NX; ++x) {
                    int bx = x / BLOCK;
                    /* Construct a deterministic bitset mask for this cell.
                     * mask bit k is set if (3*bx + 5*by + 7*bz + k) is odd. */
                    uint64_t mask = 0;
                    int bits = (int)pvs3.cellBytes * 8;
                    for (int k = 0; k < bits && k < 64; ++k) {
                        if (((3*bx + 5*by + 7*bz + k) & 1) != 0)
                            mask |= (1ULL << k);
                    }
    
                    /* Write the mask into the row buffer (little-endian per cell) */
                    size_t base = (size_t)x * (size_t)pvs3.cellBytes;
                    for (size_t b = 0; b < (size_t)pvs3.cellBytes; ++b) {
                        row[base + b] = (png_byte)((mask >> (8 * b)) & 0xFF);
                    }
                }

            /* Update original histogram (before any 3D filtering) */
            for (size_t k = 0; k < rowbytes; ++k) {
                hist_orig[(unsigned char)row[k]]++;
            }

            /* If using 3D filter, apply it (this mutates row). Then update hist_filt with filtered bytes. */
            if (use_3d) {
                png3d_filter_write_row_public(png_ptr, &row_info, row, ws);
                for (size_t k = 0; k < rowbytes; ++k) {
                    hist_filt[(unsigned char)row[k]]++;
                }
            } else {
                /* No 3D filtering: just count filtered histogram identical to original */
                for (size_t k = 0; k < rowbytes; ++k) {
                    hist_filt[(unsigned char)row[k]]++;
                }
            }

            png_write_row(png_ptr, row);
            total_bytes += rowbytes;
        }
        /* Progress */
        if ((z & 0x3F) == 0) {
            fprintf(stderr, "%s: z=%d/%d\n", filename, z, NZ);
        }
    }

    if (ws)
        png3d_write_state_free_public(png_ptr, ws);

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(row);
    fclose(fp);

    if (total_bytes != (uint64_t)NX * NY * NZ*CELLBYTES) {
        fprintf(stderr, "Warning: total bytes (%" PRIu64 ") mismatch\n", total_bytes);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int comp_level = 6; /* default */
    if (argc >= 2) {
        int v = atoi(argv[1]);
        if (v < 0) v = 0;
        if (v > 9) v = 9;
        comp_level = v;
    }

    printf("Compression level: %d\n", comp_level);
    printf("Generating volume %dx%dx%d with %dx%dx%d blocks\n", NX, NY, NZ, BLOCK, BLOCK, BLOCK);
    printf("This may take a while and produce very large files.\n");

    uint64_t hist_orig_no3d[256], hist_filt_no3d[256];
    uint64_t hist_orig_3d[256], hist_filt_3d[256];

    printf("Writing pvs3_no3d.png (no 3D filtering)...\n");
    if (write_volume_png("pvs3_no3d.png", 0, comp_level, hist_orig_no3d, hist_filt_no3d) != 0) {
        fprintf(stderr, "Failed to write pvs3_no3d.png\n");
        return 1;
    }

    printf("Writing pvs3_paeth3.png (3D PAETH3 filtering)...\n");
    if (write_volume_png("pvs3_paeth3.png", 1, comp_level, hist_orig_3d, hist_filt_3d) != 0) {
        fprintf(stderr, "Failed to write pvs3_paeth3.png\n");
        return 1;
    }

    /* Compute entropies */
    uint64_t total_bytes = (uint64_t)NX * NY * NZ;
    double ent_orig_no3d = compute_entropy_bits_per_byte(hist_orig_no3d, total_bytes);
    double ent_filt_no3d = compute_entropy_bits_per_byte(hist_filt_no3d, total_bytes);
    double ent_orig_3d = compute_entropy_bits_per_byte(hist_orig_3d, total_bytes);
    double ent_filt_3d = compute_entropy_bits_per_byte(hist_filt_3d, total_bytes);

    int64_t size_no3d = get_file_size("pvs3_no3d.png");
    int64_t size_3d = get_file_size("pvs3_paeth3.png");

    printf("\n--- Results ---\n");
    printf("Total voxels/bytes: %" PRIu64 "\n", total_bytes);

    printf("\nNo 3D filtering (pvs3_no3d.png):\n");
    printf("  File size: %" PRId64 " bytes\n", size_no3d);
    printf("  Entropy (original) : %.6f bits/byte\n", ent_orig_no3d);
    printf("  Entropy (filtered) : %.6f bits/byte\n", ent_filt_no3d);

    printf("\n3D PAETH3 filtering (pvs3_paeth3.png):\n");
    printf("  File size: %" PRId64 " bytes\n", size_3d);
    printf("  Entropy (original) : %.6f bits/byte\n", ent_orig_3d);
    printf("  Entropy (filtered) : %.6f bits/byte\n", ent_filt_3d);

    if (size_no3d > 0 && size_3d > 0) {
        double ratio = ((double)size_3d) / ((double)size_no3d);
        printf("\nFile size ratio (3D / no3d) = %.3f (smaller is better)\n", ratio);
    }

    /* Print top histogram bins for filtered 3D to help debugging (most common bytes) */
    printf("\nTop 10 byte values in 3D-filtered data (value: count):\n");
    for (int t = 0; t < 10; ++t) {
        /* find max remaining */
        uint64_t maxc = 0; int maxi = -1;
        for (int i = 0; i < 256; ++i) {
            if (hist_filt_3d[i] > maxc) { maxc = hist_filt_3d[i]; maxi = i; }
        }
        if (maxi < 0) break;
        printf("  %3d : %" PRIu64 "\n", maxi, maxc);
        hist_filt_3d[maxi] = 0; /* zero out to find next */
    }

    printf("\nDone.\n");
    return 0;
}