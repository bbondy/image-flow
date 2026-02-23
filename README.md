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
./build/bin/image_flow help
```

Main commands:
- `./build/bin/image_flow new --width <w> --height <h> --out <project.iflow>`
- `./build/bin/image_flow new --from-image <file> [--fit <w>x<h>] --out <project.iflow>`
- `./build/bin/image_flow info --in <project.iflow>`
- `./build/bin/image_flow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>`
- `./build/bin/image_flow ops --in <project.iflow> --out <project.iflow> --op "<action key=value ...>" [--op ...] [--render <image.png>]`
- `./build/bin/image_flow ops --in <project.iflow> --out <project.iflow> --ops-file <ops.txt> [--render <image.png>]`
- `cat ops.txt | ./build/bin/image_flow ops --in <project.iflow> --out <project.iflow> --stdin`

### IFLOW Ops
`image_flow ops` applies deterministic edits to an IFLOW document. Useful actions include:
- `add-layer`, `add-group`
- `add-grid-layers`
- `set-layer`, `set-group`
- `set-transform`, `concat-transform`, `clear-transform`
- `apply-effect` (`grayscale`, `sepia`, `invert`, `threshold`)
- `import-image`, `resize-layer`
- `gradient-layer` (`linear` or `radial` procedural fill)
- `checker-layer` (procedural checker/stripe fill)
- `noise-layer` (seeded noise perturbation)
- `replace-color` (soft tolerance-based remapping)
- `channel-mix` (3x3 RGB matrix)
- `draw-fill`, `draw-line`, `draw-circle`, `draw-fill-circle`, `draw-arc` (on image or mask target)
- `mask-enable`, `mask-clear`, `mask-set-pixel`
- `fill-layer`, `set-pixel`

Example:
```bash
./build/bin/image_flow new --from-image samples/tahoe200-finish.webp --fit 1200x800 --out build/output/images/demo.iflow
./build/bin/image_flow ops \
  --in build/output/images/demo.iflow \
  --out build/output/images/demo.iflow \
  --op "add-grid-layers rows=4 cols=6 border=10 opacity=0.62 fills=255,80,80,190;255,170,40,190;240,240,70,190;100,210,120,190;70,190,230,190;110,130,255,190;185,110,255,190;255,105,195,190 blends=overlay;screen;lighten;difference" \
  --render build/output/images/demo.png
```

## Sample Generator
```bash
./build/bin/generate_samples
```

Example script:
```bash
./sample_scripts/iflow_ops_example.sh
./sample_scripts/iflow_aurora_glitch_example.sh
./sample_scripts/iflow_kaleido_gridstorm_example.sh
./sample_scripts/iflow_neon_shards_example.sh
./sample_scripts/iflow_orbital_wireframe_example.sh
./sample_scripts/iflow_mask_etch_example.sh
./sample_scripts/iflow_chroma_tunnel_example.sh
```

## Run Tests
```bash
make test
```

## Output Artifacts
- App output images: `build/output/images`
- Test output artifacts: `build/output/test-images`
