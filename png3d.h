/* png3d.h
 * Prototype API for embedding a simple 3D occlusion metadata chunk ("pvs3")
 * and small runtime helpers.
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

/* Store params into the png_info so that write side may output the pvs3 chunk. */
PNG_EXPORT(void, png_set_pvs3,
    (png_struct *png_ptr, png_info *info_ptr, const png3d_pvs3_t *pvs3));

/* Read pvs3 from info (returns 1 if found). */
PNG_EXPORT(int, png_get_pvs3_from_info,
    (png_struct *png_ptr, png_info *info_ptr, png3d_pvs3_t *out));

/* Allocate/free runtime buffers for 3D filtering */
PNG_EXPORT(int, png3d_alloc_buffers, (png_struct *png_ptr));
PNG_EXPORT(void, png3d_free_buffers, (png_struct *png_ptr));

#ifdef __cplusplus
}
#endif

#endif /* PNG3D_H */
