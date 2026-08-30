/* png3d_filter.c
 * Implementation of 3D filtering for PVS data.
 *
 * This module implements the core 3D filter logic that operates on
 * 3D grids of data with spatial coherency. It provides:
 *  - XOR-based predictor (simple spatial difference)
 *  - PAETH3 predictor (3D variant of PAETH using neighbors from previous z-layer)
 *  - 3D grid-aware filtering during write
 *  - 3D grid-aware unfiltering during read
 */

#include "png3d_filter.h"
#include "png3d.h"
#include <string.h>
#include <stdlib.h>

/* Forward declarations */
static png_byte png3d_paeth3(
    png_byte a,    /* Left voxel (x-1, y, z) */
    png_byte b,    /* Above voxel (x, y-1, z) */
    png_byte c,    /* Above-left voxel (x-1, y-1, z) */
    png_byte d);   /* Back voxel (x, y, z-1) */

/* 3D PAETH predictor: extends PAETH to 3 dimensions.
 * Given 4 neighbors (left, above, above-left, back), compute the best predictor.
 */
static png_byte
png3d_paeth3(png_byte a, png_byte b, png_byte c, png_byte d)
{
    int p = (int)a + (int)b + (int)d - (int)c;
    int pa = abs(p - (int)a);
    int pb = abs(p - (int)b);
    int pc = abs(p - (int)c);
    int pd = abs(p - (int)d);

    if (pa <= pb && pa <= pc && pa <= pd)
        return a;
    else if (pb <= pc && pb <= pd)
        return b;
    else if (pc <= pd)
        return c;
    else
        return d;
}

/* ============================================================================
 * WRITE-SIDE 3D FILTERING
 * ============================================================================
 *
 * During encoding, we apply 3D predictors to decorrelate the data.
 * The data is organized as follows:
 *  - rowsPerCell: how many PNG rows form one logical grid row
 *  - cellBytes: bytes per cell in the logical grid
 *  - mapping: 0 = slice-major, row-major within slice
 */

/* XOR-based 3D filter: for each byte, XOR with the predictor.
 * Predictor is the left neighbor (if in grid mode).
 */
void
png3d_filter_row_xor(png_struct *png_ptr, png_row_info *row_info,
    png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row)
{
    size_t i;
    size_t rowbytes = row_info->rowbytes;
    unsigned int bpp = (row_info->pixel_depth + 7) >> 3;

    /* Silence compiler warnings if not using all parameters */
    (void)png_ptr;
    /* Use a copy of the original row as predictor inputs so we always read
     * original (unmodified) neighbor bytes when computing the filter.
     * This avoids using already-filtered bytes as predictors which would
     * break the intended semantics and make decoding fail. */
    {
        const png_byte *pp = prev_row;   /* Row above in XY plane (y-1) */
        const png_byte *dp = prev_z_row; /* Row above in Z dimension (z-1) */
        png_byte *orig = (png_byte *)png_malloc(png_ptr, rowbytes);
        if (orig == NULL)
            return; /* allocation failure: bail out (no filtering) */

        memcpy(orig, row, rowbytes);

        /* First bpp bytes (first voxel in row) have no left neighbor;
         * leave them as-is (they remain the same as orig). */

        /* Remaining bytes: XOR original current byte with original left byte */
        for (i = bpp; i < rowbytes; i++)
        {
            row[i] = (png_byte)(((int)orig[i] ^ (int)orig[i - bpp]) & 0xff);
        }

        png_free(png_ptr, orig);
        (void)pp;
        (void)dp;
    }
}

/* PAETH3-based 3D filter: apply PAETH predictor using 3D neighborhood.
 * Predictor = paeth3(left, above, above-left, back).
 */
