Prototype 3D filter helpers (pvs3 chunk)

This branch provides a non-intrusive prototype to store a minimal 3D
occlusion metadata blob (chunk name: "pvs3") inside PNG files. It
includes:

- png3d.h: small public API for writing & reading a pvs3 chunk.
- src/png3d.c: implementation of pack/unpack + helpers.
- tools/demo_writer.c: writes a tiny PNG and embeds a pvs3 chunk.
- tools/demo_reader.c: reads the PNG and extracts the pvs3 chunk.
- tools/CMakeLists.txt: build helpers for the demos.

How to build the demos (posix):

  mkdir build && cd build
  cmake -DPNG_PNG_INCLUDE_DIR=/usr/include -DPNG_LIBRARY=/usr/lib/libpng.so ..
  make

Then run:
  ./demo_writer
  ./demo_reader

The prototype does NOT change libpng internals or IDAT filtering yet;
it only provides a compact metadata chunk and an API. The next step is to
hook the chunk parameters into native 3D-aware filter/write functions in
pngwrite.c/pngwutil.c and corresponding unfilter code in pngread.c.
