# image-flow

`image-flow` is a C++ image toolkit with raster codecs, a layered compositor, and a deterministic CLI ops engine for procedural image editing.

## Features
- BMP/PNG/JPG/GIF encode and decode
- WebP encode and decode via `cwebp` and `dwebp` tooling (optional)
- Layer and group compositing with transforms, masks, and blend modes
- IFLOW project format for full layer-stack serialization
- CLI ops pipeline for reproducible edits from shell scripts
- Procedural drawing, filtering, color grading, texturing, and geometry ops

## Build
```bash
make
```

Binaries:
- `build/bin/image_flow`
- `build/bin/generate_samples`
- `build/bin/tests`

## CLI Commands
```bash
./build/bin/image_flow help
```

- `image_flow new --width <w> --height <h> --out <project.iflow>`
- `image_flow new --from-image <file> [--fit <w>x<h>] --out <project.iflow>`
- `image_flow info --in <project.iflow>`
- `image_flow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>`
- `image_flow ops --in <project.iflow> --out <project.iflow> --op "<action key=value ...>" [--op ...]`
- `image_flow ops --in <project.iflow> --out <project.iflow> --ops-file <ops.txt>`
- `cat ops.txt | image_flow ops --in <project.iflow> --out <project.iflow> --stdin`

## Blend Modes
- `normal`
- `multiply`
- `screen`
- `overlay`
- `darken`
- `lighten`
- `add`
- `subtract`
- `difference`
- `color-dodge`

## IFLOW Ops
### Structure
- `add-layer`
- `add-group`
- `add-grid-layers`
- `set-layer`
- `set-group`
- `import-image`
- `resize-layer`

### Transform
- `set-transform`
- `concat-transform`
- `clear-transform`

`set-transform` and `concat-transform` support:
- `matrix=a,b,c,d,tx,ty`
- or composed params: `translate=x,y` `scale=s|sx,sy` `skew=degx,degy` `rotate=deg` `pivot=x,y`

### Drawing
- `draw-fill`
- `draw-line`
- `draw-rect`
- `draw-fill-rect`
- `draw-ellipse`
- `draw-fill-ellipse`
- `draw-polyline`
- `draw-polygon`
- `draw-fill-polygon`
- `draw-flood-fill`
- `draw-circle`
- `draw-fill-circle`
- `draw-arc`

All draw ops support `target=image|mask`.
`draw-polyline`, `draw-polygon`, and `draw-fill-polygon` use `points=x0,y0;x1,y1;...`.
`draw-arc` supports `counterclockwise=true|false`.

### Masks and Pixels
- `mask-enable`
- `mask-clear`
- `mask-set-pixel`
- `fill-layer`
- `set-pixel`

### Procedural and Noise
- `gradient-layer` (`linear` or `radial`)
- `checker-layer`
- `noise-layer`
- `fractal-noise`
- `hatch`
- `pencil-strokes`

### Color and Tone
- `apply-effect effect=grayscale|sepia|invert|threshold`
- `replace-color`
- `channel-mix` (3x3 RGB matrix)
- `levels`
- `gamma`
- `curves`

### Filtering and Morphology
- `gaussian-blur`
- `edge-detect method=sobel|canny`
- `morphology op=erode|dilate`

## Example Ops Workflow
```bash
./build/bin/image_flow new --from-image samples/tahoe200-finish.webp --fit 1400x900 --out build/output/images/demo.iflow

./build/bin/image_flow ops \
  --in build/output/images/demo.iflow \
  --out build/output/images/demo.iflow \
  --op "add-layer name=Sketch width=1400 height=900 fill=0,0,0,0" \
  --op "import-image path=/1 file=samples/tahoe200-finish.webp" \
  --op "apply-effect path=/1 effect=grayscale" \
  --op "apply-effect path=/1 effect=invert" \
  --op "gaussian-blur path=/1 radius=8 sigma=3.6" \
  --op "set-layer path=/1 blend=color-dodge opacity=0.95" \
  --render build/output/images/demo.png
```

## Sample Scripts
Run one script:
```bash
./sample_scripts/iflow_pencil_sketch_example.sh
```

Run all scripts:
```bash
./sample_scripts/run_all.sh
# or
make run-scripts
```

Current sample scripts:
- `sample_scripts/iflow_ops_example.sh`
- `sample_scripts/iflow_orbital_wireframe_example.sh`
- `sample_scripts/iflow_mask_etch_example.sh`
- `sample_scripts/iflow_chroma_tunnel_example.sh`
- `sample_scripts/iflow_pencil_sketch_example.sh`

## Sample Generator
```bash
./build/bin/generate_samples
```

## Tests
```bash
make test
```

## Output Artifacts
- App output images: `build/output/images`
- Test output artifacts: `build/output/test-images`
