#include "png3d.h"
#include <string.h>
#include <stdlib.h>

/* Internal helpers: big-endian store */
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

/* Chunk name as 4 ASCII bytes; keep a terminating zero for convenience */
static const png_byte pvs3_name[5] = {'P','V','S','D','\0'};

void png_write_pvs3_chunk(png_struct *png_ptr, png_info *info_ptr, const png3d_pvs3_t *pvs3)
{
    unsigned char data[24];
    memset(data,0,sizeof(data));

    write_u32_be(data+0, pvs3->nx);
    write_u32_be(data+4, pvs3->ny);
    write_u32_be(data+8, pvs3->nz);
    write_u32_be(data+12, pvs3->rowsPerCell);
    write_u32_be(data+16, pvs3->cellBytes);
    data[20] = pvs3->mapping;
    data[21] = pvs3->predictor;
    data[22] = pvs3->version;
    data[23] = pvs3->reserved;

    /* Use libpng to write an ancillary chunk. Caller should call this before
     * writing IDAT data (i.e. after png_write_info()).
     */
    png_write_chunk(png_ptr, pvs3_name, data, sizeof(data));
}

int png_get_pvs3_from_info(png_struct *png_ptr, png_info *info_ptr, png3d_pvs3_t *out)
{
#ifdef PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED
    png_unknown_chunk *entries = NULL;
    int n = png_get_unknown_chunks(png_ptr, info_ptr, &entries);
    if (n <= 0 || entries == NULL) return 0;

    for (int i = 0; i < n; ++i)
    {
        /* Compare first four bytes of name */
        if (entries[i].name[0] == 'P' && entries[i].name[1] == 'V' &&
            entries[i].name[2] == 'S' && entries[i].name[3] == 'D')
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