void
png3d_filter_row_paeth3(png_struct *png_ptr, png_row_info *row_info,
    png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row)
{
    size_t i;
    size_t rowbytes = row_info->rowbytes;
    png_byte *rp = row;
    const png_byte *pp = prev_row;   /* Row above in XY plane */
    const png_byte *dp = prev_z_row; /* Row above in Z dimension */
    unsigned int bpp = (row_info->pixel_depth + 7) >> 3;
    size_t istop;

    /* Silence compiler warning */
    (void)png_ptr;
    /* Copy original row so predictors (especially left/above-left) read the
     * original bytes, not bytes already modified by filtering earlier in the
     * row. This matches the predictor semantics that expect original neighbors. */
    {
        png_byte *orig = (png_byte *)png_malloc(png_ptr, rowbytes);
        if (orig == NULL)
            return; /* allocation failure: bail out (no filtering) */

        memcpy(orig, row, rowbytes);

        /* First bpp bytes: no left neighbor, only vertical/back predictors */
        if (pp != NULL && dp != NULL)
        {
            for (i = 0; i < bpp; i++)
            {
                png_byte p = png3d_paeth3(0, pp[i], 0, dp[i]);
                row[i] = (png_byte)(((int)orig[i] - (int)p) & 0xff);
            }
        }
        else if (pp != NULL)
        {
            for (i = 0; i < bpp; i++)
            {
                row[i] = (png_byte)(((int)orig[i] - (int)pp[i]) & 0xff);
            }
        }
        else if (dp != NULL)
        {
            for (i = 0; i < bpp; i++)
            {
                row[i] = (png_byte)(((int)orig[i] - (int)dp[i]) & 0xff);
            }
        }
        /* else: no previous data, leave byte as-is (orig) */

        /* Remaining bytes: apply full PAETH3 with left, above, above-left, back neighbors.
         * Use 'orig' for left/above-left so we don't use already-filtered data. */
        istop = rowbytes - bpp;
        for (i = 0; i < istop; i++)
        {
            size_t idx = bpp + i;
            png_byte a = orig[idx - bpp];                  /* original left */
            png_byte b = (pp != NULL) ? pp[idx] : 0;       /* above */
            png_byte c = (pp != NULL) ? pp[idx - bpp] : 0; /* above-left */
            png_byte d = (dp != NULL) ? dp[idx] : 0;       /* back (previous z) */
            png_byte p = png3d_paeth3(a, b, c, d);
            row[idx] = (png_byte)(((int)orig[idx] - (int)p) & 0xff);
        }

        png_free(png_ptr, orig);
    }
}

/* ============================================================================
 * READ-SIDE 3D UNFILTERING
 * ============================================================================
 *
 * During decoding, we reverse the 3D filter to recover the original data.
 */

/* Reverse XOR filter: for each byte, XOR again with the predictor */
void
png3d_unfilter_row_xor(png_struct *png_ptr, png_row_info *row_info,
    png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row)
{
    size_t i;
    size_t rowbytes = row_info->rowbytes;
    png_byte *rp = row;
    unsigned int bpp = (row_info->pixel_depth + 7) >> 3;

    /* Silence compiler warnings */
    (void)png_ptr;
    (void)prev_row;
    (void)prev_z_row;

    /* First bpp bytes have no predictor */
    rp += bpp;

    /* Remaining bytes: XOR with left neighbor (already restored) */
    for (i = bpp; i < rowbytes; i++)
    {
        *rp = (png_byte)(((int)(*rp) ^ (int)(*(rp - bpp))) & 0xff);
        rp++;
    }
}

