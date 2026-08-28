"# 3D Filter Implementation for libpng3d

## Overview

This document describes the 3D filter implementation for efficient compression of PVS (Potentially Visible Sets) data and other 3D grid-based datasets with high spatial coherency.

## Architecture

### Filter Pipeline

The 3D filter operates on PNG's standard IDAT stream with an extension to support 3D spatial prediction. The key innovation is the use of three-dimensional predictors instead of standard 1D PNG filters:

```
Raw 3D Data (nx × ny × nz grid)
    ↓
3D Filtering (decorrelation along x, y, z)
    ↓
PNG Standard Filtering (if enabled)
    ↓
Zlib Deflate Compression
    ↓
IDAT Chunks
```

### Data Layout

The 3D grid is flattened into PNG rows according to the `mapping` parameter in `pvs3`:

- **Mapping 0 (Slice-Major, Row-Major within Slice)**:
  - Data is organized as z-layers (slices).
  - Each z-layer contains `ny` rows of `nx × cellBytes` bytes.
  - A logical grid row occupies `rowsPerCell` PNG rows.

### Predictor Strategies

#### 1. XOR Predictor (predictor = 0)

Simplest and fastest approach. For each byte at position (x, y, z):

```
predicted = left_byte  (from x-1)
encoded = raw ⊕ predicted
```

**Pros**:
- Very fast (single XOR operation)
- Good for highly correlated data
- Reversible without division or rounding errors

**Cons**:
- No temporal (z-axis) prediction
- Doesn't leverage above-pixel information

#### 2. PAETH3 Predictor (predictor = 1)

3D extension of PNG's standard PAETH predictor. Uses four neighbors:

```
predicted = paeth3(a, b, c, d)
where:
  a = left pixel         (x-1, y, z)
  b = above pixel        (x, y-1, z)
  c = above-left pixel   (x-1, y-1, z)
  d = back pixel         (x, y, z-1)  ← 3D extension
```

PAETH3 selects the neighbor closest to the linear prediction:

```cpp
p = a + b + d - c
pa = |p - a|, pb = |p - b|, pc = |p - c|, pd = |p - d|
predicted = select based on minimum distance
```

**Pros**:
- Leverages all 3D neighborhood information
- Better compression for smooth data
- Handles edges and transitions well

**Cons**:
- Requires 3-4 neighbor accesses per byte
- Slightly slower than XOR
- Requires previous z-layer in memory

## Implementation Details

### Write-Side Filtering (`png3d_filter.c`)

During PNG encoding:

1. **State Tracking**: Maintains a `png3d_write_state` structure that holds:
   - Current position in 3D grid (current_z, current_row_in_z)
   - Z-layer buffers (stores one complete layer)
   - Selected filter function

2. **Filter Application**:
   - For each PNG row, the filter function is called with:
     - Current row data
     - Previous row in XY plane (if available)
     - Previous row in Z dimension (if available)
   - The filter subtracts the predicted value from actual value
   - Result is passed to standard PNG filtering/compression

3. **Buffer Management**:
   - Only one z-layer needs to be kept in memory
   - Previous rows within a z-layer are cached in `z_buffers`

### Read-Side Unfiltering (`png3d_filter.c`)

During PNG decoding:

1. **State Tracking**: Similar to write-side
2. **Unfilter Application**:
   - For each PNG row, the unfilter function is called
   - The predicted value is added back to the residual
   - Result is the recovered original data
3. **Order Independence**:
   - Unfiltering is strictly sequential (depends on previously decoded rows)
   - Must be applied in the same order as filtering

## Integration with libpng

### Hook Points

1. **Write Path** (`pngwutil.c`):
   - After `png_write_info()`, apply 3D filter parameters
   - Hook into row writing to apply 3D predictor before standard filters
   - Manage z-layer state across write operations

2. **Read Path** (`pngread.c` / `pngrutil.c`):
   - After `png_read_info()`, extract 3D filter parameters
   - Hook into row reading to apply 3D unfilter after standard filters
   - Manage z-layer state across read operations

### PNG Structure Extensions

The `png_struct` needs to store:

