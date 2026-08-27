/* src/png3d_core.c
 * Core helpers to store pvs3 parameters into png_info and to allocate
 * runtime buffers.  This is an incremental, non-invasive step towards
 * a deeper integration with the filtering pipeline.
 */

#include "png3d.h"
#include "png.h"
#include <string.h>
#include <stdlib.h>

/* Helper to write a 32-bit BE integer into buffer */
static void write_u32_be(unsigned char *buf, png_uint_32 v)
{
    buf[0] = (unsigned char)((v >> 24) & 0xff);
    buf[1] = (unsigned char)((v >> 16) & 0xff);
    buf[2] = (unsigned char)((v >> 8) & 0xff);
    buf[3] = (unsigned char)(v & 0xff);
}
static png_uint_32 read_u32_be(const unsigned char *buf)
{
    return ((png_uint_32)buf[0] << 24) | ((png_uint_32)buf[1] << 16) |
           ((png_uint_32)buf[2] << 8)  | (png_uint_32)buf[3];
}

/* Chunk name bytes */
static const png_byte pvs3_name[5] = {'p','v','s','3','\0'};

void png_set_pvs3(png_struct *png_ptr, png_info *info_ptr, const png3d_pvs3_t *pvs3)
{
#ifdef PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED
    unsigned char data[24];
    write_u32_be(data+0, pvs3->nx);
    write_u32_be(data+4, pvs3->ny);
    write_u32_be(data+8, pvs3->nz);
    write_u32_be(data+12, pvs3->rowsPerCell);
    write_u32_be(data+16, pvs3->cellBytes);
    data[20] = pvs3->mapping;
    data[21] = pvs3->predictor;
    data[22] = pvs3->version;
    data[23] = pvs3->reserved;

    png_unknown_chunk unk;
    memset(&unk,0,sizeof(unk));
    memcpy(unk.name, pvs3_name, 4);
    unk.size = sizeof(data);
    unk.data = (png_byte *)png_malloc_default(png_ptr, unk.size);
    if (unk.data == NULL) return; /* OOM: silently fail */
    memcpy(unk.data, data, unk.size);
    /* Default location: write before IDAT (PNG_HAVE_IHDR/PNG_HAVE_PLTE depending) */
    unk.location = PNG_HAVE_IHDR;

    png_set_unknown_chunks(png_ptr, info_ptr, &unk, 1);
    /* The api requires location be set per-chunk separately in older libpngs */
    png_set_unknown_chunk_location(png_ptr, info_ptr, 0, PNG_HAVE_IHDR);
#endif
    (void)png_ptr; (void)info_ptr; (void)pvs3;
}

int png_get_pvs3_from_info(png_struct *png_ptr, png_info *info_ptr, png3d_pvs3_t *out)
{
#ifdef PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED
    png_unknown_chunk *entries = NULL;
    int n = png_get_unknown_chunks(png_ptr, info_ptr, &entries);
    if (n <= 0 || entries == NULL) return 0;
    for (int i = 0; i < n; ++i)
    {
        if (entries[i].name[0] == 'p' && entries[i].name[1] == 'v' &&
            entries[i].name[2] == 's' && entries[i].name[3] == '3')
        {
            if (entries[i].data != NULL && entries[i].size >= 24)
            {
                const unsigned char *d = entries[i].data;
                out->nx = read_u32_be(d+0);
                out->ny = read_u32_be(d+4);
                out->nz = read_u32_be(d+8);
                out->rowsPerCell = read_u32_be(d+12);
                out->cellBytes = read_u32_be(d+16);
                out->mapping = d[20];
                out->predictor = d[21];
                out->version = d[22];
                out->reserved = d[23];
                return 1;
            }
        }
    }
#endif
    (void)png_ptr; (void)info_ptr; (void)out;
    return 0;
}

/* Allocate slice buffer: size = rowbytes * height * bytesPerPixel? We'll allocate
 * a buffer sized (rowbytes * height) so it can hold entire image slices for
 * non-interlaced 8-bit images. Caller must ensure png_ptr->rowbytes is set.
 */
int png3d_alloc_buffers(png_struct *png_ptr)
{
    if (png_ptr == NULL) return 0;
    size_t rowbytes = png_ptr->rowbytes;
    png_uint_32 height = png_ptr->height;
    if (rowbytes == 0 || height == 0) return 0;

    size_t need = rowbytes * (size_t)height;
    if (need == 0) return 0;

    /* Free existing if any */
    if (png_ptr->slice_buf != NULL)
    {
        png_free(png_ptr, png_ptr->slice_buf);
        png_ptr->slice_buf = NULL;
        png_ptr->slice_buf_size = 0;
    }

    png_byte *buf = (png_byte *)png_malloc(png_ptr, need);
    if (buf == NULL) return 0;
    png_ptr->slice_buf = buf;
    png_ptr->slice_buf_size = need;
    png_ptr->front_slice = buf; /* start both point to buffer for now */
    png_ptr->prev_slice = buf + need/2; /* naive partition if used */
    return 1;
}

void png3d_free_buffers(png_struct *png_ptr)
{
    if (png_ptr == NULL) return;
    if (png_ptr->slice_buf != NULL)
    {
        png_free(png_ptr, png_ptr->slice_buf);
        png_ptr->slice_buf = NULL;
        png_ptr->slice_buf_size = 0;
        png_ptr->front_slice = NULL;
        png_ptr->prev_slice = NULL;
    }
}
