# image-flow

`image-flow` is a C++ image toolkit with a small raster API, codec support, and a layered compositor.  
The project builds without external image-processing libraries.

## Features
- BMP/PNG/JPG/GIF encode and decode
- WebP encode and decode via `cwebp`/`dwebp` command-line tools (optional)
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

## CLI
```bash
./build/bin/samples help
```

Main commands:
- `./build/bin/samples samples`
- `./build/bin/samples iflow new --width <w> --height <h> --out <project.iflow>`
- `./build/bin/samples iflow info --in <project.iflow>`
- `./build/bin/samples iflow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>`
- `./build/bin/samples iflow ops --in <project.iflow> --out <project.iflow> --op "<action key=value ...>" [--op ...] [--render <image.png>]`

If no command is provided, the CLI defaults to `samples`.

### IFLOW Ops
`iflow ops` applies deterministic edits to an IFLOW document. Useful actions include:
- `add-layer`, `add-group`
- `set-layer`, `set-group`
- `set-transform` (translate/rotate or matrix)
- `apply-effect` (`grayscale`, `sepia`)
- `import-image`, `resize-layer`
- `mask-enable`, `mask-clear`, `mask-set-pixel`
- `fill-layer`, `set-pixel`

Example:
```bash
./build/bin/samples iflow ops \
  --width 512 --height 512 \
  --out build/output/images/demo.iflow \
  --op "add-layer name=Base fill=0,0,0,255" \
  --op "set-layer path=/0 opacity=0.9"
```

Example script:
```bash
./sample_scripts/iflow_ops_example.sh
```

## Run Tests
```bash
make test
```

## Output Artifacts
- App output images: `build/output/images`
- Test output artifacts: `build/output/test-images`