/* Reverse PAETH3 filter */
void
png3d_unfilter_row_paeth3(png_struct *png_ptr, png_row_info *row_info,
    png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row)
{
    size_t i;
    size_t rowbytes = row_info->rowbytes;
    png_byte *rp = row;
    const png_byte *pp = prev_row;  /* Row above in XY plane */
    const png_byte *dp = prev_z_row;  /* Row above in Z dimension */
    unsigned int bpp = (row_info->pixel_depth + 7) >> 3;
    size_t istop;

    /* Silence compiler warning */
    (void)png_ptr;

    /* First bpp bytes: recover using vertical predictors */
    if (prev_row != NULL && prev_z_row != NULL)
    {
        for (i = 0; i < bpp; i++)
        {
            png_byte p = png3d_paeth3(0, pp[i], 0, dp[i]);
            rp[i] = (png_byte)(((int)rp[i] + (int)p) & 0xff);
        }
    }
    else if (prev_row != NULL)
    {
        for (i = 0; i < bpp; i++)
        {
            rp[i] = (png_byte)(((int)rp[i] + (int)pp[i]) & 0xff);
        }
    }
    else if (prev_z_row != NULL)
    {
        for (i = 0; i < bpp; i++)
        {
            rp[i] = (png_byte)(((int)rp[i] + (int)dp[i]) & 0xff);
        }
    }
    /* else: no previous data, byte is already recovered */

    /* Remaining bytes: apply full PAETH3 unfiltering */
    istop = rowbytes - bpp;
    for (i = 0; i < istop; i++)
    {
        png_byte a = rp[bpp + i - bpp];        /* Left (already restored) */
        png_byte b = (pp != NULL) ? pp[bpp + i] : 0;      /* Above */
        png_byte c = (pp != NULL) ? pp[bpp + i - bpp] : 0;  /* Above-left */
        png_byte d = (dp != NULL) ? dp[bpp + i] : 0;      /* Back (previous z) */
        png_byte p = png3d_paeth3(a, b, c, d);
        rp[bpp + i] = (png_byte)(((int)rp[bpp + i] + (int)p) & 0xff);
    }
}

/* ============================================================================
 * STATE MANAGEMENT
 * ============================================================================
 */
/* Internal state for tracking 3D filter context during write */
struct png3d_write_state
{
    png3d_pvs3_t pvs3;              /* PVS3 parameters */
    /* Buffers holding the original rows from the previous z-layer.
     * Indexed by row index within a cell (0..rowsPerCell-1). */
    png_byte **prev_z_buffers;
    png_uint_32 z_buffer_count;     /* Number of rows allocated (rowsPerCell) */

    /* Buffer holding the previous row in the current z-layer (original bytes). */
    png_byte *last_row;

    /* Scratch buffer to hold the original (unfiltered) current row so we can
     * store originals into prev_z_buffers and last_row after filtering. */
    png_byte *temp_row;

    png_uint_32 current_z;          /* Current z-layer index */
    png_uint_32 current_row_in_z;   /* Row index within current z-layer */
    size_t rowbytes;                /* Bytes per row */
    png3d_filter_fn filter_fn;      /* Active filter function */
};

/* Allocate 3D filter write state */
static struct png3d_write_state *
png3d_write_state_new(png_struct *png_ptr, const png3d_pvs3_t *pvs3,
    size_t rowbytes)
{
    struct png3d_write_state *state;
    png_uint_32 i;

    state = (struct png3d_write_state *)png_malloc(png_ptr,
        sizeof(struct png3d_write_state));

    if (state == NULL)
        return NULL;

    state->pvs3 = *pvs3;
    state->rowbytes = rowbytes;
    state->current_z = 0;
    state->current_row_in_z = 0;

    /* Keep one z-layer in memory: allocate prev_z_buffers with rowsPerCell rows */
    state->z_buffer_count = pvs3->rowsPerCell;
    if (state->z_buffer_count == 0)
        state->z_buffer_count = 1;

    /* Allocate prev_z_buffers (one full layer of original rows) */
    state->prev_z_buffers = (png_byte **)png_malloc(png_ptr,
        sizeof(png_byte *) * state->z_buffer_count);

    if (state->prev_z_buffers == NULL)
    {
        png_free(png_ptr, state);
        return NULL;
    }

    for (i = 0; i < state->z_buffer_count; i++)
    {
        state->prev_z_buffers[i] = (png_byte *)png_calloc(png_ptr, rowbytes);
        if (state->prev_z_buffers[i] == NULL)
        {
            for (png_uint_32 j = 0; j < i; j++)
                png_free(png_ptr, state->prev_z_buffers[j]);
            png_free(png_ptr, state->prev_z_buffers);
            png_free(png_ptr, state);
            return NULL;
        }
    }

    /* Allocate last_row and temp_row buffers */
    state->last_row = (png_byte *)png_calloc(png_ptr, rowbytes);
    if (state->last_row == NULL)
    {
        for (i = 0; i < state->z_buffer_count; i++)
            png_free(png_ptr, state->prev_z_buffers[i]);
        png_free(png_ptr, state->prev_z_buffers);
        png_free(png_ptr, state);
        return NULL;
    }

    state->temp_row = (png_byte *)png_calloc(png_ptr, rowbytes);
    if (state->temp_row == NULL)
    {
        png_free(png_ptr, state->last_row);
        for (i = 0; i < state->z_buffer_count; i++)
            png_free(png_ptr, state->prev_z_buffers[i]);
        png_free(png_ptr, state->prev_z_buffers);
        png_free(png_ptr, state);
        return NULL;
    }

    /* Select filter function based on predictor */
    if (pvs3->predictor == 0)  /* XOR */
        state->filter_fn = png3d_filter_row_xor;
    else if (pvs3->predictor == 1)  /* PAETH3 */
        state->filter_fn = png3d_filter_row_paeth3;
    else
        state->filter_fn = png3d_filter_row_xor;  /* Default to XOR */

    return state;
}

