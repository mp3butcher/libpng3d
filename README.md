# libpng3d - README

This repository contains a prototype patch/extension for libpng to support a 3D-aware filter for compressing PVS/bitset data.

What is included in this branch `add-3d-filter-prototype`:
- include/png3d.h : public API declarations for 3D filter parameters and helpers
- src/png3d.c      : prototype implementation for pvs3 chunk handling and API
- tools/demo_writer.c : small demo that writes a PNG and embeds the pvs3 chunk
- tools/demo_reader.c : small demo that reads the PNG and extracts pvs3 chunk
- CMakeLists.txt : simple CMake to build the demos (requires libpng dev headers)

Notes:
- This prototype intentionally avoids patching deep libpng internals. It implements the metadata chunk "pvs3" and helper APIs.
- A full native filter (introducing a new PNG filter value and integrating inside libpng's filter pipeline) will require editing core libpng sources (pngwrite.c, pngread.c, png.h) and is more invasive. This branch is the safe first step and a basis to integrate the native filter.

Next steps we can take (on request):
- Implement full native filter integration inside libpng sources (pngwrite.c/pngread.c) and add PNG_FILTER_VALUE_3D = 5 in png.h.
- Implement 3D-PAETH predictor in the write/read pipeline and ensure causal ordering guarantees.
- Add unit tests that compare IDAT sizes between standard PNG and 3D-filtered runs.

Building the demo:
- Requires libpng dev installed and cmake
  mkdir build && cd build
  cmake ..
  make
  ./demo_writer
  ./demo_reader

