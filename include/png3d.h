#pragma once

#include <stdint.h>
#include <png.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 3D filter predictor types */
#define PNG_3D_PREDICTOR_XOR 0
#define PNG_3D_PREDICTOR_PAETH3 1

/* Mapping types */
#define PNG_3D_MAPPING_ROW_MAJOR 0
#define PNG_3D_MAPPING_MORTON    1
#define PNG_3D_MAPPING_HILBERT   2

typedef struct png_3d_filterp {
    uint32_t nx;         /* grid dims */
    uint32_t ny;
    uint32_t nz;
    uint32_t rowsPerCell;/* stripes per cell in scanlines */
    uint32_t cellBytes;  /* bytes per cell (pack bits into bytes) */
    uint8_t  mapping;    /* one of PNG_3D_MAPPING_* */
    uint8_t  predictor;  /* one of PNG_3D_PREDICTOR_* */
    uint8_t  version;    /* patch version */
    uint8_t  reserved;
} png_3d_filterp;

/* Set 3D filter parameters on a png_structp. This configures writer behavior.
   Must be called before png_write_info / png_write_row. */
PNG_EXPORT(void,png_set_3d_filter)(png_structp png_ptr, const png_3d_filterp* params);
PNG_EXPORT(void,png_get_3d_filter)(png_structp png_ptr, png_3d_filterp* out_params);

/* Utilities: write/read pvs3 ancillary chunk (metadata). Writer will call this
   to embed the parameters; reader will get parsed metadata on png_read_info. */
PNG_EXPORT(void,png_write_pvs3_chunk)(png_structp png_ptr, const png_3d_filterp* params);
PNG_EXPORT(void,png_read_pvs3_chunk)(png_structp png_ptr, png_infop info_ptr, png_3d_filterp* out_params);

#ifdef __cplusplus
}
#endif
