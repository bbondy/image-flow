# image-flow

`image-flow` is a C++ image toolkit with a small raster API, codec support, and a layered compositor.  
The project builds without external image-processing libraries.

## Features
- BMP/PNG/JPG/GIF encode and decode
- Drawing primitives (`line`, `circle`, `arc`, `fill`, `fillCircle`)
- RGBA layer compositing with blend modes
- Per-layer masks
- Layer groups with nested stack traversal
- `.iflow` project serialization for preserving full layer stacks
- Unit tests covering codecs, layering, masks, groups, and serialization

## Build
```bash
make
```

## Run Tests
```bash
make test
```

## Output Artifacts
- App output images: `build/output/images`
- Test output artifacts: `build/output/test-images`
