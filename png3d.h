/* png3d.h
 * Prototype API for embedding a simple 3D occlusion metadata chunk ("pvs3")
 * into PNGs produced by this fork.  This is intentionally small and
 * non-intrusive: helpers to write/read a custom ancillary chunk and a
 * compact in-file format for a small set of parameters.
 */

#ifndef PNG3D_H
#define PNG3D_H

#include "png.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compact descriptor for the prototype pvs3 chunk (24 bytes). */
typedef struct png3d_pvs3_struct
{
    png_uint_32 nx;
    png_uint_32 ny;
    png_uint_32 nz;
    png_uint_32 rowsPerCell;
    png_uint_32 cellBytes;
    png_byte mapping;   /* mapping enum (0 = slice-major row-major inside) */
    png_byte predictor; /* 0 = XOR, 1 = PAETH3 */
    png_byte version;   /* version of chunk format */
    png_byte reserved;  /* reserved, set to 0 */
} png3d_pvs3_t;

/* Write the pvs3 chunk immediately (use before writing IDATs). */
PNG_EXPORT(void, png_write_pvs3_chunk,
    (png_struct *png_ptr, png_info *info_ptr, const png3d_pvs3_t *pvs3));

/* Read pvs3 from info (requires unknown chunks to be stored via
 * png_set_keep_unknown_chunks prior to png_read_info()). Returns 1 if found. */
PNG_EXPORT(int, png_get_pvs3_from_info,
    (png_struct *png_ptr, png_info *info_ptr, png3d_pvs3_t *out));

#ifdef __cplusplus
}
#endif

#endif /* PNG3D_H */
