'/* png3d_filter.h
 * Public API for 3D filtering operations.
 */

#ifndef PNG3D_FILTER_H
#define PNG3D_FILTER_H

#include "png.h"
#include "png3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Function pointer type for 3D filter operations */
typedef void (*png3d_filter_fn)(
    png_struct *png_ptr,
    png_row_info *row_info,
    png_byte *row,
    const png_byte *prev_row,      /* Row above in XY plane (y-1) */
    const png_byte *prev_z_row);   /* Row in previous Z layer (z-1) */

/* Write-side filters */
PNG_EXPORT(void, png3d_filter_row_xor,
    (png_struct *png_ptr, png_row_info *row_info,
     png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row));

PNG_EXPORT(void, png3d_filter_row_paeth3,
    (png_struct *png_ptr, png_row_info *row_info,
     png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row));

/* Read-side unfilters */
PNG_EXPORT(void, png3d_unfilter_row_xor,
    (png_struct *png_ptr, png_row_info *row_info,
     png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row));

PNG_EXPORT(void, png3d_unfilter_row_paeth3,
    (png_struct *png_ptr, png_row_info *row_info,
     png_byte *row, const png_byte *prev_row, const png_byte *prev_z_row));

#ifdef __cplusplus
}
#endif

#endif /* PNG3D_FILTER_H */
'
