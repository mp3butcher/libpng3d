/* png3d_filter.h
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

     typedef struct png3d_write_state png3d_write_state_t;

/* Public helper APIs to use the 3D filters from user code. */
PNG_EXPORT(png3d_write_state_t*, png3d_write_state_new_public,
    (png_struct *png_ptr, const png3d_pvs3_t *pvs3, size_t rowbytes));
PNG_EXPORT(void, png3d_write_state_free_public,
    (png_struct *png_ptr, png3d_write_state_t *state));
PNG_EXPORT(void, png3d_filter_write_row_public,
    (png_struct *png_ptr, png_row_info *row_info, png_byte *row,
     png3d_write_state_t *state));
     
#ifdef __cplusplus
}
#endif

#endif /* PNG3D_FILTER_H */

