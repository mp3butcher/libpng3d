/* src/png3d_filter.c
 * Prototype implementations for 3D filter and unfilter. These are basic
 * reference implementations (XOR predictor by default, optional PAETH3)
 * intended to be integrated into the core filtering pipeline in a later step.
 */

#include "png.h"
#include "pngstruct.h"
#include <string.h>

/* Simple PAETH function for three predictors (left, above, front) but working
 * on bytes; this is a prototype and mirrors classic PAETH behaviour on bytes.
 */
static png_byte paeth3(png_byte a, png_byte b, png_byte c)
{
    int p = (int)a + (int)b - (int)c;
    int pa = abs(p - (int)a);
    int pb = abs(p - (int)b);
    int pc = abs(p - (int)c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

/* Write-side filter: transforms 'row' in-place into filtered data.
 * This prototype expects the caller to ensure png_ptr->prev_row and
 * png_ptr->prev_slice are valid pointers (or NULL). It operates on bytes.
 */
void png_do_filter_3d(png_row_info *row_info, png_byte *row, const png_byte *prev_row)
{
    png_struct *png_ptr = NULL; /* Not available in this callback signature; we
                                 * rely on global-like storage in pngstruct for
                                 * the prototype. In a full integration these
                                 * functions would be methods with access to png_ptr.
                                 */
    (void)png_ptr; /* silence unused */

    size_t n = row_info->rowbytes;
    /* Locate our runtime buffers by using the globals in this compilation unit
     * is not available; in the real integration we will use png_ptr->front_slice
     * and png_ptr->prev_slice. For now we just use prev_row as the predictor.
     */

    /* Default predictor: XOR with prev_row if present, else leave unchanged */
    for (size_t i = 0; i < n; ++i)
    {
        png_byte above = prev_row ? prev_row[i] : 0;
        /* XOR predictor */
        row[i] = (png_byte)(row[i] ^ above);
    }
}

/* Read-side unfilter: reverse the transform applied by png_do_filter_3d. */
void png_do_unfilter_3d(png_row_info *row_info, png_byte *row, const png_byte *prev_row)
{
    size_t n = row_info->rowbytes;
    for (size_t i = 0; i < n; ++i)
    {
        png_byte above = prev_row ? prev_row[i] : 0;
        /* XOR inverse is XOR again */
        row[i] = (png_byte)(row[i] ^ above);
    }
}