```c
/* In png_struct (png_ptr) */
png3d_pvs3_t *png_3d_params;        /* 3D filter parameters if present */
struct png3d_write_state *png_3d_write_state;  /* Write-side state */
struct png3d_read_state *png_3d_read_state;    /* Read-side state */
```

## Usage Example

### Writing

```c
// Set up write structure
png_structp png_ptr = png_create_write_struct(...);
png_infop info_ptr = png_create_info_struct(png_ptr);

// Set image dimensions and parameters
png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_GRAY,
             PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
             PNG_FILTER_TYPE_DEFAULT);

png_write_info(png_ptr, info_ptr);

// Write 3D metadata
png3d_pvs3_t pvs3 = {
    .nx = 16, .ny = 16, .nz = 8,
    .rowsPerCell = 2,
    .cellBytes = 32,
    .mapping = 0,
    .predictor = 1,  /* PAETH3 */
    .version = 1,
    .reserved = 0
};
png_write_pvs3_chunk(png_ptr, info_ptr, &pvs3);

// Write rows with 3D filtering
for (z = 0; z < nz; z++) {
    for (y = 0; y < ny; y++) {
        png_byte *row = get_row_data(z, y);
        png_3d_filter_write_row(png_ptr, ...);
        png_write_row(png_ptr, row);
    }
}

png_write_end(png_ptr, info_ptr);
```

### Reading

```c
png_structp png_ptr = png_create_read_struct(...);
png_infop info_ptr = png_create_info_struct(png_ptr);

png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, 
                            (png_byte*)\"pvs3\", 1);
png_read_info(png_ptr, info_ptr);

// Extract 3D metadata
png3d_pvs3_t pvs3;
if (png_get_pvs3_from_info(png_ptr, info_ptr, &pvs3)) {
    // Set up 3D unfiltering state
    png_3d_setup_read_state(png_ptr, &pvs3);
}

// Read and unfilter rows
for (y = 0; y < height; y++) {
    png_byte row[PNG_MAX_PALETTE_LENGTH];
    png_read_row(png_ptr, row, NULL);
    // 3D unfilter is applied internally
}

png_read_end(png_ptr, info_ptr);
```

## Compression Benefits

### Expected Compression Ratios

For typical PVS data (3D bitsets with spatial coherency):

- **Without 3D filter**: ~20-30% (standard PNG filters)
- **XOR predictor**: ~40-50% (2x improvement)
- **PAETH3 predictor**: ~50-60% (2-3x improvement)

The benefits increase with:
- Higher spatial coherency
- Larger grid dimensions
- More uniform regions

## Performance Considerations

### Memory Usage

- **XOR predictor**: Minimal (one z-layer + row buffer)
- **PAETH3 predictor**: Similar (needs access to previous row and z-layer)
- **Per-row overhead**: O(rowsPerCell) bytes

### CPU Cost

- **XOR**: ~1 XOR + 1 load + 1 store per byte → ~3 ops/byte
- **PAETH3**: ~10-20 ops/byte (paeth predictor calculation)
- **Acceptable**: Well within PNG compression's overhead

## Testing Strategy

1. **Unit Tests**:
   - Verify filter/unfilter symmetry (filter → unfilter = identity)
   - Test all edge cases (first row, first column, first layer)
   - Test with various grid dimensions and cell sizes

2. **Integration Tests**:
   - Write 3D data with filter, read back, verify byte-for-byte equality
   - Compare compression ratios against standard PNG filters
   - Test with real PVS datasets

3. **Regression Tests**:
   - Ensure backward compatibility with non-3D PNG files
   - Standard PNG files should decompress identically

## Future Enhancements

1. **Adaptive Predictor Selection**:
   - Analyze data statistics
   - Choose XOR vs PAETH3 per z-layer

2. **SIMD Optimization**:
   - Vectorize filter operations (SSE2, AVX2, NEON)
   - Process multiple bytes in parallel

3. **Improved Predictors**:
   - Gradient-based predictors
   - Context-adaptive predictors
   - Machine learning-based predictors (future)

4. **Multi-threading**:
   - Process multiple z-layers in parallel
   - Thread-safe state management

## References

- PNG Specification: https://www.w3.org/TR/png/
- PAETH Filter: W3C PNG Recommendation, Section 9.2
- PVS Compression: Used in game engines for visibility optimization
"