/* Free 3D filter write state */
static void
png3d_write_state_free(png_struct *png_ptr, struct png3d_write_state *state)
{
    if (state == NULL)
        return;

    if (state->prev_z_buffers != NULL)
    {
        png_uint_32 i;
        for (i = 0; i < state->z_buffer_count; i++)
        {
            if (state->prev_z_buffers[i] != NULL)
                png_free(png_ptr, state->prev_z_buffers[i]);
        }
        png_free(png_ptr, state->prev_z_buffers);
    }

    if (state->last_row != NULL)
        png_free(png_ptr, state->last_row);
    if (state->temp_row != NULL)
        png_free(png_ptr, state->temp_row);

    png_free(png_ptr, state);
}

/* Apply 3D filter to a row during write */
void
png3d_filter_write_row(png_struct *png_ptr, png_row_info *row_info,
    png_byte *row, struct png3d_write_state *state)
{
    png_byte *prev_row = NULL;     /* Row above in XY plane (original bytes) */
    const png_byte *prev_z_row = NULL;   /* Row in previous Z layer (original bytes) */

    if (state == NULL || state->filter_fn == NULL)
        return;

    /* prev_row: previous original row within the same z-layer */
    if (state->current_row_in_z > 0)
        prev_row = state->last_row;

    /* prev_z_row: original row from previous z-layer at same row index */
    if (state->current_z > 0)
        prev_z_row = state->prev_z_buffers[state->current_row_in_z];

    /* Save original (unfiltered) row into temp_row */
    memcpy(state->temp_row, row, row_info->rowbytes);

    /* Apply the 3D filter in-place (mutates 'row' into residuals) */
    state->filter_fn(png_ptr, row_info, row, prev_row, prev_z_row);

    /* Save ORIGINAL (unfiltered) row into prev_z_buffers for the next z-layer */
    memcpy(state->prev_z_buffers[state->current_row_in_z], state->temp_row, row_info->rowbytes);

    /* Save ORIGINAL into last_row so the next row in this z-layer can use it as prev_row */
    memcpy(state->last_row, state->temp_row, row_info->rowbytes);

    /* Update position tracking */
    state->current_row_in_z++;
    if (state->current_row_in_z >= state->pvs3.rowsPerCell)
    {
        state->current_row_in_z = 0;
        state->current_z++;
    }
}

/* ------------------------------------------------------------------ */
/* Thin public wrappers so applications (demo writer) can use the state */
/* ------------------------------------------------------------------ */

/* The header png3d_filter.h exposes these prototypes. Provide definitions
 * here so demo code can link without touching internal static names.
 */
png3d_write_state_t *
png3d_write_state_new_public(png_struct *png_ptr, const png3d_pvs3_t *pvs3,
    size_t rowbytes)
{
    return (png3d_write_state_t *)png3d_write_state_new(png_ptr, pvs3, rowbytes);
}

void
png3d_write_state_free_public(png_struct *png_ptr, png3d_write_state_t *state)
{
    png3d_write_state_free(png_ptr, (struct png3d_write_state *)state);
}

void
png3d_filter_write_row_public(png_struct *png_ptr, png_row_info *row_info,
    png_byte *row, png3d_write_state_t *state)
{
    png3d_filter_write_row(png_ptr, row_info, row, (struct png3d_write_state *)state);
}


